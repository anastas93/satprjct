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
  // Конструктор с указанием режима и опциональным произвольным размером
  explicit PacketSplitter(PayloadMode mode = PayloadMode::SMALL, size_t custom = 0);
  // Установить новый режим
  void setMode(PayloadMode mode);
  // Задать произвольный размер полезной нагрузки
  void setCustomSize(size_t custom);
  // Разделить данные и занести части в буфер, возвращает ID первого пакета
  // with_id=true добавляет префикс и журналирование
  uint16_t splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len, bool with_id = true) const;
  // Возвращает размер полезной нагрузки для текущего режима
  size_t payloadSize() const;
private:
  PayloadMode mode_;
  size_t custom_ = 0; // произвольный размер, если задан
};

