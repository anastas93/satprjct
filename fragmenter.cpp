
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
    Fragment f{};
    f.payload.assign(data + offset, data + offset + chunk);
    offset += chunk;
    f.hdr.ver = cfg::PIPE_VERSION;
    f.hdr.flags = 0;
    if (ack_required) f.hdr.flags |= F_ACK_REQ;
    if (frag_cnt > 1) f.hdr.flags |= F_FRAG;
    if (i == frag_cnt-1) f.hdr.flags |= F_LAST;
    f.hdr.msg_id = msg_id;
    f.hdr.frag_idx = i;
    f.hdr.frag_cnt = frag_cnt;
    f.hdr.payload_len = (uint16_t)chunk;
    f.hdr.crc16 = 0;
    out.push_back(std::move(f));
  }
  return out;
}
