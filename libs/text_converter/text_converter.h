#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Конвертация строки UTF-8 в байты CP1251
// Возвращает массив байтов в кодировке CP1251
std::vector<uint8_t> utf8ToCp1251(const std::string& in);
