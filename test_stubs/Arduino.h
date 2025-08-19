// Minimal Arduino stub header for host compilation
// This header provides a subset of the Arduino API required to build the
// ESP32 LoRa Pipeline sources on a desktop system.  It implements timing
// functions using the C++ chrono library, provides a simple Print class,
// and defines a lightweight String class that implements only the subset
// of operations used by the pipeline and web interface code.

#ifndef ARDUINO_H
#define ARDUINO_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

// -----------------------------------------------------------------------------
// Timing functions

// Return the number of milliseconds since the program started.
static inline unsigned long millis() {
  using namespace std::chrono;
  static const auto start = high_resolution_clock::now();
  const auto now = high_resolution_clock::now();
  return (unsigned long)duration_cast<milliseconds>(now - start).count();
}

// Return the number of microseconds since the program started.
static inline unsigned long micros() {
  using namespace std::chrono;
  static const auto start = high_resolution_clock::now();
  const auto now = high_resolution_clock::now();
  return (unsigned long)duration_cast<microseconds>(now - start).count();
}

// Sleep for the specified number of milliseconds.
static inline void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Arduino flash string macro; on host simply returns the input literal.
static inline const char* F(const char* s) { return s; }

// -----------------------------------------------------------------------------
// Lightweight String class
// Implements a subset of the Arduino String API used by the pipeline code.

class String {
public:
  String() : data("") {}
  String(const char* s) : data(s ? s : "") {}
  String(const std::string& s) : data(s) {}
  String(const String& other) : data(other.data) {}
  String(char c) : data(1, c) {}
  // Конструкторы из числовых типов
  String(int v) : data(std::to_string(v)) {}
  String(unsigned v) : data(std::to_string(v)) {}
  String(long v) : data(std::to_string(v)) {}
  String(unsigned long v) : data(std::to_string(v)) {}
  // Конструктор с форматированием для плавающей точки
  String(float v, unsigned char decs = 2) { std::ostringstream ss; ss<<std::fixed<<std::setprecision(decs)<<v; data=ss.str(); }
  String(double v, unsigned char decs = 2) { std::ostringstream ss; ss<<std::fixed<<std::setprecision(decs)<<v; data=ss.str(); }
  String& operator=(const String& other) { data = other.data; return *this; }
  // Concatenation operators
  String& operator+=(const char* s) { data += (s ? s : ""); return *this; }
  String& operator+=(char c) { data.push_back(c); return *this; }
  String& operator+=(const String& s) { data += s.data; return *this; }
  friend String operator+(const String& lhs, const String& rhs) { return String(lhs.data + rhs.data); }
  friend String operator+(const String& lhs, const char* rhs) { return String(lhs.data + (rhs ? std::string(rhs) : std::string(""))); }
  friend String operator+(const char* lhs, const String& rhs) { return String((lhs ? std::string(lhs) : std::string("")) + rhs.data); }
  bool operator==(const String& other) const { return data == other.data; }
  bool operator==(const char* s) const { return data == (s ? std::string(s) : std::string("")); }
  friend bool operator==(const char* s, const String& str) { return str == s; }
  bool operator!=(const String& other) const { return !(*this == other); }
  // Element access
  char operator[](size_t i) const { return data[i]; }
  // Size
  size_t length() const { return data.size(); }
  // Find first occurrence of character; returns index or -1 if not found.
  int indexOf(char c) const {
    auto pos = data.find(c);
    return pos == std::string::npos ? -1 : (int)pos;
  }
  // Find first occurrence of substring; returns index or -1 if not found.
  int indexOf(const String& s) const {
    auto pos = data.find(s.data);
    return pos == std::string::npos ? -1 : (int)pos;
  }
  // Substring starting at 'from' with optional end index.  If end is omitted
  // or exceeds the string length, substring to end is returned.
  String substring(int from, int to) const {
    if (from < 0) from = 0;
    if (to < from) to = from;
    size_t len = data.size();
    size_t start = (size_t)from;
    size_t end = (size_t)to;
    if (start > len) start = len;
    if (end > len) end = len;
    return String(data.substr(start, end - start));
  }
  String substring(int from) const {
    if (from < 0) from = 0;
    size_t start = (size_t)from;
    if (start > data.size()) start = data.size();
    return String(data.substr(start));
  }
  // Trim leading and trailing whitespace in place.
  void trim() {
    size_t start = 0;
    while (start < data.size() && std::isspace((unsigned char)data[start])) start++;
    size_t end = data.size();
    while (end > start && std::isspace((unsigned char)data[end - 1])) end--;
    data = data.substr(start, end - start);
  }
  // Convert to int.  Returns 0 on conversion failure.
  int toInt() const {
    try { return std::stoi(data); } catch (...) { return 0; }
  }
  // Convert to float.  Returns 0.0f on conversion failure.
  float toFloat() const {
    try { return std::stof(data); } catch (...) { return 0.0f; }
  }
  // Convert to uppercase in place.
  void toUpperCase() {
    std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c){ return (char)std::toupper(c); });
  }
  // Convert to lowercase in place.
  void toLowerCase() {
    std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c){ return (char)std::tolower(c); });
  }
  // Case-insensitive comparison.
  bool equalsIgnoreCase(const String& other) const {
    if (data.size() != other.data.size()) return false;
    for (size_t i = 0; i < data.size(); ++i) {
      if (std::tolower((unsigned char)data[i]) != std::tolower((unsigned char)other.data[i])) return false;
    }
    return true;
  }
  // c_str() for interop with C APIs.
  const char* c_str() const { return data.c_str(); }
  // Expose underlying std::string for advanced uses.
  std::string str() const { return data; }
private:
  std::string data;
};

// -----------------------------------------------------------------------------
// Print and Serial

// Lightweight Print interface suitable for streaming text to stdout.  Only
// implements the subset used by the pipeline code.
class Print {
public:
  virtual ~Print() = default;
  // Write a single byte; default implementation does nothing and returns
  // zero indicating nothing was written.
  virtual size_t write(uint8_t) { return 0; }
  // Print a null‑terminated string.
  size_t print(const char* s) { return ::fwrite(s, 1, std::strlen(s), stdout); }
  // Print a String object.
  size_t print(const String& s) { return ::fwrite(s.c_str(), 1, s.length(), stdout); }
  // Print a null‑terminated string followed by a newline.
  size_t println(const char* s) { size_t n = print(s); n += ::fputc('\n', stdout) >= 0 ? 1 : 0; return n; }
  size_t println(const String& s) { size_t n = print(s); n += ::fputc('\n', stdout) >= 0 ? 1 : 0; return n; }
  // Print just a newline.
  size_t println() { return ::fputc('\n', stdout) >= 0 ? 1 : 0; }
  // Variadic printf forwarding to stdio.
  template<typename... Args>
  int printf(const char* fmt, Args... args) { return ::printf(fmt, args...); }
};

// Stream inherits Print; nothing additional is required for these tests.
class Stream : public Print {};

// Minimal HardwareSerial stub.  Only the print/println methods are used.
class HardwareSerial : public Stream {
public:
  void begin(unsigned long) {}
  using Stream::print;
  using Stream::println;
};

// Global Serial instance used by application code.  Writes to stdout.
extern HardwareSerial Serial;

#endif // ARDUINO_H
