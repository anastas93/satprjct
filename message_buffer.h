#pragma once
#include <vector>
#include <list>
#include <stdint.h>
#include <unordered_map>

enum class Qos : uint8_t { High=0, Normal=1, Low=2 };
enum class QosMode : uint8_t { Strict=0, Weighted421=1 };

struct OutgoingMessage {
  uint32_t id = 0;
  bool     ack_required = false;
  Qos      qos = Qos::Normal;
  std::vector<uint8_t> data;
};

class MessageBuffer {
public:
  MessageBuffer();

  uint32_t enqueue(const uint8_t* data, size_t len, bool ack_required);
  uint32_t enqueueQos(const uint8_t* data, size_t len, bool ack_required, Qos q);

  bool     hasPending() const;
  bool     peekNext(OutgoingMessage& out);
  void     popFront();
  void     markAcked(uint32_t msg_id);

  size_t   size() const;

  void     setNextId(uint32_t id);
  uint32_t nextId() const { return next_id_; }

  // QoS admin/status
  void     setSchedulerMode(QosMode m);
  QosMode  schedulerMode() const { return mode_; }
  size_t   lenH() const { return qH_.size(); }
  size_t   lenN() const { return qN_.size(); }
  size_t   lenL() const { return qL_.size(); }
  size_t   bytesH() const { return bytesH_; }
  size_t   bytesN() const { return bytesN_; }
  size_t   bytesL() const { return bytesL_; }

private:
  bool     pick(OutgoingMessage& out);

  std::list<OutgoingMessage> qH_, qN_, qL_;
  uint32_t next_id_;
  size_t   total_bytes_;
  size_t   bytesH_ = 0, bytesN_ = 0, bytesL_ = 0;

  struct MsgRef { Qos qos; std::list<OutgoingMessage>::iterator it; };
  std::unordered_map<uint32_t, MsgRef> index_;

  QosMode  mode_ = QosMode::Strict;
  uint8_t  rr_idx_ = 0;   // для Weighted421
};
