#define SERIAL_MIRROR_DISABLE_REMAP
#include "serial_mirror.h"

#if defined(ARDUINO)

namespace {
  // Проверка, содержит ли строка только печатные ASCII-символы и пробелы/табуляции.
  bool isMostlyPrintable(const String& s) {
    for (size_t i = 0; i < s.length(); ++i) {
      unsigned char c = static_cast<unsigned char>(s.charAt(i));
      if (c == '\t') continue;
      if (c >= 0x80 || c < 0x20 || c == 0x7F) {
        return false;
      }
    }
    return true;
  }

  // Преобразование двоичной строки в шестнадцатеричное представление для журнала
  String toHexLine(const String& s) {
    static const char kHex[] = "0123456789ABCDEF";
    String out = F("[BIN] ");
    out.reserve(out.length() + s.length() * 3);
    for (size_t i = 0; i < s.length(); ++i) {
      unsigned char c = static_cast<unsigned char>(s.charAt(i));
      out += kHex[c >> 4];
      out += kHex[c & 0x0F];
      if (i + 1 < s.length()) out += ' ';
    }
    return out;
  }
}

SerialMirror::SerialMirror(HardwareSerial& serial) : serial_(&serial), buffer_() {}

void SerialMirror::begin(unsigned long baud) {
  if (serial_) serial_->begin(baud);
}

void SerialMirror::begin(unsigned long baud, uint32_t config) {
  if (serial_) serial_->begin(baud, config);
}

void SerialMirror::end() {
  flushBufferToLog();
  if (serial_) serial_->end();
}

size_t SerialMirror::write(uint8_t ch) {
  if (!serial_) return 0;
  mirrorChar(ch);
  return serial_->write(ch);
}

size_t SerialMirror::write(const uint8_t* buffer, size_t size) {
  if (!serial_) return 0;
  for (size_t i = 0; i < size; ++i) {
    mirrorChar(buffer[i]);
  }
  return serial_->write(buffer, size);
}

int SerialMirror::availableForWrite() {
  return serial_ ? static_cast<int>(serial_->availableForWrite()) : 0;
}

int SerialMirror::available() {
  return serial_ ? serial_->available() : 0;
}

int SerialMirror::read() {
  return serial_ ? serial_->read() : -1;
}

int SerialMirror::peek() {
  return serial_ ? serial_->peek() : -1;
}

void SerialMirror::flush() {
  flushBufferToLog();
  if (serial_) serial_->flush();
}

String SerialMirror::readStringUntil(char terminator) {
  return serial_ ? serial_->readStringUntil(terminator) : String();
}

HardwareSerial& SerialMirror::base() {
  return *serial_;
}

SerialMirror::operator bool() const {
  if (!serial_) {
    return false;  // Нет базового Serial — считаем интерфейс неинициализированным.
  }
  return static_cast<bool>(*serial_);
}

void SerialMirror::mirrorChar(uint8_t ch) {
  if (ch == '\r') {
    return; // пропускаем возврат каретки
  }
  if (ch == '\n') {
    flushBufferToLog();
    return;
  }
  buffer_ += static_cast<char>(ch);
  if (buffer_.length() > 256) {
    flushBufferToLog();
  }
}

void SerialMirror::flushBufferToLog() {
  if (buffer_.length() == 0) {
    return;
  }
  String line = buffer_;
  buffer_.remove(0);
  if (line.length() == 0) {
    return;
  }
  bool printable = isMostlyPrintable(line);
  bool has_utf8 = false;
  for (size_t i = 0; i < line.length() && !has_utf8; ++i) {
    if (static_cast<unsigned char>(line.charAt(i)) >= 0x80) {
      has_utf8 = true;
    }
  }
  if (printable) {
    LogHook::append(line);
    if (has_utf8) {
      // Добавляем диагностическую запись с HEX-представлением для UTF-8, чтобы облегчить анализ.
      LogHook::append(toHexLine(line));
    }
    return;
  }
  if (has_utf8) {
    // Оставляем читаемую строку и дополняем её шестнадцатеричным дампом.
    LogHook::append(line);
  }
  LogHook::append(toHexLine(line));
}

SerialMirror SerialDebug(Serial);

// Возвращаем действие переопределения Serial -> SerialDebug для остальных файлов.
#undef SERIAL_MIRROR_DISABLE_REMAP
#ifdef Serial
#undef Serial
#endif
#define Serial SerialDebug

#endif  // defined(ARDUINO)

