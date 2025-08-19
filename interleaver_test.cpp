#ifndef ARDUINO
// Тест чередователя байтов, исключается из сборки прошивки
#include <interleaver.h>
#include <vector>
#include <cassert>
#include <iostream>

int main() {
  std::vector<uint8_t> src{0,1,2,3,4,5,6,7,8};
  std::vector<uint8_t> inter;
  interleave_bytes(src.data(), src.size(), 4, inter);
  std::vector<uint8_t> deinter;
  deinterleave_bytes(inter.data(), inter.size(), 4, deinter);
  assert(src == deinter);
  std::cout << "interleave_ok\n";
  return 0;
}
#endif // !ARDUINO
