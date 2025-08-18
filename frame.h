
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h"

struct FrameHeader {
  uint8_t  ver;        // версия протокола
  uint8_t  flags;      // флаги кадра
  uint32_t msg_id;     // идентификатор сообщения
  uint16_t frag_idx;   // номер фрагмента
  uint16_t frag_cnt;   // общее количество фрагментов
  uint16_t payload_len;// длина полезной нагрузки
  uint16_t crc16;      // CRC заголовка и данных

  // Размер заголовка в сериализованном виде
  static constexpr size_t ENCODED_SIZE = 14;

  // Кодирование заголовка в буфер (big-endian)
  bool encode(uint8_t* out, size_t len) const {
    if (!out || len < ENCODED_SIZE) return false;
    out[0] = ver;
    out[1] = flags;
    out[2] = (uint8_t)(msg_id >> 24);
    out[3] = (uint8_t)(msg_id >> 16);
    out[4] = (uint8_t)(msg_id >> 8);
    out[5] = (uint8_t)(msg_id);
    out[6] = (uint8_t)(frag_idx >> 8);
    out[7] = (uint8_t)(frag_idx);
    out[8] = (uint8_t)(frag_cnt >> 8);
    out[9] = (uint8_t)(frag_cnt);
    out[10] = (uint8_t)(payload_len >> 8);
    out[11] = (uint8_t)(payload_len);
    out[12] = (uint8_t)(crc16 >> 8);
    out[13] = (uint8_t)(crc16);
    return true;
  }

  // Декодирование заголовка из буфера (big-endian)
  static bool decode(FrameHeader& hdr, const uint8_t* buf, size_t len) {
    if (!buf || len < ENCODED_SIZE) return false;
    hdr.ver        = buf[0];
    hdr.flags      = buf[1];
    hdr.msg_id     = (uint32_t(buf[2]) << 24) |
                     (uint32_t(buf[3]) << 16) |
                     (uint32_t(buf[4]) << 8)  |
                     uint32_t(buf[5]);
    hdr.frag_idx   = (uint16_t(buf[6]) << 8) | uint16_t(buf[7]);
    hdr.frag_cnt   = (uint16_t(buf[8]) << 8) | uint16_t(buf[9]);
    hdr.payload_len= (uint16_t(buf[10])<< 8) | uint16_t(buf[11]);
    hdr.crc16      = (uint16_t(buf[12])<< 8) | uint16_t(buf[13]);
    return true;
  }
};

static_assert(FrameHeader::ENCODED_SIZE == 14, "Ожидается 14 байт заголовка");

enum FrameFlags : uint8_t {
  F_ACK_REQ = 0x01,
  F_ACK     = 0x02,
  F_ENC     = 0x04,
  F_FRAG    = 0x08,
  F_LAST    = 0x10,
};

static inline uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t init=0xFFFF) {
  uint16_t crc = init;
  for (size_t i=0; i<len; ++i) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b=0; b<8; ++b) {
      crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
    }
  }
  return crc;
}

static constexpr uint16_t FRAME_HEADER_SIZE = FrameHeader::ENCODED_SIZE;
static constexpr uint16_t FRAME_PAYLOAD_MAX = cfg::LORA_MTU - FRAME_HEADER_SIZE;
