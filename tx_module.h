#pragma once
#include <cstdint>
#include <chrono>
#include "radio_interface.h"
#include "message_buffer.h"
#include "libs/packetizer/packet_splitter.h" // подключаем разделитель пакетов из каталога libs

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
  // Задать минимальную паузу между отправками (мс)
  void setSendPause(uint32_t pause_ms);
private:
  IRadio& radio_;
  MessageBuffer& buffer_;
  PacketSplitter splitter_;
  uint32_t pause_ms_ = 0;                           // пауза между пакетами
  std::chrono::steady_clock::time_point last_send_; // время последней отправки
};

