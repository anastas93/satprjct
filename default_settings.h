#pragma once
#include <cstdint>
#include "channel_bank.h" // Банк каналов

// Значения параметров радиомодуля по умолчанию
namespace DefaultSettings {
  constexpr ChannelBank BANK = ChannelBank::EAST; // Банк каналов
  constexpr uint8_t CHANNEL = 0;                   // Номер канала
  constexpr uint8_t POWER_PRESET = 0;             // Индекс мощности
  constexpr uint8_t BW_PRESET = 3;                // Индекс полосы
  constexpr uint8_t SF_PRESET = 2;                // Индекс фактора расширения
  constexpr uint8_t CR_PRESET = 0;                // Индекс коэффициента кодирования
  constexpr size_t GATHER_BLOCK_SIZE = 223;       // Размер блока для PacketGatherer
  constexpr uint32_t SEND_PAUSE_MS = 0;           // Ожидание между отправками и приёмом (мс)
  constexpr bool DEBUG = true;                    // Флаг отладочного вывода
  // Уровни журналирования для фильтрации сообщений
  enum class LogLevel : uint8_t { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3 };
  constexpr LogLevel LOG_LEVEL = LogLevel::INFO;   // Текущий уровень вывода
}

#if !defined(LOG_MSG)
#  if defined(ARDUINO)
#    include <Arduino.h>
// Вывод сообщения с принудительным сбросом буфера Serial
#    define LOG_MSG(level, msg) do { \
      if (DefaultSettings::DEBUG && level <= DefaultSettings::LOG_LEVEL) { \
        Serial.println(msg); \
        Serial.flush(); \
      } \
    } while (0)
// Вывод значения с префиксом и сбросом буфера Serial
#    define LOG_MSG_VAL(level, prefix, val) do { \
      if (DefaultSettings::DEBUG && level <= DefaultSettings::LOG_LEVEL) { \
        Serial.print(prefix); Serial.println(val); \
        Serial.flush(); \
      } \
    } while (0)
#  else
#    include <iostream>
// Вывод сообщения в стандартный поток с окончанием строки
#    define LOG_MSG(level, msg) do { \
      if (DefaultSettings::DEBUG && level <= DefaultSettings::LOG_LEVEL) std::cout << msg << std::endl; \
    } while (0)
// Вывод значения с префиксом в стандартный поток
#    define LOG_MSG_VAL(level, prefix, val) do { \
      if (DefaultSettings::DEBUG && level <= DefaultSettings::LOG_LEVEL) \
        std::cout << prefix << val << std::endl; \
    } while (0)
#  endif
#  define LOG_ERROR(msg) LOG_MSG(DefaultSettings::LogLevel::ERROR, msg)
#  define LOG_WARN(msg)  LOG_MSG(DefaultSettings::LogLevel::WARN,  msg)
#  define LOG_INFO(msg)  LOG_MSG(DefaultSettings::LogLevel::INFO,  msg)
#  define DEBUG_LOG(msg) LOG_MSG(DefaultSettings::LogLevel::DEBUG, msg)
#  define LOG_ERROR_VAL(prefix, val) LOG_MSG_VAL(DefaultSettings::LogLevel::ERROR, prefix, val)
#  define LOG_WARN_VAL(prefix, val)  LOG_MSG_VAL(DefaultSettings::LogLevel::WARN,  prefix, val)
#  define LOG_INFO_VAL(prefix, val)  LOG_MSG_VAL(DefaultSettings::LogLevel::INFO,  prefix, val)
#  define DEBUG_LOG_VAL(prefix, val) LOG_MSG_VAL(DefaultSettings::LogLevel::DEBUG, prefix, val)
#endif

