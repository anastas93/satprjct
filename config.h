
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace cfg {
  static constexpr uint8_t  PIPE_VERSION           = 1;
  static constexpr uint16_t LORA_MTU               = 255;
  static constexpr uint16_t RX_ASSEMBLER_TTL_MS    = 15000;

  // ACK
  static constexpr bool     ACK_ENABLED_DEFAULT    = false;
  static constexpr uint16_t SEND_RETRY_MS_DEFAULT  = 1200;
  static constexpr uint8_t  SEND_RETRY_COUNT_DEFAULT = 3;
  static constexpr uint16_t INTER_FRAME_GAP_MS     = 25;

  // ENC
  static constexpr bool     ENCRYPTION_ENABLED_DEFAULT = false;
  static constexpr uint8_t  ENC_TAG_LEN            = 8;
  static constexpr uint8_t  ENC_META_LEN           = 1; // KID
  static constexpr bool     ENC_MODE_PER_FRAGMENT  = true;

  // FEC и интерливинг
  static constexpr bool     FEC_ENABLED_DEFAULT    = false;
  static constexpr uint8_t  INTERLEAVER_DEPTH_DEFAULT = 1;

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

  // Additional UART used for Radxa Zero 3W command interface
  // This UART allows bridging of commands and payloads to an external host.
  static constexpr uint32_t RADXA_UART_BAUD        = 115200;
}
