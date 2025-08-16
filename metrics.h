
#pragma once
#include <stdint.h>

struct PipelineMetrics {
  // TX/RX
  uint32_t tx_frames = 0, tx_bytes = 0, tx_msgs = 0;
  uint32_t rx_frames_ok = 0, rx_bytes = 0, rx_msgs_ok = 0;
  uint32_t rx_dup_msgs = 0, rx_crc_fail = 0;
  uint32_t rx_frags = 0;
  uint32_t rx_drop_len_mismatch = 0;
  uint32_t rx_assem_drop_ttl = 0, rx_assem_drop_overflow = 0;

  // ACK
  uint32_t tx_retries = 0, ack_fail = 0;
  uint32_t ack_seen = 0;
  uint32_t ack_time_ms_avg = 0;

  // ENC
  uint32_t dec_fail_tag = 0, dec_fail_other = 0, enc_fail = 0;
};
