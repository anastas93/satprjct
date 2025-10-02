#include "chacha20_poly1305.h"
#include "hkdf.h"
#include <algorithm>
#include <array>
#include <sodium.h>

namespace crypto {
namespace chacha20poly1305 {
namespace {

constexpr char EXPAND_INFO[] = "sat.aead.chacha20poly1305";

bool ensureInit() {
  static bool initialized = false;
  if (initialized) return true;
  if (sodium_init() == -1) {
    return false;
  }
  initialized = true;
  return true;
}

bool expandKey(const uint8_t* key, size_t key_len,
               std::array<uint8_t, KEY_SIZE>& out) {
  if (!key || key_len == 0) return false;
  if (key_len == KEY_SIZE) {
    std::copy_n(key, KEY_SIZE, out.begin());
    return true;
  }
  bool derived = crypto::hkdf::derive(nullptr,
                                      0,
                                      key,
                                      key_len,
                                      reinterpret_cast<const uint8_t*>(EXPAND_INFO),
                                      sizeof(EXPAND_INFO) - 1,
                                      out.data(),
                                      out.size());
  if (!derived) {
    for (size_t i = 0; i < out.size(); ++i) {
      out[i] = key[i % key_len];
    }
  }
  return true;
}

} // namespace

bool encrypt(const uint8_t* key, size_t key_len,
             const uint8_t* nonce, size_t nonce_len,
             const uint8_t* aad, size_t aad_len,
             const uint8_t* input, size_t input_len,
             std::vector<uint8_t>& output,
             std::vector<uint8_t>& tag) {
  if (!ensureInit()) return false;
  if (!key || !nonce || (!input && input_len)) return false;
  if (nonce_len != NONCE_SIZE) return false;
  std::array<uint8_t, KEY_SIZE> expanded{};
  if (!expandKey(key, key_len, expanded)) return false;

  output.resize(input_len);
  tag.resize(TAG_SIZE);
  unsigned long long tag_len = 0;
  int ret = crypto_aead_chacha20poly1305_ietf_encrypt_detached(
      output.data(),
      tag.data(),
      &tag_len,
      input,
      static_cast<unsigned long long>(input_len),
      aad,
      static_cast<unsigned long long>(aad_len),
      nullptr,
      nonce,
      expanded.data());
  if (ret != 0 || tag_len != TAG_SIZE) {
    output.clear();
    tag.clear();
    return false;
  }
  return true;
}

bool decrypt(const uint8_t* key, size_t key_len,
             const uint8_t* nonce, size_t nonce_len,
             const uint8_t* aad, size_t aad_len,
             const uint8_t* input, size_t input_len,
             const uint8_t* tag, size_t tag_len,
             std::vector<uint8_t>& output) {
  if (!ensureInit()) return false;
  if (!key || !nonce || (!input && input_len) || !tag) return false;
  if (nonce_len != NONCE_SIZE || tag_len != TAG_SIZE) return false;
  std::array<uint8_t, KEY_SIZE> expanded{};
  if (!expandKey(key, key_len, expanded)) return false;
  output.resize(input_len);
  int ret = crypto_aead_chacha20poly1305_ietf_decrypt_detached(
      output.data(),
      nullptr,
      input,
      static_cast<unsigned long long>(input_len),
      tag,
      aad,
      static_cast<unsigned long long>(aad_len),
      nonce,
      expanded.data());
  if (ret != 0) {
    output.clear();
    return false;
  }
  return true;
}

} // namespace chacha20poly1305
} // namespace crypto

