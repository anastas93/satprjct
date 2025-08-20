#pragma once
#include <cstdint>
#include "radio_interface.h"
#include "message_buffer.h"
#include "packetizer/packet_splitter.h"

// Модуль передачи данных
class TxModule {
public:
  // Конструктор принимает радио, буфер и режим пакета
  TxModule(IRadio& radio, MessageBuffer& buf, PayloadMode mode = PayloadMode::SMALL);
  // Смена режима размера полезной нагрузки
  void setPayloadMode(PayloadMode mode);
  // Добавляет сообщение в очередь на отправку
  uint32_t queue(const uint8_t* data, size_t len);
  // Отправляет первое сообщение из очереди (если есть)
  void loop();
private:
  IRadio& radio_;
  MessageBuffer& buffer_;
  PacketSplitter splitter_;
};

