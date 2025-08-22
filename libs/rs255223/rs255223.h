#pragma once
#include <cstdint>

// Обёртки над кодом Рида-Соломона (255,223)
// Вход: 223 байта, выход: 255 байт
namespace rs255223 {
// Кодирование блока данных
void encode(const uint8_t* in, uint8_t* out);
// Декодирование блока данных, возвращает true при успешной коррекции
bool decode(const uint8_t* in, uint8_t* out);
}
