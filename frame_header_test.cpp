#ifndef ARDUINO
// Этот файл содержит юнит‑тест заголовка кадра и не компилируется для прошивки
#include "frame.h"
#include <vector>
#include <iostream>
#include <assert.h>

// Простые тесты сериализации/десериализации заголовка
int main() {
  FrameHeader h{};
  h.ver = 1;
  h.flags = F_ACK_REQ | F_FRAG;
  h.msg_id = 0x11223344;
  h.frag_idx = 2;
  h.frag_cnt = 5;
  h.payload_len = 7;
  h.ack_mask = 0x55667788;
  h.hdr_crc = 0x1234;
  h.frame_crc = 0xABCD;

  std::vector<uint8_t> buf(FrameHeader::ENCODED_SIZE);
  assert(h.encode(buf.data(), buf.size()));

  FrameHeader h2{};
  assert(FrameHeader::decode(h2, buf.data(), buf.size()));
  assert(h2.ver == h.ver);
  assert(h2.flags == h.flags);
  assert(h2.msg_id == h.msg_id);
  assert(h2.frag_idx == h.frag_idx);
  assert(h2.frag_cnt == h.frag_cnt);
  assert(h2.payload_len == h.payload_len);
  assert(h2.ack_mask == h.ack_mask);
  assert(h2.hdr_crc == h.hdr_crc);
  assert(h2.frame_crc == h.frame_crc);

  // Проверяем отказ на усечённом буфере
  assert(!FrameHeader::decode(h2, buf.data(), buf.size()-1));

  std::cout << "frame_header_test ok\n";
  return 0;
}
#endif // !ARDUINO
