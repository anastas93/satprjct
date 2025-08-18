#ifndef ARDUINO
#include <cstdio>
#define private public
#include "tx_pipeline.h"
#undef private

class Print; // forward declaration for FrameLog::dump stub

namespace FrameLog {
void push(char, const uint8_t*, size_t) {}
void dump(Print&, unsigned int) {}
}
namespace tdd {
void maintain() {}
bool isTxPhase(unsigned long) { return false; }
}

class DummyEncryptor : public IEncryptor {
public:
  bool encrypt(const uint8_t*, size_t, const uint8_t*, size_t, std::vector<uint8_t>&) override { return false; }
  bool decrypt(const uint8_t*, size_t, const uint8_t*, size_t, std::vector<uint8_t>&) override { return false; }
  bool isReady() const override { return false; }
};

int main() {
  PipelineMetrics metrics{};
  MessageBuffer buf;
  Fragmenter frag;
  DummyEncryptor enc;
  TxPipeline tx(buf, frag, enc, metrics);

  metrics.per_ema.value = 0.0f;
  metrics.ebn0_ema.value = 10.0f;
  tx.updateProfile();
  if (tx.profile_idx_ != 0) { std::printf("P0 fail\n"); return 1; }

  metrics.per_ema.value = 0.15f;
  metrics.ebn0_ema.value = 10.0f;
  tx.updateProfile();
  if (tx.profile_idx_ != 1) { std::printf("P1 fail\n"); return 1; }

  metrics.per_ema.value = 0.25f;
  metrics.ebn0_ema.value = 6.0f;
  tx.updateProfile();
  if (tx.profile_idx_ != 2) { std::printf("P2 fail\n"); return 1; }

  metrics.per_ema.value = 0.35f;
  metrics.ebn0_ema.value = 2.0f;
  tx.updateProfile();
  if (tx.profile_idx_ != 3) { std::printf("P3 fail\n"); return 1; }

  std::printf("tx_profile_test OK\n");
  return 0;
}
#endif // ARDUINO
