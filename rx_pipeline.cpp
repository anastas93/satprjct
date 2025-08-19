
#include "rx_pipeline.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include "libs/ccsds_link/ccsds_link.h" // заголовок библиотеки CCSDS
#include "ack_bitmap.h"
#include "tdd_scheduler.h"
#include <string.h>
#include <vector>
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
: enc_(enc), metrics_(m) {
  scheduleNextAck();
}

void RxPipeline::setAckAgg(uint16_t ms) {
  ack_agg_ms_ = ms;
  scheduleNextAck();
}

void RxPipeline::scheduleNextAck() {
  if (ack_agg_ms_ == 0) { ack_agg_jitter_ms_ = 0; return; }
  // случайный интервал 50–100 мс (или ms–2*ms при настройке)
  ack_agg_jitter_ms_ = ack_agg_ms_ + (rand() % (ack_agg_ms_ + 1));
}

// Устанавливаем ожидание подтверждения смены ключа
void RxPipeline::expectKeyAck(uint8_t kid) {
  pending_kid_ = kid;
  expect_keyack_ = true;
}

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
  // обновляем наибольший подтверждённый кадр и bitmap для окна W
  if (msg_id > ack_highest_) {
    uint32_t shift = msg_id - ack_highest_;
    if (shift >= window_size_) ack_bitmap_ = 0;
    else ack_bitmap_ <<= shift;
    ack_highest_ = msg_id;
  }
  uint32_t off = ack_highest_ - msg_id;
  if (off > 0 && off <= window_size_) ack_bitmap_ |= (1u << (off - 1));

  // отправляем пакет сразу при заполнении bitmap или по таймеру 50–100 мс
  uint32_t full_mask = (window_size_ >= 32) ? 0xFFFFFFFF : ((1u << window_size_) - 1);
  bool bitmap_full = (ack_bitmap_ & full_mask) == full_mask;
  if (!bitmap_full && ack_agg_ms_ > 0 && (millis() - last_ack_sent_ms_ < ack_agg_jitter_ms_)) return;

  FrameHeader ack{};
  ack.ver = cfg::PIPE_VERSION;
  ack.flags = F_ACK;
  ack.msg_id = ack_highest_;      // передаём наибольший подтверждённый ID
  ack.frag_idx = 0; ack.frag_cnt = 0; ack.payload_len = 8;
  ack.ack_mask = ack_bitmap_;     // переносим bitmap в заголовок
  ack.hdr_crc = 0; ack.frame_crc = 0;
  uint8_t hdr_buf[FRAME_HEADER_SIZE];
  ack.encode(hdr_buf, FRAME_HEADER_SIZE);
  AckBitmap pl{ack_highest_, ack_bitmap_};
  uint8_t payload[8];
  ack_encode(pl, payload);
  uint16_t hcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE - 4, 0xFFFF);
  ack.hdr_crc = hcrc;
  ack.encode(hdr_buf, FRAME_HEADER_SIZE);
  uint16_t fcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE, 0xFFFF);
  fcrc = crc16_ccitt(payload, 8, fcrc);
  ack.frame_crc = fcrc;
  ack.encode(hdr_buf, FRAME_HEADER_SIZE);

  size_t frame_len = FRAME_HEADER_SIZE + 8;
  if (hdr_dup_enabled_) frame_len += FRAME_HEADER_SIZE;
  std::vector<uint8_t> frame(frame_len);
  memcpy(frame.data(), hdr_buf, FRAME_HEADER_SIZE);
  size_t off2 = FRAME_HEADER_SIZE;
  if (hdr_dup_enabled_) {
    memcpy(frame.data()+off2, hdr_buf, FRAME_HEADER_SIZE);
    off2 += FRAME_HEADER_SIZE;
  }
  memcpy(frame.data()+off2, payload, 8);

  Radio_sendRaw(frame.data(), frame.size());
  // логируем отправленный ACK
  FrameLog::push('T', frame.data(), frame.size(),
                 0, 0, 0,
                 0.0f, 0.0f, 0.0f,
                 0, 0, 0,
                 0,
                 8);
  last_ack_sent_ms_ = millis();
  scheduleNextAck();
  // После передачи возвращаемся в режим приёма
  tdd::maintain();
}

