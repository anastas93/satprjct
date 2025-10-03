#include <cassert>
#include <string>
#include <iostream>

// Разрешаем доступ к внутренним полям для настройки заглушек
#define private public
#define protected public
#include "../src/radio_sx1262.h"
#undef private
#undef protected

#include "../src/default_settings.h"  // макросы логирования
#include "../src/libs/log_hook/log_hook.h"
#include "stubs/Arduino.h"

// Глобальные переменные для фиксации результата колбэка
namespace {
std::string gCallbackMessage;
uint32_t gCallbackUptime = 0;

void captureIrqLog(const char* message, uint32_t uptime) {
  gCallbackMessage = message ? message : std::string();
  gCallbackUptime = uptime;
}
}

int main() {
  // Проверяем статический формат вывода с новым RX/TX timeout
  LogHook::clear();
  const uint32_t combinedFlags = RADIOLIB_SX126X_IRQ_RX_DONE |
                                 RADIOLIB_IRQ_RX_TIMEOUT |
                                 RADIOLIB_IRQ_TX_TIMEOUT;
  RadioSX1262::logIrqFlags(combinedFlags);
  auto logs = LogHook::getRecent(1);
  assert(logs.size() == 1);
  const std::string& combinedMessage = logs[0].text;
  assert(combinedMessage.find("RadioSX1262: IRQ=0x0302") == 0);
  assert(combinedMessage.find("IRQ_RX_DONE – пакет принят") != std::string::npos);
  assert(combinedMessage.find("IRQ_RX_TIMEOUT – истёк таймаут ожидания приёма") !=
         std::string::npos);
  assert(combinedMessage.find("IRQ_TX_TIMEOUT – истёк таймаут ") != std::string::npos);

  // Проверяем перенос отложенных логов и работу колбэка для таймаута TX
  LogHook::clear();
  gCallbackMessage.clear();
  gCallbackUptime = 0;
  ArduinoStub::gMillis = 123456;

  RadioSX1262 radio;
  radio.setIrqLogCallback(&captureIrqLog);
  radio.radio_.testIrqFlags = RADIOLIB_IRQ_TX_TIMEOUT;
  radio.radio_.testClearIrqState = RADIOLIB_ERR_NONE;
  radio.irqNeedsRead_ = true;
  radio.irqLogPending_ = true;

  radio.processPendingIrqLog();

  auto processedLogs = LogHook::getRecent(2);
  bool found = false;
  std::string capturedMessage;
  for (const auto& entry : processedLogs) {
    if (entry.text.find("RadioSX1262: IRQ=0x0200") == 0 &&
        entry.text.find("IRQ_TX_TIMEOUT – истёк таймаут ") !=
            std::string::npos) {
      found = true;
      capturedMessage = entry.text;
      break;
    }
  }
  assert(found);
  assert(gCallbackMessage == capturedMessage);
  assert(gCallbackUptime == ArduinoStub::gMillis);

  std::cout << "Все проверки IRQ-логов RadioSX1262 успешно пройдены" << std::endl;
  return 0;
}
