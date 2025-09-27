#include "tx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточное кодирование
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "libs/key_loader/key_loader.h" // загрузка ключа
#include "libs/crypto/aes_ccm.h" // AES-CCM шифрование
#include "libs/protocol/ack_utils.h" // проверка ACK-пакетов
#include "default_settings.h"
#include <vector>
#include <chrono>
#include <algorithm>
#include <string>
#include <array>
#include <cstring>
#ifndef ARDUINO
#include <thread>
#endif
#include "libs/simple_logger/simple_logger.h" // журнал статусов

// Максимально допустимый размер кадра
// Ограничение радиомодуля SX1262 — не более 245 байт в пакете
static constexpr size_t MAX_FRAME_SIZE = 245;      // максимально допустимый кадр

// Новый пилотный маркер: 0x7E «флаг PHY», далее ASCII "SATP" и CRC16 (0x9FD6) по этим 5 байтам.
// За счёт комбинации зарезервированного символа PHY и контрольной суммы такой паттерн
// гарантированно не появляется внутри закодированных полезных данных.
static constexpr size_t PILOT_INTERVAL = 64;             // период вставки пилотов
static constexpr std::array<uint8_t,7> PILOT_MARKER{{
    0x7E, 'S', 'A', 'T', 'P', 0xD6, 0x9F
}};
static constexpr size_t PILOT_PREFIX_LEN = PILOT_MARKER.size() - 2; // часть без CRC
static constexpr uint16_t PILOT_MARKER_CRC = 0x9FD6;                // CRC16(prefix)
[[maybe_unused]] static const bool PILOT_MARKER_CRC_OK = []() {
  return FrameHeader::crc16(PILOT_MARKER.data(), PILOT_PREFIX_LEN) == PILOT_MARKER_CRC;
}();
static_assert(PILOT_PREFIX_LEN == 5, "Ожидается пятибайтовый префикс пилота");

// Допустимая длина полезной нагрузки одного кадра с учётом заголовков и пилотов
static constexpr size_t MAX_FRAGMENT_LEN =
    MAX_FRAME_SIZE - FrameHeader::SIZE * 2 - (MAX_FRAME_SIZE / PILOT_INTERVAL) * PILOT_MARKER.size();
static constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина блока данных RS
static constexpr size_t TAG_LEN = 8;              // длина тега аутентичности
static constexpr size_t RS_ENC_LEN = 255;         // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга
static constexpr bool USE_RS = DefaultSettings::USE_RS; // включение кода RS(255,223)
static constexpr size_t CONV_TAIL_BYTES = 1;      // добавочные байты для сброса свёрточного кодера
static constexpr size_t RS_DATA_PAYLOAD = RS_DATA_LEN > TAG_LEN ? RS_DATA_LEN - TAG_LEN : 0; // размер полезных данных до тега
static constexpr size_t MAX_CONV_PLAINTEXT =
    (MAX_FRAGMENT_LEN / 2 > TAG_LEN) ? (MAX_FRAGMENT_LEN / 2 - TAG_LEN) : 0; // максимум данных для свёрточного кодера
static constexpr size_t EFFECTIVE_DATA_CHUNK =
    (MAX_CONV_PLAINTEXT && RS_DATA_PAYLOAD)
        ? (MAX_CONV_PLAINTEXT < RS_DATA_PAYLOAD ? MAX_CONV_PLAINTEXT : RS_DATA_PAYLOAD)
        : (RS_DATA_PAYLOAD ? RS_DATA_PAYLOAD : MAX_CONV_PLAINTEXT);
static_assert(EFFECTIVE_DATA_CHUNK > 0, "Размер части для кодирования должен быть положительным");
static constexpr size_t MAX_CIPHER_CHUNK = EFFECTIVE_DATA_CHUNK + TAG_LEN; // максимум байт шифртекста в одном блоке

