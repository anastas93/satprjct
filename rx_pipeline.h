
#pragma once
#include <vector>
#include <functional>
#include <map>
#include <deque>
#include <unordered_set>
#include <Arduino.h>
#include "encryptor.h"
#include "frame.h"
#include "metrics.h"
#include "config.h"

class RxPipeline {
public:
  using MsgCb = std::function<void(uint32_t, const uint8_t*, size_t)>;
  using AckCb = std::function<void(uint32_t, uint32_t)>; // highest, bitmap

  RxPipeline(IEncryptor& enc, PipelineMetrics& m);
  void onReceive(const uint8_t* frame, size_t len);
  void setMessageCallback(MsgCb cb) { cb_ = cb; }
  void setAckCallback(AckCb cb) { ack_cb_ = cb; }
  void setFecEnabled(bool v) { fec_enabled_ = v; }
  void setInterleaveDepth(uint8_t d) { interleave_depth_ = d; }
private:
  struct AsmState {
    uint32_t msg_id;
    uint16_t frag_cnt;
    std::vector< std::vector<uint8_t> > frags;
    unsigned long ts_ms;
    size_t bytes = 0;
  };
  void gc();
  bool isDup(uint32_t msg_id);
  void sendAck(uint32_t msg_id);
  void updatePhaseFromPilot(const uint8_t* pilot, size_t len); // обновление оценки фазы

  IEncryptor& enc_;
  PipelineMetrics& metrics_;
  MsgCb cb_ = nullptr;
  AckCb ack_cb_ = nullptr;
  uint32_t ack_highest_ = 0;       // наибольший подтверждённый кадр
  uint32_t ack_bitmap_ = 0;        // bitmap последних W кадров
  unsigned long last_ack_sent_ms_ = 0; // время последней отправки ACK
  std::map<uint32_t, AsmState> assemblers_;
  std::deque<uint32_t> dup_window_;
  std::unordered_set<uint32_t> dup_set_;
  size_t reasm_bytes_ = 0;
  bool fec_enabled_ = cfg::FEC_ENABLED_DEFAULT;
  uint8_t interleave_depth_ = cfg::INTERLEAVER_DEPTH_DEFAULT;
  float phase_est_ = 0.0f; // текущая оценка фазы
};

void Radio_onReceive(const uint8_t* data, size_t len);
