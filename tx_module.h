#pragma once
#include <cstdint>
#include <chrono>
#include <array>
#include "radio_interface.h"
#include "message_buffer.h"
#include "libs/packetizer/packet_splitter.h" // подключаем разделитель пакетов из каталога libs

// Модуль передачи данных с поддержкой классов QoS
class TxModule {
public:
  // Конструктор принимает радио, размеры очередей по классам QoS и режим пакета
  TxModule(IRadio& radio, const std::array<size_t,4>& capacities, PayloadMode mode = PayloadMode::SMALL);
  // Смена режима размера полезной нагрузки
  void setPayloadMode(PayloadMode mode);
  // Добавляет сообщение в очередь на отправку с указанием класса QoS (0..3)
  uint32_t queue(const uint8_t* data, size_t len, uint8_t qos = 0);
  // Отправляет первое доступное сообщение (если есть)
  void loop();
  // Задать минимальную паузу между отправками (мс)
  void setSendPause(uint32_t pause_ms);
private:
  IRadio& radio_;
  std::array<MessageBuffer,4> buffers_;             // очереди сообщений по классам QoS
  PacketSplitter splitter_;
  uint32_t pause_ms_ = 0;                           // пауза между пакетами
  std::chrono::steady_clock::time_point last_send_; // время последней отправки
};

