#pragma once
#include <cstddef>
#include <cstdint>

namespace scrambler {

// Простой LFSR-скремблер на полиноме x^16 + x^14 + x^13 + x^11
// seed задаёт стартовое значение регистра, по умолчанию 0xACE1
void scramble(uint8_t* data, std::size_t len, uint16_t seed = 0xACE1);

// Дескремблирование идентично скремблированию
inline void descramble(uint8_t* data, std::size_t len, uint16_t seed = 0xACE1) {
  scramble(data, len, seed);
}

} // namespace scrambler

