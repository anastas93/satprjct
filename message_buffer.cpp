#include "message_buffer.h"

// Добавление сообщения в буфер
uint32_t MessageBuffer::enqueue(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;
  q_.emplace_back(data, data + len);
  return next_id_++;
}

// Проверка наличия сообщений
bool MessageBuffer::hasPending() const {
  return !q_.empty();
}

// Извлечение сообщения
bool MessageBuffer::pop(std::vector<uint8_t>& out) {
  if (q_.empty()) return false;
  out = std::move(q_.front());
  q_.pop_front();
  return true;
}
