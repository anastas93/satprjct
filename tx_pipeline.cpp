
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
#include <vector>
#include <stdio.h>

// Параметры профилей передачи
struct TxProfile { uint16_t payload; TxPipeline::FecMode fec; uint8_t inter; uint8_t repeat; };
static const TxProfile PROFILES[4] = {
  {200, TxPipeline::FEC_OFF,    1, 1}, // P0: лучший канал
  {160, TxPipeline::FEC_OFF,    1, 2}, // P1
  {120, TxPipeline::FEC_RS_VIT, 8, 3}, // P2
  { 80, TxPipeline::FEC_RS_VIT,16, 4}  // P3: худший канал
};

static const float PER_THR[3]  = {0.1f, 0.2f, 0.3f};
static const float EBN0_THR[3] = {7.0f, 5.0f, 3.0f};

TxPipeline::TxPipeline(MessageBuffer& buf, Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m)
: buf_(buf), frag_(frag), enc_(enc), metrics_(m) {}

// Помещает служебное сообщение смены ключа в очередь с требованием ACK
void TxPipeline::queueKeyChange(uint8_t kid) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "KEYCHG %u", kid);
  buf_.enqueue(reinterpret_cast<uint8_t*>(tmp), n, true);
}

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

    // FEC: RS + Viterbi
    if (fec_enabled_) {
      std::vector<uint8_t> fec_buf;
      if (fec_mode_ == FEC_RS_VIT) {
        fec_encode_rs_viterbi(payload.data(), payload.size(), fec_buf);
        payload.swap(fec_buf);
      }
      // режим LDPC пока не реализован
    }

    // Байтовый интерливинг
    if (interleave_depth_ > 1) {
      std::vector<uint8_t> inter_buf;
      interleave_bytes(payload.data(), payload.size(), interleave_depth_, inter_buf);
      payload.swap(inter_buf);
    }

    // Вставляем пилотные последовательности через заданный интервал
    if (pilot_interval_bytes_ > 0 && !payload.empty()) {
      size_t pos = pilot_interval_bytes_;
      while (pos < payload.size()) {
        payload.insert(payload.begin() + pos, cfg::PILOT_SEQ, cfg::PILOT_SEQ + cfg::PILOT_LEN);
        pos += pilot_interval_bytes_ + cfg::PILOT_LEN;
      }
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

    // Формируем окончательный кадр, при необходимости дублируя заголовок
    size_t frame_len = FRAME_HEADER_SIZE + payload.size();
    if (hdr_dup_enabled_) frame_len += FRAME_HEADER_SIZE;
    frame.resize(frame_len);
    memcpy(frame.data(), hdr_buf, FRAME_HEADER_SIZE);
    size_t off = FRAME_HEADER_SIZE;
    if (hdr_dup_enabled_) {
      memcpy(frame.data()+off, hdr_buf, FRAME_HEADER_SIZE);
      off += FRAME_HEADER_SIZE;
    }
    memcpy(frame.data()+off, payload.data(), payload.size());

    // Повторяем отправку кадра согласно профилю
    for (uint8_t r = 0; r < repeat_count_; ++r) {
      Radio_sendRaw(frame.data(), frame.size());
      // фиксируем факт передачи кадра и параметры профиля
      FrameLog::push('T', frame.data(), frame.size(),
                     final_hdr.msg_id, (uint8_t)fec_mode_, interleave_depth_,
                     0.0f, 0, 0, 0,
                     (uint16_t)metrics_.rtt_ema_ms.value);
      metrics_.tx_frames++; metrics_.tx_bytes += frame.size();
      last_tx_ms_ = millis();
      while (!interFrameGap()) { delay(1); }
    }
  }
  metrics_.tx_msgs++;
}

void TxPipeline::notifyAck(uint32_t highest, uint32_t bitmap) {
  // отмечаем подтверждённые сообщения в окне
  for (auto it = pending_.begin(); it != pending_.end(); ) {
    uint32_t id = it->msg.id;
    bool ok = false;
    if (id <= highest) ok = true;
    else {
      uint32_t diff = id - highest - 1;
      if (diff < window_size_ && (bitmap & (1u << diff))) ok = true;
    }
    if (ok) {
      unsigned long dt = millis() - it->start_ms;
      metrics_.ack_time_ms_avg = (metrics_.ack_time_ms_avg == 0) ? dt : (metrics_.ack_time_ms_avg*3 + dt)/4;
      metrics_.ack_seen++;
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
}

void TxPipeline::loop() {
  // Поддерживаем расписание и переключаем радио при необходимости
  tdd::maintain();
  updateProfile();
  // Проверяем таймеры повторов для уже отправленных сообщений
  unsigned long now = millis();
  for (auto it = pending_.begin(); it != pending_.end(); ) {
    if (now - it->last_tx_ms >= it->timeout_ms) {
      if (it->retries_left > 0) {
        sendMessageFragments(it->msg);
        it->retries_left--; metrics_.tx_retries++;
        it->last_tx_ms = now;
        it->timeout_ms *= 2; // экспоненциальный бэкофф
        ++it;
      } else {
        metrics_.ack_fail++;
        it = pending_.erase(it);
      }
    } else {
      ++it;
    }
  }

  // Если сейчас не окно передачи, выходим
  if (!tdd::isTxPhase()) return;

  // Заполняем окно новыми сообщениями
  while (pending_.size() < window_size_ && buf_.hasPending()) {
    if (!interFrameGap()) break;
    OutgoingMessage m;
    if (!buf_.peekNext(m)) break;
    buf_.popFront();
    bool reqAck = ack_enabled_ || m.ack_required;
    sendMessageFragments(m);
    if (reqAck) {
      Pending p{m, max_retries_, millis(), millis(), ack_timeout_};
      pending_.push_back(std::move(p));
    }
  }
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
