#pragma once
#include <stdint.h>
#include <stddef.h>

// Типы событий, которые WebTask получает от RadioTask
enum class evt_t : uint8_t {
  EVT_RX_START,
  EVT_RX_DONE,
  EVT_TX_START,
  EVT_TX_DONE,
  EVT_KEY_CHANGED,
  EVT_METRICS
};

// Сообщение события с унифицированным payload
typedef struct {
  evt_t type;      // тип события
  uint32_t ts_ms;  // метка времени
  union {
    struct {              // для EVT_KEY_CHANGED
      uint8_t kid;        // активный ключ
      uint16_t key_crc16; // CRC ключа
    } key;
    struct {              // для EVT_METRICS
      float per;          // Packet Error Rate
      float rtt_ms;       // средний RTT
      float ebn0_db;      // средний Eb/N0
    } metrics;
  } data;
} evt_msg;

// Команды от WebTask к RadioTask
enum class cmd_t : uint8_t {
  CMD_SEND_TEXT,  // отправка произвольного текста
  CMD_SET_BW      // изменение полосы приёма
};

// Унифицированная структура команды
typedef struct {
  cmd_t type;  // тип команды
  union {
    struct {              // CMD_SEND_TEXT
      size_t len;         // длина текста
      char data[256];     // буфер данных
    } text;
    struct {              // CMD_SET_BW
      float khz;          // новая полоса (кГц)
    } bw;
  } data;
} cmd_msg;

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
extern QueueHandle_t g_evt_queue;
extern QueueHandle_t g_cmd_queue;
#else
using QueueHandle_t = void*;
extern QueueHandle_t g_evt_queue;
extern QueueHandle_t g_cmd_queue;
#endif

// Помещает простое событие или событие смены ключа в очередь
void queueEvent(evt_t type, uint8_t kid=0, uint16_t key_crc16=0);

// Публикация метрик в очередь событий
void queueMetrics(float per, float rtt_ms, float ebn0_db);
