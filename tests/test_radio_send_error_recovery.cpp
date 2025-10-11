#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "radio_sx1262.h"
#include "default_settings.h"

// Вспомогательный класс, открывающий доступ к внутреннему SX1262 для тестов
class RadioSX1262TestAccessor {
public:
  static RadioSX1262::PublicSX1262& rawRadio(RadioSX1262& radio) { return radio.radio_; }
  static bool implicitEnabled(const RadioSX1262& radio) { return radio.implicitHeaderEnabled_; }
  static size_t implicitLength(const RadioSX1262& radio) { return radio.implicitHeaderLength_; }
};

int main() {
  RadioSX1262 radio;
  auto& raw = RadioSX1262TestAccessor::rawRadio(radio);

  // Настраиваем заглушку на успешные операции начальной инициализации
  raw.setFrequencyState = RADIOLIB_ERR_NONE;
  raw.startReceiveState = RADIOLIB_ERR_NONE;
  assert(radio.begin());

  const float expectedRx = radio.getRxFrequency();
  const float expectedTx = radio.getTxFrequency();

  // Сбрасываем счётчики вызовов, чтобы отслеживать только действия send()
  raw.lastSetFrequency = 0.0f;
  raw.previousSetFrequency = 0.0f;
  raw.setFrequencyCalls = 0;
  raw.startReceiveCalls = 0;
  raw.transmitCalls = 0;
  raw.lastTransmitLength = 0;
  const size_t initialStartReceive = raw.startReceiveCalls;

  // Имитируем сбой передачи и проверяем восстановление режима приёма
  raw.transmitResult = -123; // произвольный код ошибки RadioLib
  const uint8_t payload[1] = {0x42};
  int16_t sendState = radio.send(payload, sizeof(payload));
  assert(sendState == raw.transmitResult);

  // После ошибки модуль обязан вернуться на приёмную частоту
  assert(std::fabs(raw.lastSetFrequency - expectedRx) < 1e-3f);
  // startReceive() должен быть вызван повторно для возврата в RX
  assert(raw.startReceiveCalls > initialStartReceive);
  // Перед ошибкой передача должна была переключиться на TX
  assert(raw.setFrequencyCalls >= 2);
  assert(std::fabs(raw.previousSetFrequency - expectedTx) < 1e-3f);
  assert(raw.transmitCalls == 1);
  const size_t expectedTxLen = RadioSX1262TestAccessor::implicitEnabled(radio) &&
                                      RadioSX1262TestAccessor::implicitLength(radio) > 0
                                  ? RadioSX1262TestAccessor::implicitLength(radio)
                                  : sizeof(payload);
  assert(raw.lastTransmitLength == expectedTxLen);

  std::cout << "OK" << std::endl;
  return 0;
}
