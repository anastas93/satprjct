#ifndef ARDUINO
// Юнит‑тест простейшего FEC, не участвует в прошивке
#include <fec.h>
#include <vector>
#include <cassert>
#include <iostream>

int main() {
  std::vector<uint8_t> src{10,20,30,40};
  std::vector<uint8_t> enc;
  fec_encode_repeat(src.data(), src.size(), enc);
  std::vector<uint8_t> dec;
  bool ok = fec_decode_repeat(enc.data(), enc.size(), dec);
  assert(ok && dec == src);
  enc[3] ^= 0xFF; // вносим ошибку в пару
  bool ok2 = fec_decode_repeat(enc.data(), enc.size(), dec);
  assert(!ok2);
  std::cout << "fec_ok\n";
  return 0;
}
#endif // !ARDUINO
