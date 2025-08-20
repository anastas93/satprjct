#pragma once
#include <vector>
#include <array>
#include "message_buffer.h"
#include "fragmenter.h"
#include "encryptor.h"
#include "metrics.h"
#include "frame.h"

struct PreparedFrame {
  FrameHeader hdr;             // финальный заголовок
  std::vector<uint8_t> data;   // полный кадр
};

// Класс подготовки кадров: фрагментация, шифрование, скремблирование
class PacketFormatter {
public:
  enum FecMode : uint8_t { FEC_OFF=0, FEC_RS_VIT=1, FEC_LDPC=2 };

  PacketFormatter(Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m);

  void setEncEnabled(bool v) { enc_enabled_ = v; }
  void setFecMode(FecMode m) { fec_mode_ = m; fec_enabled_ = (m != FEC_OFF); }
  void setInterleaveDepth(uint8_t d) { interleave_depth_ = d; }
  void setPayloadLen(uint16_t len) { payload_len_ = len; }
  void setPilotInterval(uint16_t b) { pilot_interval_bytes_ = b; }
  void setHeaderDup(bool v) { hdr_dup_enabled_ = v; }
  void setRepeatCount(uint8_t r) { repeat_count_ = r; }

  FecMode fecMode() const { return fec_mode_; }
  uint8_t interleaveDepth() const { return interleave_depth_; }

  // Подготовка набора кадров для сообщения
  size_t prepareFrames(const OutgoingMessage& msg, bool requireAck, std::vector<PreparedFrame>& out);

  // Применение шифрования к полезной нагрузке
  bool applyEncryption(const uint8_t* in, size_t len, const uint8_t* aad, size_t aad_len, std::vector<uint8_t>& out);

private:
  Fragmenter& frag_;
  IEncryptor& enc_;
  PipelineMetrics& metrics_;

  bool enc_enabled_ = cfg::ENCRYPTION_ENABLED_DEFAULT;
  bool fec_enabled_ = cfg::FEC_ENABLED_DEFAULT;
  FecMode fec_mode_ = (FecMode)cfg::FEC_MODE_DEFAULT;
  uint8_t interleave_depth_ = cfg::INTERLEAVER_DEPTH_DEFAULT;
  uint16_t payload_len_ = cfg::LORA_MTU - FRAME_HEADER_SIZE;
  uint16_t pilot_interval_bytes_ = cfg::PILOT_INTERVAL_BYTES_DEFAULT;
  bool hdr_dup_enabled_ = cfg::HEADER_DUP_DEFAULT;
  uint8_t repeat_count_ = 1;
};
