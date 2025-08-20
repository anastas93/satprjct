#include "packet_gatherer.h"
#include <algorithm>

// Конструктор
PacketGatherer::PacketGatherer(PayloadMode mode) : mode_(mode) {}

// Сброс
void PacketGatherer::reset() {
  data_.clear();
  complete_ = false;
}

// Размер полезной нагрузки
size_t PacketGatherer::payloadSize() const {
  switch (mode_) {
    case PayloadMode::SMALL:  return 32;
    case PayloadMode::MEDIUM: return 64;
    case PayloadMode::LARGE:  return 128;
  }
  return 32;
}

// Добавление части
void PacketGatherer::add(const uint8_t* data, size_t len) {
  if (!data || len == 0) return;
  data_.insert(data_.end(), data, data + len);
  if (len < payloadSize()) complete_ = true; // последняя часть меньше стандартного размера
}

// Завершено ли
bool PacketGatherer::isComplete() const { return complete_; }

// Получение сообщения
const std::vector<uint8_t>& PacketGatherer::get() const { return data_; }

