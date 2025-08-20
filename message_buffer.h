#pragma once
#include <deque>
#include <vector>
#include <cstdint>
#include <utility>

// Буфер сообщений для хранения данных на отправку
class MessageBuffer {
public:
  // Конструктор с ограничением на количество сообщений
  explicit MessageBuffer(size_t capacity);
  // Добавляет сообщение в буфер и возвращает его идентификатор
  uint32_t enqueue(const uint8_t* data, size_t len);
  // Проверяет, есть ли сообщения в очереди
  bool hasPending() const;
  // Извлекает первое сообщение из очереди, возвращает ID и данные
  bool pop(uint32_t& id, std::vector<uint8_t>& out);
private:
  uint32_t next_id_ = 1;                                       // следующий идентификатор
  size_t capacity_;                                            // максимальное количество сообщений
  std::deque<std::pair<uint32_t, std::vector<uint8_t>>> q_;    // очередь сообщений с идентификаторами
};
