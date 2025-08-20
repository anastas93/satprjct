#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include "message_buffer.h"

// Режимы размера полезной нагрузки
enum class PayloadMode {
  SMALL,   // около 32 байт
  MEDIUM,  // около 64 байт
  LARGE    // около 128 байт
};

// Делитель пакетов по заданному режиму
class PacketSplitter {
public:
  // Конструктор с указанием режима
  explicit PacketSplitter(PayloadMode mode = PayloadMode::SMALL);
  // Установить новый режим
  void setMode(PayloadMode mode);
  // Разделить данные и занести части в буфер, возвращает ID первого пакета
  uint32_t splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len) const;
private:
  // Возвращает размер полезной нагрузки для текущего режима
  size_t payloadSize() const;
  PayloadMode mode_;
};

