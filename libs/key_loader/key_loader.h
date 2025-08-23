#pragma once
#include <array>
#include <cstdint>
#include "default_settings.h"

namespace KeyLoader {
// Считывает ключ из файла key_storage/key.bin.
// При отсутствии файла возвращает DefaultSettings::DEFAULT_KEY.
std::array<uint8_t,16> loadKey();
}
