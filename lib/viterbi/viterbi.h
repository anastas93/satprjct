#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Классический сверточный код K=7, R=1/2 (полиномы 171/133 восьм.)
namespace vit {

// Бросает std::invalid_argument при len > 0 и data == nullptr.
void encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

// Декодирование с мягкими решениями (0..255), len должен быть чётным.
// Возвращает false при некорректных указателях или нечётной длине.
bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out);

}

