#pragma once
#include <stddef.h>
#include <stdint.h>
#include "libs/qos.h"

// Интерфейс транспорта для отправки готовых кадров
class IRadioTransport {
public:
  virtual ~IRadioTransport() = default;
  // Отправка кадра в радио
  virtual bool sendFrame(const uint8_t* data, size_t len, Qos qos) = 0;
};

// Адаптер, использующий существующий Radio_sendRaw
class RadioTransport : public IRadioTransport {
public:
  bool sendFrame(const uint8_t* data, size_t len, Qos qos) override;
};
