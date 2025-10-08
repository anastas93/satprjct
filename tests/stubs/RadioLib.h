#pragma once
// Заглушка RadioLib для модульных тестов, исключающая обращение к реальному железу
#include <cstddef>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cstring>

// Константы статусов и флагов IRQ, используемые в прошивке
#define RADIOLIB_ERR_NONE 0
#define RADIOLIB_ERR_TX_TIMEOUT -5
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
#define RADIOLIB_SX126X_IRQ_LR_FHSS_HOP 0x1000U

class Module {
public:
  Module(int, int, int, int) {}
};

// Минимальная заглушка SX1262, предоставляющая доступ к состоянию через публичные поля
class SX1262 {
public:
  explicit SX1262(Module* module) : module_(module) {}

  int16_t begin(float, float, uint8_t, uint8_t, uint8_t, int8_t, uint16_t, float, bool) {
    (void)module_;
    return beginState;
  }

  int16_t setFrequency(float freq) {
    previousSetFrequency = lastSetFrequency; // запоминаем предыдущую частоту
    lastSetFrequency = freq;          // фиксируем установленную частоту
    ++setFrequencyCalls;              // считаем вызовы смены частоты
    return setFrequencyState;
  }
  int16_t transmit(uint8_t*, size_t len) {
    lastTransmitLength = len;         // сохраняем длину последней передачи
    ++transmitCalls;                  // считаем количество передач
    if (transmitSequenceIndex < transmitSequenceLength) {
      return transmitSequence[transmitSequenceIndex++];
    }
    return transmitResult;
  }
  size_t getPacketLength(bool = true) { return testPacketLength; }
  int16_t readData(uint8_t* data, size_t len) {
    if (testReadState == RADIOLIB_ERR_NONE && data) {
      const size_t copyLen = std::min(len, testReadBufferSize);
      if (copyLen > 0) {
        std::memcpy(data, testReadBuffer.data(), copyLen);
      }
    }
    return testReadState;
  }
  float getSNR() const { return testSnr; }
  float getRSSI() const { return testRssi; }
  uint8_t randomByte() { return nextRandom++; }
  int16_t startReceive() {
    ++startReceiveCalls;              // учитываем попытки запуска приёма
    return startReceiveState;
  }
  int16_t setDio2AsRfSwitch(bool) { return setDio2AsRfSwitchState; }
  int16_t setDioIrqParams(uint16_t, uint16_t, uint16_t = RADIOLIB_SX126X_IRQ_NONE,
                          uint16_t = RADIOLIB_SX126X_IRQ_NONE) {
    return setDioIrqParamsState;
  }
  int16_t setCRC(uint8_t, uint16_t = 0x1D0F, uint16_t = 0x1021, bool = true) {
    return setCRCState;
  }
  int16_t invertIQ(bool) { return invertIqState; }
  int16_t implicitHeader(size_t) { return implicitHeaderState; }
  int16_t explicitHeader() { return explicitHeaderState; }
  int16_t reset() { return RADIOLIB_ERR_NONE; }
  int16_t setBandwidth(float) { return RADIOLIB_ERR_NONE; }
  int16_t setSpreadingFactor(int) { return RADIOLIB_ERR_NONE; }
  int16_t setCodingRate(int) { return RADIOLIB_ERR_NONE; }
  int16_t setOutputPower(int8_t) { return RADIOLIB_ERR_NONE; }
  int16_t setRxBoostedGainMode(bool, bool) { return RADIOLIB_ERR_NONE; }
  void setDio1Action(void (*)(void)) {}
  uint32_t getIrqFlags() { return testIrqFlags; }
  int16_t clearIrqFlags(uint32_t mask) {
    testIrqFlags &= ~mask;
    return testClearIrqState;
  }
  uint16_t getIrqStatus() { return static_cast<uint16_t>(testIrqFlags); }
  int16_t getIrqStatus(uint16_t* dest) {
    if (dest) {
      *dest = static_cast<uint16_t>(testIrqFlags);
    }
    return RADIOLIB_ERR_NONE;
  }
  int16_t clearIrqStatus() { return testClearIrqState; }

  Module* module_;
  float lastSetFrequency = 0.0f;       // последняя установленная частота
  float previousSetFrequency = 0.0f;   // частота из предыдущего вызова setFrequency()
  size_t setFrequencyCalls = 0;        // количество вызовов setFrequency()
  size_t startReceiveCalls = 0;        // количество вызовов startReceive()
  size_t transmitCalls = 0;            // количество вызовов transmit()
  size_t lastTransmitLength = 0;       // длина последней переданной полезной нагрузки
  int16_t transmitResult = RADIOLIB_ERR_NONE; // код возврата transmit()
  std::array<int16_t, 8> transmitSequence{}; // последовательность возвратов transmit()
  size_t transmitSequenceLength = 0;   // фактическая длина последовательности
  size_t transmitSequenceIndex = 0;    // текущий индекс в последовательности
  int16_t setFrequencyState = RADIOLIB_ERR_NONE; // код возврата setFrequency()
  int16_t startReceiveState = RADIOLIB_ERR_NONE; // код возврата startReceive()
  int16_t beginState = RADIOLIB_ERR_NONE;        // код возврата begin()
  int16_t setDio2AsRfSwitchState = RADIOLIB_ERR_NONE; // код возврата setDio2AsRfSwitch()
  int16_t setDioIrqParamsState = RADIOLIB_ERR_NONE;   // код возврата setDioIrqParams()
  int16_t setCRCState = RADIOLIB_ERR_NONE;            // код возврата setCRC()
  int16_t invertIqState = RADIOLIB_ERR_NONE;          // код возврата invertIQ()
  int16_t implicitHeaderState = RADIOLIB_ERR_NONE;    // код возврата implicitHeader()
  int16_t explicitHeaderState = RADIOLIB_ERR_NONE;    // код возврата explicitHeader()
  uint32_t testIrqFlags = 0;
  int16_t testClearIrqState = RADIOLIB_ERR_NONE;
  size_t testPacketLength = 0;
  int16_t testReadState = RADIOLIB_ERR_NONE;
  std::array<uint8_t, 256> testReadBuffer{};
  size_t testReadBufferSize = 0;
  float testSnr = 0.0f;
  float testRssi = 0.0f;
  uint8_t nextRandom = 0;
};

