#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace protocol {
namespace ack {

// Единый маркер ACK (один байт 0x06)
constexpr uint8_t MARKER = 0x06;

// Пропускаем префикс вида [TAG|N] при наличии
inline size_t skipPrefix(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;
  if (data[0] != '[') return 0;
  for (size_t i = 1; i < len; ++i) {
    if (data[i] == ']') return i + 1;
  }
  return 0;  // некорректный префикс
}

// Проверяем полезную нагрузку на соответствие ACK
inline bool isAckPayload(const uint8_t* data, size_t len) {
  if (!data) return false;
  if (len == 1 && data[0] == MARKER) return true;
  size_t offset = skipPrefix(data, len);
  if (len < offset + 3) return false;
  return data[offset] == 'A' && data[offset + 1] == 'C' && data[offset + 2] == 'K';
}

// Перегрузка для std::vector
inline bool isAckPayload(const std::vector<uint8_t>& data) {
  return isAckPayload(data.data(), data.size());
}

}  // namespace ack
}  // namespace protocol

