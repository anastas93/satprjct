
#pragma once
#include <vector>
#include "frame.h"
#include "config.h"

struct Fragment {
  FrameHeader hdr;
  std::vector<uint8_t> payload;
};

class Fragmenter {
public:
  // payload_max = эффективный лимит полезной нагрузки (учитывает ENC-overhead)
  std::vector<Fragment> split(uint32_t msg_id, const uint8_t* data, size_t len,
                              bool ack_required, uint16_t payload_max);
};
