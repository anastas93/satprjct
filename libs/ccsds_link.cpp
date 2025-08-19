#include "ccsds_link.h"
#include "../scrambler.h"
#include "../fec.h"
#include "../interleaver.h"

namespace ccsds {

// Кодирование: рандомизация -> FEC -> интерливинг
void encode(const uint8_t* in, size_t len, uint32_t msg_id,
            const Params& p, std::vector<uint8_t>& out) {
  std::vector<uint8_t> tmp(in, in + len);

  // Шаг 1: рандомизация
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

  // Шаг 3: интерливинг
  if (p.interleave > 1) {
    std::vector<uint8_t> buf;
    interleave_bytes(tmp.data(), tmp.size(), p.interleave, buf);
    tmp.swap(buf);
  }

  out.swap(tmp);
}

// Декодирование: обратная последовательность
bool decode(const uint8_t* in, size_t len, uint32_t msg_id,
            const Params& p, std::vector<uint8_t>& out, int& corrected) {
  std::vector<uint8_t> tmp(in, in + len);

  // Шаг 1: деинтерливинг
  if (p.interleave > 1) {
    std::vector<uint8_t> buf;
    deinterleave_bytes(tmp.data(), tmp.size(), p.interleave, buf);
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

  out.swap(tmp);
  return true;
}

} // namespace ccsds

