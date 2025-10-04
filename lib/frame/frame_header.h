#ifndef LIBS_FRAME_FRAME_HEADER_H
#define LIBS_FRAME_FRAME_HEADER_H
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>

// Структура заголовка кадра
struct FrameHeader {
  uint8_t ver = 1;          // версия протокола
  uint16_t msg_id = 0;      // идентификатор сообщения
  uint16_t frag_cnt = 1;    // общее число фрагментов
  uint32_t packed = 0;      // упакованные флаги, номер фрагмента и длина

  static constexpr size_t SIZE = 12;     // размер стандартного заголовка в байтах
  static constexpr size_t MIN_SIZE = 9;  // минимальный размер укороченного заголовка (без выравнивания)
  static constexpr uint32_t FLAGS_SHIFT = 24;      // старшие биты под флаги
  static constexpr uint32_t FLAGS_MASK = 0xFF000000; // маска флагов (8 бит)
  static constexpr uint32_t FRAG_SHIFT = 12;       // средние биты под индекс фрагмента (12 бит)
  static constexpr uint32_t FRAG_MASK = 0x00FFF000; // маска индекса фрагмента
  static constexpr uint32_t LEN_MASK = 0x00000FFF; // младшие биты под длину (12 бит)
  static constexpr uint8_t FLAG_ENCRYPTED = 0x01;    // полезная нагрузка зашифрована
  static constexpr uint8_t FLAG_ACK_REQUIRED = 0x02; // требуется подтверждение доставки
  static constexpr uint8_t FLAG_CONV_ENCODED = 0x04; // полезная нагрузка прошла свёрточное кодирование

  // Доступ к упакованным полям
  uint8_t getFlags() const;        // извлечение флагов
  void setFlags(uint8_t value);    // установка флагов
  uint16_t getFragIdx() const;     // извлечение индекса фрагмента
  void setFragIdx(uint16_t value); // установка индекса фрагмента
  uint16_t getPayloadLen() const;  // извлечение длины полезной нагрузки
  void setPayloadLen(uint16_t value); // установка длины полезной нагрузки

  // Кодирование заголовка в буфер (big-endian)
  bool encode(uint8_t* out, size_t out_len, const uint8_t* payload, size_t payload_len);
  // Декодирование заголовка из буфера
  static bool decode(const uint8_t* data, size_t len, FrameHeader& out);

  // Вспомогательная CRC16 для проверок пилотов и совместимости со старыми данными
  static uint16_t crc16(const uint8_t* data, size_t len);
};

#endif // LIBS_FRAME_FRAME_HEADER_H
