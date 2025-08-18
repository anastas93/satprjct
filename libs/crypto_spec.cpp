// Реализация методов и данных модуля crypto_spec

#include "crypto_spec.h"
#include <string.h>

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

  // Преобразование hex-символа в число 0..15
  static uint8_t hexToNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xFF;
  }
}

namespace crypto_spec {
  // Глобальный корневой ключ, используемый для HMAC в ECDH
  uint8_t KEYDH_ROOT_KEY[16] = {0};

  // Установка корневого ключа из hex-строки
  bool setRootKeyHex(const char* hex) {
    if (!hex) return false;
    for (int i = 0; i < 16; ++i) {
      uint8_t hi = hexToNibble(hex[i * 2]);
      uint8_t lo = hexToNibble(hex[i * 2 + 1]);
      if (hi == 0xFF || lo == 0xFF) return false;
      KEYDH_ROOT_KEY[i] = (hi << 4) | lo;
    }
    return true;
  }

  // Значение корневого ключа по умолчанию (hex-строка)
  static const char DEFAULT_ROOT_KEY[] = "73F2BC910D4A52E68C473B195DA86124";

  // Статический загрузчик, преобразующий строку в массив при старте
  struct RootKeyLoader {
    RootKeyLoader() { setRootKeyHex(DEFAULT_ROOT_KEY); }
  } rootKeyLoader;

  uint8_t CURRENT_KEY[16] = {0};
  uint16_t CURRENT_KEY_CRC = 0;

  void setCurrentKey(const uint8_t* key) {
    if (!key) return;
    memcpy(CURRENT_KEY, key, 16);
    CURRENT_KEY_CRC = crc16(CURRENT_KEY, 16);
  }
}
