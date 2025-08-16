
#include "frame_log.h"
#include "config.h"
#include <string.h>

namespace {
  struct Item { char dir; uint16_t len; uint8_t buf[cfg::LORA_MTU]; };
  static constexpr size_t CAP = 128;
  Item ring[CAP];
  size_t head = 0, stored = 0;
}

void FrameLog::push(char dir, const uint8_t* data, size_t len) {
  if (!data || len==0) return;
  size_t n = len > cfg::LORA_MTU ? cfg::LORA_MTU : len;
  Item& it = ring[head];
  it.dir = dir;
  it.len = (uint16_t)n;
  memcpy(it.buf, data, n);
  head = (head + 1) % CAP;
  if (stored < CAP) stored++; 
}

void FrameLog::dump(Print& out, unsigned int count) {
  if (stored == 0) { out.println(F("(empty)")); return; }
  if (count == 0 || count > stored) count = stored;
  size_t idx = (head + CAP - stored) % CAP;
  size_t to_print = stored;
  if (to_print > count) {
    idx = (idx + (to_print - count)) % CAP;
    to_print = count;
  }
  for (size_t i=0; i<to_print; ++i) {
    const Item& it = ring[(idx + i) % CAP];
    out.printf("[%c] %u bytes: ", it.dir, (unsigned)it.len);
    for (uint16_t j=0;j<it.len;j++) {
      uint8_t b = it.buf[j];
      const char* hex = "0123456789ABCDEF";
      char s[3]; s[0]=hex[b>>4]; s[1]=hex[b&0xF]; s[2]=0;
      out.print(s);
    }
    out.println();
  }
}
