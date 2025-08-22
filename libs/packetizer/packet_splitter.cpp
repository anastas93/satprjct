#include "packet_splitter.h"
#include <algorithm>

// Конструктор
PacketSplitter::PacketSplitter(PayloadMode mode) : mode_(mode) {}

// Смена режима
void PacketSplitter::setMode(PayloadMode mode) { mode_ = mode; }

// Размер полезной нагрузки
size_t PacketSplitter::payloadSize() const {
  switch (mode_) {
    case PayloadMode::SMALL:  return 32;
    case PayloadMode::MEDIUM: return 64;
    case PayloadMode::LARGE:  return 128;
  }
  return 32;
}

// Разделение данных и помещение в буфер
uint32_t PacketSplitter::splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len) const {
  if (!data || len == 0) return 0;
  size_t chunk = payloadSize();
  size_t parts = (len + chunk - 1) / chunk;             // сколько частей потребуется
  if (buf.freeSlots() < parts) return 0;                // недостаточно места в буфере
  size_t offset = 0;
  uint32_t first_id = 0;
  size_t added = 0;                                     // количество успешно добавленных частей
  while (offset < len) {
    size_t part = std::min(chunk, len - offset);
    uint32_t id = buf.enqueue(data + offset, part);
    if (id == 0) {                                      // ошибка добавления
      while (added--) buf.dropLast();                   // откат всех ранее добавленных частей
      return 0;
    }
    if (first_id == 0) first_id = id;
    offset += part;
    ++added;
  }
  return first_id;
}

