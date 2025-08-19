#ifndef ARDUINO
// Тест архивации и восстановления сообщений в буфере
#include <cstdio>
#include <vector>
#include "message_buffer.h"

int main() {
  MessageBuffer buf;
  uint8_t a1[]{1,2,3};
  uint8_t a2[]{4,5};
  bool ok = true;
  uint32_t id1 = buf.enqueue(a1, sizeof(a1), true);
  uint32_t id2 = buf.enqueue(a2, sizeof(a2), true);
  buf.archive(id1);
  std::vector<uint32_t> ids;
  buf.listArchived(ids);
  if (ids.size() != 1 || ids[0] != id1) { std::printf("archive step fail\n"); ok = false; }
  if (buf.size() != 1) { std::printf("queue size fail\n"); ok = false; }
  if (!buf.restoreArchived()) { std::printf("restore fail\n"); ok = false; }
  OutgoingMessage m;
  buf.peekNext(m);
  if (m.id != id1) { std::printf("restored not front\n"); ok = false; }
  buf.popFront();
  buf.peekNext(m);
  if (m.id != id2) { std::printf("order fail\n"); ok = false; }
  std::printf("archive_restore_test %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
#endif // !ARDUINO
