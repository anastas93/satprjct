#include "message_buffer.h"
#include <cstdio>
#include <cstring>

// Проверка сжатия и восстановления сообщений из архива
int main() {
  MessageBuffer buf;
  const char payload[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"; // 50 'A'
  size_t len = sizeof(payload) - 1;
  uint32_t id = buf.enqueue(reinterpret_cast<const uint8_t*>(payload), len, false);
  buf.archive(id);
  bool sizeReduced = buf.archivedBytes() < len;
  bool restored = buf.restoreArchived();
  OutgoingMessage m;
  bool peek_ok = buf.peekNext(m);
  bool data_ok = peek_ok && m.data.size() == len && std::memcmp(m.data.data(), payload, len) == 0;
  std::printf("sizeReduced=%d restored=%d data_ok=%d\n", sizeReduced, restored, data_ok);
  bool ok = sizeReduced && restored && data_ok;
  std::puts(ok ? "TEST OK" : "TEST FAIL");
  return ok ? 0 : 1;
}
