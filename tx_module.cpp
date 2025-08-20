#include "tx_module.h"
#include <vector>

TxModule::TxModule(IRadio& radio, MessageBuffer& buf)
  : radio_(radio), buffer_(buf) {}

// Помещаем сообщение в буфер
uint32_t TxModule::queue(const uint8_t* data, size_t len) {
  return buffer_.enqueue(data, len);
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
