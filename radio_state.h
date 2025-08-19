#pragma once
#include <stdint.h>

// Состояние радио: простой, приём или передача
enum class RadioState : uint8_t { Idle, Rx, Tx };
