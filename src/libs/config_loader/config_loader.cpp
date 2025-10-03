#include "config_loader.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>

#ifndef ARDUINO
#include <fstream>
#endif

#include "default_settings.h"        // значения по умолчанию и макросы логирования

namespace ConfigLoader {
namespace {

// Путь к основному конфигурационному файлу
constexpr const char* kConfigPath = "config/default.ini";

// Утилита обрезки пробелов с краёв строки
std::string trim(const std::string& value) {
  auto begin = value.begin();
  while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
    ++begin;
  }
  auto end = value.end();
  while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
    --end;
  }
  return std::string(begin, end);
}

// Преобразование строки к нижнему регистру для унификации сравнения
std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

// Преобразование строки к верхнему регистру
std::string toUpper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return value;
}

// Разбор целого беззнакового значения с контролем диапазона
bool parseUint(const std::string& text, unsigned long maxValue, unsigned long& outValue) {
  errno = 0;
  char* end = nullptr;
  unsigned long value = std::strtoul(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || (end && *end != '\0') || value > maxValue) {
    return false;
  }
  outValue = value;
  return true;
}

// Разбор логического значения
bool parseBool(const std::string& text, bool& outValue) {
  const std::string lower = toLower(text);
  if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
    outValue = true;
    return true;
  }
  if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
    outValue = false;
    return true;
  }
  return false;
}

// Преобразование строки с шестнадцатеричным ключом в массив байт
bool parseKey(const std::string& text, std::array<uint8_t, 16>& out) {
  std::string sanitized;
  sanitized.reserve(text.size());
  for (char ch : text) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      sanitized.push_back(ch);
    }
  }
  if (sanitized.size() != out.size() * 2) {
    return false;
  }
  for (size_t i = 0; i < out.size(); ++i) {
    char hi = sanitized[i * 2];
    char lo = sanitized[i * 2 + 1];
    if (!std::isxdigit(static_cast<unsigned char>(hi)) || !std::isxdigit(static_cast<unsigned char>(lo))) {
      return false;
    }
    unsigned long value = std::strtoul(sanitized.substr(i * 2, 2).c_str(), nullptr, 16);
    out[i] = static_cast<uint8_t>(value & 0xFFU);
  }
  return true;
}

// Преобразование строки в перечисление банка каналов
bool parseBank(const std::string& text, ChannelBank& outBank) {
  const std::string upper = toUpper(text);
  if (upper == "EAST") {
    outBank = ChannelBank::EAST;
    return true;
  }
  if (upper == "WEST") {
    outBank = ChannelBank::WEST;
    return true;
  }
  if (upper == "TEST") {
    outBank = ChannelBank::TEST;
    return true;
  }
  if (upper == "ALL") {
    outBank = ChannelBank::ALL;
    return true;
  }
  if (upper == "HOME") {
    outBank = ChannelBank::HOME;
    return true;
  }
  return false;
}

// Формирует конфигурацию с базовыми значениями по умолчанию
Config makeDefaults() {
  Config config;
  config.wifi.ssid = DefaultSettings::WIFI_SSID;
  config.wifi.password = DefaultSettings::WIFI_PASS;
  config.wifi.ip = "192.168.4.1";
  config.wifi.gateway = "192.168.4.1";
  config.wifi.subnet = "255.255.255.0";

  config.radio.bank = DefaultSettings::BANK;
  config.radio.channel = DefaultSettings::CHANNEL;
  config.radio.powerPreset = DefaultSettings::POWER_PRESET;
  config.radio.bwPreset = DefaultSettings::BW_PRESET;
  config.radio.sfPreset = DefaultSettings::SF_PRESET;
  config.radio.crPreset = DefaultSettings::CR_PRESET;
  config.radio.rxBoostedGain = DefaultSettings::RX_BOOSTED_GAIN;
  config.radio.useAck = DefaultSettings::USE_ACK;
  config.radio.ackRetryLimit = DefaultSettings::ACK_RETRY_LIMIT;
  config.radio.ackResponseDelayMs = DefaultSettings::ACK_RESPONSE_DELAY_MS;
  config.radio.sendPauseMs = DefaultSettings::SEND_PAUSE_MS;
  config.radio.ackTimeoutMs = DefaultSettings::ACK_TIMEOUT_MS;
  config.radio.useEncryption = DefaultSettings::USE_ENCRYPTION;
  config.radio.useRs = DefaultSettings::USE_RS;

  config.keys.defaultKey = DefaultSettings::DEFAULT_KEY;
  return config;
}

