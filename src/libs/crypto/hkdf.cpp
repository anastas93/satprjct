#include "hkdf.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace crypto {
namespace hkdf {
namespace {

constexpr size_t BLOCK_SIZE = 64;  // размер блока SHA-256

// Подготовка ключа для HMAC: усечение/дополнение до длины блока
void normalizeKey(const uint8_t* key, size_t key_len, std::array<uint8_t, BLOCK_SIZE>& out) {
  std::array<uint8_t, HASH_LEN> digest{};
  if (key_len > BLOCK_SIZE) {
    digest = crypto::sha256::hash(key, key_len);
    std::memcpy(out.data(), digest.data(), digest.size());
    std::fill(out.begin() + digest.size(), out.end(), 0);
  } else {
    std::memcpy(out.data(), key, key_len);
    std::fill(out.begin() + key_len, out.end(), 0);
  }
}

std::array<uint8_t, HASH_LEN> hmac(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len) {
  std::array<uint8_t, BLOCK_SIZE> key_block{};
  normalizeKey(key, key_len, key_block);

  std::array<uint8_t, BLOCK_SIZE> o_key_pad{};
  std::array<uint8_t, BLOCK_SIZE> i_key_pad{};
  for (size_t i = 0; i < BLOCK_SIZE; ++i) {
    o_key_pad[i] = static_cast<uint8_t>(key_block[i] ^ 0x5c);
    i_key_pad[i] = static_cast<uint8_t>(key_block[i] ^ 0x36);
  }

  crypto::sha256::Context inner;
  crypto::sha256::init(inner);
  crypto::sha256::update(inner, i_key_pad.data(), i_key_pad.size());
  if (data_len > 0 && data) {
    crypto::sha256::update(inner, data, data_len);
  }
  std::array<uint8_t, HASH_LEN> inner_hash{};
  crypto::sha256::finish(inner, inner_hash.data());

  crypto::sha256::Context outer;
  crypto::sha256::init(outer);
  crypto::sha256::update(outer, o_key_pad.data(), o_key_pad.size());
  crypto::sha256::update(outer, inner_hash.data(), inner_hash.size());
  std::array<uint8_t, HASH_LEN> result{};
  crypto::sha256::finish(outer, result.data());
  return result;
}

}  // namespace

Prk extract(const uint8_t* salt, size_t salt_len, const uint8_t* ikm, size_t ikm_len) {
  static const std::array<uint8_t, HASH_LEN> ZERO_SALT{};
  const uint8_t* salt_ptr = salt_len > 0 && salt ? salt : ZERO_SALT.data();
  size_t salt_size = salt_len > 0 && salt ? salt_len : ZERO_SALT.size();
  return hmac(salt_ptr, salt_size, ikm, ikm_len);
}

bool expand(const Prk& prk, const uint8_t* info, size_t info_len, uint8_t* okm, size_t okm_len) {
  if (!okm || okm_len == 0) return false;
  if (okm_len > MAX_OKM_LEN) return false;
  std::array<uint8_t, HASH_LEN> previous{};
  size_t generated = 0;
  uint8_t counter = 1;
  while (generated < okm_len) {
    std::array<uint8_t, HASH_LEN + 64> buffer{};  // запас под previous + info + счетчик
    size_t offset = 0;
    if (counter > 1) {
      std::memcpy(buffer.data(), previous.data(), previous.size());
      offset += previous.size();
    }
    if (info_len > 0 && info) {
      std::memcpy(buffer.data() + offset, info, info_len);
      offset += info_len;
    }
    buffer[offset++] = counter;
    auto block = hmac(prk.data(), prk.size(), buffer.data(), offset);
    std::memcpy(previous.data(), block.data(), block.size());
    size_t to_copy = std::min(block.size(), okm_len - generated);
    std::memcpy(okm + generated, block.data(), to_copy);
    generated += to_copy;
    if (counter == 0xFF) break;
    ++counter;
  }
  return generated == okm_len;
}

bool derive(const uint8_t* salt,
            size_t salt_len,
            const uint8_t* ikm,
            size_t ikm_len,
            const uint8_t* info,
            size_t info_len,
            uint8_t* okm,
            size_t okm_len) {
  if (!ikm || ikm_len == 0) return false;
  auto prk = extract(salt, salt_len, ikm, ikm_len);
  return expand(prk, info, info_len, okm, okm_len);
}

}  // namespace hkdf
}  // namespace crypto

