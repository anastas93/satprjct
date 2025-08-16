
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

namespace FrameLog {
  void push(char dir, const uint8_t* data, size_t len);
  void dump(Print& out, unsigned int count);
}
