#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>
#include "libs/crypto/chacha20_poly1305.h"
#include "libs/key_loader/key_loader.h"

// Проверка команды ENCT: шифруем и расшифровываем тестовое сообщение
#define TEST_CASE(name) void name()

TEST_CASE(enct_check) {
  const uint8_t key[16]   = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  const uint8_t nonce[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
  const char* text = "Test ENCT";                     // исходное сообщение
  size_t len = strlen(text);
  std::vector<uint8_t> cipher, tag, plain;
  assert(crypto::chacha20poly1305::encrypt(key, sizeof(key), nonce, sizeof(nonce),
                                           nullptr, 0,
                                           reinterpret_cast<const uint8_t*>(text), len,
                                           cipher, tag));
  assert(crypto::chacha20poly1305::decrypt(key, sizeof(key), nonce, sizeof(nonce),
                                           nullptr, 0,
                                           cipher.data(), cipher.size(),
                                           tag.data(), tag.size(), plain));
  assert(plain.size() == len);
  assert(std::equal(plain.begin(), plain.end(),
                    reinterpret_cast<const uint8_t*>(text)));
}

int main() {
  enct_check();
  std::cout << "OK" << std::endl;
  return 0;
}
