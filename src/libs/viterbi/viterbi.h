#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Классический сверточный код K=7, R=1/2 (полиномы 171/133 восьм.)
namespace vit {

void encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

// Декодирование с мягкими решениями (0..255), len должен быть чётным
bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out);

}

