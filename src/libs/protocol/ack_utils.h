#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace protocol {
namespace ack {

// Единый маркер ACK (один байт 0x06)
constexpr uint8_t MARKER = 0x06;

// Проверяем полезную нагрузку на соответствие ACK
inline bool isAckPayload(const uint8_t* data, size_t len) {
  if (!data) return false;
  return len == 1 && data[0] == MARKER;
}

// Перегрузка для std::vector
inline bool isAckPayload(const std::vector<uint8_t>& data) {
  return isAckPayload(data.data(), data.size());
}

}  // namespace ack
}  // namespace protocol

