#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "radio_sx1262.h"
#include "radio_interface.h"
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
  const size_t expectedTxLength =
      (RadioSX1262TestAccessor::implicitEnabled(radio) &&
       RadioSX1262TestAccessor::implicitLength(radio) > 0)
          ? RadioSX1262TestAccessor::implicitLength(radio)
          : sizeof(payload);
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
  assert(raw.lastTransmitLength == expectedTxLength);

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
  assert(raw.lastTransmitLength == expectedTxLength);
  // После успешной отправки радио возвращается на приём и флаг TX_DONE очищен
  assert((raw.testIrqFlags & RADIOLIB_SX126X_IRQ_TX_DONE) == 0U);
  assert(raw.setFrequencyCalls >= 2);
  assert(raw.startReceiveCalls >= 1);

  // Проверяем повторную попытку при реальном таймауте без флага TX_DONE.
  raw.transmitSequence = {RADIOLIB_ERR_TX_TIMEOUT, RADIOLIB_ERR_NONE, 0, 0, 0, 0, 0, 0};
  raw.transmitSequenceLength = 2;
  raw.transmitSequenceIndex = 0;
  raw.transmitResult = RADIOLIB_ERR_TX_TIMEOUT;
  raw.testIrqFlags = RADIOLIB_SX126X_IRQ_TIMEOUT;
  raw.testClearIrqState = RADIOLIB_ERR_NONE;
  raw.setFrequencyCalls = 0;
  raw.startReceiveCalls = 0;
  raw.transmitCalls = 0;
  raw.lastTransmitLength = 0;

  sendState = radio.send(payload, sizeof(payload));
  assert(sendState == RADIOLIB_ERR_NONE);
  assert(raw.transmitCalls == 2);                 // выполнена повторная попытка
  assert(raw.lastTransmitLength == expectedTxLength);
  assert(raw.testIrqFlags == 0U);                 // IRQ были очищены между попытками

  // Проверяем, что после исчерпания повторов RadioLib-таймаут
  // преобразуется в единый код шины IRadio::ERR_TIMEOUT.
  raw.transmitSequence = {RADIOLIB_ERR_TX_TIMEOUT, RADIOLIB_ERR_TX_TIMEOUT, 0, 0, 0, 0, 0, 0};
  raw.transmitSequenceLength = 2;
  raw.transmitSequenceIndex = 0;
  raw.transmitResult = RADIOLIB_ERR_TX_TIMEOUT;
  raw.testIrqFlags = 0;
  raw.setFrequencyCalls = 0;
  raw.startReceiveCalls = 0;
  raw.transmitCalls = 0;
  raw.lastTransmitLength = 0;

  sendState = radio.send(payload, sizeof(payload));
  assert(sendState == IRadio::ERR_TIMEOUT);       // внешний код объединён с интерфейсом IRadio
  assert(raw.transmitCalls == 2);                 // две попытки передачи
  assert(std::fabs(raw.lastSetFrequency - expectedRx) < 1e-3f);
  assert(raw.startReceiveCalls >= 1);             // приём восстановлен после сбоя

  std::cout << "OK" << std::endl;
  return 0;
}
