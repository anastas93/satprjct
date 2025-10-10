#include "lotest_protocol.h"

#include <algorithm>
#include <cstring>

namespace Lotest {
namespace {
constexpr uint8_t kFrameTypeMask = 0x03;   // младшие 2 бита определяют тип
constexpr uint8_t kPayloadSizeMask = 0x1F; // 5 бит достаточно для значения 0-31
constexpr uint8_t kDefaultInterleaveStep = 4;
constexpr uint8_t kRsFieldGenerator = 0x1D; // примитивный многочлен GF(256)

uint8_t gfAdd(uint8_t a, uint8_t b) {
  return a ^ b; // сложение в поле GF(256)
}

} // namespace

FrameType Frame::type() const {
  return static_cast<FrameType>(bytes[0] & kFrameTypeMask);
}

void Frame::setType(FrameType type) {
  bytes[0] = static_cast<uint8_t>((bytes[0] & ~kFrameTypeMask) | (static_cast<uint8_t>(type) & kFrameTypeMask));
}

uint8_t Frame::sequence() const {
  return bytes[1];
}

void Frame::setSequence(uint8_t seq) {
  bytes[1] = seq;
}

uint8_t Frame::payloadSize() const {
  return static_cast<uint8_t>(bytes[2] & kPayloadSizeMask);
}

void Frame::setPayloadSize(uint8_t size) {
  bytes[2] = static_cast<uint8_t>((bytes[2] & ~kPayloadSizeMask) | (size & kPayloadSizeMask));
}

uint8_t Frame::flags() const {
  return bytes[2] & static_cast<uint8_t>(~kPayloadSizeMask);
}

void Frame::setFlags(uint8_t value) {
  const uint8_t payload = static_cast<uint8_t>(bytes[2] & kPayloadSizeMask);
  bytes[2] = static_cast<uint8_t>(value | payload);
}

std::array<uint8_t, kPayloadSize> Frame::payload() const {
  std::array<uint8_t, kPayloadSize> result{};
  std::copy(bytes.begin() + 3, bytes.end(), result.begin());
  return result;
}

void Frame::setPayload(const uint8_t* data, size_t len) {
  std::fill(bytes.begin() + 3, bytes.end(), 0);
  if (len > kPayloadSize) {
    len = kPayloadSize;
  }
  if (data && len > 0) {
    std::memcpy(bytes.data() + 3, data, len);
  }
  setPayloadSize(static_cast<uint8_t>(len));
}

std::vector<uint8_t> buildInterleavingOrder(size_t count, uint8_t step) {
  if (count == 0) {
    return {};
  }
  if (step == 0) {
    step = kDefaultInterleaveStep;
  }
  std::vector<uint8_t> order;
  order.reserve(count);
  std::vector<bool> used(count, false);
  uint8_t position = 0;
  for (size_t i = 0; i < count; ++i) {
    while (used[position]) {
      position = static_cast<uint8_t>((position + 1) % count);
    }
    order.push_back(position);
    used[position] = true;
    position = static_cast<uint8_t>((position + step) % count);
  }
  return order;
}

uint16_t crc16(const uint8_t* data, size_t len, uint16_t initial) {
  uint16_t crc = initial;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x8000U) != 0U) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

uint16_t crc16(const std::vector<uint8_t>& data, uint16_t initial) {
  return crc16(data.data(), data.size(), initial);
}

uint8_t crc8(const uint8_t* data, size_t len, uint8_t initial) {
  uint8_t crc = initial;
  for (size_t i = 0; i < len; ++i) {
    uint8_t byte = data[i];
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      if ((crc & 0x80U) != 0U) {
        crc = static_cast<uint8_t>((crc << 1) ^ 0x31U);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

std::optional<AckBitmap> parseAck(const Frame& frame) {
  if (frame.type() != FrameType::Ack) {
    return std::nullopt;
  }
  AckBitmap ack;
  ack.baseSeq = frame.sequence();
  ack.count = frame.payloadSize();
  const auto payload = frame.payload();
  ack.bitmap = static_cast<uint16_t>(payload[0]) | (static_cast<uint16_t>(payload[1]) << 8);
  ack.needParity = (frame.flags() & FrameFlags::kNeedParity) != 0;
  return ack;
}

Frame buildAck(const AckBitmap& ack) {
  Frame frame;
  frame.setType(FrameType::Ack);
  frame.setSequence(ack.baseSeq);
  frame.setPayloadSize(ack.count);
  uint8_t flags = ack.needParity ? FrameFlags::kNeedParity : 0;
  frame.setFlags(flags);
  std::array<uint8_t, kPayloadSize> payload{};
  payload[0] = static_cast<uint8_t>(ack.bitmap & 0xFFU);
  payload[1] = static_cast<uint8_t>((ack.bitmap >> 8) & 0xFFU);
  payload[2] = 0;
  payload[3] = 0;
  payload[4] = 0;
  frame.setPayload(payload.data(), payload.size());
  return frame;
}

ReedSolomon1511::ReedSolomon1511() {
  alphaTo_.fill(0);
  indexOf_.fill(0);
  uint8_t mask = 1;
  alphaTo_[0] = 1;
  for (int i = 1; i < 255; ++i) {
    mask <<= 1;
    if (mask & 0x100U) {
      mask ^= kRsFieldGenerator;
    }
    alphaTo_[i] = mask;
  }
  for (int i = 0; i < 255; ++i) {
    indexOf_[alphaTo_[i]] = static_cast<uint8_t>(i);
  }
  indexOf_[0] = 255;

  generator_[0] = 1;
  for (size_t i = 1; i <= kRsParitySymbols; ++i) {
    generator_[i] = 1;
    for (size_t j = i; j > 0; --j) {
      if (generator_[j] == 0) {
        generator_[j] = generator_[j - 1];
      } else {
        uint8_t idx = indexOf_[generator_[j]];
        uint8_t add = static_cast<uint8_t>(idx + i);
        if (add >= 255) {
          add -= 255;
        }
        generator_[j] = static_cast<uint8_t>(gfAdd(alphaTo_[add], generator_[j - 1]));
      }
    }
    generator_[0] = alphaTo_[indexOf_[generator_[0]] + i >= 255 ? indexOf_[generator_[0]] + i - 255 : indexOf_[generator_[0]] + i];
  }
}

uint8_t ReedSolomon1511::gfMul(uint8_t a, uint8_t b, const std::array<uint8_t, 256>& alphaTo,
                               const std::array<uint8_t, 256>& indexOf) {
  if (a == 0 || b == 0) {
    return 0;
  }
  uint16_t sum = static_cast<uint16_t>(indexOf[a]) + static_cast<uint16_t>(indexOf[b]);
  if (sum >= 255) {
    sum -= 255;
  }
  return alphaTo[sum];
}

std::array<uint8_t, kRsParitySymbols> ReedSolomon1511::encode(const std::array<uint8_t, kRsDataSymbols>& data) const {
  std::array<uint8_t, kRsParitySymbols> parity{};
  for (uint8_t byte : data) {
    uint8_t feedback = static_cast<uint8_t>(byte ^ parity[0]);
    for (size_t j = 0; j < kRsParitySymbols - 1; ++j) {
      parity[j] = static_cast<uint8_t>(parity[j + 1] ^ gfMul(generator_[kRsParitySymbols - j - 1], feedback, alphaTo_, indexOf_));
    }
    parity[kRsParitySymbols - 1] = gfMul(generator_[0], feedback, alphaTo_, indexOf_);
  }
  return parity;
}

} // namespace Lotest

