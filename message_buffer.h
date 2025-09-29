// Защита от повторного включения заголовка
#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <vector>
#include <cstdint>
#include <cstddef>

// Буфер сообщений для хранения данных на отправку
class MessageBuffer {
public:
  // Конструктор с ограничением на количество сообщений
  static constexpr size_t DEFAULT_SLOT_SIZE = 256; // размер слота по умолчанию
  explicit MessageBuffer(size_t capacity, size_t slot_size = DEFAULT_SLOT_SIZE);
  // Добавляет сообщение в буфер и возвращает его идентификатор
  uint16_t enqueue(const uint8_t* data, size_t len);
  // Возвращает количество свободных слотов в очереди
  size_t freeSlots() const;
  // Удаляет последнее сообщение (для отката операций)
  bool dropLast();
  // Проверяет, есть ли сообщения в очереди
  bool hasPending() const;
  // Извлекает первое сообщение из очереди, возвращает ID и данные
  bool pop(uint16_t& id, std::vector<uint8_t>& out);
  // Позволяет заглянуть в начало очереди без извлечения
  const std::vector<uint8_t>* peek(uint16_t& id) const;
  // Возвращает вместимость слота данных
  size_t slotSize() const;
private:
  struct Slot {
    uint16_t id = 0;                  // сохранённый идентификатор сообщения
    std::vector<uint8_t> data;        // заранее выделенный буфер данных
    bool used = false;                // флаг заполнения слота
  };

  uint16_t next_id_ = 1;             // следующий идентификатор
  size_t capacity_;                  // максимальное количество сообщений
  size_t slot_size_;                 // максимальная длина данных одного сообщения
  std::vector<Slot> slots_;          // предвыделенные слоты хранения
  size_t head_ = 0;                  // индекс первого элемента
  size_t tail_ = 0;                  // индекс следующей позиции для добавления
  size_t size_ = 0;                  // текущее число элементов
};

#endif // MESSAGE_BUFFER_H
