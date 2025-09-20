#include "tx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточное кодирование
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "libs/key_loader/key_loader.h" // загрузка ключа
#include "libs/crypto/aes_ccm.h" // AES-CCM шифрование
#include "default_settings.h"
#include <vector>
#include <chrono>
#include <algorithm>
#include <string>
#include <array>
#include "libs/simple_logger/simple_logger.h" // журнал статусов

// Максимально допустимый размер кадра
// Ограничение радиомодуля SX1262 — не более 245 байт в пакете
static constexpr size_t MAX_FRAME_SIZE = 245;      // максимально допустимый кадр
// Допустимая длина полезной нагрузки одного кадра с учётом заголовков и пилотов
static constexpr size_t MAX_FRAGMENT_LEN =
    MAX_FRAME_SIZE - FrameHeader::SIZE * 2 - (MAX_FRAME_SIZE / 64) * 2;
static constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина блока данных RS
static constexpr size_t TAG_LEN = 8;              // длина тега аутентичности
static constexpr size_t RS_ENC_LEN = 255;         // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга
static constexpr bool USE_RS = DefaultSettings::USE_RS; // включение кода RS(255,223)

// Вставка пилотов каждые 64 байта
static std::vector<uint8_t> insertPilots(const std::vector<uint8_t>& in) {
  std::vector<uint8_t> out;
  out.reserve(in.size() + in.size() / 64 * 2);
  size_t count = 0;
  for (uint8_t b : in) {
    if (count && count % 64 == 0) {
      out.push_back(0x55);
      out.push_back(0x2D);
    }
    out.push_back(b);
    ++count;
  }
  return out;
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
  auto now = std::chrono::steady_clock::now();
  if (pause_ms_ && now - last_send_ < std::chrono::milliseconds(pause_ms_)) {
    DEBUG_LOG("TxModule: пауза");
    return false;
  }

  if (ack_enabled_ && waiting_ack_) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_attempt_);
    if (elapsed.count() < static_cast<long>(ack_timeout_ms_)) {
      return false;
    }
    if (inflight_) {
      if (inflight_->attempts_left > 0) {
        --inflight_->attempts_left;
        waiting_ack_ = false;
        DEBUG_LOG("TxModule: повтор без ACK");
      } else {
        DEBUG_LOG("TxModule: ACK не получен, перенос в архив");
        inflight_->attempts_left = ack_retry_limit_;
        archive_.push_back(std::move(*inflight_));
        inflight_.reset();
        waiting_ack_ = false;
      }
    } else {
      waiting_ack_ = false;
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
    out.expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && !isAckPayload(out.data);
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

  last_send_ = now;
  if (ack_enabled_) {
    if (message->expect_ack) {
      waiting_ack_ = true;
      last_attempt_ = now;
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

  std::string prefix;
  auto it = std::find(msg.begin(), msg.end(), ']');
  if (it != msg.end()) prefix.assign(msg.begin(), it + 1);

  PacketSplitter rs_splitter(PayloadMode::SMALL, RS_DATA_LEN - TAG_LEN);
  MessageBuffer tmp((msg.size() + (RS_DATA_LEN - TAG_LEN) - 1) / (RS_DATA_LEN - TAG_LEN));
  rs_splitter.splitAndEnqueue(tmp, msg.data(), msg.size(), false);

  std::vector<std::vector<uint8_t>> fragments;
  bool sent = false;

  while (tmp.hasPending()) {
    std::vector<uint8_t> part;
    uint32_t dummy = 0;
    if (!tmp.pop(dummy, part)) break;

    std::vector<uint8_t> enc;
    std::vector<uint8_t> tag;
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

    std::vector<uint8_t> conv;
    if (enc.size() == RS_DATA_LEN) {
      if (USE_RS) {
        uint8_t rs_buf[RS_ENC_LEN];
        rs255223::encode(enc.data(), rs_buf);
        byte_interleaver::interleave(rs_buf, RS_ENC_LEN);
        conv_codec::encodeBits(rs_buf, RS_ENC_LEN, conv);
      } else {
        conv_codec::encodeBits(enc.data(), enc.size(), conv);
      }
      if (USE_BIT_INTERLEAVER) {
        bit_interleaver::interleave(conv.data(), conv.size());
      }
    } else {
      conv = enc;
    }

    if (conv.size() > MAX_FRAGMENT_LEN) {
      size_t pieces = (conv.size() + MAX_FRAGMENT_LEN - 1) / MAX_FRAGMENT_LEN;
      PacketSplitter frag_splitter(PayloadMode::SMALL, MAX_FRAGMENT_LEN);
      MessageBuffer fb(pieces);
      frag_splitter.splitAndEnqueue(fb, conv.data(), conv.size(), false);
      LOG_WARN_VAL("TxModule: фрагмент превышает лимит, частей=", pieces);
      while (fb.hasPending()) {
        std::vector<uint8_t> sub;
        fb.pop(dummy, sub);
        fragments.push_back(std::move(sub));
      }
    } else {
      fragments.push_back(std::move(conv));
    }
  }

  uint16_t frag_cnt = static_cast<uint16_t>(fragments.size());
  for (size_t idx = 0; idx < fragments.size(); ++idx) {
    auto& conv = fragments[idx];
    FrameHeader hdr;
    hdr.msg_id = message.id;
    hdr.frag_idx = static_cast<uint16_t>(idx);
    hdr.frag_cnt = frag_cnt;
    hdr.payload_len = static_cast<uint16_t>(conv.size());
    hdr.flags = 0;
    if (encryption_enabled_) hdr.flags |= FrameHeader::FLAG_ENCRYPTED;
    if (message.expect_ack) hdr.flags |= FrameHeader::FLAG_ACK_REQUIRED;

    uint8_t hdr_buf[FrameHeader::SIZE];
    if (!hdr.encode(hdr_buf, sizeof(hdr_buf), conv.data(), conv.size())) {
      DEBUG_LOG("TxModule: ошибка кодирования заголовка");
      continue;
    }

    std::vector<uint8_t> frame;
    frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
    frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
    auto payload = insertPilots(conv);
    frame.insert(frame.end(), payload.begin(), payload.end());
    scrambler::scramble(frame.data(), frame.size());

    if (frame.size() > MAX_FRAME_SIZE) {
      LOG_ERROR_VAL("TxModule: превышен размер кадра=", frame.size());
      continue;
    }
    radio_.send(frame.data(), frame.size());
    DEBUG_LOG_VAL("TxModule: отправлен фрагмент=", idx);
    sent = true;
  }

  if (!prefix.empty()) {
    SimpleLogger::logStatus(prefix + " GO");
  }
  return sent;
}

bool TxModule::isAckPayload(const std::vector<uint8_t>& data) {
  size_t offset = 0;
  if (!data.empty() && data[0] == '[') {
    auto it = std::find(data.begin(), data.end(), ']');
    if (it != data.end()) offset = static_cast<size_t>(std::distance(data.begin(), it) + 1);
  }
  if (data.size() < offset + 3) return false;
  return data[offset] == 'A' && data[offset + 1] == 'C' && data[offset + 2] == 'K';
}

void TxModule::scheduleFromArchive() {
  if (delayed_ || archive_.empty()) return;
  delayed_.emplace(std::move(archive_.front()));
  archive_.pop_front();
  delayed_->attempts_left = ack_retry_limit_;
  delayed_->expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && !isAckPayload(delayed_->data);
  if (!ack_enabled_) delayed_->expect_ack = false;
  DEBUG_LOG("TxModule: восстановлен пакет из архива");
}

void TxModule::onSendSuccess() {
  scheduleFromArchive();
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
    delayed_->expect_ack = ack_enabled_ && ack_retry_limit_ != 0 && !isAckPayload(delayed_->data);
  }
}

void TxModule::onAckReceived() {
  if (!ack_enabled_) {
    scheduleFromArchive();
    return;
  }
  if (!waiting_ack_) return;
  waiting_ack_ = false;
  DEBUG_LOG("TxModule: ACK получен");
  if (inflight_) {
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
