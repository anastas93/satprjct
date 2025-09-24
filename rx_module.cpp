#include "rx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточный кодер/декодер
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "libs/key_loader/key_loader.h" // загрузка ключа
#include "libs/crypto/aes_ccm.h" // AES-CCM шифрование
#include "default_settings.h"         // параметры по умолчанию
#include <vector>
#include <algorithm>
#include <array>
#include <chrono>

static constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина блока данных RS
static constexpr size_t TAG_LEN = 8;              // длина тега аутентичности
static constexpr size_t RS_ENC_LEN = 255;      // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга
static constexpr bool USE_RS = DefaultSettings::USE_RS; // использовать RS(255,223)
static constexpr size_t CONV_TAIL_BYTES = 1;      // «хвост» для сброса регистра свёрточного кодера
static constexpr std::chrono::seconds PENDING_CONV_TTL(10); // максимальное время жизни незавершённого блока

// Удаление пилотов из полезной нагрузки
static void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) {               // проверка указателя и длины
    return;
  }
  out.reserve(len);
  size_t count = 0;
  for (size_t i = 0; i < len; ++i) {
    if (count && count % 64 == 0 && i + 1 < len && data[i] == 0x55 && data[i + 1] == 0x2D) {
      i++; // пропускаем пилот
      continue;
    }
    out.push_back(data[i]);
    ++count;
  }
}

// Конструктор модуля приёма
RxModule::RxModule()
    : gatherer_(PayloadMode::SMALL, DefaultSettings::GATHER_BLOCK_SIZE),
      key_(KeyLoader::loadKey()) { // ключ для последующего дешифрования
  last_conv_cleanup_ = std::chrono::steady_clock::now(); // отметка для фоновой очистки кэша свёртки
}

