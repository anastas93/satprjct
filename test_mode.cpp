#include "test_mode.h"

#include <cstdio>
#include <string>
#include <vector>

#include "libs/simple_logger/simple_logger.h"
#include "libs/text_converter/text_converter.h"

#ifndef ARDUINO
#include <chrono>
#endif

// Конструктор инициализирует исходные значения счётчиков.
TestMode::TestMode()
  : buffer_(nullptr),
    enabled_(false),
    messageId_(70000),
    sequence_(0) {
#ifndef ARDUINO
  rng_.seed(static_cast<unsigned long>(std::chrono::steady_clock::now().time_since_epoch().count()));
#endif
}

void TestMode::attachBuffer(ReceivedBuffer* buffer) {
  buffer_ = buffer;
}

void TestMode::setEnabled(bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  const char* text = enabled_ ? "TESTMODE on" : "TESTMODE off";
  SimpleLogger::logStatus(text);
#ifdef ARDUINO
  Serial.print("TESTMODE:");
  Serial.println(enabled_ ? "1" : "0");
#endif
}

bool TestMode::sendMessage(const String& payload, uint32_t& outId, String& err) {
  if (!enabled_) {
    err = "тестовый режим выключен";
    return false;
  }
  if (!buffer_) {
    err = "буфер недоступен";
    return false;
  }
  String text = payload;
  text.trim();
  if (!text.length()) {
    err = "пустое сообщение";
    return false;
  }

  // Формируем псевдо-параметры сообщения аналогично received_msg.
  const uint8_t idHi = randomByte();
  const uint8_t idLo = randomByte();
  const uint8_t dest = 255; // широковещательный адрес
  const int rssi = randomRange(-95, -45);
  const int snr = randomRange(5, 15);
  const uint8_t seq = sequence_++;

  char idBuf[5];
  std::snprintf(idBuf, sizeof(idBuf), "%02X%02X", idHi, idLo);

  String line;
  line.reserve(text.length() + 64);
  line += "RX: ";
  line += formatTime(currentMillis());
  line += ' ';
  line += String(rssi);
  line += "dBm/";
  line += String(snr);
  line += "dB ID:0x";
  line += idBuf;
  line += " Adr:";
  line += String(dest);
  line += " №:";
  line += String(seq);
  line += "*  #";
  line += text;
  line += "\r\n";

  std::vector<uint8_t> payloadBytes = utf8ToCp1251(std::string(line.c_str()));
  if (payloadBytes.empty()) {
    err = "ошибка кодировки";
    return false;
  }

  const uint32_t id = nextId();
  if (payloadBytes.size() > 0) {
    buffer_->pushReady(id, payloadBytes.data(), payloadBytes.size());
  }

#ifdef ARDUINO
  Serial.println(line);
#endif
  SimpleLogger::logStatus(std::string("RX ") + std::to_string(id));

  outId = id;
  err = String();
  return true;
}

uint32_t TestMode::nextId() {
  messageId_ += 1;
  if (messageId_ > 99999) messageId_ = 1;
  return messageId_;
}

unsigned long TestMode::currentMillis() const {
#ifdef ARDUINO
  return millis();
#else
  using clock = std::chrono::steady_clock;
  static const auto start = clock::now();
  auto diff = clock::now() - start;
  return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(diff).count());
#endif
}

String TestMode::formatTime(unsigned long ms) const {
  unsigned long total = ms / 1000;
  unsigned long hh = (total / 3600UL) % 24UL;
  unsigned long mm = (total / 60UL) % 60UL;
  unsigned long ss = total % 60UL;
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hh, mm, ss);
  return String(buf);
}

int TestMode::randomRange(int minValue, int maxValue) {
  if (maxValue < minValue) return minValue;
#ifdef ARDUINO
  long span = static_cast<long>(maxValue) - static_cast<long>(minValue) + 1;
  if (span <= 0) span = 1;
  long val = random(span);
  return static_cast<int>(minValue + val);
#else
  std::uniform_int_distribution<int> dist(minValue, maxValue);
  return dist(rng_);
#endif
}

uint8_t TestMode::randomByte() {
  return static_cast<uint8_t>(randomRange(0, 255));
}
