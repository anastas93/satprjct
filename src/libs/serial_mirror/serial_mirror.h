#pragma once
#ifndef SERIAL_MIRROR_SERIAL_MIRROR_H
#define SERIAL_MIRROR_SERIAL_MIRROR_H

// Модуль зеркалирования вывода Serial в буфер LogHook для веб-интерфейса Debug.
// Все комментарии оставлены на русском языке по требованию пользователя.

#include "libs/log_hook/log_hook.h"

#if defined(ARDUINO)
#include <Arduino.h>

class SerialMirror : public Stream {
 public:
  explicit SerialMirror(HardwareSerial& serial);

  void begin(unsigned long baud);
  void begin(unsigned long baud, uint32_t config);
  void end();

  using Print::print;
  using Print::println;

  size_t write(uint8_t ch) override;
  size_t write(const uint8_t* buffer, size_t size) override;

  int available() override;
  int read() override;
  int peek() override;
  void flush() override;

  String readStringUntil(char terminator);

  HardwareSerial& base();

  // Оператор приведения к bool для совместимости с конструкциями вида `while (!Serial)`.
  explicit operator bool() const;

 private:
  HardwareSerial* serial_;
  String buffer_;

  void mirrorChar(uint8_t ch);
  void flushBufferToLog();
};

extern SerialMirror SerialDebug;

#define SERIAL_MIRROR_ACTIVE 1

#ifndef SERIAL_MIRROR_DISABLE_REMAP
#ifdef Serial
#undef Serial
#endif
#define Serial SerialDebug
#endif

#else  // !defined(ARDUINO)

#define SERIAL_MIRROR_ACTIVE 0

#endif  // defined(ARDUINO)

#endif  // SERIAL_MIRROR_SERIAL_MIRROR_H
