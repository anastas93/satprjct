#include "sha256.h"
#include <algorithm>
#include <cstring>

namespace crypto {
namespace sha256 {

namespace {

constexpr uint32_t kInit[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
};

constexpr uint32_t kRound[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline uint32_t rotr(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (~x & z);
}

inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t big_sigma0(uint32_t x) {
  return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline uint32_t big_sigma1(uint32_t x) {
  return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline uint32_t small_sigma0(uint32_t x) {
  return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline uint32_t small_sigma1(uint32_t x) {
  return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

void process_block(Context& ctx, const uint8_t block[64]) {
  uint32_t w[64];
  for (size_t i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           (static_cast<uint32_t>(block[i * 4 + 3]));
  }
  for (size_t i = 16; i < 64; ++i) {
    w[i] = small_sigma1(w[i - 2]) + w[i - 7] + small_sigma0(w[i - 15]) + w[i - 16];
  }

  uint32_t a = ctx.state[0];
  uint32_t b = ctx.state[1];
  uint32_t c = ctx.state[2];
  uint32_t d = ctx.state[3];
  uint32_t e = ctx.state[4];
  uint32_t f = ctx.state[5];
  uint32_t g = ctx.state[6];
  uint32_t h = ctx.state[7];

  for (size_t i = 0; i < 64; ++i) {
    uint32_t t1 = h + big_sigma1(e) + ch(e, f, g) + kRound[i] + w[i];
    uint32_t t2 = big_sigma0(a) + maj(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  ctx.state[0] += a;
  ctx.state[1] += b;
  ctx.state[2] += c;
  ctx.state[3] += d;
  ctx.state[4] += e;
  ctx.state[5] += f;
  ctx.state[6] += g;
  ctx.state[7] += h;
}

}  // namespace

void init(Context& ctx) {
  std::memcpy(ctx.state.data(), kInit, sizeof(kInit));
  ctx.total_len = 0;
  ctx.buffer_len = 0;
}

void update(Context& ctx, const uint8_t* data, size_t len) {
  if (!data || len == 0) return;
  ctx.total_len += len;
  while (len > 0) {
    size_t to_copy = std::min(len, ctx.block.size() - ctx.buffer_len);
    std::memcpy(ctx.block.data() + ctx.buffer_len, data, to_copy);
    ctx.buffer_len += to_copy;
    data += to_copy;
    len -= to_copy;
    if (ctx.buffer_len == ctx.block.size()) {
      process_block(ctx, ctx.block.data());
      ctx.buffer_len = 0;
    }
  }
}

void finish(Context& ctx, uint8_t out[DIGEST_SIZE]) {
  uint64_t total_bits = ctx.total_len * 8;
  ctx.block[ctx.buffer_len++] = 0x80;
  if (ctx.buffer_len > 56) {
    while (ctx.buffer_len < 64) ctx.block[ctx.buffer_len++] = 0;
    process_block(ctx, ctx.block.data());
    ctx.buffer_len = 0;
  }
  while (ctx.buffer_len < 56) ctx.block[ctx.buffer_len++] = 0;
  for (int i = 7; i >= 0; --i) {
    ctx.block[ctx.buffer_len++] = static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF);
  }
  process_block(ctx, ctx.block.data());
  for (size_t i = 0; i < 8; ++i) {
    out[i * 4 + 0] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFF);
    out[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFF);
    out[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFF);
    out[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFF);
  }
}

std::array<uint8_t, DIGEST_SIZE> hash(const uint8_t* data, size_t len) {
  Context ctx;
  init(ctx);
  update(ctx, data, len);
  std::array<uint8_t, DIGEST_SIZE> out{};
  finish(ctx, out.data());
  return out;
}

}  // namespace sha256
}  // namespace crypto

