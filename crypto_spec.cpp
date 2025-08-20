#include "crypto_spec.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
  uint8_t CURRENT_KID = 0;

  // Колбэк, вызываемый при смене ключа или KID
  static KeyChangedCallback key_cb = nullptr;

  void setKeyChangedCallback(KeyChangedCallback cb) { key_cb = cb; }

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
    if (key_cb) key_cb();
  }

  void setActiveKid(uint8_t kid) {
    CURRENT_KID = kid;
    if (key_cb) key_cb();
  }

  uint8_t getActiveKid() { return CURRENT_KID; }

  uint16_t getKeyCrc16() { return CURRENT_KEY_CRC; }

  // Простое XOR-"шифрование" для подтверждения
  static void xorCrypt(const uint8_t* key, const uint8_t* in, uint8_t* out, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      out[i] = in[i] ^ key[i % 16];
    }
  }

  bool rotateKeyWithAck(const uint8_t* newKey, uint8_t kid, int retries) {
    static const char ACK_TEXT[] = "ключ применён";
    const size_t ACK_LEN = sizeof(ACK_TEXT) - 1;
    uint8_t enc[32];
    uint8_t dec[32];
    for (int attempt = 0; attempt < retries; ++attempt) {
      printf("KEYCHG %u попытка %d\n", kid, attempt + 1); // рассылка
      xorCrypt(newKey, (const uint8_t*)ACK_TEXT, enc, ACK_LEN);
      if (attempt == 0) enc[0] ^= 0xFF; // имитация сбоя для ретрая
      xorCrypt(newKey, enc, dec, ACK_LEN);
      if (memcmp(dec, ACK_TEXT, ACK_LEN) == 0) {
        printf("ACK шифр: ");
        for (size_t i = 0; i < ACK_LEN; ++i) printf("%02X", enc[i]);
        printf("\n");
        setCurrentKey(newKey); // активация ключа
        printf("ACTIVATE %u\n", kid);
        return true;
      }
      printf("Повтор запроса...\n");
    }
    return false;
  }

  // Автоматическая загрузка корневого ключа при старте
  static const bool ROOT_KEY_LOADED = setRootKeyHex(DEFAULT_ROOT_KEY_HEX);
}
