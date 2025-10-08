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
  assert(raw.lastTransmitLength == sizeof(payload));

  // Теперь эмулируем ситуацию, когда RadioLib возвращает таймаут,
  // хотя IRQ TX_DONE установлен — сообщение должно считаться отправленным.
  raw.transmitResult = RADIOLIB_ERR_TX_TIMEOUT;
  raw.testIrqFlags = RADIOLIB_SX126X_IRQ_TX_DONE;
  raw.testClearIrqState = RADIOLIB_ERR_NONE;
  raw.setFrequencyCalls = 0;
  raw.startReceiveCalls = 0;
  raw.transmitCalls = 0;
  raw.lastTransmitLength = 0;

  sendState = radio.send(payload, sizeof(payload));
  assert(sendState == RADIOLIB_ERR_NONE);
  assert(raw.transmitCalls == 1);
  assert(raw.lastTransmitLength == sizeof(payload));
  // После успешной отправки радио возвращается на приём и флаг TX_DONE очищен
  assert((raw.testIrqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) == 0U);
  assert(raw.setFrequencyCalls >= 2);
  assert(raw.startReceiveCalls >= 1);

  std::cout << "OK" << std::endl;
  return 0;
}
