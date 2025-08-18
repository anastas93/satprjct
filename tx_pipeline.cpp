
#include "tx_pipeline.h"
#include "frame.h"
#include "config.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include "scrambler.h"
#include "fec.h"
#include "interleaver.h"
#include "tdd_scheduler.h"
#include <Arduino.h>
#include <string.h>
#include <array>

// Параметры профилей передачи
struct TxProfile { uint16_t payload; uint8_t fec; uint8_t inter; uint8_t repeat; };
static const TxProfile PROFILES[4] = {
  {200, 0, 1, 1}, // P0: лучший канал
  {160, 0, 1, 2}, // P1
  {120, 1, 2, 3}, // P2
  { 80, 1, 4, 4}  // P3: худший канал
};

static const float PER_THR[3]  = {0.1f, 0.2f, 0.3f};
static const float EBN0_THR[3] = {7.0f, 5.0f, 3.0f};

TxPipeline::TxPipeline(MessageBuffer& buf, Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m)
: buf_(buf), frag_(frag), enc_(enc), metrics_(m) {}

bool TxPipeline::interFrameGap() {
  unsigned long now = millis();
  if (now - last_tx_ms_ < cfg::INTER_FRAME_GAP_MS) return false;
  return true;
}

void TxPipeline::sendMessageFragments(const OutgoingMessage& m) {
  bool reqAck = ack_enabled_ || m.ack_required;
  bool willEnc = enc_enabled_ && enc_.isReady();
  // Ограничение полезной нагрузки согласно выбранному профилю
  const uint16_t base_payload_max =
      (payload_len_ < (cfg::LORA_MTU - FRAME_HEADER_SIZE)) ? payload_len_ : (cfg::LORA_MTU - FRAME_HEADER_SIZE);
  const uint16_t enc_overhead = willEnc ? (1 + cfg::ENC_TAG_LEN) : 0;
  const uint16_t eff_payload_max = (base_payload_max > enc_overhead) ? (uint16_t)(base_payload_max - enc_overhead) : 0;
  auto parts = frag_.split(m.id, m.data.data(), m.data.size(), reqAck, eff_payload_max);

  std::array<uint8_t, FRAME_HEADER_SIZE> aad{};
  std::vector<uint8_t> payload; payload.reserve(cfg::LORA_MTU);
  std::vector<uint8_t> frame; frame.reserve(cfg::LORA_MTU);

  for (auto& fr : parts) {
    FrameHeader hdr = fr.hdr;
    hdr.hdr_crc = 0; hdr.frame_crc = 0;
    hdr.encode(aad.data(), aad.size());
    bool ok = true;
    payload.clear();
    if (willEnc) ok = enc_.encrypt(fr.payload.data(), fr.payload.size(), aad.data(), aad.size(), payload);
    else payload.assign(fr.payload.begin(), fr.payload.end());
    if (!ok) { metrics_.enc_fail++; continue; }

    // Скремблирование полезной нагрузки для подавления длительных последовательностей
    lfsr_scramble(payload.data(), payload.size(), (uint16_t)hdr.msg_id);

    // Простейший FEC повторным кодом
    if (fec_enabled_) {
      std::vector<uint8_t> fec_buf;
      fec_encode_repeat(payload.data(), payload.size(), fec_buf);
      payload.swap(fec_buf);
    }

    // Байтовый интерливинг
    if (interleave_depth_ > 1) {
      std::vector<uint8_t> inter_buf;
      interleave_bytes(payload.data(), payload.size(), interleave_depth_, inter_buf);
      payload.swap(inter_buf);
    }

    FrameHeader final_hdr = fr.hdr;
    if (willEnc) final_hdr.flags |= F_ENC;
    final_hdr.payload_len = (uint16_t)payload.size();
    final_hdr.hdr_crc = 0; final_hdr.frame_crc = 0;
    uint8_t hdr_buf[FRAME_HEADER_SIZE];
    final_hdr.encode(hdr_buf, FRAME_HEADER_SIZE);
    uint16_t hcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE - 4, 0xFFFF);
    final_hdr.hdr_crc = hcrc;
    final_hdr.encode(hdr_buf, FRAME_HEADER_SIZE);
    uint16_t fcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE, 0xFFFF);
    fcrc = crc16_ccitt(payload.data(), payload.size(), fcrc);
    final_hdr.frame_crc = fcrc;
    final_hdr.encode(hdr_buf, FRAME_HEADER_SIZE);

    const size_t frame_len = FRAME_HEADER_SIZE + payload.size();
    frame.resize(frame_len);
    memcpy(frame.data(), hdr_buf, FRAME_HEADER_SIZE);
    memcpy(frame.data()+FRAME_HEADER_SIZE, payload.data(), payload.size());

    // Повторяем отправку кадра согласно профилю
    for (uint8_t r = 0; r < repeat_count_; ++r) {
      Radio_sendRaw(frame.data(), frame.size());
      FrameLog::push('T', frame.data(), frame.size());
      metrics_.tx_frames++; metrics_.tx_bytes += frame.size();
      last_tx_ms_ = millis();
      while (!interFrameGap()) { delay(1); }
    }
  }
  metrics_.tx_msgs++;
}

