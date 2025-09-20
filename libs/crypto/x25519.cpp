#include "x25519.h"
#include <algorithm>
#include <array>
#include <cstdint>

#ifdef ARDUINO
#include <esp_system.h>
#else
#include <random>
#endif

namespace crypto {
namespace x25519 {

namespace detail {
int curve25519_donna(uint8_t* mypublic, const uint8_t* secret, const uint8_t* basepoint);
}

namespace {
const uint8_t BASE_POINT[32] = {9};
}

void random_bytes(uint8_t* out, size_t len) {
  if (!out || len == 0) return;
#ifdef ARDUINO
  for (size_t i = 0; i < len; ++i) {
    out[i] = static_cast<uint8_t>(esp_random() & 0xFF);
  }
#else
  static thread_local std::mt19937_64 rng([]() {
    std::random_device rd;
    uint64_t seed = (static_cast<uint64_t>(rd()) << 32) ^ rd();
    if (seed == 0) seed = 0x123456789ABCDEFu;
    return seed;
  }());
  std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFFull);
  size_t offset = 0;
  while (offset < len) {
    uint64_t value = dist(rng);
    for (int i = 0; i < 8 && offset < len; ++i) {
      out[offset++] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
  }
#endif
}

void derive_public(std::array<uint8_t, KEY_SIZE>& public_key,
                   std::array<uint8_t, KEY_SIZE>& private_key) {
  private_key[0] &= 248;
  private_key[31] &= 127;
  private_key[31] |= 64;
  detail::curve25519_donna(public_key.data(), private_key.data(), BASE_POINT);
}

bool compute_shared(const std::array<uint8_t, KEY_SIZE>& private_key,
                    const std::array<uint8_t, KEY_SIZE>& peer_public,
                    std::array<uint8_t, KEY_SIZE>& shared_secret) {
  if (std::all_of(peer_public.begin(), peer_public.end(), [](uint8_t v) { return v == 0; })) {
    shared_secret.fill(0);
    return false;
  }
  detail::curve25519_donna(shared_secret.data(), private_key.data(), peer_public.data());
  bool zero = std::all_of(shared_secret.begin(), shared_secret.end(), [](uint8_t v) { return v == 0; });
  return !zero;
}

}  // namespace x25519
}  // namespace crypto
