#include "tdd_scheduler.h"
#include "radio_adapter.h"

#ifdef ARDUINO

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
    // В не‑передающих фазах включаем приём на остаток окна
    unsigned long now = millis();
    if (!isTxPhase(now)) {
      unsigned long t = now % cycleLen();
      unsigned long remain = 0;
      if (t < txWindowMs + guardMs) {
        // Находимся в первом защитном интервале
        remain = txWindowMs + guardMs - t;
      } else if (t < txWindowMs + guardMs + ackWindowMs) {
        // Окно ожидания ACK
        remain = txWindowMs + guardMs + ackWindowMs - t;
      } else {
        // Второй защитный интервал до следующего цикла
        remain = cycleLen() - t;
      }
      Radio_forceRx(msToTicks(remain));
    }
  }

  // Установка параметров окон
  void setParams(unsigned long tx, unsigned long ack, unsigned long guard) {
    txWindowMs = tx;
    ackWindowMs = ack;
    guardMs = guard;
  }
}

#else

namespace tdd {
  // Упрощённая заглушка для десктопных тестов: считаем, что всегда можно
  // передавать и принимать ACK без таймового расписания.
  unsigned long txWindowMs = cfg::TDD_TX_WINDOW_MS;
  unsigned long ackWindowMs = cfg::TDD_ACK_WINDOW_MS;
  unsigned long guardMs     = cfg::TDD_GUARD_MS;

  unsigned long cycleLen() { return txWindowMs + ackWindowMs + 2 * guardMs; }
  Phase currentPhase(unsigned long) { return Phase::TX; }
  bool isTxPhase(unsigned long) { return true; }
  bool isAckPhase(unsigned long) { return true; }
  void maintain() {}
  void setParams(unsigned long tx, unsigned long ack, unsigned long guard) {
    txWindowMs = tx; ackWindowMs = ack; guardMs = guard;
  }
}

#endif // ARDUINO

