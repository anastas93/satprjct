#include "tdd_scheduler.h"
#include "radio_adapter.h"

namespace tdd {
  unsigned long cycleLen() {
    return TX_WINDOW + ACK_WINDOW + 2 * GUARD;
  }

  Phase currentPhase(unsigned long ms) {
    unsigned long t = ms % cycleLen();
    if (t < TX_WINDOW) return Phase::TX;
    t -= TX_WINDOW;
    if (t < GUARD) return Phase::GUARD1;
    t -= GUARD;
    if (t < ACK_WINDOW) return Phase::ACK;
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
}
