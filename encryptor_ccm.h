#pragma once
#include "encryptor.h"
#include "config.h"
#include "frame.h"
#include "crypto_spec.h"
#include <mbedtls/ccm.h>
#include <unordered_map>

// Реализация AES‑CCM с поддержкой нескольких ключей и выбором активного KID
class CcmEncryptor : public IEncryptor {
public:
  CcmEncryptor();
  ~CcmEncryptor();

  // Добавить или заменить ключ для указанного KID
  bool setKey(uint8_t kid, const uint8_t* key, size_t len);

  // Включение/отключение шифрования
  void setEnabled(bool en);

  // Установить активный KID для шифрования исходящих кадров
  void setActiveKid(uint8_t kid);
  uint8_t activeKid() const { return active_kid_; }

  // Получить указатель на ключ для KID (или nullptr)
  const uint8_t* key(uint8_t kid) const;

  bool isReady() const override;

  bool encrypt(const uint8_t* plain, size_t plain_len,
               const uint8_t* aad, size_t aad_len,
               std::vector<uint8_t>& out) override;

  bool decrypt(const uint8_t* cipher, size_t cipher_len,
               const uint8_t* aad, size_t aad_len,
               std::vector<uint8_t>& out) override;

private:
  struct KeyInfo {
    mbedtls_ccm_context ctx;
    uint8_t key[16];
    KeyInfo() { mbedtls_ccm_init(&ctx); }
    ~KeyInfo() { mbedtls_ccm_free(&ctx); }
  };

  void buildNonce(const uint8_t* aad, size_t aad_len, uint8_t n[12]) const;

  std::unordered_map<uint8_t, KeyInfo> keys_; // таблица ключей по KID
  bool enabled_ = false;                        // глобальный флаг шифрования
  uint8_t active_kid_ = 0;                      // активный KID для исходящих кадров
};

