#include "ccsds_link.h"
// Заголовки лежат в этом же каталоге библиотеки, поэтому подключаем их напрямую.
// Использование относительных путей вроде "../../scrambler.h" раньше вызывало
// ошибку компиляции «No such file or directory» в Arduino IDE.
#include "scrambler.h"
#include "fec.h"
#include "interleaver.h"

namespace ccsds {

// Кодирование: ASM -> рандомизация -> FEC -> интерливинг
void encode(const uint8_t* in, size_t len, uint32_t msg_id,
            const Params& p, std::vector<uint8_t>& out) {
  // добавляем синхрометку в начало
  std::vector<uint8_t> tmp;
  tmp.reserve(len + 4);
  tmp.push_back((uint8_t)(ASM >> 24));
  tmp.push_back((uint8_t)(ASM >> 16));
  tmp.push_back((uint8_t)(ASM >> 8));
  tmp.push_back((uint8_t)(ASM));
  tmp.insert(tmp.end(), in, in + len);

  // Шаг 1: рандомизация всего блока
  if (p.scramble && !tmp.empty()) {
    lfsr_scramble(tmp.data(), tmp.size(), (uint16_t)msg_id);
  }

  // Шаг 2: FEC
  if (p.fec == FEC_RS_VIT) {
    std::vector<uint8_t> buf;
    fec_encode_rs_viterbi(tmp.data(), tmp.size(), buf);
    tmp.swap(buf);
  } else if (p.fec == FEC_LDPC) {
    std::vector<uint8_t> buf;
    fec_encode_ldpc(tmp.data(), tmp.size(), buf);
    tmp.swap(buf);
  }

  // Шаг 3: интерливинг с ограничением 4–16
  size_t depth = (p.interleave >= 4 && p.interleave <= 16) ? p.interleave : 1;
  if (depth > 1) {
    std::vector<uint8_t> buf;
    interleave_bytes(tmp.data(), tmp.size(), depth, buf);
    tmp.swap(buf);
  }

  out.swap(tmp);
}

// Декодирование: обратный порядок и проверка ASM
bool decode(const uint8_t* in, size_t len, uint32_t msg_id,
            const Params& p, std::vector<uint8_t>& out, int& corrected) {
  std::vector<uint8_t> tmp(in, in + len);

  // Шаг 1: деинтерливинг
  size_t depth = (p.interleave >= 4 && p.interleave <= 16) ? p.interleave : 1;
  if (depth > 1) {
    std::vector<uint8_t> buf;
    deinterleave_bytes(tmp.data(), tmp.size(), depth, buf);
    tmp.swap(buf);
  }

  bool ok = true;
  corrected = 0;
  // Шаг 2: FEC
  if (p.fec == FEC_RS_VIT) {
    std::vector<uint8_t> buf;
    ok = fec_decode_rs_viterbi(tmp.data(), tmp.size(), buf, corrected);
    tmp.swap(buf);
  } else if (p.fec == FEC_LDPC) {
    std::vector<uint8_t> buf;
    ok = fec_decode_ldpc(tmp.data(), tmp.size(), buf, corrected);
    tmp.swap(buf);
  }
  if (!ok) return false;

  // Шаг 3: деррандомизация
  if (p.scramble && !tmp.empty()) {
    lfsr_descramble(tmp.data(), tmp.size(), (uint16_t)msg_id);
  }

  // Шаг 4: проверка ASM
  if (tmp.size() < 4) return false;
  uint32_t got = (uint32_t(tmp[0]) << 24) | (uint32_t(tmp[1]) << 16) |
                 (uint32_t(tmp[2]) << 8) | uint32_t(tmp[3]);
  if (got != ASM) return false;
  out.assign(tmp.begin() + 4, tmp.end());
  return true;
}

} // namespace ccsds

