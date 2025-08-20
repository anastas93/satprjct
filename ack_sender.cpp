#include "ack_sender.h"
#include "config.h"
#include <string.h>

// Формирование и отправка ACK-кадра
bool AckSender::send(uint32_t highest, uint32_t bitmap, // QOS: Высокий Формирование и отправка ACK
                     uint8_t window_size, bool hdr_dup,
                     uint8_t fec, uint8_t inter) {
  // Проверяем, можно ли отправлять ACK в текущем слоте
  if (!tdd::isAckPhase()) return false;
  (void)window_size; // пока не используется, но оставлен для совместимости

  FrameHeader ack{};
  ack.ver = cfg::PIPE_VERSION;
  ack.flags = F_ACK;
  ack.msg_id = highest;
  ack.frag_idx = 0; ack.frag_cnt = 0; ack.payload_len = 8;
  ack.ack_mask = bitmap;
  ack.hdr_crc = 0; ack.frame_crc = 0;
  uint8_t hdr_buf[FRAME_HEADER_SIZE];
  ack.encode(hdr_buf, FRAME_HEADER_SIZE);
  AckBitmap pl{highest, bitmap};
  uint8_t payload[8];
  ack_encode(pl, payload);
  uint16_t hcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE - 4, 0xFFFF);
  ack.hdr_crc = hcrc;
  ack.encode(hdr_buf, FRAME_HEADER_SIZE);
  uint16_t fcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE, 0xFFFF);
  fcrc = crc16_ccitt(payload, 8, fcrc);
  ack.frame_crc = fcrc;
  ack.encode(hdr_buf, FRAME_HEADER_SIZE);

  size_t frame_len = FRAME_HEADER_SIZE + 8;
  if (hdr_dup) frame_len += FRAME_HEADER_SIZE;
  std::vector<uint8_t> frame(frame_len);
  memcpy(frame.data(), hdr_buf, FRAME_HEADER_SIZE);
  size_t off = FRAME_HEADER_SIZE;
  if (hdr_dup) {
    memcpy(frame.data() + off, hdr_buf, FRAME_HEADER_SIZE);
    off += FRAME_HEADER_SIZE;
  }
  memcpy(frame.data() + off, payload, 8);

  if (!Radio_sendRaw(frame.data(), frame.size(), Qos::High)) return false;
  FrameLog::push('T', frame.data(), frame.size(),
                 0, fec, inter,
                 0.0f, 0.0f, 0.0f,
                 0, 0, 0,
                 0,
                 8);
  tdd::maintain();
  return true;
}

