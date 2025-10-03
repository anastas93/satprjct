#pragma once
// Заглушка RadioLib для модульных тестов, исключающая обращение к реальному железу
#include <cstddef>
#include <cstdint>

// Константы статусов и флагов IRQ, используемые в прошивке
#define RADIOLIB_ERR_NONE 0
#define RADIOLIB_ERR_CHANNEL_BUSY -16
#define RADIOLIB_ERR_CHANNEL_BUSY_LBT -17
#define RADIOLIB_SX126X_IRQ_NONE 0x0000U
#define RADIOLIB_SX126X_IRQ_ALL 0xFFFFU
#define RADIOLIB_SX126X_IRQ_TX_DONE 0x0001U
#define RADIOLIB_SX126X_IRQ_RX_DONE 0x0002U
#define RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED 0x0004U
#define RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID 0x0008U
#define RADIOLIB_SX126X_IRQ_HEADER_VALID 0x0010U
#define RADIOLIB_SX126X_IRQ_HEADER_ERR 0x0020U
#define RADIOLIB_SX126X_IRQ_CRC_ERR 0x0040U
#define RADIOLIB_SX126X_IRQ_TIMEOUT 0x0080U
#define RADIOLIB_IRQ_RX_TIMEOUT 0x0100U
#define RADIOLIB_IRQ_TX_TIMEOUT 0x0200U
#define RADIOLIB_SX126X_IRQ_CAD_DONE 0x0400U
#define RADIOLIB_SX126X_IRQ_CAD_DETECTED 0x0800U

class Module {
public:
  Module(int, int, int, int) {}
};

// Минимальная заглушка SX1262, предоставляющая доступ к состоянию через публичные поля
class SX1262 {
public:
  explicit SX1262(Module* module) : module_(module) {}

  int16_t begin(float, float, int, int, uint8_t, int8_t, uint16_t, float, bool) {
    (void)module_;
    return RADIOLIB_ERR_NONE;
  }

  int16_t setFrequency(float) { return RADIOLIB_ERR_NONE; }
  int16_t transmit(uint8_t*, size_t) { return RADIOLIB_ERR_NONE; }
  size_t getPacketLength(bool = true) { return testPacketLength; }
  int16_t readData(uint8_t*, size_t) { return testReadState; }
  float getSNR() const { return testSnr; }
  float getRSSI() const { return testRssi; }
  uint8_t randomByte() { return nextRandom++; }
  int16_t startReceive() { return RADIOLIB_ERR_NONE; }
  int16_t reset() { return RADIOLIB_ERR_NONE; }
  int16_t setBandwidth(float) { return RADIOLIB_ERR_NONE; }
  int16_t setSpreadingFactor(int) { return RADIOLIB_ERR_NONE; }
  int16_t setCodingRate(int) { return RADIOLIB_ERR_NONE; }
  int16_t setOutputPower(int8_t) { return RADIOLIB_ERR_NONE; }
  int16_t setRxBoostedGainMode(bool, bool) { return RADIOLIB_ERR_NONE; }
  void setDio1Action(void (*)(void)) {}
  uint32_t getIrqFlags() { return testIrqFlags; }
  int16_t clearIrqFlags(uint32_t) { return testClearIrqState; }
  uint16_t getIrqStatus() { return static_cast<uint16_t>(testIrqFlags); }
  int16_t getIrqStatus(uint16_t* dest) {
    if (dest) {
      *dest = static_cast<uint16_t>(testIrqFlags);
    }
    return RADIOLIB_ERR_NONE;
  }
  int16_t clearIrqStatus() { return testClearIrqState; }

  Module* module_;
  uint32_t testIrqFlags = 0;
  int16_t testClearIrqState = RADIOLIB_ERR_NONE;
  size_t testPacketLength = 0;
  int16_t testReadState = RADIOLIB_ERR_NONE;
  float testSnr = 0.0f;
  float testRssi = 0.0f;
  uint8_t nextRandom = 0;
};

