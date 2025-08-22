#include "scrambler.h"

namespace scrambler {

void scramble(uint8_t* data, std::size_t len, uint16_t seed) {
  if (!data || len == 0) return; // проверка указателя и длины
  uint16_t lfsr = seed ? seed : 0xACE1u; // стартовое значение регистра
  for (std::size_t i = 0; i < len; ++i) {
    uint8_t mask = 0;
    for (int b = 0; b < 8; ++b) {
      // Полином x^16 + x^14 + x^13 + x^11 (0xD008)
      uint16_t bit = ((lfsr >> 15) ^ (lfsr >> 13) ^ (lfsr >> 12) ^ (lfsr >> 10)) & 1;
      lfsr = static_cast<uint16_t>((lfsr << 1) | bit);
      mask |= static_cast<uint8_t>((lfsr & 1) << (7 - b));
    }
    data[i] ^= mask; // наложение маски
  }
}

} // namespace scrambler

