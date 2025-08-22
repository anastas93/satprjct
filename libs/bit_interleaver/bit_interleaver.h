#pragma once
#include <cstddef>
#include <cstdint>

// Битовый интерливинг фиксированной глубины
namespace bit_interleaver {
// Перемежение бит внутри буфера (len — количество байт)
void interleave(uint8_t* buf, size_t len);
// Обратное перемежение бит
void deinterleave(uint8_t* buf, size_t len);
}
