#include "text_converter.h"
#include <cstdint>

// Преобразование одного символа Unicode в CP1251
static uint8_t unicodeToCp1251(uint16_t code) {
  // Диапазон кириллицы А..я
  if (code >= 0x0410 && code <= 0x044F) {
    return static_cast<uint8_t>(code - 0x0410 + 0xC0);
  }
  // Буквы Ё и ё
  if (code == 0x0401) return 0xA8;
  if (code == 0x0451) return 0xB8;
  // Символы ASCII передаются без изменений
  if (code <= 0x007F) return static_cast<uint8_t>(code);
  // Для остальных возвращаем знак вопроса
  return static_cast<uint8_t>('?');
}

// Конвертация строки UTF-8 в массив байтов CP1251
std::vector<uint8_t> utf8ToCp1251(const std::string& in) {
  std::vector<uint8_t> out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size();) {
    uint8_t c = static_cast<uint8_t>(in[i]);
    if ((c & 0x80) == 0) {                // одиночный байт ASCII
      out.push_back(c);
      ++i;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < in.size()) { // двухбайтовая последовательность
      uint8_t c2 = static_cast<uint8_t>(in[i + 1]);
      uint16_t code = ((c & 0x1F) << 6) | (c2 & 0x3F);
      out.push_back(unicodeToCp1251(code));
      i += 2;
    } else {
      // Некорректная последовательность
      out.push_back(static_cast<uint8_t>('?'));
      ++i;
    }
  }
  return out;
}
