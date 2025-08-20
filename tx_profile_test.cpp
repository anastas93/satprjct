#ifndef ARDUINO
#include <cstdio>
#define private public
#include "tx_pipeline.h"
#include "libs/ccsds_link/ccsds_link.h" // заголовок библиотеки CCSDS
#undef private

class Print; // заглушка для FrameLog::dump

// Минимальные заглушки для функций радиоинтерфейса и журнала кадров
class DummyRadio : public IRadioTransport {
public:
  bool setFrequency(uint32_t) override { return true; }
  bool transmit(const uint8_t*, size_t, Qos) override { return true; }
  void openRx(uint32_t) override {}
} radio;
namespace FrameLog {
void push(char, const uint8_t*, size_t,
          uint32_t, uint8_t, uint8_t,
          float, float, float,
          uint8_t, uint16_t, uint8_t,
          uint16_t, uint16_t) {}
void dump(Print&, unsigned int) {}
}
namespace tdd {
void maintain() {}
bool isTxPhase(unsigned long) { return false; }
}

// Заглушка таймера Arduino
unsigned long millis() { static unsigned long t = 0; return t += 100; }

// Заглушки модуля CCSDS: кодирование/декодирование не требуются в тесте
namespace ccsds {
void encode(const uint8_t*, size_t, uint32_t, const Params&, std::vector<uint8_t>&) {}
bool decode(const uint8_t*, size_t, uint32_t, const Params&, std::vector<uint8_t>&, int&) { return false; }
}

// Глобальные переменные для порогов автонастройки
bool g_autoRate = true;
float g_perHigh = 0.1f, g_perLow = 0.0f;
float g_ebn0High = 100.0f, g_ebn0Low = -100.0f;

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
  PacketFormatter fmt(frag, enc, metrics);
  TxPipeline tx(buf, fmt, radio, metrics);

  metrics.per_window.clear();
  metrics.ebn0_window.clear();
  metrics.per_window.add(0.0f);
  metrics.ebn0_window.add(10.0f);
  tx.controlProfile();
  if (tx.profile_idx_ != 0) { std::printf("P0 fail\n"); return 1; }

  metrics.per_window.clear();
  metrics.ebn0_window.clear();
  metrics.per_window.add(0.15f);
  metrics.ebn0_window.add(10.0f);
  tx.controlProfile();
  if (tx.profile_idx_ != 1) { std::printf("P1 fail\n"); return 1; }

  metrics.per_window.clear();
  metrics.ebn0_window.clear();
  metrics.per_window.add(0.25f);
  metrics.ebn0_window.add(10.0f);
  tx.controlProfile();
  if (tx.profile_idx_ != 2) { std::printf("P2 fail\n"); return 1; }

  metrics.per_window.clear();
  metrics.ebn0_window.clear();
  metrics.per_window.add(0.35f);
  metrics.ebn0_window.add(10.0f);
  tx.controlProfile();
  if (tx.profile_idx_ != 3) { std::printf("P3 fail\n"); return 1; }

  std::printf("tx_profile_test OK\n");
  return 0;
}
#endif // ARDUINO
