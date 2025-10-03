#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "rx_serial_dump.h"
#include "../src/libs/log_hook/log_hook.h"

// Заглушка Serial с ограниченным буфером для проверки сценария усечения
class LimitedSerial {
public:
  explicit LimitedSerial(size_t capacity) : capacity_(capacity) {}

  int availableForWrite() {
    size_t used = buffer_.size();
    if (used >= capacity_) {
      return 0;
    }
    return static_cast<int>(capacity_ - used);
  }

  void print(const char* text) {
    write(reinterpret_cast<const uint8_t*>(text), std::strlen(text));
  }

  void println(const char* text) {
    print(text);
    write('\n');
  }

  size_t write(const uint8_t* data, size_t len) {
    size_t writable = static_cast<size_t>(availableForWrite());
    size_t toCopy = std::min(len, writable);
    buffer_.append(reinterpret_cast<const char*>(data), toCopy);
    return toCopy;
  }

  size_t write(uint8_t byte) {
    if (availableForWrite() <= 0) {
      return 0;
    }
    buffer_.push_back(static_cast<char>(byte));
    return 1;
  }

  const std::string& output() const { return buffer_; }

  void reset() { buffer_.clear(); }

private:
  size_t capacity_;
  std::string buffer_;
};

int main() {
  {
    LimitedSerial serial(32);
    LogHook::clear();
    const uint8_t payload[] = {'H','e','l','l','o'};
    bool complete = dumpRxToSerialWithPrefix(serial, payload, sizeof(payload));
    assert(complete);
    auto logs = LogHook::getRecent(5);
    assert(logs.empty());
    assert(serial.output().find("RX: Hello\n") == 0);
  }

  {
    LimitedSerial serial(8); // места хватит только на префикс и часть данных
    LogHook::clear();
    const uint8_t payload[] = {'D','a','t','a','X'};
    bool complete = dumpRxToSerialWithPrefix(serial, payload, sizeof(payload));
    assert(!complete); // дамп должен завершиться усечением
    auto logs = LogHook::getRecent(1);
    assert(!logs.empty());
    assert(logs.back().text.find("RX dump truncated") != std::string::npos);
    assert(serial.output().find("RX: Data") == 0);
  }

  std::cout << "OK" << std::endl;
  return 0;
}

