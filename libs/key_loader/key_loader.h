#pragma once
#include <array>
#include <cstdint>
#include "default_settings.h"

namespace KeyLoader {
// Загружает ключ из памяти (NVS на ESP32 или файл на ПК).
// При отсутствии значения возвращает DefaultSettings::DEFAULT_KEY.
std::array<uint8_t,16> loadKey();

// Сохраняет ключ в память (NVS или файл).
// Возвращает true при успешной записи.
bool saveKey(const std::array<uint8_t,16>& key);
}
