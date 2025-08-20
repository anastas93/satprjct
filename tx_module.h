#pragma once
#include <cstdint>
#include "radio_interface.h"
#include "message_buffer.h"

// Модуль передачи данных
class TxModule {
public:
  TxModule(IRadio& radio, MessageBuffer& buf);
  // Добавляет сообщение в очередь на отправку
  uint32_t queue(const uint8_t* data, size_t len);
  // Отправляет первое сообщение из очереди (если есть)
  void loop();
private:
  IRadio& radio_;
  MessageBuffer& buffer_;
};
