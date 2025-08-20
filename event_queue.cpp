// Реализация очередей событий и команд
#include "event_queue.h"

#ifdef ARDUINO
#include <Arduino.h>
QueueHandle_t g_evt_queue = nullptr;
QueueHandle_t g_cmd_queue = nullptr;

// Кладём событие (простое или смена ключа) в очередь
void queueEvent(evt_t type, uint8_t kid, uint16_t key_crc16) {
  if (!g_evt_queue) return;
  evt_msg m{};
  m.type = type;
  m.ts_ms = millis();
  if (type == evt_t::EVT_KEY_CHANGED) {
    m.data.key.kid = kid;
    m.data.key.key_crc16 = key_crc16;
  }
  xQueueSend(g_evt_queue, &m, 0);
}

// Публикация метрик в очередь событий
void queueMetrics(float per, float rtt_ms, float ebn0_db) {
  if (!g_evt_queue) return;
  evt_msg m{};
  m.type = evt_t::EVT_METRICS;
  m.ts_ms = millis();
  m.data.metrics.per = per;
  m.data.metrics.rtt_ms = rtt_ms;
  m.data.metrics.ebn0_db = ebn0_db;
  xQueueSend(g_evt_queue, &m, 0);
}
#else
QueueHandle_t g_evt_queue = nullptr;
QueueHandle_t g_cmd_queue = nullptr;
void queueEvent(evt_t, uint8_t, uint16_t) {}
void queueMetrics(float, float, float) {}
#endif
