
#pragma once
#include <stdint.h>

// Скользящее окно фиксированного размера
struct SlidingWindow {
  static const uint8_t SIZE = 16;   // длина окна
  float data[SIZE] = {0};           // массив измерений
  uint8_t idx = 0;                  // позиция для следующей записи
  uint8_t count = 0;                // сколько значений реально заполнено
  float sum = 0.0f;                 // сумма для быстрого среднего

  // добавить новое измерение в окно
  inline void add(float sample) {
    if (count < SIZE) {
      data[idx] = sample; sum += sample; count++; idx = (idx + 1) % SIZE;
    } else {
      sum -= data[idx];
      data[idx] = sample;
      sum += sample;
      idx = (idx + 1) % SIZE;
    }
  }

  // очистить содержимое окна
  inline void clear() { idx = 0; count = 0; sum = 0.0f; }

  // получить среднее по окну
  inline float avg() const { return count ? (sum / count) : 0.0f; }
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

  // Скользящие окна метрик качества канала
  SlidingWindow per_window;     // Packet Error Rate (0 или 1)
  SlidingWindow rtt_window_ms;  // время кругового обхода
  SlidingWindow goodput_window; // полезная пропускная способность
  SlidingWindow ebn0_window;    // средний Eb/N0
};
