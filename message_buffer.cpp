#include "message_buffer.h"
#include "default_settings.h"

// Конструктор с заданной вместимостью и размером слота
MessageBuffer::MessageBuffer(size_t capacity, size_t slot_size) {
  if (capacity == 0) {
    LOG_WARN("MessageBuffer: запрошена нулевая вместимость, используем минимум 1");
    capacity = 1;                               // защищаемся от деления по модулю 0
  }
  if (slot_size == 0) {
    LOG_WARN("MessageBuffer: запрошен нулевой размер слота, используем минимум 1 байт");
    slot_size = 1;                              // исключаем невозможные размеры сообщений
  }
  capacity_ = capacity;
  slot_size_ = slot_size;
  slots_.resize(capacity_);
  // Не пытаемся заранее выделить память под все слоты, чтобы не спровоцировать
  // std::bad_alloc на устройствах с ограниченным ОЗУ (ESP32 и подобные).
}

// Добавление сообщения в буфер
uint32_t MessageBuffer::enqueue(const uint8_t* data, size_t len) {
  if (!data || len == 0) {                     // проверка входных данных
    DEBUG_LOG("MessageBuffer: пустой ввод");
    return 0;
  }
  if (len > slot_size_) {                      // защита от переполнения слота
    DEBUG_LOG("MessageBuffer: размер сообщения превышает слот");
    return 0;
  }
  if (size_ >= capacity_) {                    // проверка переполнения
    DEBUG_LOG("MessageBuffer: переполнение");
    return 0;
  }
  uint32_t id = next_id_++;                    // текущий идентификатор
  Slot& slot = slots_[tail_];
  slot.id = id;
  slot.data.assign(data, data + len);          // наполняем предварительно выделенный буфер
  slot.used = true;
  tail_ = (tail_ + 1) % capacity_;
  ++size_;
  DEBUG_LOG_VAL("MessageBuffer: добавлен id=", id);
  return id;
}

// Количество свободных слотов
size_t MessageBuffer::freeSlots() const {
  return capacity_ - size_;
}

// Удаление последнего сообщения (для отката)
bool MessageBuffer::dropLast() {
  if (size_ == 0) {
    DEBUG_LOG("MessageBuffer: удаление из пустой очереди");
    return false;
  }
  tail_ = (tail_ + capacity_ - 1) % capacity_;
  Slot& slot = slots_[tail_];
  slot.data.clear();
  slot.used = false;
  --size_;
  // Идентификаторы сообщений должны оставаться уникальными, даже если
  // откатили незавершённую операцию. Поэтому счётчик не уменьшаем, чтобы
  // исключить повторную выдачу уже использованных значений.
  DEBUG_LOG("MessageBuffer: удалено последнее сообщение");
  return true;
}

// Проверка наличия сообщений
bool MessageBuffer::hasPending() const {
  return size_ > 0;
}

// Извлечение сообщения
bool MessageBuffer::pop(uint32_t& id, std::vector<uint8_t>& out) {
  if (size_ == 0) {                            // очередь пуста
    DEBUG_LOG("MessageBuffer: извлечение из пустой очереди");
    return false;
  }
  Slot& slot = slots_[head_];
  id = slot.id;                                // возвращаем идентификатор
  out.assign(slot.data.begin(), slot.data.end()); // копируем данные в выходной буфер
  slot.data.clear();
  slot.used = false;
  head_ = (head_ + 1) % capacity_;
  --size_;
  DEBUG_LOG_VAL("MessageBuffer: извлечён id=", id);
  return true;
}

const std::vector<uint8_t>* MessageBuffer::peek(uint32_t& id) const {
  if (size_ == 0) {                              // очередь пуста
    DEBUG_LOG("MessageBuffer: просмотр пустой очереди");
    return nullptr;
  }
  const Slot& slot = slots_[head_];
  id = slot.id;
  return slot.used ? &slot.data : nullptr;      // возвращаем указатель на данные без копирования
}

size_t MessageBuffer::slotSize() const {
  return slot_size_;
}
