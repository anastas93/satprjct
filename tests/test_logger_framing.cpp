#include <cassert>
#include <cstdint>
#include <string>

#include "../src/logger.h"
#include "../src/default_settings.h"

int main() {
  const std::string payload = "FRAME DATA";
  const uint32_t timestamp = 0x12345678;
  auto frame = Logger::makeFrame(Logger::RecordType::Text,
                                 DefaultSettings::LogLevel::WARN,
                                 timestamp,
                                 payload);
  Logger::RecordType parsedType;
  DefaultSettings::LogLevel parsedLevel;
  uint32_t parsedTimestamp = 0;
  std::string parsedPayload;
  bool ok = Logger::parseFrame(frame, parsedType, parsedLevel, parsedTimestamp, parsedPayload);
  assert(ok);
  assert(parsedType == Logger::RecordType::Text);
  assert(parsedLevel == DefaultSettings::LogLevel::WARN);
  assert(parsedTimestamp == timestamp);
  assert(parsedPayload == payload);

  // Проверяем защиту от порчи CRC
  frame.back() ^= 0xFF;
  Logger::RecordType dummyType;
  DefaultSettings::LogLevel dummyLevel;
  uint32_t dummyTimestamp = 0;
  std::string dummyPayload;
  bool invalid = Logger::parseFrame(frame, dummyType, dummyLevel, dummyTimestamp, dummyPayload);
  assert(!invalid);

  return 0;
}