// Передаём данные колбэку, если заголовок валиден
void RxModule::onReceive(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;                     // защита от пустого указателя

  auto now = std::chrono::steady_clock::now();       // фиксируем момент для очистки временных структур
  cleanupPendingConv(now);

  auto forward_raw = [&](const uint8_t* raw, size_t raw_len) {
    if (!raw || raw_len == 0) return;               // пропускаем пустые вызовы
    if (buf_) {                                     // сохраняем сырые пакеты по отдельному счётчику
      buf_->pushRaw(0, raw_counter_++, raw, raw_len);
    }
    if (cb_) {                                      // передаём оригинальные данные напрямую
      cb_(raw, raw_len);
    }
  };

  if (len < FrameHeader::SIZE * 2) {                // недостаточно данных для заголовков
    forward_raw(data, len);
    return;
  }

  if (!cb_ && !buf_) return;                        // некому отдавать результат

  frame_buf_.assign(data, data + len);                     // переиспользуемый буфер кадра
  scrambler::descramble(frame_buf_.data(), frame_buf_.size()); // дескремблируем весь кадр

  FrameHeader hdr;
  bool ok = FrameHeader::decode(frame_buf_.data(), frame_buf_.size(), hdr);
  if (!ok)
    ok = FrameHeader::decode(frame_buf_.data() + FrameHeader::SIZE,
                             frame_buf_.size() - FrameHeader::SIZE, hdr);
  if (!ok) {                                         // оба заголовка повреждены
    forward_raw(data, len);
    return;
  }

  const uint8_t* payload_p = frame_buf_.data() + FrameHeader::SIZE * 2;
  size_t payload_len = frame_buf_.size() - FrameHeader::SIZE * 2;
  removePilots(payload_p, payload_len, payload_buf_);      // убираем пилоты без выделений
  if (payload_buf_.size() != hdr.payload_len) return;      // несоответствие длины
  if (!hdr.checkFrameCrc(payload_buf_.data(), payload_buf_.size())) return; // неверный CRC

  if (!assembling_ || hdr.msg_id != active_msg_id_) {      // обнаружили новое сообщение
    gatherer_.reset();
    assembling_ = true;
    active_msg_id_ = hdr.msg_id;
    expected_frag_cnt_ = hdr.frag_cnt;
    next_frag_idx_ = 0;
  }
  if (hdr.frag_idx == 0) {                                // явный старт новой последовательности
    if (next_frag_idx_ != 0) {
      gatherer_.reset();
    }
    expected_frag_cnt_ = hdr.frag_cnt;
    next_frag_idx_ = 0;
  }
  if (hdr.frag_idx != next_frag_idx_) {                   // пришёл неожиданный индекс
    if (hdr.frag_idx != 0) {
      gatherer_.reset();
      assembling_ = false;
      expected_frag_cnt_ = 0;
      next_frag_idx_ = 0;
      return;                                             // дожидаемся корректной последовательности
    }
    next_frag_idx_ = 0;
  }

  // Деинтерливинг и декодирование
  result_buf_.clear();
  work_buf_.clear();
  size_t result_len = 0;
  const bool conv_flag = (hdr.flags & FrameHeader::FLAG_CONV_ENCODED) != 0;
  size_t cipher_len_hint = 0;
  if (conv_flag) {
    if (hdr.payload_len < CONV_TAIL_BYTES * 2 || (hdr.payload_len % 2) != 0) {
      return;                                             // некорректная длина для свёрточного блока
    }
    cipher_len_hint = static_cast<size_t>(hdr.payload_len / 2);
    if (cipher_len_hint < CONV_TAIL_BYTES) {
      return;                                             // невозможная длина
    }
    cipher_len_hint -= CONV_TAIL_BYTES;
  }
  std::vector<uint8_t> assembled_payload;
  std::vector<uint8_t>* payload_ptr = &payload_buf_;

  if (conv_flag) {
    size_t expected_conv_len = cipher_len_hint ? static_cast<size_t>(cipher_len_hint + CONV_TAIL_BYTES) * 2 : 0;
    const uint64_t conv_key = (static_cast<uint64_t>(hdr.msg_id) << 32) | hdr.frag_idx;
    if (expected_conv_len && payload_buf_.size() < expected_conv_len) {
      auto& slot = pending_conv_[conv_key];
      if (slot.expected_len != expected_conv_len) {
        slot.expected_len = expected_conv_len;        // запоминаем ожидаемую длину
        slot.data.clear();                            // обнуляем накопленный буфер
      }
      slot.data.insert(slot.data.end(), payload_buf_.begin(), payload_buf_.end());
      slot.last_update = now;
      if (slot.data.size() < expected_conv_len) {
        return;                                      // ждём остальные части до полного блока
      }
      if (slot.data.size() > expected_conv_len) {
        pending_conv_.erase(conv_key);               // превышение означает ошибку, сбрасываем
        return;
      }
      assembled_payload.swap(slot.data);
      pending_conv_.erase(conv_key);
      payload_ptr = &assembled_payload;
    } else {
      auto it = pending_conv_.find(conv_key);
      if (it != pending_conv_.end()) {
        it->second.data.insert(it->second.data.end(), payload_buf_.begin(), payload_buf_.end());
        it->second.last_update = now;
        if (!it->second.expected_len && expected_conv_len) {
          it->second.expected_len = expected_conv_len; // инициализация ожидания если не было
        }
        if (it->second.expected_len && it->second.data.size() == it->second.expected_len) {
          assembled_payload.swap(it->second.data);
          pending_conv_.erase(it);
          payload_ptr = &assembled_payload;
        } else {
          if (!it->second.expected_len || it->second.data.size() < it->second.expected_len) {
            return;                                  // ждём продолжение
          }
          pending_conv_.erase(it);                   // превышение длины — ошибка
          return;
        }
      } else if (expected_conv_len && payload_buf_.size() > expected_conv_len) {
        return;                                      // пришло больше, чем ожидалось
      }
    }
  } else {
    const uint64_t conv_key = (static_cast<uint64_t>(hdr.msg_id) << 32) | hdr.frag_idx;
    pending_conv_.erase(conv_key);                   // очищаем возможные остатки при смене режима
  }

  if (conv_flag) {
    if (!payload_ptr->empty() && USE_BIT_INTERLEAVER) {
      bit_interleaver::deinterleave(payload_ptr->data(), payload_ptr->size());
    }
    if (!conv_codec::viterbiDecode(payload_ptr->data(), payload_ptr->size(), result_buf_)) return;
    if (cipher_len_hint) {
      size_t required = static_cast<size_t>(cipher_len_hint) + CONV_TAIL_BYTES;
      if (result_buf_.size() < required) {
        return;                                      // получили меньше, чем ожидалось
      }
      if (result_buf_.size() > required) {
        result_buf_.resize(required);                // отбрасываем лишние байты декодера
      }
      if (result_buf_.size() >= CONV_TAIL_BYTES) {
        result_buf_.erase(result_buf_.end() - CONV_TAIL_BYTES, result_buf_.end());
      }
    }
    result_len = result_buf_.size();
  } else if (USE_RS && payload_buf_.size() == RS_ENC_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // деинтерливинг бит
    if (!conv_codec::viterbiDecode(payload_buf_.data(), payload_buf_.size(), work_buf_)) return;
    if (!work_buf_.empty())
      byte_interleaver::deinterleave(work_buf_.data(), work_buf_.size()); // байтовый деинтерливинг
    result_buf_.resize(RS_DATA_LEN);
    if (!rs255223::decode(work_buf_.data(), result_buf_.data())) return;
    result_len = RS_DATA_LEN;
  } else if (USE_RS && payload_buf_.size() == RS_ENC_LEN) {
    byte_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // байтовый деинтерливинг
    result_buf_.resize(RS_DATA_LEN);
    if (!rs255223::decode(payload_buf_.data(), result_buf_.data())) return;
    result_len = RS_DATA_LEN;
  } else if (!USE_RS && payload_buf_.size() == RS_DATA_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // деинтерливинг бит
    if (!conv_codec::viterbiDecode(payload_buf_.data(), payload_buf_.size(), result_buf_)) return;
    result_len = result_buf_.size();
  } else {
    result_buf_.assign(payload_buf_.begin(), payload_buf_.end()); // кадр без кодирования
    result_len = result_buf_.size();
  }

  // Дешифрование и сборка сообщения
  if (result_len < TAG_LEN) return;                  // недостаточно данных
  const uint8_t* cipher = result_buf_.data();
  size_t cipher_len = result_len - TAG_LEN;
  const uint8_t* tag = result_buf_.data() + cipher_len;
  bool encrypted = (hdr.flags & FrameHeader::FLAG_ENCRYPTED) != 0;
  bool should_decrypt = encrypted || encryption_forced_;
  bool decrypt_ok = false;
  if (should_decrypt) {
    uint16_t nonce_idx = hdr.frag_idx;               // индекс части задаёт уникальный нонс
    nonce_ = KeyLoader::makeNonce(hdr.msg_id, nonce_idx);
    decrypt_ok = decrypt_ccm(key_.data(), key_.size(), nonce_.data(), nonce_.size(),
                             nullptr, 0, cipher, cipher_len,
                             tag, TAG_LEN, plain_buf_);
    if (!decrypt_ok) {
      if (encrypted) {
        LOG_ERROR("RxModule: ошибка дешифрования");
        return;                                       // завершаем обработку повреждённого кадра
      }
      LOG_WARN("RxModule: получен незашифрованный фрагмент при принудительном режиме");
    }
  }
  if (!should_decrypt || (!decrypt_ok && !encrypted)) {
    plain_buf_.assign(result_buf_.begin(), result_buf_.begin() + cipher_len); // принимаем открытый текст
  }

  if (!plain_buf_.empty() && plain_buf_[0] == '[') {  // удаляем префикс [ID|n]
    auto it = std::find(plain_buf_.begin(), plain_buf_.end(), ']');
    if (it != plain_buf_.end()) {
      plain_buf_.erase(plain_buf_.begin(), it + 1);
    }
  }
  gatherer_.add(plain_buf_.data(), plain_buf_.size());
  plain_buf_.clear();                                  // очищаем буфер, сохраняя вместимость
  ++next_frag_idx_;                                    // ожидаем следующий индекс фрагмента
  if (hdr.frag_idx + 1 == hdr.frag_cnt) {             // последний фрагмент
    const auto& full = gatherer_.get();
    if (buf_) {                                       // при наличии внешнего буфера сохраняем данные
      buf_->pushReady(hdr.msg_id, full.data(), full.size());
    }
    if (cb_) {                                        // передаём сообщение пользователю
      cb_(full.data(), full.size());
    }
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
}

// Установка колбэка для обработки сообщений
void RxModule::setCallback(Callback cb) {
  cb_ = cb;
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
