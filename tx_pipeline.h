
#pragma once
#include "message_cache.h"
#include "packet_formatter.h"
#include "radio_interface.h"
#include "metrics.h"
#include "ack_sender.h"
#include <deque>
#include <stddef.h>

class TxPipeline {
public:
  TxPipeline(MessageCache& cache, PacketFormatter& fmt, IRadioTransport& radio, PipelineMetrics& m);
  void loop();
  void enableAck(bool v) { ack_enabled_ = v; }
  // настройка параметров повторов
  void setRetry(uint8_t n, uint16_t ms) { max_retries_ = n; ack_timeout_ = ms; }
  bool ackEnabled() const { return ack_enabled_; }
  void notifyAck(uint32_t highest, uint32_t bitmap);
  void setEncEnabled(bool v) { formatter_.setEncEnabled(v); }
  void setFecMode(PacketFormatter::FecMode m) { formatter_.setFecMode(m); }
  void setInterleaveDepth(uint8_t d) { formatter_.setInterleaveDepth(d); }
  // Установка максимального размера полезной нагрузки в байтах
  void setPayloadLen(uint16_t len) { formatter_.setPayloadLen(len); }
  // Установка интервала пилотных вставок (0 = выключить)
  void setPilotInterval(uint16_t b) { formatter_.setPilotInterval(b); }
  // Управление дублированием заголовка
  void setHeaderDup(bool v) { formatter_.setHeaderDup(v); hdr_dup_enabled_ = v; }
  // Установка размера окна SR-ARQ
  void setWindowSize(uint8_t w) { window_size_ = w; }
  // Установка предела фрагментов в серии перед ожиданием ACK.
  // n — допустимое число подряд отправленных фрагментов (1..32)
  void setBurstFrags(uint8_t n) { burst_limit_ = n; }
  // Поместить сообщение KEYCHG <kid> в очередь с требованием ACK
  void queueKeyChange(uint8_t kid);
  // Поместить сообщение KEYACK <kid> в очередь без требования ACK
  void queueKeyAck(uint8_t kid);
  // Ручная установка профиля передачи P0..P3
  void setProfile(uint8_t p);
  // Отправка ACK из TX-пайплайна (зеркальная роль)
  bool sendAck(uint32_t highest, uint32_t bitmap);
private:
  size_t sendMessageFragments(const OutgoingMessage& m);
  bool interFrameGap();
  void controlProfile();      // контроллер изменения профиля по метрикам
  void applyProfile(uint8_t p);

  MessageCache& cache_;
  PacketFormatter& formatter_;
  IRadioTransport& radio_;
  PipelineMetrics& metrics_;

  bool ack_enabled_ = cfg::ACK_ENABLED_DEFAULT;
  // окно неподтверждённых кадров SR-ARQ
  uint8_t window_size_ = cfg::SR_WINDOW_DEFAULT;
  struct Pending {
    OutgoingMessage msg;          // сохранённое сообщение
    uint8_t retries_left;         // сколько повторов осталось
    unsigned long start_ms;       // время первой отправки
    unsigned long last_tx_ms;     // время последней передачи
    uint16_t timeout_ms;          // индивидуальный таймер кадра
    uint8_t  backoff_stage;       // ступень бэкоффа
  };
  // очередь кадров, ожидающих подтверждения
  std::deque<Pending> pending_;
  uint16_t ack_timeout_ = cfg::ACK_TIMEOUT;
  uint8_t  max_retries_ = cfg::MAX_RETRIES;
  unsigned long last_tx_ms_ = 0;

  // контроль серии фрагментов и ожидание подтверждения
  uint8_t burst_limit_ = cfg::SR_WINDOW_DEFAULT; // размер серии
  size_t  frags_in_burst_ = 0;                   // отправлено в текущей серии
  bool    waiting_ack_ = false;                  // флаг ожидания ACK
  unsigned long burst_wait_ms_ = 0;              // начало ожидания

  uint8_t profile_idx_ = 0;                   // текущий профиль P0..P3
  AckSender ack_sender_;
  bool hdr_dup_enabled_ = cfg::HEADER_DUP_DEFAULT;
};
