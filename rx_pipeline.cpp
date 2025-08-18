
#include "rx_pipeline.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include "scrambler.h"
#include "fec.h"
#include "interleaver.h"
#include "ack_bitmap.h"
#include "tdd_scheduler.h"
#include <string.h>
#include <algorithm> // для std::erase_if
#include <stdlib.h>
#include "encryptor_ccm.h"

// Простая функция soft-combining для повторов байтов
static void softCombinePairs(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  if (len % 2 != 0) return;
  size_t out_len = len / 2;
  out.resize(out_len);
  for (size_t i = 0; i < out_len; ++i) {
    uint8_t a = in[2*i];
    uint8_t b = in[2*i + 1];
    out[i] = a & b; // берём биты, совпавшие в обеих копиях
  }
}

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
  // Отправляем ACK только в соответствующее окно
  if (!tdd::isAckPhase()) return;
  // обновляем наибольший подтверждённый кадр и bitmap для окна W=8
  if (msg_id > ack_highest_) {
    uint32_t shift = msg_id - ack_highest_;
    if (shift >= 8) ack_bitmap_ = 0;
    else ack_bitmap_ <<= shift;
    ack_highest_ = msg_id;
  }
  uint32_t off = ack_highest_ - msg_id;
  if (off > 0 && off <= 8) ack_bitmap_ |= (1u << (off - 1));

  // агрегируем ACK по времени
  if (millis() - last_ack_sent_ms_ < cfg::T_ACK_AGG) return;

  FrameHeader ack{};
  ack.ver = cfg::PIPE_VERSION;
  ack.flags = F_ACK;
  ack.msg_id = 0; // поле не используется
  ack.frag_idx = 0; ack.frag_cnt = 0; ack.payload_len = 8;
  ack.hdr_crc = 0; ack.frame_crc = 0;
  uint8_t buf[FRAME_HEADER_SIZE + 8];
  ack.encode(buf, FRAME_HEADER_SIZE);
  AckBitmap pl{ack_highest_, ack_bitmap_};
  ack_encode(pl, buf + FRAME_HEADER_SIZE);
  uint16_t hcrc = crc16_ccitt(buf, FRAME_HEADER_SIZE - 4, 0xFFFF);
  ack.hdr_crc = hcrc;
  ack.encode(buf, FRAME_HEADER_SIZE);
  uint16_t fcrc = crc16_ccitt(buf, FRAME_HEADER_SIZE, 0xFFFF);
  fcrc = crc16_ccitt(buf + FRAME_HEADER_SIZE, 8, fcrc);
  ack.frame_crc = fcrc;
  ack.encode(buf, FRAME_HEADER_SIZE);
  Radio_sendRaw(buf, FRAME_HEADER_SIZE + 8);
  // логируем отправленный ACK
  FrameLog::push('T', buf, FRAME_HEADER_SIZE + 8,
                 0, 0, 0, 0.0f, 0, 0, 0, 0);
  last_ack_sent_ms_ = millis();
  // После передачи возвращаемся в режим приёма
  tdd::maintain();
}

