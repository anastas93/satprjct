
#include "tx_pipeline.h"
#include "frame.h"
#include "config.h"
#include "frame_log.h"
#include "tdd_scheduler.h"
#include <Arduino.h>
#include <string.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h> // для rand()

// Параметры профилей передачи
struct TxProfile { uint16_t payload; PacketFormatter::FecMode fec; uint8_t inter; uint8_t repeat; };
// Профили ограничены диапазоном полезной нагрузки 64–160 байт
static const TxProfile PROFILES[4] = {
  {160, PacketFormatter::FEC_OFF,    1, 1}, // P0: лучший канал
  {128, PacketFormatter::FEC_OFF,    1, 2}, // P1
  { 96, PacketFormatter::FEC_RS_VIT, 8, 3}, // P2
  { 64, PacketFormatter::FEC_RS_VIT,16, 4}  // P3: худший канал
};

// Пороговые значения приходят из глобальных переменных
extern bool g_autoRate;
extern float g_perHigh, g_perLow, g_ebn0High, g_ebn0Low;

// Применяет случайный джиттер ±10–20% к базовому таймауту
static uint16_t addJitter(uint16_t base) {
  int sign = (rand() & 1) ? 1 : -1;
  int pct = 10 + (rand() % 11); // 10..20%
  uint16_t delta = (base * pct) / 100;
  uint16_t res = (sign > 0) ? base + delta : (base > delta ? base - delta : 1);
  return res;
}

TxPipeline::TxPipeline(MessageCache& cache, PacketFormatter& fmt, IRadioTransport& radio, PipelineMetrics& m)
: cache_(cache), formatter_(fmt), radio_(radio), metrics_(m) {}

// Помещает служебное сообщение смены ключа в очередь с требованием ACK
void TxPipeline::queueKeyChange(uint8_t kid) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "KEYCHG %u", kid);
  cache_.enqueue(reinterpret_cast<uint8_t*>(tmp), n, true);
}

// Помещает служебное сообщение подтверждения смены ключа
void TxPipeline::queueKeyAck(uint8_t kid) {
  char tmp[16];
  int n = snprintf(tmp, sizeof(tmp), "KEYACK %u", kid);
  cache_.enqueue(reinterpret_cast<uint8_t*>(tmp), n, false);
}

// Публичная обёртка для установки профиля вручную
void TxPipeline::setProfile(uint8_t p) {
  applyProfile(p);
}

bool TxPipeline::interFrameGap() {
  unsigned long now = millis();
  if (now - last_tx_ms_ < cfg::INTER_FRAME_GAP_MS) return false;
  return true;
}

size_t TxPipeline::sendMessageFragments(const OutgoingMessage& m) { // QOS: Средний Подготовка и отправка кадров
  bool reqAck = ack_enabled_ || m.ack_required;
  std::vector<PreparedFrame> frames;
  formatter_.prepareFrames(m, reqAck, frames);

  size_t sent = 0;
  for (auto& pf : frames) {
    // Передача кадра через абстрактный радиоинтерфейс
    radio_.transmit(pf.data.data(), pf.data.size(), m.qos);
    FrameLog::push('T', pf.data.data(), pf.data.size(),
                   pf.hdr.msg_id, (uint8_t)formatter_.fecMode(), formatter_.interleaveDepth(),
                   0.0f, 0.0f, 0.0f,
                   0, 0, 0,
                   (uint16_t)metrics_.rtt_window_ms.avg(),
                   pf.hdr.payload_len);
    metrics_.tx_frames++; metrics_.tx_bytes += pf.data.size();
    last_tx_ms_ = millis();
    sent++;
    while (!interFrameGap()) { delay(1); }
  }
  metrics_.tx_msgs++;
  return sent;
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
      cache_.markAcked(id);
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
  // выясняем количество свободных слотов окна SR-ARQ и заполняем их из архива
  size_t free_slots = (window_size_ > pending_.size()) ? (window_size_ - pending_.size()) : 0;
  if (free_slots) {
    cache_.restoreArchived(free_slots);
  }
  // Сброс ожидания после прихода ACK
  if (waiting_ack_) {
    waiting_ack_ = false; frags_in_burst_ = 0; burst_wait_ms_ = 0;
  }
}

void TxPipeline::loop() {
  // Поддерживаем расписание и переключаем радио при необходимости
  tdd::maintain();
  controlProfile();      // адаптация параметров передачи
  // Проверяем таймеры повторов для уже отправленных кадров
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
        cache_.archive(it->msg.id);
        metrics_.ack_fail++;
        it = pending_.erase(it);
      }
    } else {
      ++it;
    }
  }

  // Если сейчас не окно передачи, выходим
  if (!tdd::isTxPhase()) return;

  // При активном ACK ждём подтверждение серии
  if (ack_enabled_ && waiting_ack_) {
    if (millis() - burst_wait_ms_ < ack_timeout_) return; // ещё ждём
    waiting_ack_ = false; frags_in_burst_ = 0;            // таймаут истёк
  }

  // Заполняем окно новыми сообщениями
  while (pending_.size() < window_size_ && cache_.hasPending()) {
    if (!interFrameGap()) break;
    OutgoingMessage m;
    if (!cache_.peek(m)) break;
    bool reqAck = ack_enabled_ || m.ack_required;
    size_t sent = sendMessageFragments(m);
    frags_in_burst_ += sent;
    if (reqAck) {
      Pending p{m, max_retries_, millis(), millis(), addJitter(ack_timeout_), 0}; // стартовый таймаут с джиттером
      pending_.push_back(std::move(p));
    } else {
      // без требования ACK удаляем сразу
      cache_.markAcked(m.id);
    }
    if (ack_enabled_ && frags_in_burst_ >= burst_limit_) {
      waiting_ack_ = true; burst_wait_ms_ = millis();
      break; // открываем окно ожидания
    }
  }
}

// Определение профиля по метрикам PER и Eb/N0
// Контроллер профиля передачи на основе метрик
void TxPipeline::controlProfile() {
  if (!g_autoRate) return; // автонастройка выключена
  // средние значения PER и Eb/N0 из скользящих окон
  float per = metrics_.per_window.avg();
  float ebn0 = metrics_.ebn0_window.avg();
  // ухудшение или улучшение профиля в зависимости от порогов
  if (per > g_perHigh || ebn0 < g_ebn0Low) {
    if (profile_idx_ < 3) applyProfile(profile_idx_ + 1);
  } else if (per < g_perLow && ebn0 > g_ebn0High) {
    if (profile_idx_ > 0) applyProfile(profile_idx_ - 1);
  }
}

// Применение конкретного профиля P0..P3
void TxPipeline::applyProfile(uint8_t p) {
  if (p > 3) p = 3;
  profile_idx_ = p;
  const TxProfile& pr = PROFILES[p];
  // Ограничиваем полезную нагрузку 64–160 байт
  uint16_t payload = pr.payload;
  if (payload < 64) payload = 64;
  else if (payload > 160) payload = 160;
  formatter_.setPayloadLen(payload);
  formatter_.setFecMode(pr.fec);
  formatter_.setInterleaveDepth(pr.inter);
  formatter_.setRepeatCount(pr.repeat);
}
