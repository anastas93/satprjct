#include "rs255223.h"
#include "../rs/rs.h"
#include <vector>
#include <cstring>

namespace rs255223 {

// Простейшая обёртка: копируем 223 байта и добавляем паритет
void encode(const uint8_t* in, uint8_t* out) {
  if (!in || !out) return;
  std::vector<uint8_t> vec;
  // используем существующую реализацию
  rs255::encode(in, 223, vec);
  std::memcpy(out, vec.data(), vec.size());
}

// Декодирование блока; out должен иметь размер 223 байта
bool decode(const uint8_t* in, uint8_t* out) {
  if (!in || !out) return false;
  std::vector<uint8_t> vec;
  int corrected = 0;
  bool ok = rs255::decode(in, 255, vec, corrected);
  if (ok && vec.size() >= 223) {
    std::memcpy(out, vec.data(), 223);
  }
  return ok;
}

} // namespace rs255223