// Вставка пилотов каждые 64 байта
static size_t insertPilots(const uint8_t* in, size_t len, uint8_t* out, size_t out_capacity) {
  size_t written = 0;
  size_t count = 0;
  for (size_t i = 0; i < len; ++i) {
    if (count && count % PILOT_INTERVAL == 0) {
      if (written + PILOT_MARKER.size() > out_capacity) {
        return 0;                                     // защита от выхода за пределы буфера
      }
      std::memcpy(out + written, PILOT_MARKER.data(), PILOT_MARKER.size());
      written += PILOT_MARKER.size();
    }
    if (written >= out_capacity) {
      return 0;                                       // буфер переполнен
    }
    out[written++] = in[i];
    ++count;
  }
  return written;
}

// Инициализация модуля передачи с классами QoS
TxModule::TxModule(IRadio& radio, const std::array<size_t,4>& capacities, PayloadMode mode)
  : radio_(radio), buffers_{MessageBuffer(capacities[0]), MessageBuffer(capacities[1]),
                             MessageBuffer(capacities[2]), MessageBuffer(capacities[3])},
    splitter_(mode), key_(KeyLoader::loadKey()) {
  // ключ считывается один раз и используется при шифровании
  last_send_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(pause_ms_);
  last_attempt_ = last_send_;
}

// Смена режима пакета
void TxModule::setPayloadMode(PayloadMode mode) { splitter_.setMode(mode); }

// Помещаем сообщение в очередь согласно классу QoS
uint32_t TxModule::queue(const uint8_t* data, size_t len, uint8_t qos) {
  if (!data || len == 0) {                        // проверка указателя
    DEBUG_LOG("TxModule: пустой ввод");
    return 0;
  }
  if (qos > 3) qos = 3;                           // ограничение диапазона QoS
  bool is_plain_ack = protocol::ack::isAckPayload(data, len);
  if (is_plain_ack) {
    PendingMessage ack_msg;                       // формируем отдельную запись для ACK
    ack_msg.id = next_ack_id_++;                  // выделяем идентификатор вне общей очереди
    if (next_ack_id_ < 0x80000000u) next_ack_id_ = 0x80000000u; // не даём переполнению уйти в «обычный» диапазон
    ack_msg.data.assign(1, protocol::ack::MARKER); // всегда отправляем минимальный ACK
    ack_msg.qos = qos;                            // запоминаем исходный класс
    ack_msg.attempts_left = 0;                    // повторы не нужны
    ack_msg.expect_ack = false;                   // ACK никогда не ждёт подтверждения
    ack_queue_.push_back(std::move(ack_msg));
    DEBUG_LOG("TxModule: ACK добавлен в быстрый буфер id=%u qos=%u",
              static_cast<unsigned int>(ack_msg.id),
              static_cast<unsigned int>(ack_msg.qos));
    return ack_queue_.back().id;
  }
  DEBUG_LOG_VAL("TxModule: постановка длины=", len);
  uint32_t res = splitter_.splitAndEnqueue(buffers_[qos], data, len);
  if (res) {
    DEBUG_LOG_VAL("TxModule: сообщение id=", res);
  } else {
    DEBUG_LOG("TxModule: ошибка постановки");
  }
  return res;
}

