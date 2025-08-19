#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>

// Модуль CCSDS звена: вставка ASM, рандомизация, FEC и интерливинг
// Все функции работают в терминах байтов
namespace ccsds {
  // прикреплённая синхрометка (Attached Sync Marker)
  static constexpr uint32_t ASM = 0x1ACFFC1D;

  // режимы FEC
  enum FecMode : uint8_t { FEC_OFF = 0, FEC_RS_VIT = 1, FEC_LDPC = 2 };

  struct Params {
    FecMode fec = FEC_OFF;          // выбранный режим FEC
    uint8_t interleave = 1;         // глубина интерливера (1=выключен)
    bool scramble = true;          // выполнять ли рандомизацию
  };

  // Кодирование: ASM -> рандомизация -> FEC -> интерливинг
  void encode(const uint8_t* in, size_t len, uint32_t msg_id,
              const Params& p, std::vector<uint8_t>& out);

  // Декодирование: обратный порядок и проверка ASM.
  // Возвращает true при успешном исправлении ошибок и совпадении ASM.
  bool decode(const uint8_t* in, size_t len, uint32_t msg_id,
              const Params& p, std::vector<uint8_t>& out, int& corrected);
}

