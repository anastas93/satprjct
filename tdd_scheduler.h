#pragma once
#include <Arduino.h>
#include "config.h"

namespace tdd {
  // Константы временных окон (мс)
  static constexpr unsigned long TX_WINDOW = cfg::TDD_TX_WINDOW_MS;  // окно передачи
  static constexpr unsigned long ACK_WINDOW = cfg::TDD_ACK_WINDOW_MS; // окно ожидания подтверждений
  static constexpr unsigned long GUARD = cfg::TDD_GUARD_MS;           // защитный интервал между окнами

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
}
