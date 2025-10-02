#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Конвертация строки UTF-8 в байты CP1251
// Возвращает массив байтов в кодировке CP1251
std::vector<uint8_t> utf8ToCp1251(const std::string& in);

// Конвертация последовательности байтов CP1251 в UTF-8 строку
// Невалидные символы заменяются на знак вопроса
std::string cp1251ToUtf8(const std::vector<uint8_t>& data);
