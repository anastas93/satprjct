#include <cassert>
#include <iostream>
#include <vector>
#include "../libs/text_converter/text_converter.h"

int main() {
  // Проверка конвертации русских символов
  auto bytes = utf8ToCp1251("Привет");
  std::vector<uint8_t> expected = {0xCF, 0xF0, 0xE8, 0xE2, 0xE5, 0xF2};
  assert(bytes == expected);
  // Проверка ASCII
  auto ascii = utf8ToCp1251("ABC");
  std::vector<uint8_t> expected_ascii = {'A','B','C'};
  assert(ascii == expected_ascii);
  std::cout << "OK" << std::endl;
  return 0;
}
