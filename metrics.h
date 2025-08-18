
#pragma once
#include <stdint.h>

// Простая экспоненциальная скользящая средняя
struct EmaCounter {
  float value = 0.0f;    // текущее значение EMA
  float alpha = 0.1f;    // коэффициент сглаживания
  bool initialized = false;

  // обновление счётчика новым измерением
  inline void update(float sample) {
    if (initialized)
      value = alpha * sample + (1.0f - alpha) * value;
    else
      { value = sample; initialized = true; }
  }
};

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

  // FEC
  uint32_t rx_fec_fail = 0;      // число неудачных декодирований
  uint32_t rx_fec_corrected = 0; // сколько байт исправлено

  // Канальные параметры
  float last_snr = 0.0f;   // последний измеренный SNR
  float last_ebn0 = 0.0f;  // последний измеренный Eb/N0

  // EMA-метрики качества канала
  EmaCounter per_ema;      // Packet Error Rate
  EmaCounter rtt_ema_ms;   // время кругового обхода
  EmaCounter goodput_ema;  // полезная пропускная способность
  EmaCounter snr_ema;      // средний SNR
  EmaCounter ebn0_ema;     // средний Eb/N0
};
