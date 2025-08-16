
#include "fragmenter.h"
#include <algorithm>
#include <string.h>

std::vector<Fragment> Fragmenter::split(uint32_t msg_id, const uint8_t* data, size_t len,
                                        bool ack_required, uint16_t payload_max) {
  std::vector<Fragment> out;
  if (!data || len==0) return out;
  if (payload_max < 1) return out;
  uint16_t frag_cnt = (len + payload_max - 1) / payload_max;
  out.reserve(frag_cnt);
  size_t offset = 0;
  for (uint16_t i=0; i<frag_cnt; ++i) {
    size_t chunk = std::min<size_t>(payload_max, len - offset);

    std::vector<uint8_t> payload(data + offset, data + offset + chunk);
    offset += chunk;

    FrameHeader hdr{};
    hdr.ver = cfg::PIPE_VERSION;
    hdr.flags = 0;
    if (ack_required) hdr.flags |= F_ACK_REQ;
    if (frag_cnt > 1) hdr.flags |= F_FRAG;
    if (i == frag_cnt-1) hdr.flags |= F_LAST;
    hdr.msg_id = msg_id;
    hdr.frag_idx = i;
    hdr.frag_cnt = frag_cnt;
    hdr.payload_len = (uint16_t)chunk;
    hdr.crc16 = 0;

    out.emplace_back(Fragment{hdr, std::move(payload)});
  }
  return out;
}
