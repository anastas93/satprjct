
#include "rx_pipeline.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include <string.h>

RxPipeline::RxPipeline(IEncryptor& enc, PipelineMetrics& m)
: enc_(enc), metrics_(m) {}

void RxPipeline::gc() {
  const unsigned long now = millis();
  for (auto it = assemblers_.begin(); it != assemblers_.end();) {
    if (now - it->second.ts_ms > cfg::RX_ASSEMBLER_TTL_MS) {
      reasm_bytes_ -= it->second.bytes;
      metrics_.rx_assem_drop_ttl++;
      it = assemblers_.erase(it);
    } else {
      ++it;
    }
  }
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
  uint16_t crc = crc16_ccitt(reinterpret_cast<const uint8_t*>(&ack), sizeof(FrameHeader), 0xFFFF);
  ack.crc16 = crc;
  uint8_t buf[sizeof(FrameHeader)];
  memcpy(buf, &ack, sizeof(FrameHeader));
  Radio_sendRaw(buf, sizeof(FrameHeader));
  FrameLog::push('T', buf, sizeof(FrameHeader));
}

void RxPipeline::onReceive(const uint8_t* frame, size_t len) {
  if (!frame || len < sizeof(FrameHeader)) return;
  FrameHeader hdr;
  memcpy(&hdr, frame, sizeof(FrameHeader));
  if (hdr.ver != cfg::PIPE_VERSION) return;
  if (len != sizeof(FrameHeader) + hdr.payload_len) { metrics_.rx_drop_len_mismatch++; return; }

  uint16_t got_crc = hdr.crc16;
  FrameHeader tmp = hdr; tmp.crc16 = 0;
  uint16_t crc = crc16_ccitt(reinterpret_cast<const uint8_t*>(&tmp), sizeof(FrameHeader), 0xFFFF);
  crc = crc16_ccitt(frame + sizeof(FrameHeader), hdr.payload_len, crc);
  if (crc != got_crc) { metrics_.rx_crc_fail++; return; }

  FrameLog::push('R', frame, len);

  if (hdr.flags & F_ACK) { if (ack_cb_) ack_cb_(hdr.msg_id); return; }

  const uint8_t* p = frame + sizeof(FrameHeader);
  std::vector<uint8_t> plain;
  if (hdr.flags & F_ENC) {
    std::vector<uint8_t> aad(reinterpret_cast<uint8_t*>(&tmp), reinterpret_cast<uint8_t*>(&tmp)+sizeof(FrameHeader));
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
