#pragma once
#include <cstddef>
#include "default_settings.h"              // доступ к значениям по умолчанию
#include "libs/config_loader/config_loader.h" // доступ к текущей конфигурации

namespace fec {

// Профиль кодирования, действующий в текущей конфигурации
struct Profile {
  bool use_rs = false;             // требуется ли код Рида — Соломона
  bool use_conv = false;           // активна ли свёртка
  bool use_bit_interleaver = false; // задействован ли битовый интерливинг
};

// Возвращает профиль кодирования на основании конфигурации
inline Profile currentProfile() {
  Profile profile;
  const auto& radio = ConfigLoader::getConfig().radio;
  profile.use_rs = radio.useRs;
  profile.use_conv = radio.useConv;
  profile.use_bit_interleaver = radio.useBitInterleaver;
  return profile;
}

// Константы размеров блоков для RS-кодирования
constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина исходного блока
constexpr size_t RS_ENC_LEN = 255;                                  // длина закодированного блока

} // namespace fec
