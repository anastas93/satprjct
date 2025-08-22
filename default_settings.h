#pragma once
#include <cstdint>

enum class ChannelBank { EAST, WEST, TEST }; // Банк каналов

// Значения параметров радиомодуля по умолчанию
namespace DefaultSettings {
  constexpr ChannelBank BANK = ChannelBank::EAST; // Банк каналов
  constexpr uint8_t CHANNEL = 0;                   // Номер канала
  constexpr uint8_t POWER_PRESET = 0;             // Индекс мощности
  constexpr uint8_t BW_PRESET = 3;                // Индекс полосы
  constexpr uint8_t SF_PRESET = 2;                // Индекс фактора расширения
  constexpr uint8_t CR_PRESET = 0;                // Индекс коэффициента кодирования
  constexpr bool DEBUG = true;                    // Флаг отладочного вывода
}

#if !defined(DEBUG_LOG)
#  if defined(ARDUINO)
#    include <Arduino.h>
#    define DEBUG_LOG(msg) do { if (DefaultSettings::DEBUG) Serial.println(msg); } while (0)
#    define DEBUG_LOG_VAL(prefix, val) do { \
      if (DefaultSettings::DEBUG) { Serial.print(prefix); Serial.println(val); } \
    } while (0)
#  else
#    include <iostream>
#    define DEBUG_LOG(msg) do { if (DefaultSettings::DEBUG) std::cout << msg << std::endl; } while (0)
#    define DEBUG_LOG_VAL(prefix, val) do { \
      if (DefaultSettings::DEBUG) std::cout << prefix << val << std::endl; \
    } while (0)
#  endif
#endif

