
#pragma once
#include "message_buffer.h"
#include "fragmenter.h"
#include "encryptor.h"
#include "metrics.h"
#include <deque>

class TxPipeline {
public:
  TxPipeline(MessageBuffer& buf, Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m);
  void loop();
  void enableAck(bool v) { ack_enabled_ = v; }
  // настройка параметров повторов
  void setRetry(uint8_t n, uint16_t ms) { max_retries_ = n; ack_timeout_ = ms; }
  bool ackEnabled() const { return ack_enabled_; }
  void notifyAck(uint32_t highest, uint32_t bitmap);
  void setEncEnabled(bool v) { enc_enabled_ = v; }
  enum FecMode : uint8_t { FEC_OFF=0, FEC_RS_VIT=1, FEC_LDPC=2 };
  void setFecMode(FecMode m) { fec_mode_ = m; fec_enabled_ = (m != FEC_OFF); }
  void setInterleaveDepth(uint8_t d) { interleave_depth_ = d; }
  // Установка максимального размера полезной нагрузки в байтах
  void setPayloadLen(uint16_t len) { payload_len_ = len; }
  // Установка интервала пилотных вставок (0 = выключить)
  void setPilotInterval(uint16_t b) { pilot_interval_bytes_ = b; }
  // Управление дублированием заголовка
  void setHeaderDup(bool v) { hdr_dup_enabled_ = v; }
  // Установка размера окна SR-ARQ
  void setWindowSize(uint8_t w) { window_size_ = w; }
  // Поместить сообщение KEYCHG <kid> в очередь с требованием ACK
  void queueKeyChange(uint8_t kid);
private:
  void sendMessageFragments(const OutgoingMessage& m);
  bool interFrameGap();
  void updateProfile();       // обновление профиля по метрикам
  void applyProfile(uint8_t p);

  MessageBuffer& buf_;
  Fragmenter& frag_;
  IEncryptor& enc_;
  PipelineMetrics& metrics_;

  bool ack_enabled_ = cfg::ACK_ENABLED_DEFAULT;
  // интервал вставки пилотов в байтах
  uint16_t pilot_interval_bytes_ = cfg::PILOT_INTERVAL_BYTES_DEFAULT;
  // флаг дублирования заголовка
  bool hdr_dup_enabled_ = cfg::HEADER_DUP_DEFAULT;
  // окно неподтверждённых сообщений
  uint8_t window_size_ = cfg::SR_WINDOW_DEFAULT;
  struct Pending {
    OutgoingMessage msg;          // сохранённое сообщение
    uint8_t retries_left;         // сколько повторов осталось
    unsigned long start_ms;       // время первой отправки
    unsigned long last_tx_ms;     // время последней передачи
    uint16_t timeout_ms;          // текущий таймаут ожидания
  };
  std::deque<Pending> pending_;
  uint16_t ack_timeout_ = cfg::ACK_TIMEOUT;
  uint8_t  max_retries_ = cfg::MAX_RETRIES;
  unsigned long last_tx_ms_ = 0;

  bool enc_enabled_ = cfg::ENCRYPTION_ENABLED_DEFAULT;
  bool fec_enabled_ = cfg::FEC_ENABLED_DEFAULT;
  FecMode fec_mode_ = (FecMode)cfg::FEC_MODE_DEFAULT; // текущий режим FEC
  uint8_t interleave_depth_ = cfg::INTERLEAVER_DEPTH_DEFAULT;

  uint16_t payload_len_ = cfg::LORA_MTU - FRAME_HEADER_SIZE; // максимальная полезная нагрузка
  uint8_t repeat_count_ = 1;                  // повторение кадров
  uint8_t profile_idx_ = 0;                   // текущий профиль P0..P3
};
