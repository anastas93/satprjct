#pragma once
#include <stdint.h>
#include <stddef.h>

// Универсальный интерфейс для радиомодулей
class IRadioTransport {
public:
  virtual ~IRadioTransport() = default;

  // Установка рабочей частоты в герцах
  virtual bool setFrequency(uint32_t hz) = 0;

  // Передача произвольного блока данных
  virtual bool transmit(const uint8_t* data, size_t len) = 0;

  // Открыть приём на заданное число тиков
  virtual void openRx(uint32_t rx_ticks) = 0;

  // Настройка параметров модуляции
  virtual bool setBandwidth(uint32_t khz) = 0;
  virtual bool setSpreadingFactor(uint8_t sf) = 0;
  virtual bool setCodingRate(uint8_t cr4x) = 0;
  virtual bool setTxPower(int8_t dBm) = 0;
  virtual bool setRxBoost(bool on) = 0;

  // Получение качественных метрик канала
  virtual bool getSNR(float& snr) = 0;
  virtual bool getEbN0(float& ebn0) = 0;
  virtual bool getLinkQuality(bool& good) = 0;
  virtual bool getRSSI(float& rssi) = 0;
  virtual bool isSynced() = 0;
};

// Глобальный указатель на активный транспорт
extern IRadioTransport* g_radio;

