#include "tx_module.h"
#include <vector>

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
  if (buffer_.pop(id, msg)) {
    radio_.send(msg.data(), msg.size());
  }
}

