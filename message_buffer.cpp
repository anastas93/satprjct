#include "message_buffer.h"
#include "config.h"

MessageBuffer::MessageBuffer()
  : next_id_(1), total_bytes_(0), mode_(QosMode::Strict), rr_idx_(0) {}

uint32_t MessageBuffer::enqueue(const uint8_t* data, size_t len, bool ack_required) {
  return enqueueQos(data, len, ack_required, Qos::Normal);
}

uint32_t MessageBuffer::enqueueQos(const uint8_t* data, size_t len, bool ack_required, Qos q) {
  if (!data || len == 0) return 0;
  if (total_bytes_ + len > cfg::TX_BUF_MAX_BYTES) return 0;

  size_t* bucket = (q==Qos::High)   ? &bytesH_
                  : (q==Qos::Normal)? &bytesN_
                                    : &bytesL_;
  size_t cap     = (q==Qos::High)   ? cfg::TX_BUF_QOS_CAP_HIGH
                  : (q==Qos::Normal)? cfg::TX_BUF_QOS_CAP_NORMAL
                                    : cfg::TX_BUF_QOS_CAP_LOW;
  if ((*bucket) + len > cap) return 0;

  uint32_t id = next_id_++;
  OutgoingMessage m;
  m.id = id;
  m.ack_required = ack_required;
  m.qos = q;
  m.data.assign(data, data + len);

  if (q==Qos::High) {
    qH_.push_back(std::move(m));
    auto it = std::prev(qH_.end());
    index_[id] = {Qos::High, it};
  } else if (q==Qos::Normal) {
    qN_.push_back(std::move(m));
    auto it = std::prev(qN_.end());
    index_[id] = {Qos::Normal, it};
  } else {
    qL_.push_back(std::move(m));
    auto it = std::prev(qL_.end());
    index_[id] = {Qos::Low, it};
  }

  *bucket += len;
  total_bytes_ += len;
  return id;
}

bool MessageBuffer::hasPending() const {
  return !qH_.empty() || !qN_.empty() || !qL_.empty();
}

void MessageBuffer::setSchedulerMode(QosMode m) {
  mode_ = m;
  rr_idx_ = 0;
}

bool MessageBuffer::pick(OutgoingMessage& out) {
  auto pick_strict = [&]()->bool{
    if (!qH_.empty()) { out = qH_.front(); return true; }
    if (!qN_.empty()) { out = qN_.front(); return true; }
    if (!qL_.empty()) { out = qL_.front(); return true; }
    return false;
  };

  if (mode_ == QosMode::Strict) return pick_strict();

  // Weighted 4:2:1: H,H,H,H,N,N,L
  static const uint8_t pat[7] = {0,0,0,0,1,1,2};
  for (int tries = 0; tries < 7; ++tries) {
    uint8_t p = pat[rr_idx_];
    rr_idx_ = (uint8_t)((rr_idx_ + 1) % 7);
    if (p == 0) { if (!qH_.empty()) { out = qH_.front(); return true; } }
    else if (p == 1) { if (!qN_.empty()) { out = qN_.front(); return true; } }
    else { if (!qL_.empty()) { out = qL_.front(); return true; } }
  }
  return pick_strict();
}

bool MessageBuffer::peekNext(OutgoingMessage& out) {
  return pick(out);
}

void MessageBuffer::popFront() {
  if (!qH_.empty()) {
    index_.erase(qH_.front().id);
    total_bytes_ -= qH_.front().data.size();
    bytesH_ -= qH_.front().data.size();
    qH_.pop_front();
    return;
  }
  if (!qN_.empty()) {
    index_.erase(qN_.front().id);
    total_bytes_ -= qN_.front().data.size();
    bytesN_ -= qN_.front().data.size();
    qN_.pop_front();
    return;
  }
  if (!qL_.empty()) {
    index_.erase(qL_.front().id);
    total_bytes_ -= qL_.front().data.size();
    bytesL_ -= qL_.front().data.size();
    qL_.pop_front();
    return;
  }
}

void MessageBuffer::markAcked(uint32_t msg_id) {
  auto it = index_.find(msg_id);
  if (it == index_.end()) return;
  auto ref = it->second;
  size_t sz = ref.it->data.size();
  total_bytes_ -= sz;
  if (ref.qos == Qos::High) {
    bytesH_ -= sz;
    qH_.erase(ref.it);
  } else if (ref.qos == Qos::Normal) {
    bytesN_ -= sz;
    qN_.erase(ref.it);
  } else {
    bytesL_ -= sz;
    qL_.erase(ref.it);
  }
  index_.erase(it);
}

size_t MessageBuffer::size() const {
  return qH_.size() + qN_.size() + qL_.size();
}

void MessageBuffer::setNextId(uint32_t id) {
  if (id == 0) id = 1;
  next_id_ = id;
}