// Пытаемся отправить первое сообщение
bool TxModule::loop() {
  if (processImmediateAck()) {                    // приоритетная отправка подтверждений
    return true;
  }
  auto now = std::chrono::steady_clock::now();
  if (pause_ms_ && now - last_send_ < std::chrono::milliseconds(pause_ms_)) {
    DEBUG_LOG("TxModule: пауза");
    radio_.ensureReceiveMode();
    return false;
  }

  if (ack_enabled_ && waiting_ack_) {
    if (ack_timeout_ms_ == 0) {
      DEBUG_LOG("TxModule: ожидание ACK отключено нулевым тайм-аутом");
      waiting_ack_ = false;
      if (inflight_) {
        if (!inflight_->status_prefix.empty()) {
          SimpleLogger::logStatus(inflight_->status_prefix + " GO");
        }
        inflight_->attempts_left = ack_retry_limit_;
        inflight_->expect_ack = false;
        inflight_.reset();
      }
      scheduleFromArchive();
    } else {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_attempt_);
      if (elapsed.count() < static_cast<long>(ack_timeout_ms_)) {
        radio_.ensureReceiveMode();
        return false;
      }
      if (inflight_) {
        if (inflight_->attempts_left > 0) {
          --inflight_->attempts_left;
          waiting_ack_ = false;
          DEBUG_LOG("TxModule: повтор без ACK");
        } else {
          DEBUG_LOG("TxModule: ACK не получен, перенос в архив");
          uint8_t failed_qos = inflight_->qos;
          std::string failed_tag = inflight_->packet_tag;
          inflight_->attempts_left = ack_retry_limit_;
          archive_.push_back(std::move(*inflight_));
          inflight_.reset();
          waiting_ack_ = false;
          archiveFollowingParts(failed_qos, failed_tag);
        }
      } else {
        waiting_ack_ = false;
      }
    }
  }

  if (!ack_enabled_ && !delayed_ && !archive_.empty() && !inflight_) {
    scheduleFromArchive();
  }

  PendingMessage* message = nullptr;
  std::optional<PendingMessage> temp;

  auto fetchNext = [&](PendingMessage& out) -> bool {
    MessageBuffer* buf = nullptr;
    uint8_t qos_idx = 0;
    for (size_t i = 0; i < buffers_.size(); ++i) {
      if (buffers_[i].hasPending()) { buf = &buffers_[i]; qos_idx = static_cast<uint8_t>(i); break; }
    }
    if (!buf) {
      DEBUG_LOG("TxModule: очереди пусты");
      return false;
    }
    std::vector<uint8_t> msg;
    uint32_t id = 0;
    if (!buf->pop(id, msg)) {
      DEBUG_LOG("TxModule: ошибка извлечения");
      return false;
    }
    out.id = id;
    out.data = std::move(msg);
    out.qos = qos_idx;
    out.attempts_left = ack_retry_limit_;
    out.expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && ack_timeout_ms_ != 0 && !isAckPayload(out.data);
    out.packet_tag = extractPacketTag(out.data);
    out.status_prefix = extractStatusPrefix(out.data);
    if (!ack_enabled_) out.expect_ack = false;
    return true;
  };

  if (ack_enabled_) {
    if (!inflight_) {
      if (delayed_) {
        inflight_.emplace(std::move(*delayed_));
        delayed_.reset();
      } else {
        PendingMessage fresh;
        if (!fetchNext(fresh)) return false;
        inflight_.emplace(std::move(fresh));
      }
    }
    if (!inflight_) return false;
    message = &*inflight_;
  } else {
    if (delayed_) {
      temp.emplace(std::move(*delayed_));
      delayed_.reset();
    } else {
      PendingMessage fresh;
      if (!fetchNext(fresh)) return false;
      fresh.expect_ack = false;
      temp.emplace(std::move(fresh));
    }
    if (!temp) return false;
    message = &*temp;
  }

  if (!message) return false;
  bool sent = transmit(*message);
  if (!sent) {
    DEBUG_LOG("TxModule: отправка не удалась");
    if (ack_enabled_ && inflight_) {
      inflight_->attempts_left = ack_retry_limit_;
      archive_.push_back(std::move(*inflight_));
      inflight_.reset();
      waiting_ack_ = false;
    }
    return false;
  }

  auto send_moment = last_send_;
  if (ack_enabled_) {
    if (message->expect_ack) {
      waiting_ack_ = true;
      last_attempt_ = send_moment;
    } else {
      waiting_ack_ = false;
      if (inflight_) {
        inflight_->attempts_left = ack_retry_limit_;
        inflight_.reset();
      }
      onSendSuccess();
    }
  } else {
    onSendSuccess();
  }
  return true;
}

// Установка паузы между отправками
void TxModule::setSendPause(uint32_t pause_ms) {
  pause_ms_ = pause_ms;
  last_send_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(pause_ms);
}

uint32_t TxModule::getSendPause() const {
  return pause_ms_;
}

