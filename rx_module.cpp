#include "rx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточный кодер/декодер
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "libs/key_loader/key_loader.h" // загрузка ключа
#include "libs/crypto/aes_ccm.h" // AES-CCM шифрование
#include "libs/crypto/chacha20_poly1305.h" // AEAD ChaCha20-Poly1305
#include "libs/protocol/ack_utils.h" // проверка ACK для фильтрации буфера
#include "default_settings.h"         // параметры по умолчанию
#include <vector>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <utility>

static constexpr size_t PILOT_INTERVAL = 64;             // период вставки пилотов
static constexpr std::array<uint8_t,7> PILOT_MARKER{{
    0x7E, 'S', 'A', 'T', 'P', 0xD6, 0x9F
}};                                                     // маркер пилота с CRC16
static constexpr size_t PILOT_PREFIX_LEN = PILOT_MARKER.size() - 2; // часть маркера без CRC
static constexpr uint16_t PILOT_MARKER_CRC = 0x9FD6;                // CRC16(prefix)
[[maybe_unused]] static const bool PILOT_MARKER_CRC_OK = []() {
  return FrameHeader::crc16(PILOT_MARKER.data(), PILOT_PREFIX_LEN) == PILOT_MARKER_CRC;
}();

static constexpr uint8_t FRAME_VERSION_AEAD = 2;  // новая версия кадра
static constexpr size_t TAG_LEN_V1 = 8;           // длина тега в старом формате
static constexpr size_t TAG_LEN = crypto::chacha20poly1305::TAG_SIZE; // длина тега Poly1305
static constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина блока данных RS
static constexpr size_t RS_ENC_LEN = 255;      // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга
static constexpr bool USE_RS = DefaultSettings::USE_RS; // использовать RS(255,223)
static constexpr size_t CONV_TAIL_BYTES = 1;      // «хвост» для сброса регистра свёрточного кодера
static constexpr std::chrono::seconds PENDING_CONV_TTL(10); // максимальное время жизни незавершённого блока
static bool isDecimal(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// Проверяем, соответствует ли подпоследовательность новому пилотному маркеру
static bool isPilotMarker(const uint8_t* data, size_t len) {
  if (!data || len < PILOT_MARKER.size()) return false;
  if (!std::equal(PILOT_MARKER.begin(), PILOT_MARKER.begin() + PILOT_PREFIX_LEN, data)) {
    return false;
  }
  uint16_t crc = static_cast<uint16_t>(data[PILOT_PREFIX_LEN]) |
                 (static_cast<uint16_t>(data[PILOT_PREFIX_LEN + 1]) << 8);
  if (crc != PILOT_MARKER_CRC) return false;
  return FrameHeader::crc16(data, PILOT_PREFIX_LEN) == crc;
}

// Удаление пилотов из полезной нагрузки
static void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) {               // проверка указателя и длины
    return;
  }
  out.reserve(len);
  size_t count = 0;
  size_t i = 0;
  while (i < len) {
    if (count && count % PILOT_INTERVAL == 0) {
      size_t remaining = len - i;
      if (remaining >= PILOT_MARKER.size() && isPilotMarker(data + i, remaining)) {
        i += PILOT_MARKER.size();
        continue; // пропускаем весь маркер
      }
    }
    out.push_back(data[i]);
    ++count;
    ++i;
  }
}

static constexpr size_t COMPACT_HEADER_SIZE = 7; // версия, frag_cnt и packed

static std::array<uint8_t,COMPACT_HEADER_SIZE> makeCompactHeader(uint8_t version,
                                                                 uint16_t frag_cnt,
                                                                 uint32_t packed_meta) {
  std::array<uint8_t,COMPACT_HEADER_SIZE> compact{};
  compact[0] = version;
  compact[1] = static_cast<uint8_t>(frag_cnt >> 8);
  compact[2] = static_cast<uint8_t>(frag_cnt);
  compact[3] = static_cast<uint8_t>(packed_meta >> 24);
  compact[4] = static_cast<uint8_t>(packed_meta >> 16);
  compact[5] = static_cast<uint8_t>(packed_meta >> 8);
  compact[6] = static_cast<uint8_t>(packed_meta);
  return compact;
}

