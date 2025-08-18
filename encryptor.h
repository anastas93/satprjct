
#pragma once
#include <vector>
#include <stdint.h>
#include <stddef.h>

class IEncryptor {
public:
  virtual ~IEncryptor() = default;
  virtual bool encrypt(const uint8_t* plain, size_t plain_len,
                       const uint8_t* aad, size_t aad_len,
                       std::vector<uint8_t>& out) = 0;
  virtual bool decrypt(const uint8_t* cipher, size_t cipher_len,
                       const uint8_t* aad, size_t aad_len,
                       std::vector<uint8_t>& out) = 0;
  virtual bool isReady() const = 0;
  // Смена активного ключа по идентификатору; по умолчанию ничего не делаем
  virtual void setActiveKid(uint8_t) {}
};

class NoEncryptor : public IEncryptor {
public:
  bool encrypt(const uint8_t* plain, size_t len,
               const uint8_t*, size_t, std::vector<uint8_t>& out) override {
    out.assign(plain, plain+len); return true;
  }
  bool decrypt(const uint8_t* cipher, size_t len,
               const uint8_t*, size_t, std::vector<uint8_t>& out) override {
    out.assign(cipher, cipher+len); return true;
  }
  bool isReady() const override { return false; }
  // Для заглушки смена ключа не требуется
  void setActiveKid(uint8_t) override {}
};