void TxModule::setAckTimeout(uint32_t timeout_ms) {
  ack_timeout_ms_ = timeout_ms;
  auto now = std::chrono::steady_clock::now();
  if (ack_timeout_ms_ == 0) {
    last_attempt_ = now;
    bool had_waiting = waiting_ack_;
    waiting_ack_ = false;
    if (inflight_) {
      if (had_waiting && !inflight_->status_prefix.empty()) {
        SimpleLogger::logStatus(inflight_->status_prefix + " GO");
      }
      inflight_->attempts_left = ack_retry_limit_;
      inflight_->expect_ack = false;
      if (had_waiting) {
        inflight_.reset();
      }
    }
    if (delayed_) {
      delayed_->attempts_left = ack_retry_limit_;
      delayed_->expect_ack = false;
    }
    scheduleFromArchive();
  } else {
    if (waiting_ack_) {
      last_attempt_ = now - std::chrono::milliseconds(timeout_ms);
    }
    if (inflight_) {
      inflight_->expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && ack_timeout_ms_ != 0 && !isAckPayload(inflight_->data);
    }
    if (delayed_) {
      delayed_->expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && ack_timeout_ms_ != 0 && !isAckPayload(delayed_->data);
    }
  }
}

uint32_t TxModule::getAckTimeout() const {
  return ack_timeout_ms_;
}

void TxModule::reloadKey() {
  key_ = KeyLoader::loadKey();
  DEBUG_LOG("TxModule: ключ перечитан");
}