static std::array<uint8_t,COMPACT_HEADER_SIZE + 2> makeAad(uint8_t version,
                                                           uint16_t frag_cnt,
                                                           uint32_t packed_meta,
                                                           uint16_t msg_id) {
  auto compact = makeCompactHeader(version, frag_cnt, packed_meta);
  std::array<uint8_t,COMPACT_HEADER_SIZE + 2> aad{};
  std::copy(compact.begin(), compact.end(), aad.begin());
  aad[COMPACT_HEADER_SIZE] = static_cast<uint8_t>(msg_id >> 8);
  aad[COMPACT_HEADER_SIZE + 1] = static_cast<uint8_t>(msg_id);
  return aad;
}

struct RxModule::RxProfilingScope {
  RxModule& owner;                                        // ссылка на модуль для доступа к состоянию
  ProfilingSnapshot snapshot;                             // формируемый снимок
  bool enabled = false;                                   // активна ли диагностика
  std::chrono::steady_clock::time_point start{};          // начало обработки
  std::chrono::steady_clock::time_point stage_start{};    // отметка последнего этапа
  bool drop_recorded = false;                             // зарегистрирована ли причина дропа

  explicit RxProfilingScope(RxModule& module, size_t raw_len)
      : owner(module), enabled(module.profiling_enabled_) {
    if (enabled) {
      snapshot.valid = true;
      snapshot.raw_len = raw_len;
      start = stage_start = std::chrono::steady_clock::now();
    }
  }

  void mark(std::chrono::microseconds ProfilingSnapshot::*field) {
    if (!enabled) return;
    auto now = std::chrono::steady_clock::now();
    snapshot.*field = std::chrono::duration_cast<std::chrono::microseconds>(now - stage_start);
    stage_start = now;
  }

  void noteDecodedLen(size_t len) {
    if (!enabled) return;
    snapshot.decoded_len = len;
  }

  void noteConv(bool used) {
    if (!enabled) return;
    snapshot.conv = used;
  }

  void noteDecrypt(bool performed) {
    if (!enabled) return;
    snapshot.decrypted = performed;
  }

  void markDrop(const char* stage) {
    if (!drop_recorded) {
      owner.registerDrop(stage ? stage : "");           // фиксируем причину независимо от профилирования
      drop_recorded = true;
    }
    if (!enabled) return;
    if (!snapshot.dropped) {
      snapshot.dropped = true;
      snapshot.drop_stage = stage ? stage : "";
    }
  }

  ~RxProfilingScope() {
    if (!enabled) return;
    auto finish = std::chrono::steady_clock::now();
    snapshot.total = std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
    owner.last_profile_ = snapshot;
  }
};

// Конструктор модуля приёма
RxModule::RxModule()
    : gatherer_(PayloadMode::SMALL, DefaultSettings::GATHER_BLOCK_SIZE),
      key_(KeyLoader::loadKey()) { // ключ для последующего дешифрования
  last_conv_cleanup_ = std::chrono::steady_clock::now(); // отметка для фоновой очистки кэша свёртки
}

