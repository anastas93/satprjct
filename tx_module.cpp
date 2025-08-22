#include "tx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "default_settings.h"
#include <vector>
#include <chrono>

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

// Инициализация модуля передачи
TxModule::TxModule(IRadio& radio, MessageBuffer& buf, PayloadMode mode)
  : radio_(radio), buffer_(buf), splitter_(mode) {
  last_send_ = std::chrono::steady_clock::now();
}

// Смена режима пакета
void TxModule::setPayloadMode(PayloadMode mode) { splitter_.setMode(mode); }

// Помещаем сообщение в буфер через делитель
uint32_t TxModule::queue(const uint8_t* data, size_t len) {
  if (!data || len == 0) {                        // проверка указателя
    DEBUG_LOG("TxModule: пустой ввод");
    return 0;
  }
  DEBUG_LOG_VAL("TxModule: постановка длины=", len);
  uint32_t res = splitter_.splitAndEnqueue(buffer_, data, len);
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
  if (!buffer_.hasPending()) {
    DEBUG_LOG("TxModule: очередь пуста");
    return;
  }
  std::vector<uint8_t> msg;
  uint32_t id = 0;                      // получаемый идентификатор сообщения
  if (!buffer_.pop(id, msg)) {
    DEBUG_LOG("TxModule: ошибка извлечения");
    return;
  }

  FrameHeader hdr;
  hdr.msg_id = id;
  hdr.payload_len = static_cast<uint16_t>(msg.size());
  uint8_t hdr_buf[FrameHeader::SIZE];
  if (!hdr.encode(hdr_buf, sizeof(hdr_buf), msg.data(), msg.size())) {
    DEBUG_LOG("TxModule: ошибка кодирования заголовка");
    return;
  }

  std::vector<uint8_t> frame;
  frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
  frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE); // повтор заголовка
  auto payload = insertPilots(msg);
  frame.insert(frame.end(), payload.begin(), payload.end());

  radio_.send(frame.data(), frame.size());
  DEBUG_LOG_VAL("TxModule: отправлен кадр id=", id);
  last_send_ = std::chrono::steady_clock::now();
}

// Установка паузы между отправками
void TxModule::setSendPause(uint32_t pause_ms) {
  pause_ms_ = pause_ms;
  last_send_ = std::chrono::steady_clock::now() - std::chrono::milliseconds(pause_ms);
}
