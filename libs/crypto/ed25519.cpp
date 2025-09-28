#include "ed25519.h"
#include <sodium.h>

namespace crypto {
namespace ed25519 {

namespace {

// Глобальный флаг инициализации libsodium
bool ensureInit() {
  static bool initialized = false;
  if (initialized) return true;
  if (sodium_init() == -1) {
    return false;
  }
  initialized = true;
  return true;
}

}  // namespace

bool verify(const uint8_t* message, size_t message_len,
            const std::array<uint8_t, SIGNATURE_SIZE>& signature,
            const std::array<uint8_t, PUBLIC_KEY_SIZE>& public_key) {
  if (!ensureInit()) return false;
  return crypto_sign_ed25519_verify_detached(signature.data(), message, message_len,
                                             public_key.data()) == 0;
}

}  // namespace ed25519
}  // namespace crypto

