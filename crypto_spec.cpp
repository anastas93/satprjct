#include "crypto_spec.h"
#include <string.h>
#include <stdlib.h>

namespace {
  // Локальная функция для расчёта CRC-16 (CRC-CCITT)
  static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
      crc ^= (uint16_t)data[i] << 8;
      for (int j = 0; j < 8; ++j) {
        if (crc & 0x8000) {
          crc = (crc << 1) ^ 0x1021;
        } else {
          crc <<= 1;
        }
      }
    }
    return crc;
  }

  // Строка по умолчанию для корневого ключа (32 hex-символа)
  static const char DEFAULT_ROOT_KEY_HEX[] = "73F2BC910D4A52E68C473B195DA86124";
}

namespace crypto_spec {
  // Корневой ключ для HMAC, заполняется из hex-строки
  uint8_t KEYDH_ROOT_KEY[16] = {0};

  uint8_t CURRENT_KEY[16] = {0};
  uint16_t CURRENT_KEY_CRC = 0;

  bool setRootKeyHex(const char* hex) {
    if (!hex || strlen(hex) != 32) return false;
    for (int i = 0; i < 16; ++i) {
      char buf[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
      char* end = nullptr;
      long val = strtol(buf, &end, 16);
      if (*end != '\0') return false;
      KEYDH_ROOT_KEY[i] = static_cast<uint8_t>(val);
    }
    return true;
  }

  void setCurrentKey(const uint8_t* key) {
    if (!key) return;
    memcpy(CURRENT_KEY, key, 16);
    CURRENT_KEY_CRC = crc16(CURRENT_KEY, 16);
  }

  // Автоматическая загрузка корневого ключа при старте
  static const bool ROOT_KEY_LOADED = setRootKeyHex(DEFAULT_ROOT_KEY_HEX);
}