// Передаём данные колбэку, если заголовок валиден
void RxModule::onReceive(const uint8_t* data, size_t len) {
  RxProfilingScope profile_scope(*this, len);           // начинаем измерение пути кадра
  if (!data || len == 0) {                              // защита от пустого указателя
    profile_scope.markDrop("пустой кадр");
    return;
  }

  auto now = std::chrono::steady_clock::now();          // фиксируем момент для очистки временных структур
  cleanupPendingConv(now);
  cleanupPendingSplits(now);

  if (len == 1 && data[0] == protocol::ack::MARKER) {
    if (ack_cb_) {
      ack_cb_();                                     // немедленно уведомляем передатчик
    }
    if (cb_) {                                       // пробрасываем полезную нагрузку для совместимости
      cb_(data, len);
    }
    profile_scope.mark(&ProfilingSnapshot::deliver);
    return;                                          // компактный ACK обработан, остальные шаги не нужны
  }

  auto forward_raw = [&](const uint8_t* raw, size_t raw_len) {
    if (!raw || raw_len == 0) return;                  // пропускаем пустые вызовы
    if (buf_) {                                        // сохраняем сырые пакеты по отдельному счётчику
      buf_->pushRaw(0, raw_counter_++, raw, raw_len);
    }
    if (cb_) {                                         // передаём оригинальные данные напрямую
      cb_(raw, raw_len);
    }
    profile_scope.mark(&ProfilingSnapshot::deliver);   // фиксируем передачу наружу
  };

  if (len < FrameHeader::SIZE) {                        // недостаточно данных для заголовка
    forward_raw(data, len);
    profile_scope.markDrop("короткий кадр без заголовка");
    return;
  }

  if (!cb_ && !buf_) {                                 // некому отдавать результат
    profile_scope.markDrop("нет приёмника данных");
    return;
  }

  frame_buf_.assign(data, data + len);                        // переиспользуемый буфер кадра
  scrambler::descramble(frame_buf_.data(), frame_buf_.size()); // дескремблируем весь кадр
  profile_scope.mark(&ProfilingSnapshot::descramble);

  FrameHeader primary_hdr;
  FrameHeader secondary_hdr;
  bool primary_ok = FrameHeader::decode(frame_buf_.data(), frame_buf_.size(), primary_hdr);
  const bool has_second_header = frame_buf_.size() >= FrameHeader::SIZE * 2;
  bool duplicate_confirmed = false;
  if (has_second_header) {
    duplicate_confirmed = std::equal(frame_buf_.begin(), frame_buf_.begin() + FrameHeader::SIZE,
                                     frame_buf_.begin() + FrameHeader::SIZE);
  }
  bool legacy_frame = primary_ok && primary_hdr.ver < FRAME_VERSION_AEAD;
  bool secondary_ok = false;
  if (has_second_header && (duplicate_confirmed || legacy_frame)) {
    secondary_ok = FrameHeader::decode(frame_buf_.data() + FrameHeader::SIZE,
                                       frame_buf_.size() - FrameHeader::SIZE, secondary_hdr);
  }
  profile_scope.mark(&ProfilingSnapshot::header);
  if (!primary_ok && !secondary_ok) {                   // заголовок не распознан
    forward_raw(data, len);
    profile_scope.markDrop("заголовок повреждён");
    return;
  }

  FrameHeader hdr = primary_ok ? primary_hdr : secondary_hdr;
  bool headers_match = false;
  if (primary_ok && secondary_ok) {
    headers_match = (primary_hdr.ver == secondary_hdr.ver) &&
                    (primary_hdr.msg_id == secondary_hdr.msg_id) &&
                    (primary_hdr.getFragIdx() == secondary_hdr.getFragIdx()) &&
                    (primary_hdr.frag_cnt == secondary_hdr.frag_cnt) &&
                    (primary_hdr.getFlags() == secondary_hdr.getFlags()) &&
                    (primary_hdr.getPayloadLen() == secondary_hdr.getPayloadLen());
    if (!headers_match && duplicate_confirmed) {
      LOG_WARN("RxModule: обнаружено расхождение копий заголовка");
    }
  }

  size_t payload_offset = FrameHeader::SIZE;
  if (!primary_ok && secondary_ok) {
    payload_offset = FrameHeader::SIZE * 2;                // принимаем старый формат с дублированным заголовком
  }

  auto extract_payload = [&](size_t offset) {
    const uint8_t* payload_p = frame_buf_.data() + offset;
    size_t payload_len = frame_buf_.size() - offset;
    removePilots(payload_p, payload_len, payload_buf_);
  };

  extract_payload(payload_offset);
  if (payload_buf_.size() != hdr.getPayloadLen() && headers_match && frame_buf_.size() >= FrameHeader::SIZE * 2) {
    payload_offset = FrameHeader::SIZE * 2;               // пробуем интерпретацию старого формата
    extract_payload(payload_offset);
  }
  profile_scope.mark(&ProfilingSnapshot::payload_extract);
  if (payload_buf_.size() != hdr.getPayloadLen()) {
    profile_scope.markDrop("несовпадение длины payload");
    return;
  }
  const uint8_t hdr_flags = hdr.getFlags();
  const bool ack_no_flags = (hdr_flags &
                             (FrameHeader::FLAG_ENCRYPTED | FrameHeader::FLAG_CONV_ENCODED)) == 0;
  const bool ack_single_fragment = hdr.frag_cnt == 1 && hdr.getFragIdx() == 0;
  const bool ack_marker_only = payload_buf_.size() == 1 && payload_buf_.front() == protocol::ack::MARKER;
  if (ack_no_flags && ack_single_fragment && ack_marker_only) {
    profile_scope.noteConv(false);                         // подтверждение приходит без кодирования
    profile_scope.noteDecrypt(false);                      // шифрование не применяется
    profile_scope.mark(&ProfilingSnapshot::decode);        // отмечаем обход декодера
    profile_scope.mark(&ProfilingSnapshot::decrypt);       // и отсутствие дешифрования
    if (ack_cb_) {
      ack_cb_();                                           // уведомляем о полученном подтверждении
    }
    if (cb_) {
      cb_(payload_buf_.data(), payload_buf_.size());       // пробрасываем маркер наружу
    }
    profile_scope.mark(&ProfilingSnapshot::deliver);
    return;                                                // ACK обработан, дальнейшие этапы не нужны
  }

  if (!assembling_ || hdr.msg_id != active_msg_id_) {       // обнаружили новое сообщение
    if (assembling_) {
      inflight_prefix_.erase(active_msg_id_);
    }
    gatherer_.reset();
    assembling_ = true;
    active_msg_id_ = hdr.msg_id;
    expected_frag_cnt_ = hdr.frag_cnt;
    next_frag_idx_ = 0;
  }
  if (hdr.getFragIdx() == 0) {                             // явный старт новой последовательности
    if (next_frag_idx_ != 0) {
      inflight_prefix_.erase(active_msg_id_);
      gatherer_.reset();
    }
    expected_frag_cnt_ = hdr.frag_cnt;
    next_frag_idx_ = 0;
  }
  if (hdr.getFragIdx() != next_frag_idx_) {                // пришёл неожиданный индекс
    if (hdr.getFragIdx() != 0) {
      inflight_prefix_.erase(active_msg_id_);
      gatherer_.reset();
      assembling_ = false;
      expected_frag_cnt_ = 0;
      next_frag_idx_ = 0;
      profile_scope.markDrop("нарушена последовательность фрагментов");
      return;                                              // дожидаемся корректной последовательности
    }
    next_frag_idx_ = 0;
  }

  // Деинтерливинг и декодирование
  result_buf_.clear();
  work_buf_.clear();
  size_t result_len = 0;
  const bool conv_flag = (hdr_flags & FrameHeader::FLAG_CONV_ENCODED) != 0;
  profile_scope.noteConv(conv_flag);
  size_t cipher_len_hint = 0;
  if (conv_flag) {
    if (hdr.getPayloadLen() < CONV_TAIL_BYTES * 2 || (hdr.getPayloadLen() % 2) != 0) {
      profile_scope.markDrop("некорректная длина свёрточного блока");
      return;
    }
    cipher_len_hint = static_cast<size_t>(hdr.getPayloadLen() / 2);
    if (cipher_len_hint < CONV_TAIL_BYTES) {
      profile_scope.markDrop("слишком короткий свёрточный блок");
      return;
    }
    cipher_len_hint -= CONV_TAIL_BYTES;
  }
  std::vector<uint8_t> assembled_payload;
  std::vector<uint8_t>* payload_ptr = &payload_buf_;

  if (conv_flag) {
    size_t expected_conv_len = cipher_len_hint ? static_cast<size_t>(cipher_len_hint + CONV_TAIL_BYTES) * 2 : 0;
    const uint64_t conv_key = (static_cast<uint64_t>(hdr.msg_id) << 32) | hdr.getFragIdx();
    if (expected_conv_len && payload_buf_.size() < expected_conv_len) {
      auto [it, inserted] = pending_conv_.try_emplace(conv_key);
      auto& slot = it->second;
      if (inserted) {
        slot.last_update = now;
      }
      if (slot.expected_len != expected_conv_len) {
        slot.expected_len = expected_conv_len;         // запоминаем ожидаемую длину
        slot.data.clear();                             // обнуляем накопленный буфер
      }
      slot.data.insert(slot.data.end(), payload_buf_.begin(), payload_buf_.end());
      slot.last_update = now;
      trimPendingConv();
      profile_scope.markDrop("ожидание продолжения свёрточного блока");
      return;                                          // ждём остальные части до полного блока
    }
    if (expected_conv_len && payload_buf_.size() > expected_conv_len) {
      pending_conv_.erase(conv_key);
      profile_scope.markDrop("переполнение свёрточного блока");
      return;
    }
    auto it = pending_conv_.find(conv_key);
    if (it != pending_conv_.end()) {
      it->second.data.insert(it->second.data.end(), payload_buf_.begin(), payload_buf_.end());
      it->second.last_update = now;
      trimPendingConv();
      if (!it->second.expected_len && expected_conv_len) {
        it->second.expected_len = expected_conv_len;  // инициализация ожидания если не было
      }
      if (it->second.expected_len && it->second.data.size() == it->second.expected_len) {
        assembled_payload.swap(it->second.data);
        pending_conv_.erase(it);
        payload_ptr = &assembled_payload;
      } else {
        if (!it->second.expected_len || it->second.data.size() < it->second.expected_len) {
          profile_scope.markDrop("ожидание хвоста свёрточного блока");
          return;                                      // ждём продолжение
        }
        pending_conv_.erase(it);                       // превышение длины — ошибка
        profile_scope.markDrop("сбой длины свёрточного блока");
        return;
      }
    }
  } else {
    const uint64_t conv_key = (static_cast<uint64_t>(hdr.msg_id) << 32) | hdr.getFragIdx();
    pending_conv_.erase(conv_key);                    // очищаем возможные остатки при смене режима
  }

  bool decode_ok = true;
  if (conv_flag) {
    if (!payload_ptr->empty() && USE_BIT_INTERLEAVER) {
      bit_interleaver::deinterleave(payload_ptr->data(), payload_ptr->size());
    }
    if (!conv_codec::viterbiDecode(payload_ptr->data(), payload_ptr->size(), result_buf_)) {
      decode_ok = false;
    } else {
      if (cipher_len_hint) {
        size_t required = static_cast<size_t>(cipher_len_hint) + CONV_TAIL_BYTES;
        if (result_buf_.size() < required) {
          decode_ok = false;                           // получили меньше, чем ожидалось
        } else {
          if (result_buf_.size() > required) {
            result_buf_.resize(required);             // отбрасываем лишние байты декодера
          }
          if (result_buf_.size() >= CONV_TAIL_BYTES) {
            result_buf_.erase(result_buf_.end() - CONV_TAIL_BYTES, result_buf_.end());
          }
        }
      }
      result_len = result_buf_.size();
    }
  } else if (USE_RS && payload_buf_.size() == RS_ENC_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // деинтерливинг бит
    if (!conv_codec::viterbiDecode(payload_buf_.data(), payload_buf_.size(), work_buf_)) {
      decode_ok = false;
    } else {
      if (!work_buf_.empty())
        byte_interleaver::deinterleave(work_buf_.data(), work_buf_.size()); // байтовый деинтерливинг
      result_buf_.resize(RS_DATA_LEN);
      if (!rs255223::decode(work_buf_.data(), result_buf_.data())) {
        decode_ok = false;
      } else {
        result_len = RS_DATA_LEN;
      }
    }
  } else if (USE_RS && payload_buf_.size() == RS_ENC_LEN) {
    byte_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // байтовый деинтерливинг
    result_buf_.resize(RS_DATA_LEN);
    if (!rs255223::decode(payload_buf_.data(), result_buf_.data())) {
      decode_ok = false;
    } else {
      result_len = RS_DATA_LEN;
    }
  } else if (!USE_RS && payload_buf_.size() == RS_DATA_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // деинтерливинг бит
    if (!conv_codec::viterbiDecode(payload_buf_.data(), payload_buf_.size(), result_buf_)) {
      decode_ok = false;
    } else {
      result_len = result_buf_.size();
    }
  } else {
    result_buf_.assign(payload_buf_.begin(), payload_buf_.end());  // кадр без кодирования
    result_len = result_buf_.size();
  }

  profile_scope.noteDecodedLen(result_len);
  profile_scope.mark(&ProfilingSnapshot::decode);
  if (!decode_ok) {
    profile_scope.markDrop("сбой декодирования");
    return;
  }

  // Дешифрование и сборка сообщения
  size_t tag_len = (hdr.ver >= FRAME_VERSION_AEAD) ? TAG_LEN : TAG_LEN_V1;
  if (result_len < tag_len) {
    profile_scope.markDrop("payload короче тега");
    return;                                          // недостаточно данных
  }
  const uint8_t* cipher = result_buf_.data();
  size_t cipher_len = result_len - tag_len;
  const uint8_t* tag = result_buf_.data() + cipher_len;
  bool encrypted = (hdr_flags & FrameHeader::FLAG_ENCRYPTED) != 0;
  bool should_decrypt = encrypted || encryption_forced_;
  bool decrypt_ok = false;
  if (should_decrypt) {
    nonce_ = KeyLoader::makeNonce(hdr.ver, hdr.frag_cnt, hdr.packed, hdr.msg_id); // packed содержит флаги, индекс и длину
    if (hdr.ver >= FRAME_VERSION_AEAD) {
      auto aad = makeAad(hdr.ver, hdr.frag_cnt, hdr.packed, hdr.msg_id);
      decrypt_ok = crypto::chacha20poly1305::decrypt(key_.data(), key_.size(),
                                                     nonce_.data(), nonce_.size(),
                                                     aad.data(), aad.size(),
                                                     cipher, cipher_len,
                                                     tag, tag_len,
                                                     plain_buf_);
    } else {
      decrypt_ok = decrypt_ccm(key_.data(), key_.size(), nonce_.data(), nonce_.size(),
                               nullptr, 0, cipher, cipher_len,
                               tag, tag_len, plain_buf_);
    }
    if (!decrypt_ok) {
      if (encrypted) {
        LOG_ERROR("RxModule: ошибка дешифрования");
        profile_scope.mark(&ProfilingSnapshot::decrypt);
        profile_scope.markDrop("ошибка AEAD");
        return;                                      // завершаем обработку повреждённого кадра
      }
      LOG_WARN("RxModule: получен незашифрованный фрагмент при принудительном режиме");
    }
  }
  profile_scope.noteDecrypt(should_decrypt);
  if (!should_decrypt || (!decrypt_ok && !encrypted)) {
    plain_buf_.assign(result_buf_.begin(), result_buf_.begin() + cipher_len); // принимаем открытый текст
  }
  profile_scope.mark(&ProfilingSnapshot::decrypt);

  size_t prefix_len = 0;
  SplitPrefixInfo split_info = parseSplitPrefix(plain_buf_, prefix_len);
  if (split_info.valid) {
    inflight_prefix_[hdr.msg_id] = split_info;
  } else {
    auto it_pref = inflight_prefix_.find(hdr.msg_id);
    if (it_pref != inflight_prefix_.end()) {
      split_info = it_pref->second;
    }
  }
  if (prefix_len > 0 && prefix_len <= plain_buf_.size()) {
    plain_buf_.erase(plain_buf_.begin(), plain_buf_.begin() + static_cast<std::ptrdiff_t>(prefix_len));
  }
  gatherer_.add(plain_buf_.data(), plain_buf_.size());
  plain_buf_.clear();                                   // очищаем буфер, сохраняя вместимость
  profile_scope.mark(&ProfilingSnapshot::assemble);

  ++next_frag_idx_;                                     // ожидаем следующий индекс фрагмента
  if (hdr.getFragIdx() + 1 == hdr.frag_cnt) {          // последний фрагмент
    const auto& full = gatherer_.get();
    auto split_result = handleSplitPart(split_info, full, hdr.msg_id);
    if (!split_result.deliver) {
      assembling_ = false;
      expected_frag_cnt_ = 0;
      next_frag_idx_ = 0;
      profile_scope.markDrop("ожидание остальных частей split");
      return;                                         // результат ещё не готов
    }
    const uint8_t* deliver_ptr = full.data();
    size_t deliver_len = full.size();
    if (!split_result.use_original) {
      deliver_ptr = split_result.data.data();
      deliver_len = split_result.data.size();
    }
    bool is_ack_payload = protocol::ack::isAckPayload(deliver_ptr, deliver_len);
    if (is_ack_payload && ack_cb_) {
      ack_cb_();
    }
    if (buf_ && !is_ack_payload) {
      buf_->pushReady(hdr.msg_id, deliver_ptr, deliver_len);
    }
    if (cb_) {
      cb_(deliver_ptr, deliver_len);
    }
    profile_scope.mark(&ProfilingSnapshot::deliver);

    inflight_prefix_.erase(hdr.msg_id);
    gatherer_.reset();
    assembling_ = false;
    expected_frag_cnt_ = 0;
    next_frag_idx_ = 0;
  }
}

