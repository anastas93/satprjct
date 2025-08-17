#pragma once
// Простой заглушечный класс Preferences для компиляции на хосте
class Preferences {
public:
  void begin(const char*, bool = false) {}
  void end() {}
  uint32_t getUInt(const char*, uint32_t def = 0) { return def; }
  void putUInt(const char*, uint32_t) {}
  // Дополнительные методы, используемые в проекте
  void putBytes(const char*, const void*, size_t) {}
  void putUChar(const char*, uint8_t) {}
  void putBool(const char*, bool) {}
};
