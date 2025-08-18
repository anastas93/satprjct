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
}

namespace crypto_spec {
  // Статический корневой ключ, используемый для HMAC в ECDH
  const uint8_t KEYDH_ROOT_KEY[16] = {
    0x73, 0xF2, 0xBC, 0x91, 0x0D, 0x4A, 0x52, 0xE6,
    0x8C, 0x47, 0x3B, 0x19, 0x5D, 0xA8, 0x61, 0x24
  };

  uint8_t CURRENT_KEY[16] = {0};
  uint16_t CURRENT_KEY_CRC = 0;

  void setCurrentKey(const uint8_t* key) {
    if (!key) return;
    memcpy(CURRENT_KEY, key, 16);
    CURRENT_KEY_CRC = crc16(CURRENT_KEY, 16);
  }
}
