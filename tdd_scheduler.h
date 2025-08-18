#pragma once
#include <Arduino.h>
#include "config.h"

namespace tdd {
  // Константы временных окон (мс)
  extern unsigned long txWindowMs;  // окно передачи
  extern unsigned long ackWindowMs; // окно ожидания подтверждений
  extern unsigned long guardMs;     // защитный интервал между окнами

  enum class Phase { TX, GUARD1, ACK, GUARD2 };

  // Полная длина цикла TDD
  unsigned long cycleLen();

  // Определение текущей фазы по времени
  Phase currentPhase(unsigned long ms = millis());

  // Проверка, находимся ли мы в окне передачи
  bool isTxPhase(unsigned long ms = millis());

  // Проверка, находимся ли мы в окне приёма ACK
  bool isAckPhase(unsigned long ms = millis());

  // Принудительное переключение в RX вне фазы TX
  void maintain();

  // Установка параметров окон в мс
  void setParams(unsigned long tx, unsigned long ack, unsigned long guard);
}