void RxModule::cleanupPendingConv(std::chrono::steady_clock::time_point now) {
  if (pending_conv_.empty()) return;                      // нечего очищать
  if (!assembling_ && now - last_conv_cleanup_ < std::chrono::seconds(1)) return; // не запускаем очистку слишком часто
  last_conv_cleanup_ = now;
  for (auto it = pending_conv_.begin(); it != pending_conv_.end();) {
    if (now - it->second.last_update > PENDING_CONV_TTL) {
      it = pending_conv_.erase(it);                       // удаляем устаревший хвост
    } else {
      ++it;
    }
  }
  trimPendingConv();
}

void RxModule::cleanupPendingSplits(std::chrono::steady_clock::time_point now) {
  if (pending_split_.empty()) return;
  for (auto it = pending_split_.begin(); it != pending_split_.end();) {
    if (now - it->second.last_update > PENDING_SPLIT_TTL) {
      it = pending_split_.erase(it);
    } else {
      ++it;
    }
  }
  trimPendingSplits();
}

void RxModule::trimPendingConv() {
  while (pending_conv_.size() > PENDING_CONV_LIMIT) {
    auto victim = std::min_element(pending_conv_.begin(), pending_conv_.end(),
                                   [](const auto& lhs, const auto& rhs) {
                                     return lhs.second.last_update < rhs.second.last_update;
                                   });
    if (victim == pending_conv_.end()) break;
    pending_conv_.erase(victim);
  }
}

