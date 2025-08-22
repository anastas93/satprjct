#include "bit_interleaver.h"
#include <vector>
#include <cstring>

namespace {
// Глубина интерливинга по умолчанию
static constexpr size_t BDEPTH = 8;
}

namespace bit_interleaver {
// Перемежение бит в буфере
void interleave(uint8_t* buf, size_t len) {
  if (!buf || BDEPTH <= 1) return; // проверка указателя и глубины
  size_t total_bits = len * 8;
  size_t cols = (total_bits + BDEPTH - 1) / BDEPTH;
  std::vector<uint8_t> matrix(BDEPTH * cols, 0);
  // Раскладываем биты по матрице
  for (size_t i = 0; i < total_bits; ++i) {
    uint8_t bit = (buf[i >> 3] >> (7 - (i & 7))) & 1;
    matrix[i] = bit;
  }
  std::memset(buf, 0, len); // очищаем исходный буфер
  size_t k = 0;
  for (size_t c = 0; c < cols; ++c) {
    for (size_t r = 0; r < BDEPTH; ++r) {
      size_t idx = r * cols + c;
      if (idx < total_bits) {
        if (matrix[idx]) buf[k >> 3] |= (1 << (7 - (k & 7)));
        ++k;
      }
    }
  }
}

// Обратное перемежение бит в буфере
void deinterleave(uint8_t* buf, size_t len) {
  if (!buf || BDEPTH <= 1) return; // проверка указателя и глубины
  size_t total_bits = len * 8;
  size_t cols = (total_bits + BDEPTH - 1) / BDEPTH;
  std::vector<uint8_t> matrix(BDEPTH * cols, 0);
  size_t k = 0;
  for (size_t c = 0; c < cols; ++c) {
    for (size_t r = 0; r < BDEPTH; ++r) {
      size_t idx = r * cols + c;
      if (idx < total_bits) {
        uint8_t bit = (buf[k >> 3] >> (7 - (k & 7))) & 1;
        matrix[idx] = bit;
        ++k;
      }
    }
  }
  std::memset(buf, 0, len);
  for (size_t i = 0; i < total_bits; ++i) {
    if (matrix[i]) buf[i >> 3] |= (1 << (7 - (i & 7)));
  }
}
} // namespace bit_interleaver
