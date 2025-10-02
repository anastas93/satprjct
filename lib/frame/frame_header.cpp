#include "frame_header.h"
uint8_t FrameHeader::getFlags() const {
  return static_cast<uint8_t>((packed & FLAGS_MASK) >> FLAGS_SHIFT);
}

void FrameHeader::setFlags(uint8_t value) {
  packed = (packed & ~FLAGS_MASK) | (static_cast<uint32_t>(value & 0xFF) << FLAGS_SHIFT);
}

uint16_t FrameHeader::getFragIdx() const {
  return static_cast<uint16_t>((packed & FRAG_MASK) >> FRAG_SHIFT);
}

void FrameHeader::setFragIdx(uint16_t value) {
  packed = (packed & ~FRAG_MASK) | ((static_cast<uint32_t>(value) & 0x0FFF) << FRAG_SHIFT);
}

uint16_t FrameHeader::getPayloadLen() const {
  return static_cast<uint16_t>(packed & LEN_MASK);
}

void FrameHeader::setPayloadLen(uint16_t value) {
  packed = (packed & ~LEN_MASK) | (static_cast<uint32_t>(value) & LEN_MASK);
}

uint16_t FrameHeader::crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

// Кодирование заголовка
bool FrameHeader::encode(uint8_t* out, size_t out_len, const uint8_t* payload, size_t payload_len) {
  if (!out || out_len < SIZE) return false;
  if (!payload && payload_len) return false;
  if (payload_len > LEN_MASK) return false; // длина не должна превышать 12-битовый диапазон

  uint32_t packed_local = packed;
  packed_local &= ~LEN_MASK;
  packed_local |= static_cast<uint32_t>(payload_len) & LEN_MASK;
  packed = packed_local;

  /*
   * Формат заголовка (big-endian):
   * [0]   - версия протокола
   * [1-2] - идентификатор сообщения (uint16_t)
   * [3-4] - количество фрагментов (uint16_t)
   * [5-8] - упакованное поле: биты [31:24] флаги, [23:12] индекс фрагмента,
   *         [11:0] длина полезной нагрузки
   * [9-11] - зарезервировано (нули для выравнивания под AEAD-тег)
   */
  out[0] = ver;
  out[1] = static_cast<uint8_t>(msg_id >> 8);
  out[2] = static_cast<uint8_t>(msg_id);
  out[3] = static_cast<uint8_t>(frag_cnt >> 8);
  out[4] = static_cast<uint8_t>(frag_cnt);
  out[5] = static_cast<uint8_t>(packed_local >> 24);
  out[6] = static_cast<uint8_t>(packed_local >> 16);
  out[7] = static_cast<uint8_t>(packed_local >> 8);
  out[8] = static_cast<uint8_t>(packed_local);
  out[9] = 0;
  out[10] = 0;
  out[11] = 0;
  return true;
}

// Декодирование заголовка
bool FrameHeader::decode(const uint8_t* data, size_t len, FrameHeader& out) {
  if (!data || len < SIZE) return false;
  out.ver = data[0];
  out.msg_id = static_cast<uint16_t>(data[1] << 8 | data[2]);
  out.frag_cnt = static_cast<uint16_t>(data[3] << 8 | data[4]);
  out.packed = (static_cast<uint32_t>(data[5]) << 24) |
               (static_cast<uint32_t>(data[6]) << 16) |
               (static_cast<uint32_t>(data[7]) << 8) |
               static_cast<uint32_t>(data[8]);
  return true;
}