bool TxModule::transmit(const PendingMessage& message) {
  const auto& msg = message.data;
  if (msg.empty()) {
    DEBUG_LOG("TxModule: пустой пакет");
    return false;
  }

  std::string prefix = message.status_prefix;
  if (prefix.empty()) {
    prefix = extractStatusPrefix(msg);
  }

  PacketSplitter rs_splitter(PayloadMode::SMALL, EFFECTIVE_DATA_CHUNK);
  MessageBuffer tmp((msg.size() + EFFECTIVE_DATA_CHUNK - 1) / EFFECTIVE_DATA_CHUNK);
  rs_splitter.splitAndEnqueue(tmp, msg.data(), msg.size(), false);

  struct PreparedFragment {
    std::array<uint8_t, MAX_FRAGMENT_LEN> payload{}; // предвыделенный буфер полезной нагрузки
    uint16_t payload_size = 0;                       // фактическая длина полезной нагрузки
    bool conv_encoded = false;                      // применялось ли свёрточное кодирование
    uint16_t cipher_len = 0;                        // длина шифртекста вместе с тегом
    uint16_t plain_len = 0;                         // длина исходных данных до шифрования
    uint16_t chunk_idx = 0;                         // номер блока внутри сообщения для нонса
  };

  std::vector<PreparedFragment> fragments;
  fragments.reserve((msg.size() + EFFECTIVE_DATA_CHUNK - 1) / EFFECTIVE_DATA_CHUNK);
  bool sent = false;

  std::vector<uint8_t> part;
  part.reserve(tmp.slotSize());                     // используем вместимость слота
  std::vector<uint8_t> enc;
  enc.reserve(MAX_CIPHER_CHUNK);
  std::vector<uint8_t> tag;
  tag.reserve(TAG_LEN);
  std::vector<uint8_t> conv;
  conv.reserve(MAX_FRAGMENT_LEN);
  std::vector<uint8_t> conv_input;
  conv_input.reserve(RS_ENC_LEN + CONV_TAIL_BYTES);

  while (tmp.hasPending()) {
    part.clear();
    uint32_t dummy = 0;
    if (!tmp.pop(dummy, part)) break;

    enc.clear();
    tag.clear();
    const size_t plain_len = part.size();
    uint16_t current_idx = static_cast<uint16_t>(fragments.size());
    auto nonce = KeyLoader::makeNonce(message.id, current_idx);
    if (encryption_enabled_) {
      if (!encrypt_ccm(key_.data(), key_.size(), nonce.data(), nonce.size(),
                       nullptr, 0, part.data(), part.size(), enc, tag, TAG_LEN)) {
        LOG_ERROR("TxModule: ошибка шифрования");
        continue;
      }
      enc.insert(enc.end(), tag.begin(), tag.end());
    } else {
      enc.assign(part.begin(), part.end());
      enc.insert(enc.end(), TAG_LEN, 0x00);              // добавляем пустой тег для выравнивания
    }

    const size_t cipher_len = enc.size();
    if (cipher_len > MAX_CIPHER_CHUNK) {
      LOG_WARN_VAL("TxModule: блок шифртекста превышает лимит=", cipher_len);
      continue;                                       // пропускаем некорректный блок
    }

    bool conv_applied = false;
    if (USE_RS && cipher_len == RS_DATA_LEN) {
      uint8_t rs_buf[RS_ENC_LEN];
      rs255223::encode(enc.data(), rs_buf);
      byte_interleaver::interleave(rs_buf, RS_ENC_LEN);
      conv_input.assign(rs_buf, rs_buf + RS_ENC_LEN);
      conv_input.insert(conv_input.end(), CONV_TAIL_BYTES, 0x00);
      conv_codec::encodeBits(conv_input.data(), conv_input.size(), conv);
      conv_applied = true;
      if (USE_BIT_INTERLEAVER && !conv.empty()) {
        bit_interleaver::interleave(conv.data(), conv.size());
      }
    } else if (!enc.empty()) {
      conv_input.assign(enc.begin(), enc.end());
      conv_input.insert(conv_input.end(), CONV_TAIL_BYTES, 0x00); // добавляем «хвост» для сброса регистра
      conv_codec::encodeBits(conv_input.data(), conv_input.size(), conv);
      conv_applied = true;
      if (USE_BIT_INTERLEAVER && !conv.empty()) {
        bit_interleaver::interleave(conv.data(), conv.size());
      }
    }

    if (!conv_applied) {
      conv.assign(enc.begin(), enc.end());            // fallback: отправляем без свёртки
    }

    if (conv_applied && conv.size() > MAX_FRAGMENT_LEN) {
      LOG_WARN_VAL("TxModule: свёрнутый блок превышает лимит=", conv.size());
      conv.assign(enc.begin(), enc.end());            // возвращаемся к исходному варианту
      conv_applied = false;
    }

    PreparedFragment frag;
    if (conv.size() > frag.payload.size()) {
      LOG_WARN("TxModule: подготовленный блок не помещается в слот");
      continue;
    }
    frag.payload_size = static_cast<uint16_t>(conv.size());
    if (!conv.empty()) {
      std::memcpy(frag.payload.data(), conv.data(), conv.size());
    }
    if (conv_applied) {
      frag.conv_encoded = true;
      frag.cipher_len = static_cast<uint16_t>(cipher_len);
      frag.plain_len = static_cast<uint16_t>(plain_len);
    }
    frag.chunk_idx = current_idx;
    fragments.push_back(frag);
  }

  uint16_t frag_cnt = static_cast<uint16_t>(fragments.size());
  std::array<uint8_t, MAX_FRAME_SIZE> frame_buf{};      // общий буфер кадра
  for (size_t idx = 0; idx < fragments.size(); ++idx) {
    auto& frag = fragments[idx];
    FrameHeader hdr;
    hdr.msg_id = message.id;
    hdr.frag_idx = static_cast<uint16_t>(idx);
    hdr.frag_cnt = frag_cnt;
    hdr.payload_len = frag.payload_size;
    hdr.flags = 0;
    if (encryption_enabled_) hdr.flags |= FrameHeader::FLAG_ENCRYPTED;
    if (message.expect_ack) hdr.flags |= FrameHeader::FLAG_ACK_REQUIRED;
    if (frag.conv_encoded) {
      hdr.flags |= FrameHeader::FLAG_CONV_ENCODED;
    }
    hdr.ack_mask = 0;                               // поле свободно для реальных масок подтверждений

    uint8_t hdr_buf[FrameHeader::SIZE];
    if (!hdr.encode(hdr_buf, sizeof(hdr_buf), frag.payload.data(), frag.payload_size)) {
      DEBUG_LOG("TxModule: ошибка кодирования заголовка");
      continue;
    }

    size_t frame_size = 0;
    std::memcpy(frame_buf.data() + frame_size, hdr_buf, FrameHeader::SIZE);
    frame_size += FrameHeader::SIZE;
    std::memcpy(frame_buf.data() + frame_size, hdr_buf, FrameHeader::SIZE);
    frame_size += FrameHeader::SIZE;
    size_t payload_bytes = insertPilots(frag.payload.data(), frag.payload_size,
                                        frame_buf.data() + frame_size,
                                        frame_buf.size() - frame_size);
    if (payload_bytes == 0 && frag.payload_size != 0) {
      LOG_ERROR("TxModule: вставка пилотов не удалась");
      continue;
    }
    frame_size += payload_bytes;
    scrambler::scramble(frame_buf.data(), frame_size);

    if (frame_size > MAX_FRAME_SIZE) {
      LOG_ERROR_VAL("TxModule: превышен размер кадра=", frame_size);
      continue;
    }
    waitForPauseWindow();
    radio_.send(frame_buf.data(), frame_size);
    last_send_ = std::chrono::steady_clock::now();
    DEBUG_LOG_VAL("TxModule: отправлен фрагмент=", idx);
    sent = true;
  }

  if (!prefix.empty() && (!ack_enabled_ || !message.expect_ack)) {
    SimpleLogger::logStatus(prefix + " GO");
  }
  return sent;
}

