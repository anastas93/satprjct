#include "text_converter.h"
#include <cstdint>

// Таблица соответствия байтов CP1251 и Unicode
static uint16_t cp1251ToUnicode(uint8_t code) {
  static const uint16_t table[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
  };
  if (code < 0x80) return static_cast<uint16_t>(code);
  uint16_t value = table[code - 0x80];
  if (value == 0x0000) return static_cast<uint16_t>('?');
  return value;
}

// Добавление Unicode-символа в строку UTF-8
static void appendUtf8(std::string& out, uint16_t code) {
  if (code < 0x80) {
    out.push_back(static_cast<char>(code));
  } else if (code < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  }
}

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

// Конвертация CP1251 в UTF-8
std::string cp1251ToUtf8(const std::vector<uint8_t>& data) {
  std::string out;
  out.reserve(data.size());
  for (uint8_t b : data) {
    uint16_t code = cp1251ToUnicode(b);
    appendUtf8(out, code);
  }
  return out;
}
