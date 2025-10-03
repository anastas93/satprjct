#ifndef DEFAULT_SETTINGS_H
#define DEFAULT_SETTINGS_H // защита от повторного включения

#include <cstdint>
#include <array>
#include "channel_bank.h" // Банк каналов
#include "libs/log_hook/log_hook.h" // буферизованный хук логирования
#ifdef ARDUINO
#include "libs/serial_mirror/serial_mirror.h" // зеркалирование Serial в веб-журнал
#endif
#include <string>
#include <cstdarg>  // для работы с переменным числом аргументов
#include <cstdio>   // для vsnprintf
#include <sstream>  // для формирования строк логов
#ifndef ARDUINO
#  include <iostream>
#else
#  include <Arduino.h>
#endif

// Значения параметров радиомодуля по умолчанию
namespace DefaultSettings {
  constexpr ChannelBank BANK = ChannelBank::TEST; // Банк каналов
  constexpr uint8_t CHANNEL = 0;                   // Номер канала
  constexpr uint8_t POWER_PRESET = 0;             // Индекс мощности
  constexpr uint8_t BW_PRESET = 2;                // Индекс полосы (15,63 кГц)
  constexpr uint8_t SF_PRESET = 2;                // Индекс фактора расширения
  constexpr uint8_t CR_PRESET = 0;                // Индекс коэффициента кодирования
  constexpr bool RX_BOOSTED_GAIN = true;         // Режим повышенного усиления приёмника
  constexpr size_t GATHER_BLOCK_SIZE = 110;       // Размер блока для PacketGatherer
  constexpr uint32_t SEND_PAUSE_MS = 370;          // Ожидание между отправками и приёмом (мс)
  constexpr uint32_t ACK_TIMEOUT_MS = 320;         // Тайм-аут ожидания ACK перед повтором (мс)
  constexpr uint32_t PING_WAIT_MS = 500;           // Ожидание ответа на пинг (мс)
  constexpr size_t PING_PACKET_SIZE = 5;           // Размер пинг-пакета (байты)
  constexpr size_t SERIAL_BUFFER_LIMIT = 500UL * 1024UL; // Максимальный размер буфера приёма по Serial (байты)
  constexpr size_t TX_QUEUE_CAPACITY = 160;        // Ёмкость очередей TxModule (до четырёх сообщений по 5000 байт)
  constexpr bool USE_RS = false;                   // использовать кодирование RS(255,223)
  constexpr bool USE_ACK = false;                  // использовать подтверждения ACK
  constexpr uint8_t ACK_RETRY_LIMIT = 3;           // Количество повторных отправок при ожидании ACK
  constexpr uint32_t ACK_RESPONSE_DELAY_MS = 20;   // Задержка перед отправкой ACK после приёма (мс)
  constexpr bool USE_ENCRYPTION = true;            // Использовать шифрование AES-CCM
  constexpr const char* WIFI_SSID = "sat_ap";      // SSID точки доступа
  constexpr const char* WIFI_PASS = "12345678";    // пароль точки доступа
  constexpr bool DEBUG = true;                    // Флаг отладочного вывода
  constexpr bool SERIAL_FLUSH_AFTER_LOG = false;  // Принудительный Serial.flush() после каждой строки
  constexpr bool RX_SERIAL_DUMP_ENABLED = true;   // Включение текстового дампа входящих пакетов в Serial при наличии хоста
  // Уровни журналирования для фильтрации сообщений
  enum class LogLevel : uint8_t { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3 };
  constexpr LogLevel LOG_LEVEL = LogLevel::DEBUG;   // Текущий уровень вывода
  constexpr uint32_t LOGGER_RATE_LINES_PER_SEC = 80; // Ограничение скорости вывода строк
  constexpr uint32_t LOGGER_RATE_BURST = 40;         // Максимальный размер «пакета» до притормаживания
  constexpr uint16_t PREAMBLE_LENGTH = 16;          // Длина преамбулы LoRa (символы)
  // Ключ шифрования по умолчанию (16 байт)
  constexpr std::array<uint8_t, 16> DEFAULT_KEY{
      0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x0E, 0x0F};
}

#if !defined(LOG_MSG)
namespace Logger {
  bool enqueue(DefaultSettings::LogLevel level, const std::string& line);
  bool enqueueFromISR(DefaultSettings::LogLevel level, const std::string& line);
  bool enqueuef(DefaultSettings::LogLevel level, const char* fmt, ...);
  bool enqueuefFromISR(DefaultSettings::LogLevel level, const char* fmt, ...);
}