void RxPipeline::onReceive(const uint8_t* frame, size_t len) {
  // Держим радио в нужном состоянии
  tdd::maintain();
  if (!frame || len < FRAME_HEADER_SIZE*2) return;
  std::vector<uint8_t> hdr_buf;
  softCombinePairs(frame, FRAME_HEADER_SIZE*2, hdr_buf);
  FrameHeader hdr;
  if (!FrameHeader::decode(hdr, hdr_buf.data(), hdr_buf.size())) return;
  if (hdr.ver != cfg::PIPE_VERSION) return;
  if (len != FRAME_HEADER_SIZE*2 + hdr.payload_len) { metrics_.rx_drop_len_mismatch++; return; }

  // Проверяем CRC заголовка
  FrameHeader tmp = hdr; tmp.hdr_crc = 0; tmp.frame_crc = 0;
  tmp.encode(hdr_buf.data(), FRAME_HEADER_SIZE);
  uint16_t hcrc2 = crc16_ccitt(hdr_buf.data(), FRAME_HEADER_SIZE - 4, 0xFFFF);
  if (hcrc2 != hdr.hdr_crc) { metrics_.rx_crc_fail++; return; }

  // Проверяем общий CRC кадра
  tmp.hdr_crc = hdr.hdr_crc; tmp.frame_crc = 0;
  tmp.encode(hdr_buf.data(), FRAME_HEADER_SIZE);
  uint16_t fcrc2 = crc16_ccitt(hdr_buf.data(), FRAME_HEADER_SIZE, 0xFFFF);
  fcrc2 = crc16_ccitt(frame + FRAME_HEADER_SIZE*2, hdr.payload_len, fcrc2);
  if (fcrc2 != hdr.frame_crc) { metrics_.rx_crc_fail++; return; }

  float snr = 0.0f;
  Radio_getSNR(snr); // измеряем качество канала

  if (hdr.flags & F_ACK) {
    // фиксируем входящий ACK без дополнительной обработки
    FrameLog::push('R', frame, len, hdr.msg_id, fec_mode_, interleave_depth_,
                   snr, 0, 0, 0, 0);
    if (hdr.payload_len == 8) {
      AckBitmap a{}; ack_decode(a, frame + FRAME_HEADER_SIZE);
      if (ack_cb_) ack_cb_(a.highest, a.bitmap);
    }
    return;
  }

  const uint8_t* p = frame + FRAME_HEADER_SIZE*2;
  std::vector<uint8_t> data(p, p + hdr.payload_len);

  // Удаляем пилоты и обновляем фазу
  if (!data.empty()) {
    size_t pos = cfg::PILOT_INTERVAL_BYTES;
    while (pos + cfg::PILOT_LEN <= data.size()) {
      updatePhaseFromPilot(data.data() + pos, cfg::PILOT_LEN);
      data.erase(data.begin() + pos, data.begin() + pos + cfg::PILOT_LEN);
      pos += cfg::PILOT_INTERVAL_BYTES;
    }
  }

  if (interleave_depth_ > 1) {
    std::vector<uint8_t> tmp;
    deinterleave_bytes(data.data(), data.size(), interleave_depth_, tmp);
    data.swap(tmp);
  }

  int corrected = 0;
  if (fec_enabled_) {
    std::vector<uint8_t> tmp;
    if (fec_mode_ == FEC_RS_VIT) {
      if (!fec_decode_rs_viterbi(data.data(), data.size(), tmp, corrected)) {
        metrics_.rx_fec_fail++;
        // логируем неудачное декодирование
        FrameLog::push('R', frame, len, hdr.msg_id, fec_mode_, interleave_depth_,
                       snr, corrected, 0, 1, 0);
        return;
      }
      metrics_.rx_fec_corrected += corrected;
      data.swap(tmp);
    }
    // LDPC не реализован
  }

  lfsr_descramble(data.data(), data.size(), (uint16_t)hdr.msg_id);

  // фиксируем успешно принятый кадр
  FrameLog::push('R', frame, len, hdr.msg_id, fec_mode_, interleave_depth_,
                 snr, corrected, 0, 0, 0);
  std::vector<uint8_t> plain;
  if (hdr.flags & F_ENC) {
    uint8_t hb[FRAME_HEADER_SIZE];
    hdr.encode(hb, FRAME_HEADER_SIZE);
    std::vector<uint8_t> aad(hb, hb + FRAME_HEADER_SIZE);
    if (!enc_.decrypt(data.data(), data.size(), aad.data(), aad.size(), plain)) { metrics_.dec_fail_tag++; return; }
  } else {
    plain = std::move(data);
  }

  metrics_.rx_frames_ok++; metrics_.rx_bytes += len;

  if (!(hdr.flags & F_FRAG)) {
    // Проверяем служебное сообщение смены ключа
    if (plain.size() >= 7 && memcmp(plain.data(), "KEYCHG ", 7) == 0) {
      uint8_t kid = (uint8_t)strtoul((const char*)plain.data() + 7, nullptr, 10);
      if (auto ccm = dynamic_cast<CcmEncryptor*>(&enc_)) ccm->setActiveKid(kid);
      metrics_.rx_msgs_ok++;
      if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
      gc();
      return;
    }
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

// Простейшая обработка пилотной последовательности для обновления фазы
void RxPipeline::updatePhaseFromPilot(const uint8_t* pilot, size_t len) {
  int sum = 0;
  for (size_t i = 0; i < len; ++i) sum += (int8_t)pilot[i];
  // Экспоненциальное сглаживание оценки
  phase_est_ = 0.9f * phase_est_ + 0.1f * sum;
}
