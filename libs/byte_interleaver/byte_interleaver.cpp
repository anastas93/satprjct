#include "byte_interleaver.h"
#include <vector>

namespace {
// Фиксированная глубина интерливинга
static constexpr size_t DEPTH = 8;
}

namespace byte_interleaver {
// Перемежение байтов в буфере
void interleave(uint8_t* buf, size_t len) {
  if (!buf || DEPTH <= 1) return;            // проверка указателя и глубины
  size_t cols = (len + DEPTH - 1) / DEPTH;   // число столбцов матрицы
  std::vector<uint8_t> matrix(DEPTH * cols, 0);
  for (size_t i = 0; i < len; ++i) matrix[i] = buf[i];
  size_t k = 0;
  for (size_t c = 0; c < cols; ++c) {
    for (size_t r = 0; r < DEPTH; ++r) {
      size_t idx = r * cols + c;
      if (idx < len) buf[k++] = matrix[idx];
    }
  }
}

// Обратное перемежение буфера
void deinterleave(uint8_t* buf, size_t len) {
  if (!buf || DEPTH <= 1) return;            // проверка указателя и глубины
  size_t cols = (len + DEPTH - 1) / DEPTH;   // число столбцов матрицы
  std::vector<uint8_t> matrix(DEPTH * cols, 0);
  size_t k = 0;
  for (size_t c = 0; c < cols; ++c) {
    for (size_t r = 0; r < DEPTH; ++r) {
      size_t idx = r * cols + c;
      if (idx < len && k < len) matrix[idx] = buf[k++];
    }
  }
  for (size_t i = 0; i < len; ++i) buf[i] = matrix[i];
}
}
