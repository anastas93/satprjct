
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

namespace FrameLog {
  // Записываем сведения о кадре вместе с метриками канала и параметрами
  void push(char dir, const uint8_t* data, size_t len,
            uint32_t seq = 0, uint8_t fec_mode = 0, uint8_t interleave = 0,
            float snr_db = 0.0f, float ebn0_db = 0.0f, float rssi = 0.0f,
            uint8_t rs_corrections = 0, uint16_t viterbi_metric = 0,
            uint8_t drop_reason = 0, uint16_t rtt_estimate = 0,
            uint16_t frag_size = 0);
  // Выводим последние N записей на переданный поток
  void dump(Print& out, unsigned int count);
  // вернуть последние записи в виде JSON массива
  String json(unsigned int count, int drop_filter = -1);
}
