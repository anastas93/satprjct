#include "../event_queue.h"
QueueHandle_t g_evt_queue = nullptr;
QueueHandle_t g_cmd_queue = nullptr;
void queueEvent(evt_t, uint8_t, uint16_t) {}
void queueMetrics(float, float, float) {}
