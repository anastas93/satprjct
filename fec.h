#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "libs/rs/rs.h"
#include "libs/viterbi/viterbi.h"
#include "libs/ldpc/ldpc.h"

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

// Сложный FEC: RS(255,223) + Viterbi (K=7,R=1/2)
inline void fec_encode_rs_viterbi(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  std::vector<uint8_t> rs;
  rs255::encode(in, len, rs);
  vit::encode(rs.data(), rs.size(), out);
}

inline bool fec_decode_rs_viterbi(const uint8_t* in, size_t len, std::vector<uint8_t>& out, int& corrected) {
  std::vector<uint8_t> tmp;
  if (!vit::decode(in, len, tmp)) return false;
  bool ok = rs255::decode(tmp.data(), tmp.size(), out, corrected);
  return ok;
}

// Заглушка LDPC: пока просто копирует входные данные
inline void fec_encode_ldpc(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  ldpc::encode(in, len, out);
}

inline bool fec_decode_ldpc(const uint8_t* in, size_t len, std::vector<uint8_t>& out, int& corrected) {
  return ldpc::decode(in, len, out, corrected);
}
