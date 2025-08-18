
#pragma once
#include "message_buffer.h"
#include "fragmenter.h"
#include "encryptor.h"
#include "metrics.h"

class TxPipeline {
public:
  TxPipeline(MessageBuffer& buf, Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m);
  void loop();
  void enableAck(bool v) { ack_enabled_ = v; }
  void setRetry(uint8_t n, uint16_t ms) { retry_count_ = n; retry_ms_ = ms; }
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
  uint8_t  retry_count_ = cfg::SEND_RETRY_COUNT_DEFAULT;
  uint16_t retry_ms_    = cfg::SEND_RETRY_MS_DEFAULT;
  bool waiting_ack_ = false;
  uint32_t waiting_id_ = 0;
  uint8_t  retries_left_ = 0;
  unsigned long last_tx_ms_ = 0;
  unsigned long ack_start_ms_ = 0;
  bool ack_received_ = false;

  bool enc_enabled_ = cfg::ENCRYPTION_ENABLED_DEFAULT;
  bool fec_enabled_ = cfg::FEC_ENABLED_DEFAULT;
  uint8_t fec_mode_ = 0;                       // тип FEC: 0-выкл,1-повтор
  uint8_t interleave_depth_ = cfg::INTERLEAVER_DEPTH_DEFAULT;

  uint16_t payload_len_ = cfg::LORA_MTU - FRAME_HEADER_SIZE; // максимальная полезная нагрузка
  uint8_t repeat_count_ = 1;                  // повторение кадров
  uint8_t profile_idx_ = 0;                   // текущий профиль P0..P3
};
