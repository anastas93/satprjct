#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Обёртки для свёрточного кодера и декодера Витерби
namespace conv_codec {
// Кодирование бит (байты трактуются как последовательность бит)
void encodeBits(const uint8_t* data, size_t len, std::vector<uint8_t>& out);
// Декодирование алгоритмом Витерби (жёсткие решения)
bool viterbiDecode(const uint8_t* data, size_t len, std::vector<uint8_t>& out);
}
