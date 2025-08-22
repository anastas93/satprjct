#include "message_buffer.h"

// Конструктор с заданной вместимостью
MessageBuffer::MessageBuffer(size_t capacity) : capacity_(capacity) {}

// Добавление сообщения в буфер
uint32_t MessageBuffer::enqueue(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;             // проверка входных данных
  if (q_.size() >= capacity_) return 0;        // проверка переполнения
  uint32_t id = next_id_++;                    // текущий идентификатор
  q_.emplace_back(id, std::vector<uint8_t>(data, data + len));
  return id;
}

// Количество свободных слотов
size_t MessageBuffer::freeSlots() const {
  return capacity_ - q_.size();
}

// Удаление последнего сообщения (для отката)
bool MessageBuffer::dropLast() {
  if (q_.empty()) return false;
  q_.pop_back();
  --next_id_;                                  // возвращаем идентификатор назад
  return true;
}

// Проверка наличия сообщений
bool MessageBuffer::hasPending() const {
  return !q_.empty();
}

// Извлечение сообщения
bool MessageBuffer::pop(uint32_t& id, std::vector<uint8_t>& out) {
  if (q_.empty()) return false;                // очередь пуста
  auto& front = q_.front();
  id = front.first;                            // возвращаем идентификатор
  out = std::move(front.second);               // забираем данные
  q_.pop_front();
  return true;
}