bool TxModule::isAckPayload(const std::vector<uint8_t>& data) {
  return protocol::ack::isAckPayload(data);
}

std::string TxModule::extractPacketTag(const std::vector<uint8_t>& data) {
  if (data.empty() || data.front() != '[') return {};
  auto pipe = std::find(data.begin(), data.end(), '|');
  if (pipe == data.end() || pipe == data.begin() + 1) return {};
  return std::string(data.begin() + 1, pipe);          // возвращаем часть между '[' и '|'
}

std::string TxModule::extractStatusPrefix(const std::vector<uint8_t>& data) {
  if (data.empty() || data.front() != '[') return {};
  auto it = std::find(data.begin(), data.end(), ']');
  if (it == data.end()) return {};
  return std::string(data.begin(), it + 1);
}

void TxModule::archiveFollowingParts(uint8_t qos, const std::string& tag) {
  if (tag.empty() || qos >= buffers_.size()) return;    // нет смысла обрабатывать пустой тег
  auto& buf = buffers_[qos];
  while (true) {
    uint32_t peek_id = 0;
    const std::vector<uint8_t>* next = buf.peek(peek_id);
    if (!next) break;                                   // очередь пуста
    if (extractPacketTag(*next) != tag) break;          // следующий элемент принадлежит другому пакету
    uint32_t id = 0;
    std::vector<uint8_t> data;
    if (!buf.pop(id, data)) break;                      // защита от расхождений между peek и pop
    PendingMessage archived;
    archived.id = id;
    archived.data = std::move(data);
    archived.qos = qos;
    archived.attempts_left = ack_retry_limit_;
    archived.expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && ack_timeout_ms_ != 0 && !isAckPayload(archived.data);
    archived.packet_tag = tag;
    archived.status_prefix = extractStatusPrefix(archived.data);
    archive_.push_back(std::move(archived));
    DEBUG_LOG("TxModule: часть пакета перенесена в архив");
  }
}

void TxModule::scheduleFromArchive() {
  if (delayed_ || archive_.empty()) return;
  delayed_.emplace(std::move(archive_.front()));
  archive_.pop_front();
  delayed_->attempts_left = ack_retry_limit_;
  delayed_->expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && ack_timeout_ms_ != 0 && !isAckPayload(delayed_->data);
  if (!ack_enabled_) delayed_->expect_ack = false;
  DEBUG_LOG("TxModule: восстановлен пакет из архива");
}

void TxModule::onSendSuccess() {
  scheduleFromArchive();
}

