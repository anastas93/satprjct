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
    return false; // ждём паузу
  }
  // ищем первую непустую очередь QoS
  MessageBuffer* buf = nullptr;
  for (auto& b : buffers_) {
    if (b.hasPending()) { buf = &b; break; }
  }
  if (!buf) {                                     // все очереди пусты
    DEBUG_LOG("TxModule: очереди пусты");
    return false;
  }
  std::vector<uint8_t> msg;
  uint32_t id = 0;                      // получаемый идентификатор сообщения
  if (!buf->pop(id, msg)) {
    DEBUG_LOG("TxModule: ошибка извлечения");
    return false;
  }
  std::string prefix;                   // извлечём префикс [ID|n]
  auto it = std::find(msg.begin(), msg.end(), ']');
  if (it != msg.end()) {
    prefix.assign(msg.begin(), it + 1);
  }

  // Шифруем сообщение по блокам перед кодированием и фрагментацией
  PacketSplitter rs_splitter(PayloadMode::SMALL, RS_DATA_LEN - TAG_LEN);
  MessageBuffer tmp((msg.size() + (RS_DATA_LEN - TAG_LEN) - 1) /
                    (RS_DATA_LEN - TAG_LEN));
  rs_splitter.splitAndEnqueue(tmp, msg.data(), msg.size(), false);

  std::vector<std::vector<uint8_t>> fragments;      // конечные фрагменты для отправки
  bool sent = false;                                // флаг успешной передачи

  while (tmp.hasPending()) {
    std::vector<uint8_t> part;
    uint32_t dummy;
    if (!tmp.pop(dummy, part)) break;               // защита от ошибок

    // Шифруем блок и добавляем тег аутентичности
    std::vector<uint8_t> enc;
    std::vector<uint8_t> tag;
    uint16_t current_idx = static_cast<uint16_t>(fragments.size());
    auto nonce = KeyLoader::makeNonce(id, current_idx); // нонс уникален для каждого фрагмента
    if (!encrypt_ccm(key_.data(), key_.size(), nonce.data(), nonce.size(),
                     nullptr, 0, part.data(), part.size(), enc, tag, TAG_LEN)) {
      LOG_ERROR("TxModule: ошибка шифрования");
      continue;                                     // пропускаем при ошибке
    }
    enc.insert(enc.end(), tag.begin(), tag.end());  // добавляем тег к данным
    part.swap(enc);

    std::vector<uint8_t> conv;                      // закодированный блок
    if (part.size() == RS_DATA_LEN) {
      if (USE_RS) {
        uint8_t rs_buf[RS_ENC_LEN];
        rs255223::encode(part.data(), rs_buf);            // кодируем блок RS
        byte_interleaver::interleave(rs_buf, RS_ENC_LEN); // байтовый интерливинг
        conv_codec::encodeBits(rs_buf, RS_ENC_LEN, conv); // свёрточное кодирование
      } else {
        conv_codec::encodeBits(part.data(), part.size(), conv); // только свёрточный кодер
      }
      if (USE_BIT_INTERLEAVER)
        bit_interleaver::interleave(conv.data(), conv.size()); // битовый интерливинг
    } else {
      conv = part;                                       // короткий блок без кодирования
    }

    if (conv.size() > MAX_FRAGMENT_LEN) {                // проверяем лимит
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
    hdr.msg_id = id;                                   // один ID для всех фрагментов
    hdr.frag_idx = static_cast<uint16_t>(idx);         // номер фрагмента
    hdr.frag_cnt = frag_cnt;                           // общее число фрагментов
    hdr.payload_len = static_cast<uint16_t>(conv.size());
    uint8_t hdr_buf[FrameHeader::SIZE];
    if (!hdr.encode(hdr_buf, sizeof(hdr_buf), conv.data(), conv.size())) {
      DEBUG_LOG("TxModule: ошибка кодирования заголовка");
      continue;
    }

    std::vector<uint8_t> frame;
    frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
    frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE); // повтор заголовка
    auto payload = insertPilots(conv);
    frame.insert(frame.end(), payload.begin(), payload.end());
    scrambler::scramble(frame.data(), frame.size());          // скремблируем кадр

    if (frame.size() > MAX_FRAME_SIZE) {                     // финальная проверка
      LOG_ERROR_VAL("TxModule: превышен размер кадра=", frame.size());
      continue;                                             // пропускаем отправку
    }
    radio_.send(frame.data(), frame.size());                // отправка кадра
    DEBUG_LOG_VAL("TxModule: отправлен фрагмент=", idx);
    sent = true;
  }
  if (!prefix.empty()) {
    SimpleLogger::logStatus(prefix + " GO");
  }
  if (sent) {
    last_send_ = std::chrono::steady_clock::now();
  }
  return sent;
}

// Установка паузы между отправками
void TxModule::setSendPause(uint32_t pause_ms) {
  pause_ms_ = pause_ms;
  last_send_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(pause_ms);
}

void TxModule::reloadKey() {
  key_ = KeyLoader::loadKey();
}
