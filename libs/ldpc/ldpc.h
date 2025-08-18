#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Заглушечная реализация LDPC (пока просто копирует данные)
namespace ldpc {
inline void encode(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  out.assign(in, in + len);
}

inline bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out, int& corrected) {
  out.assign(in, in + len);
  corrected = 0;
  return true; // всегда успешно
}
} // namespace ldpc
