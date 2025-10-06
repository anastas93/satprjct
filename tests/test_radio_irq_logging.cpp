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

size_t formatIrqLogMessage(uint32_t flags, char* buffer, size_t capacity);

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
                                 RADIOLIB_IRQ_TX_TIMEOUT |
                                 RADIOLIB_SX126X_IRQ_LR_FHSS_HOP;
  RadioSX1262::logIrqFlags(combinedFlags);
  auto logs = LogHook::getRecent(1);
  assert(logs.size() == 1);
  const std::string& combinedMessage = logs[0].text;
  char manualBuffer[512];
  size_t manualLen = formatIrqLogMessage(combinedFlags, manualBuffer, sizeof(manualBuffer));
  assert(combinedMessage.find("RadioSX1262: IRQ=0x1302") == 0);
  assert(manualLen <= 240);
  assert(combinedMessage.find("IRQ_RX_DONE – RX завершён") != std::string::npos);
  assert(combinedMessage.find("IRQ_RX_TIMEOUT – таймаут RX") != std::string::npos);
  assert(combinedMessage.find("IRQ_TX_TIMEOUT – таймаут TX") != std::string::npos);
  assert(combinedMessage.find("IRQ_LR_FHSS_HOP – LR-FHSS hop") != std::string::npos);

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
        entry.text.find("IRQ_TX_TIMEOUT – таймаут TX") != std::string::npos) {
      found = true;
      capturedMessage = entry.text;
      break;
    }
  }
  assert(found);
  assert(gCallbackMessage == capturedMessage);
  assert(gCallbackUptime == ArduinoStub::gMillis);

  // Проверяем, что loop() выводит полезную нагрузку пакета побайтно
  LogHook::clear();
  RadioSX1262 packetLogRadio;
  packetLogRadio.packetReady_ = true; // активируем обработку в loop()
  packetLogRadio.radio_.testPacketLength = 4;
  packetLogRadio.radio_.testReadBuffer = {0x00, 0xA1, 0xB2, 0xC3};
  packetLogRadio.radio_.testReadBufferSize = 4;
  packetLogRadio.radio_.testSnr = 7.5f;
  packetLogRadio.radio_.testRssi = -112.3f;
  packetLogRadio.loop();

  auto packetLogs = LogHook::getRecent(4);
  bool dumpFound = false;
  for (const auto& entry : packetLogs) {
    if (entry.text.find("RadioSX1262: принят пакет длиной 4 байт") != std::string::npos &&
        entry.text.find("00 A1 B2 C3") != std::string::npos) {
      dumpFound = true;
      break;
    }
  }
  assert(dumpFound);

  std::cout << "Все проверки IRQ-логов и дампа пакетов RadioSX1262 успешно пройдены" << std::endl;
  return 0;
}