void RxPipeline::onReceive(const uint8_t* frame, size_t len) {
  // Держим радио в нужном состоянии
  tdd::maintain();
  size_t hdr_len = hdr_dup_enabled_ ? FRAME_HEADER_SIZE*2 : FRAME_HEADER_SIZE;
  if (!frame || len < hdr_len) return;
  std::vector<uint8_t> hdr_buf;
  if (hdr_dup_enabled_) softCombinePairs(frame, FRAME_HEADER_SIZE*2, hdr_buf);
  else hdr_buf.assign(frame, frame + FRAME_HEADER_SIZE);
  FrameHeader hdr;
  if (!FrameHeader::decode(hdr, hdr_buf.data(), hdr_buf.size())) return;
  if (hdr.ver != cfg::PIPE_VERSION) return;
  if (len != hdr_len + hdr.payload_len) { metrics_.rx_drop_len_mismatch++; return; }

  // Проверяем CRC заголовка
  FrameHeader tmp = hdr; tmp.hdr_crc = 0; tmp.frame_crc = 0;
  tmp.encode(hdr_buf.data(), FRAME_HEADER_SIZE);
  uint16_t hcrc2 = crc16_ccitt(hdr_buf.data(), FRAME_HEADER_SIZE - 4, 0xFFFF);
  if (hcrc2 != hdr.hdr_crc) { metrics_.rx_crc_fail++; return; }

  // Проверяем общий CRC кадра
  tmp.hdr_crc = hdr.hdr_crc; tmp.frame_crc = 0;
  tmp.encode(hdr_buf.data(), FRAME_HEADER_SIZE);
  uint16_t fcrc2 = crc16_ccitt(hdr_buf.data(), FRAME_HEADER_SIZE, 0xFFFF);
  fcrc2 = crc16_ccitt(frame + hdr_len, hdr.payload_len, fcrc2);
  if (fcrc2 != hdr.frame_crc) { metrics_.rx_crc_fail++; return; }

  float snr = 0.0f;
  Radio_getSNR(snr); // измеряем отношение сигнал/шум
  float ebn0 = 0.0f;
  Radio_getEbN0(ebn0); // измеряем Eb/N0
  float rssi = 0.0f;
  Radio_getRSSI(rssi); // измеряем уровень RSSI

  if (hdr.flags & F_ACK) {
    // фиксируем входящий ACK
    FrameLog::push('R', frame, len, hdr.msg_id, fec_mode_, interleave_depth_,
                   snr, ebn0, rssi,
                   0, 0, 0,
                   0,
                   hdr.payload_len);
    uint32_t hi = hdr.msg_id;
    uint32_t bm = hdr.ack_mask;
    if (hdr.payload_len == 8) {
      const uint8_t* pl = frame + hdr_len;
      AckBitmap a{}; ack_decode(a, pl);
      hi = a.highest;
      bm = a.bitmap;
    }
    if (ack_cb_) ack_cb_(hi, bm);
    return;
  }

  const uint8_t* p = frame + hdr_len;
  std::vector<uint8_t> data(p, p + hdr.payload_len);

  // Удаляем пилоты и обновляем фазу
  if (pilot_interval_bytes_ > 0 && !data.empty()) {
    size_t pos = pilot_interval_bytes_;
    while (pos + cfg::PILOT_LEN <= data.size()) {
      updatePhaseFromPilot(data.data() + pos, cfg::PILOT_LEN);
      data.erase(data.begin() + pos, data.begin() + pos + cfg::PILOT_LEN);
      pos += pilot_interval_bytes_;
    }
  }

  int corrected = 0;
  ccsds::Params cp;
  cp.fec = fec_enabled_ ? (ccsds::FecMode)fec_mode_ : ccsds::FEC_OFF;
  cp.interleave = interleave_depth_;
  cp.scramble = true;
  std::vector<uint8_t> ccsds_buf;
  // Обратная цепочка CCSDS с проверкой ASM
  if (!ccsds::decode(data.data(), data.size(), hdr.msg_id, cp, ccsds_buf, corrected)) {
    metrics_.rx_fec_fail++;
    FrameLog::push('R', frame, len, hdr.msg_id, fec_mode_, interleave_depth_,
                   snr, ebn0, rssi,
                   corrected, 0, 1,
                   0,
                   hdr.payload_len);
    return;
  }
  metrics_.rx_fec_corrected += corrected;
  data.swap(ccsds_buf);

  // фиксируем успешно принятый кадр
  FrameLog::push('R', frame, len, hdr.msg_id, fec_mode_, interleave_depth_,
                 snr, ebn0, rssi,
                 corrected, 0, 0,
                 0,
                 hdr.payload_len);
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
    // Проверяем служебные сообщения KEYCHG/KEYACK
    if (plain.size() >= 7 && memcmp(plain.data(), "KEYCHG ", 7) == 0) {
      uint8_t kid = (uint8_t)strtoul((const char*)plain.data() + 7, nullptr, 10);
      // Запоминаем ожидаемый KID и ждём финального подтверждения
      pending_kid_ = kid; expect_final_ack_ = true;
      if (keyack_cb_) keyack_cb_(kid); // отправляем ответ KEYACK
      metrics_.rx_msgs_ok++;
      if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
      gc();
      return;
    }
    if (plain.size() >= 7 && memcmp(plain.data(), "KEYACK ", 7) == 0) {
      uint8_t kid = (uint8_t)strtoul((const char*)plain.data() + 7, nullptr, 10);
      if (expect_keyack_ && kid == pending_kid_) {
        // Получили подтверждение на наш KEYCHG
        enc_.setActiveKid(kid);
        expect_keyack_ = false; pending_kid_ = 0;
        if (keyack_cb_) keyack_cb_(kid); // уведомляем вторую сторону
      } else if (expect_final_ack_ && kid == pending_kid_) {
        // Завершающее подтверждение от инициатора
        enc_.setActiveKid(kid);
        expect_final_ack_ = false; pending_kid_ = 0;
      }
      metrics_.rx_msgs_ok++;
      if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
      gc();
      return;
    }
    if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
    if (isDup(hdr.msg_id)) { metrics_.rx_dup_msgs++; gc(); return; }
    dup_window_.push_back(hdr.msg_id);
    dup_set_.insert(hdr.msg_id);
    if (expected_id_ == 0) expected_id_ = hdr.msg_id;
    if (hdr.msg_id == expected_id_) {
      if (cb_) cb_(hdr.msg_id, plain.data(), plain.size());
      metrics_.rx_msgs_ok++;
      expected_id_++;
      // выдаём кадры из буфера по порядку
      while (true) {
        auto it = ooo_buf_.find(expected_id_);
        if (it == ooo_buf_.end()) break;
        if (cb_) cb_(expected_id_, it->second.data(), it->second.size());
        metrics_.rx_msgs_ok++;
        ooo_buf_.erase(it);
        expected_id_++;
      }
    } else if (hdr.msg_id > expected_id_) {
      ooo_buf_[hdr.msg_id] = plain;
    }
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
    if (hdr.flags & F_ACK_REQ) sendAck(hdr.msg_id);
    if (isDup(hdr.msg_id)) {
      metrics_.rx_dup_msgs++;
    } else {
      dup_window_.push_back(hdr.msg_id);
      dup_set_.insert(hdr.msg_id);
      if (expected_id_ == 0) expected_id_ = hdr.msg_id;
      if (hdr.msg_id == expected_id_) {
        if (cb_) cb_(hdr.msg_id, full.data(), full.size());
        metrics_.rx_msgs_ok++;
        expected_id_++;
        while (true) {
          auto it2 = ooo_buf_.find(expected_id_);
          if (it2 == ooo_buf_.end()) break;
          if (cb_) cb_(expected_id_, it2->second.data(), it2->second.size());
          metrics_.rx_msgs_ok++;
          ooo_buf_.erase(it2);
          expected_id_++;
        }
      } else if (hdr.msg_id > expected_id_) {
        ooo_buf_[hdr.msg_id] = std::move(full);
      }
    }
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
