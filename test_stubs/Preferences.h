#pragma once
// Простой заглушечный класс Preferences для компиляции на хосте
class Preferences {
public:
  void begin(const char*, bool = false) {}
  void end() {}
  uint32_t getUInt(const char*, uint32_t def = 0) { return def; }
  void putUInt(const char*, uint32_t) {}
};
