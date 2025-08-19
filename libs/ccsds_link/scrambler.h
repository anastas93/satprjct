#pragma once
#include <stdint.h>
#include <stddef.h>

// Простой LFSR-скремблер, совместимый с подходом CCSDS TM
// Инициализируется номером сообщения для исключения длинных последовательностей
inline void lfsr_scramble(uint8_t* data, size_t len, uint16_t seed) {
  uint16_t lfsr = seed ? seed : 0xACE1u; // стартовое значение
  for (size_t i = 0; i < len; ++i) {
    uint8_t mask = 0;
    for (int b = 0; b < 8; ++b) {
      // Полином x^16 + x^14 + x^13 + x^11 (0xD008)
      uint16_t bit = ((lfsr >> 15) ^ (lfsr >> 13) ^ (lfsr >> 12) ^ (lfsr >> 10)) & 1;
      lfsr = (uint16_t)((lfsr << 1) | bit);
      mask |= (uint8_t)((lfsr & 1) << (7 - b));
    }
    data[i] ^= mask; // наложение маски
  }
}

// Дескремблирование идентично скремблированию
inline void lfsr_descramble(uint8_t* data, size_t len, uint16_t seed) {
  lfsr_scramble(data, len, seed);
}
