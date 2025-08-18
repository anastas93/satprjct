#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Простейший повторный код R=1/2
inline void fec_encode_repeat(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  out.resize(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out[2*i] = in[i];
    out[2*i + 1] = in[i];
  }
}

// Декодирование повторного кода, возвращает false при несовпадении пары
inline bool fec_decode_repeat(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  if (len % 2 != 0) return false;
  size_t out_len = len / 2;
  out.resize(out_len);
  bool ok = true;
  for (size_t i = 0; i < out_len; ++i) {
    uint8_t a = in[2*i];
    uint8_t b = in[2*i + 1];
    out[i] = a;
    if (a != b) ok = false;
  }
  return ok;
}
