#include "tdd_scheduler.h"
#include "radio_adapter.h"

namespace tdd {
  // Текущие значения окон в мс
  unsigned long txWindowMs = cfg::TDD_TX_WINDOW_MS;
  unsigned long ackWindowMs = cfg::TDD_ACK_WINDOW_MS;
  unsigned long guardMs     = cfg::TDD_GUARD_MS;

  // Полная длина цикла
  unsigned long cycleLen() {
    return txWindowMs + ackWindowMs + 2 * guardMs;
  }

  // Определение текущей фазы
  Phase currentPhase(unsigned long ms) {
    unsigned long t = ms % cycleLen();
    if (t < txWindowMs) return Phase::TX;
    t -= txWindowMs;
    if (t < guardMs) return Phase::GUARD1;
    t -= guardMs;
    if (t < ackWindowMs) return Phase::ACK;
    return Phase::GUARD2;
  }

  bool isTxPhase(unsigned long ms) {
    return currentPhase(ms) == Phase::TX;
  }

  bool isAckPhase(unsigned long ms) {
    return currentPhase(ms) == Phase::ACK;
  }

  void maintain() {
    // В не-передающих фазах включаем приём
    unsigned long now = millis();
    if (!isTxPhase(now)) {
      Radio_forceRx();
    }
  }

  // Установка параметров окон
  void setParams(unsigned long tx, unsigned long ack, unsigned long guard) {
    txWindowMs = tx;
    ackWindowMs = ack;
    guardMs = guard;
  }
}

