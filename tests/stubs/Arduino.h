#pragma once
// Заглушка Arduino API для хостовых модульных тестов без аппаратуры
#include <cstdint>

namespace ArduinoStub {
// Значения виртуального времени в милли- и микросекундах
inline uint32_t gMillis = 0;
inline uint32_t gMicros = 0;
}

inline uint32_t millis() { return ArduinoStub::gMillis; }
inline uint32_t micros() { return ArduinoStub::gMicros; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline void attachInterrupt(int, void (*)(void), int) {}
inline void detachInterrupt(int) {}
inline void noInterrupts() {}
inline void interrupts() {}