void RxModule::trimPendingSplits() {
  while (pending_split_.size() > PENDING_SPLIT_LIMIT) {
    auto victim = std::min_element(pending_split_.begin(), pending_split_.end(),
                                   [](const auto& lhs, const auto& rhs) {
                                     return lhs.second.last_update < rhs.second.last_update;
                                   });
    if (victim == pending_split_.end()) break;
    pending_split_.erase(victim);
  }
}

void RxModule::tickCleanup() {
  auto now = std::chrono::steady_clock::now();
  cleanupPendingConv(now);
  cleanupPendingSplits(now);
}

void RxModule::resetDropStats() {
  drop_stats_ = DropStats{};                              // обнуляем счётчики причин дропа
}

RxModule::SplitPrefixInfo RxModule::parseSplitPrefix(const std::vector<uint8_t>& data, size_t& prefix_len) const {
  SplitPrefixInfo info;
  prefix_len = 0;
  if (data.empty() || data.front() != '[') return info;
  auto end = std::find(data.begin(), data.end(), ']');
  if (end == data.end()) return info;
  std::string content(data.begin() + 1, end);
  auto pipe = content.find('|');
  if (pipe == std::string::npos || pipe == 0) return info;
  info.tag = content.substr(0, pipe);
  std::string rest = content.substr(pipe + 1);
  size_t slash = rest.find('/');
  std::string part_str = rest.substr(0, slash);
  if (!isDecimal(part_str)) return info;
  info.part = static_cast<size_t>(std::stoul(part_str));
  if (slash != std::string::npos) {
    std::string total_str = rest.substr(slash + 1);
    if (!isDecimal(total_str)) return info;
    info.total = static_cast<size_t>(std::stoul(total_str));
  }
  if (info.total == 0 || info.part == 0 || info.part > info.total) {
    info.valid = false;
  } else {
    info.valid = true;
  }
  prefix_len = static_cast<size_t>(std::distance(data.begin(), end + 1));
  return info;
}

