#pragma once
#include <cstdint>
#include <cstddef>

// Простые функции байтового интерливинга с фиксированной глубиной
namespace byte_interleaver {
// Перемежение байтов в буфере
void interleave(uint8_t* buf, size_t len);
// Обратное перемежение буфера
void deinterleave(uint8_t* buf, size_t len);
}
