#include <stdint.h>
#include <stddef.h>

// Заглушки частот для единичного TX-движка в тестах
float g_freq_tx_mhz = 433.1f;
float g_freq_rx_mhz = 433.1f;

// Простейшая заглушка низкоуровневой передачи
bool radioTransmit(const uint8_t* data, size_t len) {
  (void)data;
  (void)len;
  return true;
}
