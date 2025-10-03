#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "http_body_reader.h"

// Макет клиента, имитирующий чтение Wi-Fi потока с JPEG и нулевыми байтами.
class MockClient {
public:
  explicit MockClient(std::vector<uint8_t> data)
      : buffer_(std::move(data)) {}

  int read(uint8_t* dst, size_t len) {
    if (position_ >= buffer_.size()) {
      connected_ = false;
      return 0;
    }
    size_t chunk = std::min(len, buffer_.size() - position_);
    std::memcpy(dst, buffer_.data() + position_, chunk);
    position_ += chunk;
    if (position_ >= buffer_.size()) {
      connected_ = false;
    }
    return static_cast<int>(chunk);
  }

  int read() {
    if (position_ >= buffer_.size()) {
      connected_ = false;
      return -1;
    }
    int value = static_cast<int>(buffer_[position_++]);
    if (position_ >= buffer_.size()) {
      connected_ = false;
    }
    return value;
  }

  int available() const {
    return static_cast<int>(buffer_.size() - position_);
  }

  bool connected() const {
    return connected_ || available() > 0;
  }

private:
  std::vector<uint8_t> buffer_;
  size_t position_ = 0;
  mutable bool connected_ = true;
};

int main() {
  // Минимальное JPEG-подобное тело с нулевыми байтами внутри.
  std::vector<uint8_t> jpeg = {
      0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46,
      0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x60,
      0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xDB,
      0x00, 0x43, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01,
      0x01, 0x00, 0x00, 0x3F, 0x00, 0x00, 0xFF, 0xD9};

  MockClient client(jpeg);
  std::vector<uint8_t> payload;
  bool lengthExceeded = false;
  bool incomplete = false;

  bool ok = HttpBodyReader::readBodyToVector(
      client,
      jpeg.size(),
      4096,
      payload,
      []() {},
      lengthExceeded,
      incomplete);

  assert(ok);
  assert(!lengthExceeded);
  assert(!incomplete);
  assert(payload == jpeg);
  assert(payload[20] == 0x00);
  assert(payload[21] == 0x00);

  // Проверяем чтение без известной длины.
  MockClient clientUnknown(jpeg);
  std::vector<uint8_t> payloadUnknown;
  lengthExceeded = false;
  incomplete = false;
  ok = HttpBodyReader::readBodyToVector(
      clientUnknown,
      static_cast<size_t>(-1),
      4096,
      payloadUnknown,
      []() {},
      lengthExceeded,
      incomplete);

  assert(ok);
  assert(!lengthExceeded);
  assert(!incomplete);
  assert(payloadUnknown == jpeg);

  return 0;
}

