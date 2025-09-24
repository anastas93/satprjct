#pragma once

// Модуль тестового режима передачи/приёма, использующего имитацию
// функций SendMsg_BR и received_msg из референсной прошивки.

#include <Arduino.h>
#include <stdint.h>

#ifndef ARDUINO
#include <random>
#endif

#include "libs/received_buffer/received_buffer.h"

class TestMode {
public:
  TestMode();

  // Привязка буфера принятых сообщений (используется для вывода в чат).
  void attachBuffer(ReceivedBuffer* buffer);

  // Включение/выключение тестового режима.
  void setEnabled(bool enabled);

  // Текущее состояние режима.
  bool isEnabled() const { return enabled_; }

  // Имитация отправки текста: формируется строка в стиле received_msg,
  // сохраняется в буфер и возвращается идентификатор записи.
  bool sendMessage(const String& payload, uint32_t& outId, String& err);

private:
  ReceivedBuffer* buffer_;
  bool enabled_;
  uint32_t messageId_;
  uint8_t sequence_;

#ifndef ARDUINO
  std::mt19937 rng_;
#endif

  uint32_t nextId();
  unsigned long currentMillis() const;
  String formatTime(unsigned long ms) const;
  int randomRange(int minValue, int maxValue);
  uint8_t randomByte();
};
