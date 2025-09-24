#include "packet_gatherer.h"
#include <algorithm>

// Конструктор
PacketGatherer::PacketGatherer(PayloadMode mode, size_t custom)
    : mode_(mode), custom_(custom) {}

// Сброс
void PacketGatherer::reset() {
  data_.clear();
  complete_ = false;
  expected_chunk_ = 0;
}

// Размер полезной нагрузки
size_t PacketGatherer::payloadSize() const {
  if (custom_) return custom_;
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
  if (data_.empty()) {
    expected_chunk_ = len;                           // запоминаем фактический размер первой части
    if (expected_chunk_ == 0) expected_chunk_ = payloadSize();
  }
  data_.insert(data_.end(), data, data + len);
  if (expected_chunk_ == 0) expected_chunk_ = payloadSize();
  if (len < expected_chunk_) {
    complete_ = true;                               // последняя часть меньше базовой — значит, сообщение завершено
  } else {
    complete_ = false;                              // продолжаем накапливать остальные части
  }
}

// Завершено ли
bool PacketGatherer::isComplete() const { return complete_; }

// Получение сообщения
const std::vector<uint8_t>& PacketGatherer::get() const { return data_; }

