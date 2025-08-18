#pragma once
#include <stdint.h>
#include <stddef.h>

// Структура полезной нагрузки ACK с bitmap последних кадров
struct AckBitmap {
  uint32_t highest;   // наибольший подтверждённый идентификатор
  uint32_t bitmap;    // биты для предыдущих 32 идентификаторов
};

inline void ack_encode(const AckBitmap& a, uint8_t* out) {
  out[0] = (uint8_t)(a.highest >> 24);
  out[1] = (uint8_t)(a.highest >> 16);
  out[2] = (uint8_t)(a.highest >> 8);
  out[3] = (uint8_t)(a.highest);
  out[4] = (uint8_t)(a.bitmap >> 24);
  out[5] = (uint8_t)(a.bitmap >> 16);
  out[6] = (uint8_t)(a.bitmap >> 8);
  out[7] = (uint8_t)(a.bitmap);
}

inline void ack_decode(AckBitmap& a, const uint8_t* in) {
  a.highest = (uint32_t(in[0]) << 24) | (uint32_t(in[1]) << 16) |
              (uint32_t(in[2]) << 8)  | uint32_t(in[3]);
  a.bitmap  = (uint32_t(in[4]) << 24) | (uint32_t(in[5]) << 16) |
              (uint32_t(in[6]) << 8)  | uint32_t(in[7]);
}
