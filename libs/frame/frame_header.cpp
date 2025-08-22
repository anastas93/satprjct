#include "frame_header.h"
#include <cstring>
#include <vector>

// Подсчёт CRC16 (полином x^16 + x^12 + x^5 + 1)
uint16_t FrameHeader::crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
  }
  return crc;
}

// Кодирование заголовка
bool FrameHeader::encode(uint8_t* out, size_t out_len, const uint8_t* payload, size_t payload_len) {
  if (!out || out_len < SIZE) return false;
  if (!payload && payload_len) return false;

  out[0] = ver;
  out[1] = flags;
  out[2] = static_cast<uint8_t>(msg_id >> 24);
  out[3] = static_cast<uint8_t>(msg_id >> 16);
  out[4] = static_cast<uint8_t>(msg_id >> 8);
  out[5] = static_cast<uint8_t>(msg_id);
  out[6] = static_cast<uint8_t>(frag_idx >> 8);
  out[7] = static_cast<uint8_t>(frag_idx);
  out[8] = static_cast<uint8_t>(frag_cnt >> 8);
  out[9] = static_cast<uint8_t>(frag_cnt);
  out[10] = static_cast<uint8_t>(payload_len >> 8);
  out[11] = static_cast<uint8_t>(payload_len);
  out[12] = static_cast<uint8_t>(ack_mask >> 24);
  out[13] = static_cast<uint8_t>(ack_mask >> 16);
  out[14] = static_cast<uint8_t>(ack_mask >> 8);
  out[15] = static_cast<uint8_t>(ack_mask);

  hdr_crc = crc16(out, 16);
  out[16] = static_cast<uint8_t>(hdr_crc >> 8);
  out[17] = static_cast<uint8_t>(hdr_crc);

  std::vector<uint8_t> tmp(out, out + 18);
  if (payload && payload_len) tmp.insert(tmp.end(), payload, payload + payload_len);
  frame_crc = crc16(tmp.data(), tmp.size());
  out[18] = static_cast<uint8_t>(frame_crc >> 8);
  out[19] = static_cast<uint8_t>(frame_crc);
  return true;
}

// Проверка контрольной суммы кадра
bool FrameHeader::checkFrameCrc(const uint8_t* payload, size_t payload_len) const {
  uint8_t hdr[18];
  hdr[0] = ver;
  hdr[1] = flags;
  hdr[2] = static_cast<uint8_t>(msg_id >> 24);
  hdr[3] = static_cast<uint8_t>(msg_id >> 16);
  hdr[4] = static_cast<uint8_t>(msg_id >> 8);
  hdr[5] = static_cast<uint8_t>(msg_id);
  hdr[6] = static_cast<uint8_t>(frag_idx >> 8);
  hdr[7] = static_cast<uint8_t>(frag_idx);
  hdr[8] = static_cast<uint8_t>(frag_cnt >> 8);
  hdr[9] = static_cast<uint8_t>(frag_cnt);
  hdr[10] = static_cast<uint8_t>(payload_len >> 8);
  hdr[11] = static_cast<uint8_t>(payload_len);
  hdr[12] = static_cast<uint8_t>(ack_mask >> 24);
  hdr[13] = static_cast<uint8_t>(ack_mask >> 16);
  hdr[14] = static_cast<uint8_t>(ack_mask >> 8);
  hdr[15] = static_cast<uint8_t>(ack_mask);
  uint16_t hc = crc16(hdr, 16);
  hdr[16] = static_cast<uint8_t>(hc >> 8);
  hdr[17] = static_cast<uint8_t>(hc);
  std::vector<uint8_t> tmp(hdr, hdr + 18);
  if (payload && payload_len) tmp.insert(tmp.end(), payload, payload + payload_len);
  return crc16(tmp.data(), tmp.size()) == frame_crc;
}

// Декодирование заголовка
bool FrameHeader::decode(const uint8_t* data, size_t len, FrameHeader& out) {
  if (!data || len < SIZE) return false;
  out.ver = data[0];
  out.flags = data[1];
  out.msg_id = (static_cast<uint32_t>(data[2]) << 24) |
               (static_cast<uint32_t>(data[3]) << 16) |
               (static_cast<uint32_t>(data[4]) << 8)  |
               static_cast<uint32_t>(data[5]);
  out.frag_idx = (static_cast<uint16_t>(data[6]) << 8) | data[7];
  out.frag_cnt = (static_cast<uint16_t>(data[8]) << 8) | data[9];
  out.payload_len = (static_cast<uint16_t>(data[10]) << 8) | data[11];
  out.ack_mask = (static_cast<uint32_t>(data[12]) << 24) |
                 (static_cast<uint32_t>(data[13]) << 16) |
                 (static_cast<uint32_t>(data[14]) << 8) |
                 static_cast<uint32_t>(data[15]);
  out.hdr_crc = (static_cast<uint16_t>(data[16]) << 8) | data[17];
  out.frame_crc = (static_cast<uint16_t>(data[18]) << 8) | data[19];

  uint16_t calc = crc16(data, 16);
  return calc == out.hdr_crc;
}
