
#include "frame_log.h"
#include "config.h"
#include <string.h>

namespace {
  // Элемент кольцевого буфера с основными параметрами кадра
  struct Item {
    char dir;                 // направление 'T' или 'R'
    uint16_t len;             // длина полезных данных
    uint32_t seq;             // счётчик/идентификатор кадра
    uint8_t  fec_mode;        // используемый режим FEC
    uint8_t  interleave;      // глубина интерливера
    float    snr_ebn0;        // измеренный SNR или Eb/N0
    uint8_t  rs_corr;         // число исправленных байт RS
    uint16_t viterbi;         // метрика Витерби
    uint8_t  drop_reason;     // причина отбрасывания
    uint16_t rtt;             // оценка RTT
    uint8_t  buf[cfg::LORA_MTU];
  };
  static constexpr size_t CAP = 128;
  Item ring[CAP];
  size_t head = 0, stored = 0;
}

void FrameLog::push(char dir, const uint8_t* data, size_t len,
                    uint32_t seq, uint8_t fec_mode, uint8_t interleave,
                    float snr_ebn0, uint8_t rs_corrections,
                    uint16_t viterbi_metric, uint8_t drop_reason,
                    uint16_t rtt_estimate) {
  if (!data || len==0) return;
  size_t n = len > cfg::LORA_MTU ? cfg::LORA_MTU : len;
  Item& it = ring[head];
  it.dir = dir;
  it.len = (uint16_t)n;
  it.seq = seq;
  it.fec_mode = fec_mode;
  it.interleave = interleave;
  it.snr_ebn0 = snr_ebn0;
  it.rs_corr = rs_corrections;
  it.viterbi = viterbi_metric;
  it.drop_reason = drop_reason;
  it.rtt = rtt_estimate;
  memcpy(it.buf, data, n);
  head = (head + 1) % CAP;
  if (stored < CAP) stored++;
}

void FrameLog::dump(Print& out, unsigned int count) {
  if (stored == 0) { out.println(F("(empty)")); return; }
  if (count == 0 || count > stored) count = stored;
  size_t idx = (head + CAP - stored) % CAP;
  size_t to_print = stored;
  if (to_print > count) {
    idx = (idx + (to_print - count)) % CAP;
    to_print = count;
  }
  for (size_t i=0; i<to_print; ++i) {
    const Item& it = ring[(idx + i) % CAP];
    out.printf("[%c seq=%lu fec=%u int=%u snr=%.1f rs=%u vit=%u drop=%u rtt=%u] %u bytes: ",
               it.dir, (unsigned long)it.seq, it.fec_mode, it.interleave,
               it.snr_ebn0, it.rs_corr, it.viterbi, it.drop_reason,
               it.rtt, (unsigned)it.len);
    for (uint16_t j=0;j<it.len;j++) {
      uint8_t b = it.buf[j];
      const char* hex = "0123456789ABCDEF";
      char s[3]; s[0]=hex[b>>4]; s[1]=hex[b&0xF]; s[2]=0;
      out.print(s);
    }
    out.println();
  }
}
