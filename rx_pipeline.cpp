
#include "rx_pipeline.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include <string.h>
#include <algorithm> // для std::erase_if

RxPipeline::RxPipeline(IEncryptor& enc, PipelineMetrics& m)
: enc_(enc), metrics_(m) {}

void RxPipeline::gc() {
  const unsigned long now = millis();
  // удаляем просроченные сборщики через стандартный алгоритм
  std::erase_if(assemblers_, [&](auto& item) {
    bool expired = now - item.second.ts_ms > cfg::RX_ASSEMBLER_TTL_MS;
    if (expired) {
      reasm_bytes_ -= item.second.bytes;
      metrics_.rx_assem_drop_ttl++;
    }
    return expired;
  });
  while (dup_window_.size() > cfg::RX_DUP_WINDOW) {
    dup_set_.erase(dup_window_.front());
    dup_window_.pop_front();
  }
}

bool RxPipeline::isDup(uint32_t msg_id) {
  return dup_set_.find(msg_id) != dup_set_.end();
}

void RxPipeline::sendAck(uint32_t msg_id) {
  FrameHeader ack{};
  ack.ver = cfg::PIPE_VERSION;
  ack.flags = F_ACK;
  ack.msg_id = msg_id;
  ack.frag_idx = 0; ack.frag_cnt = 0; ack.payload_len = 0; ack.crc16 = 0;
  uint8_t buf[FRAME_HEADER_SIZE];
  ack.encode(buf, FRAME_HEADER_SIZE);
  uint16_t crc = crc16_ccitt(buf, FRAME_HEADER_SIZE, 0xFFFF);
  ack.crc16 = crc;
  ack.encode(buf, FRAME_HEADER_SIZE);
  Radio_sendRaw(buf, FRAME_HEADER_SIZE);
  FrameLog::push('T', buf, FRAME_HEADER_SIZE);
}

void RxPipeline::onReceive(const uint8_t* frame, size_t len) {
  if (!frame || len < FRAME_HEADER_SIZE) return;
  FrameHeader hdr;
  if (!FrameHeader::decode(hdr, frame, len)) return;
  if (hdr.ver != cfg::PIPE_VERSION) return;
  if (len != FRAME_HEADER_SIZE + hdr.payload_len) { metrics_.rx_drop_len_mismatch++; return; }

  uint16_t got_crc = hdr.crc16;
  FrameHeader tmp = hdr; tmp.crc16 = 0;
  uint8_t hdr_buf[FRAME_HEADER_SIZE];
  tmp.encode(hdr_buf, FRAME_HEADER_SIZE);
  uint16_t crc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE, 0xFFFF);
  crc = crc16_ccitt(frame + FRAME_HEADER_SIZE, hdr.payload_len, crc);
  if (crc != got_crc) { metrics_.rx_crc_fail++; return; }

  FrameLog::push('R', frame, len);

  if (hdr.flags & F_ACK) { if (ack_cb_) ack_cb_(hdr.msg_id); return; }

  const uint8_t* p = frame + FRAME_HEADER_SIZE;
  std::vector<uint8_t> plain;
  if (hdr.flags & F_ENC) {
    std::vector<uint8_t> aad(hdr_buf, hdr_buf + FRAME_HEADER_SIZE);
    if (!enc_.decrypt(p, hdr.payload_len, aad.data(), aad.size(), plain)) { metrics_.dec_fail_tag++; return; }
  } else {
    plain.assign(p, p + hdr.payload_len);
  }

  metrics_.rx_frames_ok++; metrics_.rx_bytes += len;

  if (!(hdr.flags & F_FRAG)) {
    if (!isDup(hdr.msg_id)) {
      dup_window_.push_back(hdr.msg_id);
      dup_set_.insert(hdr.msg_id);
      if (cb_) cb_(hdr.msg_id, plain.data(), plain.size());
      metrics_.rx_msgs_ok++;
      if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
    } else { metrics_.rx_dup_msgs++; }
    gc(); return;
  }

  metrics_.rx_frags++;
  // Enforce max assemblers
  if (assemblers_.find(hdr.msg_id) == assemblers_.end()) {
    if (assemblers_.size() >= cfg::RX_REASM_MAX_ASSEMBLERS) { metrics_.rx_assem_drop_overflow++; return; }
  }
  auto& as = assemblers_[hdr.msg_id];
  if (as.frags.empty()) {
    as.msg_id = hdr.msg_id; as.frag_cnt = hdr.frag_cnt;
    as.frags.resize(hdr.frag_cnt); as.ts_ms = millis(); as.bytes = 0;
  }
  as.ts_ms = millis();
  if (hdr.frag_idx < as.frags.size()) {
    size_t new_bytes = as.bytes - as.frags[hdr.frag_idx].size() + plain.size();
    if (new_bytes > cfg::RX_REASM_PER_MSG_CAP || (reasm_bytes_ - as.frags[hdr.frag_idx].size() + plain.size()) > cfg::RX_REASM_CAP_BYTES) {
      assemblers_.erase(hdr.msg_id); metrics_.rx_assem_drop_overflow++; return; }
    reasm_bytes_ = reasm_bytes_ - as.frags[hdr.frag_idx].size() + plain.size();
    as.bytes = new_bytes;
    as.frags[hdr.frag_idx] = std::move(plain);
  }

  bool complete = true; size_t total = 0;
  for (const auto& v : as.frags) { if (v.empty()) { complete=false; break; } total += v.size(); }
  if (complete) {
    std::vector<uint8_t> full; full.reserve(total);
    for (auto& v : as.frags) full.insert(full.end(), v.begin(), v.end());
    if (!isDup(hdr.msg_id)) {
      dup_window_.push_back(hdr.msg_id);
      dup_set_.insert(hdr.msg_id);
      if (cb_) cb_(hdr.msg_id, full.data(), full.size());
      metrics_.rx_msgs_ok++;
      if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
    } else { metrics_.rx_dup_msgs++; }
    reasm_bytes_ -= as.bytes; assemblers_.erase(hdr.msg_id);
  }
  gc();
}
