#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Модуль CCSDS звена: рандомизация -> FEC -> интерливинг
// Все функции работают в терминах байтов
namespace ccsds {
  // режимы FEC
  enum FecMode : uint8_t { FEC_OFF = 0, FEC_RS_VIT = 1, FEC_LDPC = 2 };

  struct Params {
    FecMode fec = FEC_OFF;          // выбранный режим FEC
    uint8_t interleave = 1;         // глубина интерливера (1=выключен)
    bool scramble = true;          // выполнять ли рандомизацию
  };

  // Кодирование: рандомизация -> FEC -> интерливинг
  void encode(const uint8_t* in, size_t len, uint32_t msg_id,
              const Params& p, std::vector<uint8_t>& out);

  // Декодирование: обратная последовательность.
  // Возвращает true при успешном исправлении ошибок.
  bool decode(const uint8_t* in, size_t len, uint32_t msg_id,
              const Params& p, std::vector<uint8_t>& out, int& corrected);
}

