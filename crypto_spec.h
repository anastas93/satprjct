#pragma once
#include <stdint.h>
#include <stddef.h>

// Спецификация статических криптографических параметров
namespace crypto_spec {
  // Корневой ключ для проверки HMAC при ECDH
  // может быть установлен из 32‑символьной hex‑строки
  extern uint8_t KEYDH_ROOT_KEY[16];

  // Текущий AES-ключ и его CRC-16 для быстрого доступа
  extern uint8_t CURRENT_KEY[16];
  extern uint16_t CURRENT_KEY_CRC;
  // Текущий идентификатор ключа (KID)
  extern uint8_t CURRENT_KID;

  // Установка нового AES-ключа с автоматическим расчётом CRC-16
  void setCurrentKey(const uint8_t* key);
  // Запоминает активный KID для статуса
  void setActiveKid(uint8_t kid);

  // Тип колбэка для уведомления о смене ключа
  typedef void (*KeyChangedCallback)();
  // Регистрация колбэка; вызывается после обновления KID или ключа
  void setKeyChangedCallback(KeyChangedCallback cb);

  // Получение текущего KID
  uint8_t getActiveKid();

  // Возврат CRC‑16 текущего ключа
  uint16_t getKeyCrc16();

  // Установка корневого ключа из 32‑символьной hex‑строки
  // Возвращает true при успешном разборе строки
  bool setRootKeyHex(const char* hex);

  // Ротация ключа с зашифрованным подтверждением "ключ применён"
  // `kid` — идентификатор ключа, `retries` — число попыток
  // Возвращает true при успешной активации
  bool rotateKeyWithAck(const uint8_t* newKey, uint8_t kid, int retries);
}
