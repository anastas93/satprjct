#define SERIAL_MIRROR_DISABLE_REMAP
#include "serial_mirror.h"

#if defined(ARDUINO)

namespace {
  // Проверка строки на печатность с возможностью сообщить о наличии UTF-8 байтов.
  // Если встретились управляющие символы ASCII (кроме табуляции), возвращаем false.
  // Параметр hasUtf8Out сообщает вызывающему коду, присутствуют ли байты >= 0x80.
  bool isMostlyPrintable(const String& s, bool* hasUtf8Out = nullptr) {
    bool hasUtf8 = false;
    for (size_t i = 0; i < s.length(); ++i) {
      unsigned char c = static_cast<unsigned char>(s.charAt(i));
      if (c >= 0x80) {
        hasUtf8 = true;
        continue;
      }
      if (c == '\t') continue;
      if (c < 0x20 || c == 0x7F) {
        if (hasUtf8Out) {
          *hasUtf8Out = hasUtf8;
        }
        return false;
      }
    }
    if (hasUtf8Out) {
      *hasUtf8Out = hasUtf8;
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
  bool hasUtf8 = false;
  bool printable = isMostlyPrintable(line, &hasUtf8);
  if (printable) {
    LogHook::append(line);
    return;
  }
  if (hasUtf8) {
    LogHook::append(line);
    LogHook::append(F("[SerialMirror] UTF-8 строка сохранена как текст (без HEX)"));
    return;
  }
  LogHook::append(toHexLine(line));
  LogHook::append(String(F("[SerialMirror] Бинарная строка преобразована в HEX, длина = ")) +
                  String(static_cast<unsigned long>(line.length())));
}

SerialMirror SerialDebug(Serial);

#endif  // defined(ARDUINO)

