#pragma once
#include <stdint.h>
#include <stddef.h>
#include <mutex>
#include "libs/qos.h"

// Опции единичной передачи кадра
struct TxOptions {
  uint32_t freq_hz;   // частота передачи в Гц
  uint8_t  profile;   // профиль P0..P3
  bool     scramble;  // применять ли скремблер
};

class TxEngine {
public:
  // Отправка кадра с гарантированной установкой частоты
  bool sendFrame(const uint8_t* data, size_t len, const TxOptions& opts);
};

// Глобальный экземпляр движка и мьютекс радиомодуля
extern TxEngine g_tx_engine;
extern std::mutex gRadioMutex;

// Совместимый API для модулей, обращающихся к старому интерфейсу
bool Radio_sendRaw(const uint8_t* data, size_t len, Qos q = Qos::Normal);
