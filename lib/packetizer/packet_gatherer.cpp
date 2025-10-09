#include "packet_gatherer.h"
// Конструктор
PacketGatherer::PacketGatherer(PayloadMode mode, size_t custom)
    : mode_(mode), custom_(custom) {}

// Сброс
void PacketGatherer::reset() {
  data_.clear();
  complete_ = false;
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
  if (!data || len == 0) return;                     // нечего добавлять

  const size_t prev_size = data_.size();
  data_.reserve(prev_size + len);                    // уменьшаем число реаллокаций при длинных сообщениях
  data_.insert(data_.end(), data, data + len);       // дописываем фрагмент в общий буфер

  const size_t chunk = payloadSize();
  if (chunk == 0) {                                  // защитный случай: произвольный размер не задан
    complete_ = true;
    return;
  }

  complete_ = (len < chunk);                         // если фрагмент меньше базового размера — это финальная часть
}

// Завершено ли
bool PacketGatherer::isComplete() const { return complete_; }

// Получение сообщения
const std::vector<uint8_t>& PacketGatherer::get() const { return data_; }

