#include "tx_module.h"
#include "frame/frame_header.h"
#include <vector>

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
  : radio_(radio), buffer_(buf), splitter_(mode) {}

// Смена режима пакета
void TxModule::setPayloadMode(PayloadMode mode) { splitter_.setMode(mode); }

// Помещаем сообщение в буфер через делитель
uint32_t TxModule::queue(const uint8_t* data, size_t len) {
  return splitter_.splitAndEnqueue(buffer_, data, len);
}

// Пытаемся отправить первое сообщение
void TxModule::loop() {
  if (!buffer_.hasPending()) return;
  std::vector<uint8_t> msg;
  uint32_t id = 0;                      // получаемый идентификатор сообщения
  if (!buffer_.pop(id, msg)) return;

  FrameHeader hdr;
  hdr.msg_id = id;
  hdr.payload_len = static_cast<uint16_t>(msg.size());
  uint8_t hdr_buf[FrameHeader::SIZE];
  if (!hdr.encode(hdr_buf, sizeof(hdr_buf), msg.data(), msg.size())) return;

  std::vector<uint8_t> frame;
  frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
  frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE); // повтор заголовка
  auto payload = insertPilots(msg);
  frame.insert(frame.end(), payload.begin(), payload.end());

  radio_.send(frame.data(), frame.size());
}