void TxModule::waitForPauseWindow() {
  if (pause_ms_ == 0) return;
  auto target = last_send_ + std::chrono::milliseconds(pause_ms_);
  auto now = std::chrono::steady_clock::now();
  if (now >= target) return;
  auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(target - now);
  if (remaining.count() <= 0) return;
  radio_.ensureReceiveMode();
#ifdef ARDUINO
  delay(static_cast<unsigned long>(remaining.count()));
#else
  std::this_thread::sleep_for(remaining);
#endif
}

bool TxModule::processImmediateAck() {
  if (ack_queue_.empty()) return false;            // нечего отправлять
  PendingMessage ack = std::move(ack_queue_.front());
  ack_queue_.pop_front();
  const uint32_t ack_id = ack.id;
  const uint8_t ack_qos = ack.qos;
  bool sent = transmit(ack);                      // пытаемся отправить ACK обычным пайплайном
  if (!sent) {                                    // не удалось — возвращаем в начало очереди
    ack_queue_.push_front(std::move(ack));
    return false;
  }
  onSendSuccess();                                // позволяем архиву продолжить отдачу
  DEBUG_LOG("TxModule: ACK отправлен вне очереди id=%u qos=%u",
            static_cast<unsigned int>(ack_id),
            static_cast<unsigned int>(ack_qos));
  return true;
}

void TxModule::setAckEnabled(bool enabled) {
  if (ack_enabled_ == enabled) return;
  ack_enabled_ = enabled;
  DEBUG_LOG(enabled ? "TxModule: ACK включён" : "TxModule: ACK выключен");
  if (!ack_enabled_) {
    waiting_ack_ = false;
    if (inflight_) {
      inflight_->expect_ack = false;
      inflight_->attempts_left = ack_retry_limit_;
      inflight_.reset();
    }
    scheduleFromArchive();
  }
}

void TxModule::setAckRetryLimit(uint8_t retries) {
  ack_retry_limit_ = retries;
  if (inflight_) {
    inflight_->attempts_left = std::min(inflight_->attempts_left, ack_retry_limit_);
    if (ack_retry_limit_ == 0) inflight_->expect_ack = false;
  }
  if (delayed_) {
    delayed_->attempts_left = ack_retry_limit_;
    delayed_->expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && ack_timeout_ms_ != 0 && !isAckPayload(delayed_->data);
  }
  if (ack_retry_limit_ == 0 && waiting_ack_) {
    // При обнулении лимита считаем пакет доставленным и не ждём ACK
    waiting_ack_ = false;
    if (inflight_) {
      if (!inflight_->status_prefix.empty()) {
        SimpleLogger::logStatus(inflight_->status_prefix + " GO");
      }
      inflight_->attempts_left = ack_retry_limit_;
      inflight_.reset();
    }
    onSendSuccess();
  }
}

void TxModule::onAckReceived() {
  if (!ack_enabled_) {
    scheduleFromArchive();
    return;
  }
  if (!waiting_ack_) return;
  waiting_ack_ = false;
  uint32_t inflight_id = inflight_ ? inflight_->id : 0;
  uint8_t inflight_qos = inflight_ ? inflight_->qos : 0;
  DEBUG_LOG("TxModule: ACK получен для id=%u qos=%u",
            static_cast<unsigned int>(inflight_id),
            static_cast<unsigned int>(inflight_qos));
  if (inflight_) {
    if (!inflight_->status_prefix.empty()) {
      SimpleLogger::logStatus(inflight_->status_prefix + " GO");
    }
    inflight_->attempts_left = ack_retry_limit_;
    inflight_.reset();
  }
  scheduleFromArchive();
}

void TxModule::setEncryptionEnabled(bool enabled) {
  if (encryption_enabled_ == enabled) return;
  encryption_enabled_ = enabled;
  DEBUG_LOG(enabled ? "TxModule: шифрование включено" : "TxModule: шифрование отключено");
}

void TxModule::prepareExternalSend() {
  waitForPauseWindow();
}

void TxModule::completeExternalSend() {
  last_send_ = std::chrono::steady_clock::now();
}
