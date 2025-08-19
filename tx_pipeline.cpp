
#include "tx_pipeline.h"
#include "frame.h"
#include "config.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include "libs/ccsds_link.h"
#include "tdd_scheduler.h"
#include <Arduino.h>
#include <string.h>
#include <array>
#include <vector>
#include <stdio.h>
#include <stdlib.h> // для rand()

// Параметры профилей передачи
struct TxProfile { uint16_t payload; TxPipeline::FecMode fec; uint8_t inter; uint8_t repeat; };
// Профили ограничены диапазоном полезной нагрузки 64–160 байт
static const TxProfile PROFILES[4] = {
  {160, TxPipeline::FEC_OFF,    1, 1}, // P0: лучший канал
  {128, TxPipeline::FEC_OFF,    1, 2}, // P1
  { 96, TxPipeline::FEC_RS_VIT, 8, 3}, // P2
  { 64, TxPipeline::FEC_RS_VIT,16, 4}  // P3: худший канал
};

static const float PER_THR[3]  = {0.1f, 0.2f, 0.3f};
static const float EBN0_THR[3] = {7.0f, 5.0f, 3.0f};

// Применяет случайный джиттер ±10–20% к базовому таймауту
static uint16_t addJitter(uint16_t base) {
  int sign = (rand() & 1) ? 1 : -1;
  int pct = 10 + (rand() % 11); // 10..20%
  uint16_t delta = (base * pct) / 100;
  uint16_t res = (sign > 0) ? base + delta : (base > delta ? base - delta : 1);
  return res;
}

TxPipeline::TxPipeline(MessageBuffer& buf, Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m)
: buf_(buf), frag_(frag), enc_(enc), metrics_(m) {}

// Помещает служебное сообщение смены ключа в очередь с требованием ACK
void TxPipeline::queueKeyChange(uint8_t kid) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "KEYCHG %u", kid);
  buf_.enqueue(reinterpret_cast<uint8_t*>(tmp), n, true);
}

// Помещает служебное сообщение подтверждения смены ключа
void TxPipeline::queueKeyAck(uint8_t kid) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "KEYACK %u", kid);
  buf_.enqueue(reinterpret_cast<uint8_t*>(tmp), n, false);
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

    // Комплексная обработка CCSDS: рандомизация, FEC и интерливинг
    ccsds::Params cp;
    cp.fec = fec_enabled_ ? (ccsds::FecMode)fec_mode_ : ccsds::FEC_OFF;
    cp.interleave = interleave_depth_;
    cp.scramble = true;
    std::vector<uint8_t> ccsds_buf;
    ccsds::encode(payload.data(), payload.size(), hdr.msg_id, cp, ccsds_buf);
    payload.swap(ccsds_buf);

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
  size_t before = pending_.size();
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
  // восстанавливаем из архива столько сообщений, сколько освободилось слотов
  size_t freed = before - pending_.size();
  for (size_t i = 0; i < freed; ++i) {
    if (!buf_.restoreArchived()) break;
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
        float mult = 1.0f;
        if (it->backoff_stage == 0) mult = 1.5f;
        else mult = 2.0f; // далее остаётся на 2.0
        if (it->backoff_stage < 2) it->backoff_stage++;
        uint32_t next = (uint32_t)(ack_timeout_ * mult);
        if (next > cfg::ACK_TIMEOUT_LIMIT) next = cfg::ACK_TIMEOUT_LIMIT;
        it->timeout_ms = addJitter((uint16_t)next);
        ++it;
      } else {
        // при полном исчерпании повторов переносим сообщение в архив
        buf_.archive(it->msg.id);
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
      Pending p{m, max_retries_, millis(), millis(), addJitter(ack_timeout_), 0}; // стартовый таймаут с джиттером
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
  // Ограничиваем полезную нагрузку 64–160 байт
  payload_len_ = pr.payload;
  if (payload_len_ < 64) payload_len_ = 64;
  else if (payload_len_ > 160) payload_len_ = 160;
  fec_mode_ = pr.fec;
  fec_enabled_ = (fec_mode_ != 0);
  interleave_depth_ = pr.inter;
  repeat_count_ = pr.repeat;
}
