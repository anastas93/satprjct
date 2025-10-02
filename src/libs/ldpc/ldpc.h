#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Простейший LDPC на основе кода Хэмминга (12,8).
// Каждый байт расширяется до 12 бит с четырьмя проверочными битами.
// Такой код способен исправлять одиночные ошибки.
namespace ldpc {

// Формирует 12‑битовое кодовое слово для одного байта данных
inline uint16_t encode_byte(uint8_t d) {
  uint16_t cw = 0;
  auto set = [&](int pos, int val) { if (val) cw |= (1u << (pos - 1)); };
  // размещаем информационные биты по позициям 3,5,6,7,9,10,11,12
  set(3, (d >> 0) & 1);  set(5, (d >> 1) & 1);
  set(6, (d >> 2) & 1);  set(7, (d >> 3) & 1);
  set(9, (d >> 4) & 1);  set(10, (d >> 5) & 1);
  set(11, (d >> 6) & 1); set(12, (d >> 7) & 1);
  auto get = [&](int pos) { return (cw >> (pos - 1)) & 1u; };
  // вычисляем проверочные биты p1,p2,p4,p8
  int p1 = get(3) ^ get(5) ^ get(7) ^ get(9) ^ get(11);
  int p2 = get(3) ^ get(6) ^ get(7) ^ get(10) ^ get(11);
  int p4 = get(5) ^ get(6) ^ get(7) ^ get(12);
  int p8 = get(9) ^ get(10) ^ get(11) ^ get(12);
  set(1, p1); set(2, p2); set(4, p4); set(8, p8);
  return cw;
}

// Кодирование массива байтов
inline void encode(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  out.resize(len * 2);
  for (size_t i = 0; i < len; ++i) {
    uint16_t cw = encode_byte(in[i]);
    out[2 * i]     = (uint8_t)(cw >> 8);
    out[2 * i + 1] = (uint8_t)(cw & 0xFF);
  }
}

// Декодирование; при обнаружении и исправлении ошибок увеличивается счётчик corrected
inline bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out, int& corrected) {
  if (len % 2) return false; // ожидаем чётное число байтов
  size_t n = len / 2;
  out.resize(n);
  corrected = 0;
  bool ok = true;
  auto get = [&](uint16_t cw, int pos) { return (cw >> (pos - 1)) & 1u; };
  for (size_t i = 0; i < n; ++i) {
    uint16_t cw = (uint16_t(in[2 * i]) << 8) | in[2 * i + 1];
    int s1 = get(cw,1) ^ get(cw,3) ^ get(cw,5) ^ get(cw,7) ^ get(cw,9) ^ get(cw,11);
    int s2 = get(cw,2) ^ get(cw,3) ^ get(cw,6) ^ get(cw,7) ^ get(cw,10) ^ get(cw,11);
    int s4 = get(cw,4) ^ get(cw,5) ^ get(cw,6) ^ get(cw,7) ^ get(cw,12);
    int s8 = get(cw,8) ^ get(cw,9) ^ get(cw,10) ^ get(cw,11) ^ get(cw,12);
    int syn = s1 | (s2 << 1) | (s4 << 2) | (s8 << 3);
    if (syn) {
      if (syn >= 1 && syn <= 12) { cw ^= (1u << (syn - 1)); corrected++; }
      else ok = false; // ошибка вне допустимого диапазона
    }
    uint8_t d = (get(cw,3)  << 0) | (get(cw,5)  << 1) |
                (get(cw,6)  << 2) | (get(cw,7)  << 3) |
                (get(cw,9)  << 4) | (get(cw,10) << 5) |
                (get(cw,11) << 6) | (get(cw,12) << 7);
    out[i] = d;
  }
  return ok;
}

} // namespace ldpc