// Разбор строки внутри конкретной секции
void applySetting(Config& config, const std::string& section, const std::string& key, const std::string& value) {
  if (section == "wifi") {
    if (key == "ssid") {
      config.wifi.ssid = value;
    } else if (key == "password") {
      config.wifi.password = value;
    } else if (key == "ip") {
      config.wifi.ip = value;
    } else if (key == "gateway") {
      config.wifi.gateway = value;
    } else if (key == "subnet") {
      config.wifi.subnet = value;
    } else {
      LOG_WARN("Config: неизвестный параметр %s в секции [wifi]", key.c_str());
    }
    return;
  }
  if (section == "radio") {
    if (key == "bank") {
      ChannelBank bank;
      if (parseBank(value, bank)) {
        config.radio.bank = bank;
      } else {
        LOG_WARN("Config: некорректное значение bank=%s", value.c_str());
      }
    } else if (key == "channel") {
      unsigned long parsed = 0;
      if (parseUint(value, 255UL, parsed)) {
        config.radio.channel = static_cast<uint8_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный channel=%s", value.c_str());
      }
    } else if (key == "powerPreset") {
      unsigned long parsed = 0;
      if (parseUint(value, 255UL, parsed)) {
        config.radio.powerPreset = static_cast<uint8_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный powerPreset=%s", value.c_str());
      }
    } else if (key == "bwPreset") {
      unsigned long parsed = 0;
      if (parseUint(value, 255UL, parsed)) {
        config.radio.bwPreset = static_cast<uint8_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный bwPreset=%s", value.c_str());
      }
    } else if (key == "sfPreset") {
      unsigned long parsed = 0;
      if (parseUint(value, 255UL, parsed)) {
        config.radio.sfPreset = static_cast<uint8_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный sfPreset=%s", value.c_str());
      }
    } else if (key == "crPreset") {
      unsigned long parsed = 0;
      if (parseUint(value, 255UL, parsed)) {
        config.radio.crPreset = static_cast<uint8_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный crPreset=%s", value.c_str());
      }
    } else if (key == "rxBoostedGain") {
      bool parsed = false;
      if (parseBool(value, parsed)) {
        config.radio.rxBoostedGain = parsed;
      } else {
        LOG_WARN("Config: некорректный rxBoostedGain=%s", value.c_str());
      }
    } else if (key == "useAck") {
      bool parsed = false;
      if (parseBool(value, parsed)) {
        config.radio.useAck = parsed;
      } else {
        LOG_WARN("Config: некорректный useAck=%s", value.c_str());
      }
    } else if (key == "ackRetryLimit") {
      unsigned long parsed = 0;
      if (parseUint(value, 255UL, parsed)) {
        config.radio.ackRetryLimit = static_cast<uint8_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный ackRetryLimit=%s", value.c_str());
      }
    } else if (key == "ackResponseDelayMs") {
      unsigned long parsed = 0;
      if (parseUint(value, 100000UL, parsed)) {
        config.radio.ackResponseDelayMs = static_cast<uint32_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный ackResponseDelayMs=%s", value.c_str());
      }
    } else if (key == "sendPauseMs") {
      unsigned long parsed = 0;
      if (parseUint(value, 100000UL, parsed)) {
        config.radio.sendPauseMs = static_cast<uint32_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный sendPauseMs=%s", value.c_str());
      }
    } else if (key == "ackTimeoutMs") {
      unsigned long parsed = 0;
      if (parseUint(value, 100000UL, parsed)) {
        config.radio.ackTimeoutMs = static_cast<uint32_t>(parsed);
      } else {
        LOG_WARN("Config: некорректный ackTimeoutMs=%s", value.c_str());
      }
    } else if (key == "useEncryption") {
      bool parsed = false;
      if (parseBool(value, parsed)) {
        config.radio.useEncryption = parsed;
      } else {
        LOG_WARN("Config: некорректный useEncryption=%s", value.c_str());
      }
    } else if (key == "useRs") {
      bool parsed = false;
      if (parseBool(value, parsed)) {
        config.radio.useRs = parsed;
      } else {
        LOG_WARN("Config: некорректный useRs=%s", value.c_str());
      }
    } else {
      LOG_WARN("Config: неизвестный параметр %s в секции [radio]", key.c_str());
    }
    return;
  }
  if (section == "keys") {
    if (key == "default") {
      std::array<uint8_t, 16> parsed{};
      if (parseKey(value, parsed)) {
        config.keys.defaultKey = parsed;
      } else {
        LOG_WARN("Config: некорректный формат ключа default");
      }
    } else {
      LOG_WARN("Config: неизвестный параметр %s в секции [keys]", key.c_str());
    }
    return;
  }
  LOG_WARN("Config: неизвестная секция [%s]", section.c_str());
}

// Читает конфигурацию из файла и дополняет значения по умолчанию
Config load() {
  Config config = makeDefaults();
#ifndef ARDUINO
  std::ifstream file(kConfigPath);
  if (!file.good()) {
    LOG_WARN("Config: файл %s не найден, используются значения по умолчанию", kConfigPath);
    return config;
  }
  std::string line;
  std::string section;
  while (std::getline(file, line)) {
    std::string trimmed = trim(line);
    if (trimmed.empty()) {
      continue; // пропускаем пустые строки
    }
    if (trimmed.front() == ';' || trimmed.front() == '#') {
      continue; // комментарий в стиле ini
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      section = toLower(trim(trimmed.substr(1, trimmed.size() - 2)));
      continue;
    }
    auto eqPos = trimmed.find('=');
    if (eqPos == std::string::npos) {
      LOG_WARN("Config: строка без разделителя '=' пропущена: %s", trimmed.c_str());
      continue;
    }
    std::string key = toLower(trim(trimmed.substr(0, eqPos)));
    std::string value = trim(trimmed.substr(eqPos + 1));
    if (key.empty()) {
      LOG_WARN("Config: обнаружен пустой ключ в секции [%s]", section.c_str());
      continue;
    }
    applySetting(config, section, key, value);
  }
#else
  LOG_INFO("Config: чтение конфигурации с диска отключено в прошивке, используются стандартные значения");
#endif
  return config;
}

Config gConfig = load();

} // namespace

const Config& getConfig() {
  return gConfig;
}

const Config& reload() {
  gConfig = load();
  return gConfig;
}

} // namespace ConfigLoader
