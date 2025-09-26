#include "x25519.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>

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
  // Сбор энтропии должен устойчиво работать как при наличии аппаратного генератора,
  // так и при его отсутствии, поэтому формируем seed из нескольких источников.
  static thread_local std::mt19937_64 rng([]() {
    auto mix = [](uint64_t acc, uint64_t value) {
      acc ^= value + 0x9e3779b97f4a7c15ull + (acc << 6) + (acc >> 2);
      return acc;
    };

    static std::atomic<uint64_t> counter{0};
    uint64_t seed = mix(0x9e3779b97f4a7c15ull, counter.fetch_add(1, std::memory_order_relaxed));
    seed = mix(seed, static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    seed = mix(seed, reinterpret_cast<uint64_t>(&seed));

    try {
      std::random_device rd;
      if (rd.entropy() > 0.0) {
        seed = mix(seed, (static_cast<uint64_t>(rd()) << 32) ^ rd());
      } else {
        throw std::runtime_error("std::random_device has insufficient entropy");
      }
    } catch (...) {
      std::ifstream urandom("/dev/urandom", std::ios::binary);
      if (urandom) {
        uint64_t extra = 0;
        urandom.read(reinterpret_cast<char*>(&extra), sizeof(extra));
        if (urandom.gcount() == static_cast<std::streamsize>(sizeof(extra))) {
          seed = mix(seed, extra);
        }
      }
      seed = mix(seed, static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
      seed = mix(seed, reinterpret_cast<uint64_t>(&urandom));
    }

    seed ^= (seed >> 33);
    seed *= 0xff51afd7ed558ccdULL;
    seed ^= (seed >> 33);
    seed *= 0xc4ceb9fe1a85ec53ULL;
    seed ^= (seed >> 33);

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