void TxPipeline::notifyAck(uint32_t highest, uint32_t bitmap) {
  if (!waiting_ack_) return;
  if (waiting_id_ <= highest) { ack_received_ = true; return; }
  uint32_t diff = waiting_id_ - highest - 1;
  if (diff < 32 && (bitmap & (1u << diff))) ack_received_ = true;
}

void TxPipeline::loop() {
  // Поддерживаем расписание и переключаем радио при необходимости
  tdd::maintain();
  updateProfile();
  if (waiting_ack_) {
    if (ack_received_) {
      unsigned long dt = millis() - ack_start_ms_;
      metrics_.ack_time_ms_avg = (metrics_.ack_time_ms_avg == 0) ? dt : (metrics_.ack_time_ms_avg*3 + dt)/4;
      metrics_.ack_seen++;
      buf_.markAcked(waiting_id_);
      // после успешного ACK возвращаем одно сообщение из архива
      buf_.restoreArchived();
      waiting_ack_ = false; waiting_id_ = 0; retries_left_ = 0; ack_received_ = false;
      return;
    }
    unsigned long now = millis();
    if ((now - last_tx_ms_) >= retry_ms_) {
      if (retries_left_ > 0) {
        OutgoingMessage m;
        if (buf_.peekNext(m) && m.id == waiting_id_) {
          sendMessageFragments(m);
          retries_left_--; metrics_.tx_retries++;
        } else {
          waiting_ack_ = false;
        }
      } else {
        // при провале всех попыток переносим сообщение в архив
        metrics_.ack_fail++;
        buf_.archive(waiting_id_);
        waiting_ack_ = false;
        waiting_id_ = 0;
      }
    }
    return;
  }

  // Если сейчас не окно передачи, выходим
  if (!tdd::isTxPhase()) return;
  if (!buf_.hasPending()) return;
  if (!interFrameGap()) return;
  OutgoingMessage m;
  if (!buf_.peekNext(m)) return;

  bool reqAck = ack_enabled_ || m.ack_required;
  sendMessageFragments(m);
  if (reqAck) { waiting_ack_ = true; waiting_id_ = m.id; retries_left_ = retry_count_; ack_start_ms_ = millis(); ack_received_ = false; }
  else { buf_.popFront(); }
}

// Определение профиля по метрикам PER и Eb/N0
void TxPipeline::updateProfile() {
  float per = metrics_.per_ema.value;
  float ebn0 = metrics_.ebn0_ema.value;
  uint8_t idx = 0;
  if (per > PER_THR[0] || ebn0 < EBN0_THR[0]) idx = 1;
  if (per > PER_THR[1] || ebn0 < EBN0_THR[1]) idx = 2;
  if (per > PER_THR[2] || ebn0 < EBN0_THR[2]) idx = 3;
  if (idx != profile_idx_) applyProfile(idx);
}

// Применение конкретного профиля P0..P3
void TxPipeline::applyProfile(uint8_t p) {
  if (p > 3) p = 3;
  profile_idx_ = p;
  const TxProfile& pr = PROFILES[p];
  payload_len_ = pr.payload;
  fec_mode_ = pr.fec;
  fec_enabled_ = (fec_mode_ != 0);
  interleave_depth_ = pr.inter;
  repeat_count_ = pr.repeat;
}
