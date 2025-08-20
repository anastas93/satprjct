#pragma once
#include <stddef.h>
#include <stdint.h>
#include "libs/qos.h"

// Интерфейс радиотранспорта для абстракции аппаратного модуля
class IRadioTransport {
public:
  virtual ~IRadioTransport() = default;
  // Установка рабочей частоты в Гц
  virtual bool setFrequency(uint32_t hz) = 0;
  // Передача массива байт с учётом приоритета QoS
  virtual bool transmit(const uint8_t* data, size_t len, Qos qos) = 0;
  // Открыть окно приёма на указанное число тиков
  virtual void openRx(uint32_t rx_ticks) = 0;
};

// Глобальный экземпляр радиоинтерфейса для текущего модуля
extern IRadioTransport& g_radio;
