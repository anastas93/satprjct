#pragma once
#include <stdint.h>
#include <vector>
#include "frame.h"
#include "ack_bitmap.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include "tdd_scheduler.h"
#include "libs/qos.h"

// Класс для формирования и отправки ACK-кадров
class AckSender {
public:
  // Отправить ACK с указанными параметрами окна
  bool send(uint32_t highest, uint32_t bitmap,
            uint8_t window_size, bool hdr_dup,
            uint8_t fec, uint8_t inter);
};

