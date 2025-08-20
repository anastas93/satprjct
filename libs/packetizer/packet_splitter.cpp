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
  size_t offset = 0;
  uint32_t first_id = 0;
  while (offset < len) {
    size_t part = std::min(chunk, len - offset);
    uint32_t id = buf.enqueue(data + offset, part);
    if (id == 0) break; // буфер переполнен
    if (first_id == 0) first_id = id;
    offset += part;
  }
  return first_id;
}

