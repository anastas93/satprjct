
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
    float    snr_db;          // измеренный SNR, дБ
    float    ebn0_db;         // измеренный Eb/N0, дБ
    float    rssi;            // уровень сигнала RSSI, дБм
    uint8_t  rs_corr;         // число исправленных байт RS
    uint16_t viterbi;         // метрика Витерби
    uint8_t  drop_reason;     // причина отбрасывания
    uint16_t rtt;             // оценка RTT
    uint16_t frag_size;       // размер полезной нагрузки
    uint8_t  buf[cfg::LORA_MTU];
  };
  static constexpr size_t CAP = 128;
  Item ring[CAP];
  size_t head = 0, stored = 0;
}

void FrameLog::push(char dir, const uint8_t* data, size_t len,
                    uint32_t seq, uint8_t fec_mode, uint8_t interleave,
                    float snr_db, float ebn0_db, float rssi,
                    uint8_t rs_corrections, uint16_t viterbi_metric,
                    uint8_t drop_reason, uint16_t rtt_estimate,
                    uint16_t frag_size) {
  if (!data || len==0) return;
  size_t n = len > cfg::LORA_MTU ? cfg::LORA_MTU : len;
  Item& it = ring[head];
  it.dir = dir;
  it.len = (uint16_t)n;
  it.seq = seq;
  it.fec_mode = fec_mode;
  it.interleave = interleave;
  it.snr_db = snr_db;
  it.ebn0_db = ebn0_db;
  it.rssi = rssi;
  it.rs_corr = rs_corrections;
  it.viterbi = viterbi_metric;
  it.drop_reason = drop_reason;
  it.rtt = rtt_estimate;
  it.frag_size = frag_size;
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
    out.printf("[%c seq=%lu fec=%u int=%u frag=%u snr=%.1f ebn0=%.1f rssi=%.1f rs=%u vit=%u drop=%u rtt=%u] %u bytes: ",
               it.dir, (unsigned long)it.seq, it.fec_mode, it.interleave,
               it.frag_size, it.snr_db, it.ebn0_db, it.rssi,
               it.rs_corr, it.viterbi, it.drop_reason, it.rtt,
               (unsigned)it.len);
    for (uint16_t j=0;j<it.len;j++) {
      uint8_t b = it.buf[j];
      const char* hex = "0123456789ABCDEF";
      char s[3]; s[0]=hex[b>>4]; s[1]=hex[b&0xF]; s[2]=0;
      out.print(s);
    }
    out.println();
  }
}

// Формирует JSON с последними записями, опционально фильтруя по drop_reason
String FrameLog::json(unsigned int count, int drop_filter) {
  String s = "[";
  if (stored == 0) { s += ']'; return s; }
  if (count == 0 || count > stored) count = stored;
  size_t idx = (head + CAP - stored) % CAP;
  size_t to_print = stored;
  if (to_print > count) { idx = (idx + (to_print - count)) % CAP; to_print = count; }
  bool first = true;
  for (size_t i = 0; i < to_print; ++i) {
    const Item& it = ring[(idx + i) % CAP];
    if (drop_filter >= 0 && it.drop_reason != drop_filter) continue;
    if (!first) s += ','; first = false;
    s += '{';
    s += "\"dir\":\""; s += it.dir; s += "\",";
    s += "\"seq\":"; s += String((unsigned long)it.seq); s += ',';
    s += "\"len\":"; s += String((unsigned)it.len); s += ',';
    s += "\"fec\":"; s += String((unsigned)it.fec_mode); s += ',';
    s += "\"inter\":"; s += String((unsigned)it.interleave); s += ',';
    s += "\"frag\":"; s += String((unsigned)it.frag_size); s += ',';
    s += "\"snr\":"; s += String(it.snr_db, 1); s += ',';
    s += "\"ebn0\":"; s += String(it.ebn0_db, 1); s += ',';
    s += "\"rssi\":"; s += String(it.rssi, 1); s += ',';
    s += "\"rs\":"; s += String((unsigned)it.rs_corr); s += ',';
    s += "\"vit\":"; s += String((unsigned)it.viterbi); s += ',';
    s += "\"drop\":"; s += String((unsigned)it.drop_reason); s += ',';
    s += "\"rtt\":"; s += String((unsigned)it.rtt);
    s += '}';
  }
  s += ']';
  return s;
}
