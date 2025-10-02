#include "conv_codec.h"
#include "../viterbi/viterbi.h"

namespace conv_codec {
// Простейшая обёртка над существующей реализацией
void encodeBits(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  if (!data || len == 0) return; // проверка входа
  vit::encode(data, len, out);
}

// Декодер Витерби для жёстких решений
bool viterbiDecode(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  if (!data || len == 0) return false; // проверка входа
  return vit::decode(data, len, out);
}
} // namespace conv_codec
