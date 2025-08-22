#include "tx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточное кодирование
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "default_settings.h"
#include <vector>
#include <chrono>

// Максимально допустимый размер кадра
static constexpr size_t MAX_FRAME_SIZE = 255;      // максимально допустимый кадр
static constexpr size_t RS_DATA_LEN = 223;         // длина блока данных RS
static constexpr size_t RS_ENC_LEN = 255;         // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга

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
    splitter_(mode) {
  last_send_ = std::chrono::steady_clock::now();
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
void TxModule::loop() {
  auto now = std::chrono::steady_clock::now();
  if (pause_ms_ && now - last_send_ < std::chrono::milliseconds(pause_ms_)) {
    DEBUG_LOG("TxModule: пауза");
    return; // ждём паузу
  }
  // ищем первую непустую очередь QoS
  MessageBuffer* buf = nullptr;
  for (auto& b : buffers_) {
    if (b.hasPending()) { buf = &b; break; }
  }
  if (!buf) {                                     // все очереди пусты
    DEBUG_LOG("TxModule: очереди пусты");
    return;
  }
  std::vector<uint8_t> msg;
  uint32_t id = 0;                      // получаемый идентификатор сообщения
  if (!buf->pop(id, msg)) {
    DEBUG_LOG("TxModule: ошибка извлечения");
    return;
  }

  // TODO: шифрование сообщения (не реализовано)

  // Разбиваем сообщение на блоки по 223 байта перед RS-кодированием
  PacketSplitter rs_splitter(PayloadMode::SMALL, RS_DATA_LEN);
  size_t parts = (msg.size() + RS_DATA_LEN - 1) / RS_DATA_LEN;
  MessageBuffer tmp(parts);
  rs_splitter.splitAndEnqueue(tmp, msg.data(), msg.size());

  for (size_t idx = 0; tmp.hasPending(); ++idx) {
    std::vector<uint8_t> part;
    uint32_t dummy;
    if (!tmp.pop(dummy, part)) break; // защита от ошибок

    std::vector<uint8_t> conv;
    if (part.size() == RS_DATA_LEN) {
      // Полный блок: кодируем RS и применяем интерливинги
      uint8_t rs_buf[RS_ENC_LEN];
      rs255223::encode(part.data(), rs_buf);               // кодируем блок
      byte_interleaver::interleave(rs_buf, RS_ENC_LEN);    // байтовый интерливинг
      conv_codec::encodeBits(rs_buf, RS_ENC_LEN, conv);    // свёрточное кодирование
      if (USE_BIT_INTERLEAVER)
        bit_interleaver::interleave(conv.data(), conv.size()); // битовый интерливинг
    } else {
      // Короткий блок передаём без кодирования
      conv = part;
    }

    FrameHeader hdr;
    hdr.msg_id = id;                                   // один ID для всех фрагментов
    hdr.frag_idx = static_cast<uint16_t>(idx);         // номер фрагмента
    hdr.frag_cnt = static_cast<uint16_t>(parts);       // общее количество
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
    scrambler::scramble(frame.data(), frame.size());          // скремблируем весь кадр

    if (frame.size() > MAX_FRAME_SIZE) {            // проверка лимита
      LOG_ERROR_VAL("TxModule: превышен размер кадра=", frame.size());
      continue; // пропускаем отправку слишком больших фрагментов
    }
    radio_.send(frame.data(), frame.size());        // отправка кадра
    DEBUG_LOG_VAL("TxModule: отправлен фрагмент=", idx);
  }
  last_send_ = std::chrono::steady_clock::now();
}

// Установка паузы между отправками
void TxModule::setSendPause(uint32_t pause_ms) {
  pause_ms_ = pause_ms;
  last_send_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(pause_ms);
}
