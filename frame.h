
#pragma once
#include <stdint.h>
#include "config.h"

#pragma pack(push, 1)
struct FrameHeader {
  uint8_t  ver;
  uint8_t  flags;
  uint32_t msg_id;
  uint16_t frag_idx;
  uint16_t frag_cnt;
  uint16_t payload_len;
  uint16_t crc16;
};
#pragma pack(pop)

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

static constexpr uint16_t FRAME_HEADER_SIZE = sizeof(FrameHeader);
static constexpr uint16_t FRAME_PAYLOAD_MAX = cfg::LORA_MTU - FRAME_HEADER_SIZE;