namespace LogDetail {
  inline bool logMsg(DefaultSettings::LogLevel level, const char* msg) {
    return Logger::enqueue(level, std::string(msg));
  }

  inline bool logMsgFromISR(DefaultSettings::LogLevel level, const char* msg) {
    return Logger::enqueueFromISR(level, std::string(msg));
  }

  template <typename T>
  inline bool logMsgVal(DefaultSettings::LogLevel level, const char* prefix, const T& val) {
    std::ostringstream oss;
    oss << prefix << val;
    return Logger::enqueue(level, oss.str());
  }

  template <typename T>
  inline bool logMsgValFromISR(DefaultSettings::LogLevel level, const char* prefix, const T& val) {
    std::ostringstream oss;
    oss << prefix << val;
    return Logger::enqueueFromISR(level, oss.str());
  }
} // namespace LogDetail

#  define LOG_MSG(level, msg) LogDetail::logMsg(level, msg)
#  define LOG_MSG_FROM_ISR(level, msg) LogDetail::logMsgFromISR(level, msg)
#  define LOG_MSG_VAL(level, prefix, val) LogDetail::logMsgVal(level, prefix, val)
#  define LOG_MSG_VAL_FROM_ISR(level, prefix, val) LogDetail::logMsgValFromISR(level, prefix, val)
#  define LOG_MSG_FMT(level, ...) Logger::enqueuef(level, __VA_ARGS__)
#  define LOG_MSG_FMT_FROM_ISR(level, ...) Logger::enqueuefFromISR(level, __VA_ARGS__)
#  define LOG_ERROR(...) LOG_MSG_FMT(DefaultSettings::LogLevel::ERROR, __VA_ARGS__)
#  define LOG_WARN(...)  LOG_MSG_FMT(DefaultSettings::LogLevel::WARN,  __VA_ARGS__)
#  define LOG_INFO(...)  LOG_MSG_FMT(DefaultSettings::LogLevel::INFO,  __VA_ARGS__)
#  define DEBUG_LOG(...) LOG_MSG_FMT(DefaultSettings::LogLevel::DEBUG, __VA_ARGS__)
#  define LOG_ERROR_FROM_ISR(...) LOG_MSG_FMT_FROM_ISR(DefaultSettings::LogLevel::ERROR, __VA_ARGS__)
#  define LOG_WARN_FROM_ISR(...)  LOG_MSG_FMT_FROM_ISR(DefaultSettings::LogLevel::WARN,  __VA_ARGS__)
#  define LOG_INFO_FROM_ISR(...)  LOG_MSG_FMT_FROM_ISR(DefaultSettings::LogLevel::INFO,  __VA_ARGS__)
#  define DEBUG_LOG_FROM_ISR(...) LOG_MSG_FMT_FROM_ISR(DefaultSettings::LogLevel::DEBUG, __VA_ARGS__)
#  define LOG_ERROR_VAL(prefix, val) LOG_MSG_VAL(DefaultSettings::LogLevel::ERROR, prefix, val)
#  define LOG_WARN_VAL(prefix, val)  LOG_MSG_VAL(DefaultSettings::LogLevel::WARN,  prefix, val)
#  define LOG_INFO_VAL(prefix, val)  LOG_MSG_VAL(DefaultSettings::LogLevel::INFO,  prefix, val)
#  define DEBUG_LOG_VAL(prefix, val) LOG_MSG_VAL(DefaultSettings::LogLevel::DEBUG, prefix, val)
#  define LOG_ERROR_VAL_FROM_ISR(prefix, val) LOG_MSG_VAL_FROM_ISR(DefaultSettings::LogLevel::ERROR, prefix, val)
#  define LOG_WARN_VAL_FROM_ISR(prefix, val)  LOG_MSG_VAL_FROM_ISR(DefaultSettings::LogLevel::WARN,  prefix, val)
#  define LOG_INFO_VAL_FROM_ISR(prefix, val)  LOG_MSG_VAL_FROM_ISR(DefaultSettings::LogLevel::INFO,  prefix, val)
#  define DEBUG_LOG_VAL_FROM_ISR(prefix, val) LOG_MSG_VAL_FROM_ISR(DefaultSettings::LogLevel::DEBUG, prefix, val)
#endif // !defined(LOG_MSG)

#endif // DEFAULT_SETTINGS_H

