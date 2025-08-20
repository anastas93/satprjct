#pragma once
#include "message_buffer.h"
#include <unordered_map>
#include <vector>

// Кеш исходных сообщений, который хранит их до получения ACK
// и позволяет архивировать и восстанавливать сообщения по QoS.
class MessageCache {
public:
  MessageCache() = default;

  // Поместить сообщение в очередь с учётом приоритета QoS
  uint32_t enqueue(const uint8_t* data, size_t len, bool ack_required, Qos q = Qos::Normal);
  uint32_t enqueueQos(const uint8_t* data, size_t len, bool ack_required, Qos q) {
    return enqueue(data, len, ack_required, q);
  }

  // Получить следующее сообщение без удаления из очереди
  bool peek(OutgoingMessage& out);

  // Отметить сообщение как подтверждённое и удалить его
  void markAcked(uint32_t msg_id);

  // Переместить сообщение в архив после исчерпания попыток ACK
  void archive(uint32_t msg_id);

  // Восстановить до count сообщений из архива в активную очередь
  size_t restoreArchived(size_t count);
  bool restoreArchived() { return restoreArchived(1) > 0; }

  bool hasPending() const { return buf_.hasPending(); }
  void setSchedulerMode(QosMode m) { buf_.setSchedulerMode(m); }
  QosMode schedulerMode() const { return buf_.schedulerMode(); }
  size_t lenH() const { return buf_.lenH(); }
  size_t lenN() const { return buf_.lenN(); }
  size_t lenL() const { return buf_.lenL(); }
  size_t bytesH() const { return buf_.bytesH(); }
  size_t bytesN() const { return buf_.bytesN(); }
  size_t bytesL() const { return buf_.bytesL(); }
  size_t size() const { return buf_.size(); }
  void setNextId(uint32_t id) { buf_.setNextId(id); }
  uint32_t nextId() const { return buf_.nextId(); }
  void listArchived(std::vector<uint32_t>& out) const { buf_.listArchived(out); }

private:
  MessageBuffer buf_;
  std::unordered_map<uint32_t, OutgoingMessage> inflight_;
};

