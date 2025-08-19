
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace cfg {
  static constexpr uint8_t  PIPE_VERSION           = 1;
  static constexpr uint16_t LORA_MTU               = 255;
  static constexpr uint16_t RX_ASSEMBLER_TTL_MS    = 15000;

  // ACK
  static constexpr bool     ACK_ENABLED_DEFAULT = false;
  // базовый таймаут ожидания подтверждения
  static constexpr uint16_t ACK_TIMEOUT   = 1200;   // мс
  // максимальный таймаут с учётом бэкоффа
  static constexpr uint16_t ACK_TIMEOUT_LIMIT = 5000; // мс
  // максимальное число повторов при отсутствии ACK
  static constexpr uint8_t  MAX_RETRIES   = 3;
  // интервал агрегирования ACK (значение по умолчанию)
  static constexpr uint16_t T_ACK_AGG_DEFAULT = 50;     // мс
  // совместимость со старым API
  static constexpr uint16_t SEND_RETRY_MS_DEFAULT  = ACK_TIMEOUT;
  static constexpr uint8_t  SEND_RETRY_COUNT_DEFAULT = MAX_RETRIES;
  static constexpr uint16_t INTER_FRAME_GAP_MS = 25;

  // ENC
  static constexpr bool     ENCRYPTION_ENABLED_DEFAULT = false;
  static constexpr uint8_t  ENC_TAG_LEN            = 8;
  static constexpr uint8_t  ENC_META_LEN           = 1; // KID
  static constexpr bool     ENC_MODE_PER_FRAGMENT  = true;

  // FEC и интерливинг
  static constexpr uint8_t  FEC_MODE_DEFAULT       = 0; // 0-off,1-rs_vit,2-ldpc
  static constexpr bool     FEC_ENABLED_DEFAULT    = (FEC_MODE_DEFAULT!=0);
  static constexpr uint8_t  INTERLEAVER_DEPTH_DEFAULT = 1; // 1/4/8/16

  // Buffers
  static constexpr size_t   TX_BUF_MAX_BYTES        = 48 * 1024;
  static constexpr size_t   TX_BUF_QOS_CAP_HIGH     = 24 * 1024;
  static constexpr size_t   TX_BUF_QOS_CAP_NORMAL   = 16 * 1024;
  static constexpr size_t   TX_BUF_QOS_CAP_LOW      = 12 * 1024;
  static constexpr uint16_t TX_BUF_MAX_MSGS         = 256;
  static constexpr size_t   RX_REASM_CAP_BYTES      = 64 * 1024;
  static constexpr uint16_t RX_REASM_MAX_ASSEMBLERS = 8;
  static constexpr size_t   RX_REASM_PER_MSG_CAP    = 8 * 1024;

  static constexpr uint8_t  RX_DUP_WINDOW          = 64;

  // SR-ARQ окно и дублирование заголовка
  static constexpr uint8_t  SR_WINDOW_DEFAULT      = 8;
  static constexpr bool     HEADER_DUP_DEFAULT     = true;

  // Настройка временных слотов TDD (мс)
  static constexpr uint16_t TDD_TX_WINDOW_MS  = 1000; // окно передачи
  static constexpr uint16_t TDD_ACK_WINDOW_MS = 300;  // окно ожидания подтверждений
  static constexpr uint16_t TDD_GUARD_MS      = 50;   // защитный интервал

    // Пилотные вставки и неравномерная защита
    // значение по умолчанию, может изменяться через веб-интерфейс
    static constexpr size_t   PILOT_INTERVAL_BYTES_DEFAULT = 64; // интервал между пилотами
    static constexpr uint8_t  PILOT_SEQ[2]        = {0x55, 0x2D}; // короткий пилот
    static constexpr size_t   PILOT_LEN           = sizeof(PILOT_SEQ);

  // Additional UART used for Radxa Zero 3W command interface
  // This UART allows bridging of commands and payloads to an external host.
  static constexpr uint32_t RADXA_UART_BAUD        = 115200;
}
