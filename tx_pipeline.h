
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
  void setFecEnabled(bool v) { fec_enabled_ = v; fec_mode_ = v ? 1 : 0; }
  void setInterleaveDepth(uint8_t d) { interleave_depth_ = d; }
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
  // окно нен подтверждённых сообщений
  static constexpr size_t WINDOW_SIZE = 8;
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
  uint8_t fec_mode_ = 0;                       // тип FEC: 0-выкл,1-повтор
  uint8_t interleave_depth_ = cfg::INTERLEAVER_DEPTH_DEFAULT;

  uint16_t payload_len_ = cfg::LORA_MTU - FRAME_HEADER_SIZE; // максимальная полезная нагрузка
  uint8_t repeat_count_ = 1;                  // повторение кадров
  uint8_t profile_idx_ = 0;                   // текущий профиль P0..P3
};
