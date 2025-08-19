#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Перемежение байтов матрицей глубины depth
inline void interleave_bytes(const uint8_t* in, size_t len, size_t depth, std::vector<uint8_t>& out) {
  if (depth <= 1 || !in) { out.assign(in, in + len); return; }
  size_t cols = (len + depth - 1) / depth;
  std::vector<uint8_t> matrix(depth * cols, 0);
  for (size_t i = 0; i < len; ++i) matrix[i] = in[i];
  out.clear(); out.reserve(len);
  for (size_t c = 0; c < cols; ++c) {
    for (size_t r = 0; r < depth; ++r) {
      size_t idx = r * cols + c;
      if (idx < len) out.push_back(matrix[idx]);
    }
  }
}

// Обратное перемежение
inline void deinterleave_bytes(const uint8_t* in, size_t len, size_t depth, std::vector<uint8_t>& out) {
  if (depth <= 1 || !in) { out.assign(in, in + len); return; }
  size_t cols = (len + depth - 1) / depth;
  std::vector<uint8_t> matrix(depth * cols, 0);
  size_t k = 0;
  for (size_t c = 0; c < cols; ++c) {
    for (size_t r = 0; r < depth; ++r) {
      size_t idx = r * cols + c;
      if (idx < len && k < len) matrix[idx] = in[k++];
    }
  }
  out.assign(matrix.begin(), matrix.begin() + len);
}