RxModule::SplitProcessResult RxModule::handleSplitPart(const SplitPrefixInfo& info,
                                                       const std::vector<uint8_t>& chunk,
                                                       uint32_t msg_id) {
  SplitProcessResult res;
  if (!info.valid) {
    res.deliver = true;
    res.use_original = true;
    return res;
  }
  auto now = std::chrono::steady_clock::now();
  auto [slot_it, inserted] = pending_split_.try_emplace(info.tag);
  auto& slot = slot_it->second;
  if (inserted) {
    slot.last_update = now;
  }
  if (info.part == 1 || slot.expected_total != info.total) {
    slot.data.clear();
    slot.next_part = 1;
    slot.expected_total = info.total;
  }
  if (info.part != slot.next_part) {
    slot.data.clear();
    slot.next_part = 1;
    slot.expected_total = info.total;
    if (info.part != 1) {
      slot.last_update = now;
      return res;
    }
  }
  slot.data.insert(slot.data.end(), chunk.begin(), chunk.end());
  slot.last_update = now;
  trimPendingSplits();
  slot.next_part = info.part + 1;
  if (info.part < info.total) {
    if (buf_) {
      buf_->pushSplit(msg_id, slot.data.data(), slot.data.size());
    }
    return res;
  }
  res.deliver = true;
  res.use_original = false;
  res.data = std::move(slot.data);
  pending_split_.erase(info.tag);
  return res;
}

void RxModule::registerDrop(const std::string& stage) {
  std::string key = stage.empty() ? std::string("не указано") : stage; // подставляем понятный ярлык
  ++drop_stats_.total;                                                  // суммарный счётчик
  ++drop_stats_.by_stage[key];                                          // накопление по причинам
}

void RxModule::enableProfiling(bool enable) {
  profiling_enabled_ = enable;                       // фиксируем текущее состояние профилирования
  if (!enable) {
    last_profile_ = ProfilingSnapshot{};             // сбрасываем предыдущий снимок
  }
}

// Установка колбэка для обработки сообщений
void RxModule::setCallback(Callback cb) {
  cb_ = cb;
}

// Привязка обработчика, который уведомляется о приходе компактного ACK
void RxModule::setAckCallback(std::function<void()> cb) {
  ack_cb_ = std::move(cb);
}

// Указание внешнего буфера для сохранения готовых сообщений
void RxModule::setBuffer(ReceivedBuffer* buf) {
  buf_ = buf;
}

void RxModule::reloadKey() {
  key_ = KeyLoader::loadKey();
  DEBUG_LOG("RxModule: ключ перечитан");
}

void RxModule::setEncryptionEnabled(bool enabled) {
  encryption_forced_ = enabled;
}
