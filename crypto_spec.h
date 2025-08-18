#pragma once
#include <stdint.h>
#include <stddef.h>

// Спецификация статических криптографических параметров
namespace crypto_spec {
  // Корневой ключ для проверки HMAC при ECDH
  extern const uint8_t KEYDH_ROOT_KEY[16];

  // Текущий AES-ключ и его CRC-16 для быстрого доступа
  extern uint8_t CURRENT_KEY[16];
  extern uint16_t CURRENT_KEY_CRC;

  // Установка нового AES-ключа с автоматическим расчётом CRC-16
  void setCurrentKey(const uint8_t* key);
}
