#pragma once
#ifndef DEFAULT_SETTINGS_H
#define DEFAULT_SETTINGS_H // защита от повторного включения

#include <cstdint>
#include <array>
#include "channel_bank.h" // Банк каналов
#include <string>
#ifndef ARDUINO
#  include <sstream>
#  include <iostream>
#else
#  include <Arduino.h>
#endif

// Значения параметров радиомодуля по умолчанию
namespace DefaultSettings {
  constexpr ChannelBank BANK = ChannelBank::EAST; // Банк каналов
  constexpr uint8_t CHANNEL = 0;                   // Номер канала
  constexpr uint8_t POWER_PRESET = 0;             // Индекс мощности
  constexpr uint8_t BW_PRESET = 3;                // Индекс полосы
  constexpr uint8_t SF_PRESET = 2;                // Индекс фактора расширения
  constexpr uint8_t CR_PRESET = 0;                // Индекс коэффициента кодирования
  constexpr size_t GATHER_BLOCK_SIZE = 110;       // Размер блока для PacketGatherer
  constexpr uint32_t SEND_PAUSE_MS = 80;           // Ожидание между отправками и приёмом (мс)
  constexpr uint32_t PING_WAIT_MS = 500;           // Ожидание ответа на пинг (мс)
  constexpr size_t PING_PACKET_SIZE = 5;           // Размер пинг-пакета (байты)
  constexpr size_t SERIAL_BUFFER_LIMIT = 500UL * 1024UL; // Максимальный размер буфера приёма по Serial (байты)
  constexpr size_t TX_QUEUE_CAPACITY = 160;        // Ёмкость очередей TxModule (до четырёх сообщений по 5000 байт)
  constexpr bool USE_RS = false;                   // использовать кодирование RS(255,223)
  constexpr bool USE_ACK = false;                  // использовать подтверждения ACK
  constexpr const char* WIFI_SSID = "sat_ap";      // SSID точки доступа
  constexpr const char* WIFI_PASS = "12345678";    // пароль точки доступа
  constexpr bool DEBUG = true;                    // Флаг отладочного вывода
  // Уровни журналирования для фильтрации сообщений
  enum class LogLevel : uint8_t { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3 };
  constexpr LogLevel LOG_LEVEL = LogLevel::DEBUG;   // Текущий уровень вывода
  // Ключ шифрования по умолчанию (16 байт)
  constexpr std::array<uint8_t, 16> DEFAULT_KEY{
      0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F};
}

#if !defined(LOG_MSG)
// Вспомогательные функции фильтрации повторяющихся сообщений
namespace LogDetail {
#ifdef ARDUINO
  // Проверка необходимости вывода сообщения (Arduino)
  inline bool shouldPrint(DefaultSettings::LogLevel level, const String& msg) {
    static String last;                                   // последнее выведенное сообщение
    if (!DefaultSettings::DEBUG || level > DefaultSettings::LOG_LEVEL) return false;
    if (last == msg) return false;                        // пропускаем дубль
    last = msg;
    return true;
  }

  // Вывод строки с фильтрацией дублей
  inline void logMsg(DefaultSettings::LogLevel level, const char* msg) {
    if (!shouldPrint(level, String(msg))) return;
    Serial.println(msg);
    Serial.flush();
  }

  // Вывод строки с значением и фильтрацией дублей
  template <typename T>
  inline void logMsgVal(DefaultSettings::LogLevel level, const char* prefix, const T& val) {
    String full = String(prefix) + String(val);
    if (!shouldPrint(level, full)) return;
    Serial.print(prefix); Serial.println(val);
    Serial.flush();
  }
#else
  // Проверка необходимости вывода сообщения (стандартный вывод)
  inline bool shouldPrint(DefaultSettings::LogLevel level, const std::string& msg) {
    static std::string last;                              // последнее выведенное сообщение
    if (!DefaultSettings::DEBUG || level > DefaultSettings::LOG_LEVEL) return false;
    if (last == msg) return false;                        // пропускаем дубль
    last = msg;
    return true;
  }

  // Вывод строки в стандартный поток
  inline void logMsg(DefaultSettings::LogLevel level, const char* msg) {
    std::string s(msg);
    if (!shouldPrint(level, s)) return;
    std::cout << msg << std::endl;
  }

  // Вывод строки с значением в стандартный поток
  template <typename T>
  inline void logMsgVal(DefaultSettings::LogLevel level, const char* prefix, const T& val) {
    std::ostringstream oss;
    oss << prefix << val;
    std::string s = oss.str();
    if (!shouldPrint(level, s)) return;
    std::cout << s << std::endl;
  }
#endif
} // namespace LogDetail

// Макросы верхнего уровня для логирования
#  define LOG_MSG(level, msg) LogDetail::logMsg(level, msg)
#  define LOG_MSG_VAL(level, prefix, val) LogDetail::logMsgVal(level, prefix, val)
#  define LOG_ERROR(msg) LOG_MSG(DefaultSettings::LogLevel::ERROR, msg)
#  define LOG_WARN(msg)  LOG_MSG(DefaultSettings::LogLevel::WARN,  msg)
#  define LOG_INFO(msg)  LOG_MSG(DefaultSettings::LogLevel::INFO,  msg)
#  define DEBUG_LOG(msg) LOG_MSG(DefaultSettings::LogLevel::DEBUG, msg)
#  define LOG_ERROR_VAL(prefix, val) LOG_MSG_VAL(DefaultSettings::LogLevel::ERROR, prefix, val)
#  define LOG_WARN_VAL(prefix, val)  LOG_MSG_VAL(DefaultSettings::LogLevel::WARN,  prefix, val)
#  define LOG_INFO_VAL(prefix, val)  LOG_MSG_VAL(DefaultSettings::LogLevel::INFO,  prefix, val)
#  define DEBUG_LOG_VAL(prefix, val) LOG_MSG_VAL(DefaultSettings::LogLevel::DEBUG, prefix, val)
#endif // !defined(LOG_MSG)

#endif // DEFAULT_SETTINGS_H

