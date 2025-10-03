#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// Реализация кода Рида-Соломона (255,223) над GF(2^8)
namespace rs255 {

// Кодирование: к входным данным добавляется 32 байта паритета.
// Бросает std::invalid_argument при len > 0 и data == nullptr.
void encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

// Декодирование: возвращает true при успешном исправлении,
// corrected содержит число исправленных байтов.
// Возвращает false при len < 32 или некорректных указателях.
bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out, int& corrected);

}

