#pragma once
#include "radio_sx1262.h"

// Значения параметров радиомодуля по умолчанию
namespace DefaultSettings {
  constexpr ChannelBank BANK = ChannelBank::EAST; // Банк каналов
  constexpr uint8_t CHANNEL = 0;                   // Номер канала
  constexpr uint8_t POWER_PRESET = 9;             // Индекс мощности
  constexpr uint8_t BW_PRESET = 3;                // Индекс полосы
  constexpr uint8_t SF_PRESET = 2;                // Индекс фактора расширения
  constexpr uint8_t CR_PRESET = 0;                // Индекс коэффициента кодирования
}

