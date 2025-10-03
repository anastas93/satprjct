#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H // защита от повторного включения

#include <array>
#include <cstdint>
#include <string>

#include "channel_bank.h"          // перечисление банков каналов

namespace ConfigLoader {

// Настройки Wi-Fi точки доступа
struct WifiConfig {
  std::string ssid;    // базовый SSID точки доступа
  std::string password; // пароль точки доступа
  std::string ip;       // статический IP адрес точки доступа
  std::string gateway;  // адрес шлюза по умолчанию
  std::string subnet;   // маска подсети
};

// Радионастройки, применяемые при запуске
struct RadioConfig {
  ChannelBank bank;        // выбранный банк каналов
  uint8_t channel;         // номер канала
  uint8_t powerPreset;     // предустановленный уровень мощности
  uint8_t bwPreset;        // индекс полосы
  uint8_t sfPreset;        // индекс фактора расширения
  uint8_t crPreset;        // индекс коэффициента кодирования
  bool rxBoostedGain;      // признак усиленного приёма
  bool useAck;             // использовать подтверждения ACK
  uint8_t ackRetryLimit;   // лимит повторных отправок ACK
  uint32_t ackResponseDelayMs; // задержка перед ответом ACK
  uint32_t sendPauseMs;    // пауза между передачей и приёмом
  uint32_t ackTimeoutMs;   // тайм-аут ожидания ACK
  bool useEncryption;      // включить шифрование
  bool useRs;              // включить код Рида-Соломона
};

// Ключи шифрования по умолчанию
struct KeysConfig {
  std::array<uint8_t, 16> defaultKey; // базовый симметричный ключ
};

// Совокупность всех загружаемых настроек
struct Config {
  WifiConfig wifi;   // параметры Wi-Fi точки доступа
  RadioConfig radio; // параметры радиомодуля
  KeysConfig keys;   // ключи шифрования
};

// Возвращает загруженную конфигурацию (с подстановкой значений по умолчанию).
const Config& getConfig();

// Принудительно перечитывает конфигурацию с диска, возвращает актуальное состояние.
const Config& reload();

} // namespace ConfigLoader

#endif // CONFIG_LOADER_H
