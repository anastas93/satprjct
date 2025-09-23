#pragma once
#ifndef LIBS_FRAME_FRAME_HEADER_H
#define LIBS_FRAME_FRAME_HEADER_H
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

// Структура заголовка кадра
struct FrameHeader {
  uint8_t ver = 1;          // версия протокола
  uint8_t flags = 0;        // флаги кадра
  uint32_t msg_id = 0;      // идентификатор сообщения
  uint16_t frag_idx = 0;    // номер текущего фрагмента
  uint16_t frag_cnt = 1;    // общее число фрагментов
  uint16_t payload_len = 0; // длина полезной нагрузки без пилотов
  uint32_t ack_mask = 0;    // маска подтверждённых сообщений
  uint16_t hdr_crc = 0;     // CRC заголовка
  uint16_t frame_crc = 0;   // CRC всего кадра

  static constexpr size_t SIZE = 20; // размер заголовка в байтах
  static constexpr uint8_t FLAG_ENCRYPTED = 0x01;    // полезная нагрузка зашифрована
  static constexpr uint8_t FLAG_ACK_REQUIRED = 0x02; // требуется подтверждение доставки
  static constexpr uint8_t FLAG_CONV_ENCODED = 0x04; // полезная нагрузка прошла свёрточное кодирование

  // Кодирование заголовка в буфер (big-endian) с подсчётом CRC
  bool encode(uint8_t* out, size_t out_len, const uint8_t* payload, size_t payload_len);
  // Декодирование заголовка из буфера с проверкой CRC
  static bool decode(const uint8_t* data, size_t len, FrameHeader& out);

  // Подсчёт CRC16 (полином 0x1021)
  static uint16_t crc16(const uint8_t* data, size_t len);
  // Проверка контрольной суммы кадра
  bool checkFrameCrc(const uint8_t* payload, size_t payload_len) const;
};

#endif // LIBS_FRAME_FRAME_HEADER_H
