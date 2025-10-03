#include "key_transfer_waiter.h"

void KeyTransferWaiter::start(uint32_t now_ms,
                              uint32_t timeout_ms,
                              bool serial_watcher,
                              bool http_watcher) {
  waiting_ = true;
  serial_watcher_ = serial_watcher;
  http_watcher_ = http_watcher;
  serial_pending_ = false;
  http_pending_ = false;
  started_at_ms_ = now_ms;
  timeout_ms_ = timeout_ms;
  result_json_.clear();
}

void KeyTransferWaiter::finalizeSuccess(const std::string& json_result) {
  finalize(json_result);
}

void KeyTransferWaiter::finalizeError(const std::string& json_result) {
  finalize(json_result);
}

void KeyTransferWaiter::clear() {
  waiting_ = false;
  serial_watcher_ = false;
  http_watcher_ = false;
  serial_pending_ = false;
  http_pending_ = false;
  started_at_ms_ = 0;
  timeout_ms_ = 0;
  result_json_.clear();
}

bool KeyTransferWaiter::isExpired(uint32_t now_ms) const {
  if (!waiting_) return false;
  if (timeout_ms_ == 0) return false;
  return static_cast<uint32_t>(now_ms - started_at_ms_) >= timeout_ms_;
}

std::string KeyTransferWaiter::consumeSerial() {
  if (!serial_pending_) return std::string();
  std::string out = result_json_;
  serial_pending_ = false;
  serial_watcher_ = false;
  dropResultIfDelivered();
  return out;
}

std::string KeyTransferWaiter::consumeHttp() {
  if (!http_pending_) return std::string();
  std::string out = result_json_;
  http_pending_ = false;
  http_watcher_ = false;
  dropResultIfDelivered();
  return out;
}

void KeyTransferWaiter::finalize(const std::string& json_result) {
  waiting_ = false;
  result_json_ = json_result;
  serial_pending_ = serial_watcher_;
  http_pending_ = http_watcher_;
  started_at_ms_ = 0;
  timeout_ms_ = 0;
}

void KeyTransferWaiter::dropResultIfDelivered() {
  if (!serial_pending_ && !http_pending_) {
    result_json_.clear();
  }
}
