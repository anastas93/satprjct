#include "message_cache.h"

// Добавление сообщения в очередь с учётом QoS
uint32_t MessageCache::enqueue(const uint8_t* data, size_t len, bool ack_required, Qos q) {
  return buf_.enqueueQos(data, len, ack_required, q);
}

// Получить следующее сообщение для отправки
bool MessageCache::peek(OutgoingMessage& out) {
  if (!buf_.peekNext(out)) return false;
  // после выбора переносим сообщение в архив буфера, чтобы
  // оно не появлялось повторно в очереди
  buf_.archive(out.id);
  // сохраняем копию для возможных повторов
  inflight_[out.id] = out;
  return true;
}

// Пометить сообщение как подтверждённое
void MessageCache::markAcked(uint32_t msg_id) {
  inflight_.erase(msg_id);
  buf_.markAcked(msg_id); // удаляет из архива
}

// Переместить сообщение в архив при полном исчерпании повторов
void MessageCache::archive(uint32_t msg_id) {
  inflight_.erase(msg_id); // сообщение уже находится в архиве buf_
  // ничего дополнительно делать не нужно
}

// Восстановить сообщения из архива
size_t MessageCache::restoreArchived(size_t count) {
  return buf_.restoreArchived(count);
}
