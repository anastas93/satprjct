#pragma once
#include <deque>
#include <vector>
#include <cstdint>

// Буфер сообщений для хранения данных на отправку
class MessageBuffer {
public:
  // Добавляет сообщение в буфер и возвращает его идентификатор
  uint32_t enqueue(const uint8_t* data, size_t len);
  // Проверяет, есть ли сообщения в очереди
  bool hasPending() const;
  // Извлекает первое сообщение из очереди
  bool pop(std::vector<uint8_t>& out);
private:
  uint32_t next_id_ = 1;                 // следующий идентификатор
  std::deque<std::vector<uint8_t>> q_;   // очередь сообщений
};
