#include <cassert>
#include <cstdint>
#include <vector>
#include <iostream>

#include "radio_sx1262.h"
#include "default_settings.h"

// Доступ к внутреннему состоянию RadioSX1262 для модульных тестов
class RadioSX1262TestAccessor {
public:
  static RadioSX1262::PublicSX1262& rawRadio(RadioSX1262& radio) { return radio.radio_; }
  static bool implicitEnabled(const RadioSX1262& radio) { return radio.implicitHeaderEnabled_; }
  static size_t implicitLength(const RadioSX1262& radio) { return radio.implicitHeaderLength_; }
  static void forceImplicit(RadioSX1262& radio, bool enabled, size_t len) {
    radio.implicitHeaderEnabled_ = enabled;
    radio.implicitHeaderLength_ = len;
  }
};

static void resetStub(RadioSX1262::PublicSX1262& raw) {
  raw.transmitCalls = 0;
  raw.transmitLengthHistory.clear();
  raw.transmitPayloadHistory.clear();
  raw.implicitHeaderCalls = 0;
  raw.explicitHeaderCalls = 0;
}

// Проверяем, что данные разбиваются на несколько implicit-фрагментов
static void testImplicitFragmentation() {
  RadioSX1262 radio;
  auto& raw = RadioSX1262TestAccessor::rawRadio(radio);
  raw.setFrequencyState = RADIOLIB_ERR_NONE;
  raw.startReceiveState = RADIOLIB_ERR_NONE;
  raw.transmitResult = RADIOLIB_ERR_NONE;

  assert(radio.begin());
  assert(RadioSX1262TestAccessor::implicitEnabled(radio));
  const size_t implicitLen = RadioSX1262TestAccessor::implicitLength(radio);
  assert(implicitLen > 0);

  resetStub(raw);

  const size_t payloadSize = implicitLen * 2 + implicitLen / 2; // три фрагмента: 32,32,16 (при implicitLen=32)
  std::vector<uint8_t> payload(payloadSize);
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<uint8_t>(i & 0xFFU);
  }

  const int16_t state = radio.send(payload.data(), payload.size());
  assert(state == RADIOLIB_ERR_NONE);
  assert(raw.transmitCalls == 3);
  assert(raw.transmitLengthHistory.size() == 3);
  assert(raw.transmitPayloadHistory.size() == 3);
  for (size_t i = 0; i < raw.transmitLengthHistory.size(); ++i) {
    assert(raw.transmitLengthHistory[i] == implicitLen);
  }

  const auto& first = raw.transmitPayloadHistory[0];
  assert(first.size() == implicitLen);
  for (size_t i = 0; i < implicitLen; ++i) {
    assert(first[i] == payload[i]);
  }

  const auto& second = raw.transmitPayloadHistory[1];
  assert(second.size() == implicitLen);
  for (size_t i = 0; i < implicitLen; ++i) {
    assert(second[i] == payload[implicitLen + i]);
  }

  const auto& third = raw.transmitPayloadHistory[2];
  assert(third.size() == implicitLen);
  const size_t remainder = payloadSize - 2 * implicitLen;
  for (size_t i = 0; i < remainder; ++i) {
    assert(third[i] == payload[2 * implicitLen + i]);
  }
  for (size_t i = remainder; i < implicitLen; ++i) {
    assert(third[i] == 0U); // оставшиеся байты должны быть обнулены
  }

  assert(raw.implicitHeaderCalls == 0); // send() не должен менять конфигурацию заголовков
  assert(raw.explicitHeaderCalls == 0);
}

// Проверяем фрагментацию в явном режиме (explicit header)
static void testExplicitFragmentation() {
  RadioSX1262 radio;
  auto& raw = RadioSX1262TestAccessor::rawRadio(radio);
  raw.setFrequencyState = RADIOLIB_ERR_NONE;
  raw.startReceiveState = RADIOLIB_ERR_NONE;
  raw.transmitResult = RADIOLIB_ERR_NONE;

  assert(radio.begin());
  RadioSX1262TestAccessor::forceImplicit(radio, false, 0); // имитируем explicit header

  resetStub(raw);

  constexpr size_t kMaxPacketSize = 245; // лимит SX1262
  const size_t payloadSize = kMaxPacketSize + 17; // гарантированно больше одного кадра
  std::vector<uint8_t> payload(payloadSize);
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<uint8_t>((i * 3) & 0xFFU);
  }

  const int16_t state = radio.send(payload.data(), payload.size());
  assert(state == RADIOLIB_ERR_NONE);
  assert(raw.transmitCalls == 2);
  assert(raw.transmitLengthHistory.size() == 2);
  assert(raw.transmitPayloadHistory.size() == 2);
  assert(raw.transmitLengthHistory[0] == kMaxPacketSize);
  assert(raw.transmitLengthHistory[1] == 17);

  const auto& tail = raw.transmitPayloadHistory[1];
  assert(tail.size() == 17);
  for (size_t i = 0; i < tail.size(); ++i) {
    assert(tail[i] == payload[kMaxPacketSize + i]);
  }

  assert(raw.implicitHeaderCalls == 0);
  assert(raw.explicitHeaderCalls == 0);
}

int main() {
  testImplicitFragmentation();
  testExplicitFragmentation();
  std::cout << "OK" << std::endl;
  return 0;
}
