#include "key_transfer.h"
#include <algorithm>
#include <cstring>
#include "libs/frame/frame_header.h"
#include "libs/scrambler/scrambler.h"
#include "libs/crypto/aes_ccm.h"

namespace KeyTransfer {
namespace {

constexpr size_t MAX_FRAME_SIZE = 245;                   // ограничение SX1262
constexpr size_t MAX_FRAGMENT_LEN =
    MAX_FRAME_SIZE - FrameHeader::SIZE * 2 - (MAX_FRAME_SIZE / 64) * 2;
constexpr uint32_t NONCE_SALT = 0x4B54524E;              // "KTRN" — соль нонса

// Статический корневой ключ AES-CCM для защищённого обмена
const std::array<uint8_t,16> ROOT_KEY{
  0x4B, 0x54, 0x52, 0x46, // "KTRF"
  0x2D, 0x52, 0x4F, 0x4F, // "-ROO"
  0x54, 0x2D, 0x4B, 0x45, // "T-KE"
  0x59, 0x21, 0x01, 0x00  // "Y!\x01\x00"
};

// Добавление пилотов каждые 64 байта
std::vector<uint8_t> insertPilots(const std::vector<uint8_t>& in) {
  std::vector<uint8_t> out;
  out.reserve(in.size() + in.size() / 64 * 2);
  size_t count = 0;
  for (uint8_t b : in) {
    if (count && count % 64 == 0) {
      out.push_back(0x55);
      out.push_back(0x2D);
    }
    out.push_back(b);
    ++count;
  }
  return out;
}

// Удаление пилотов из полезной нагрузки
void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) return;
  out.reserve(len);
  size_t count = 0;
  for (size_t i = 0; i < len; ++i) {
    if (count && count % 64 == 0 && i + 1 < len && data[i] == 0x55 && data[i + 1] == 0x2D) {
      ++i;
      continue;
    }
    out.push_back(data[i]);
    ++count;
  }
}

} // namespace

const std::array<uint8_t,16>& rootKey() { return ROOT_KEY; }

std::array<uint8_t,12> makeNonce(uint32_t msg_id, uint16_t frag_idx) {
  std::array<uint8_t,12> nonce{};
  nonce[0] = static_cast<uint8_t>(msg_id & 0xFF);
  nonce[1] = static_cast<uint8_t>((msg_id >> 8) & 0xFF);
  nonce[2] = static_cast<uint8_t>((msg_id >> 16) & 0xFF);
  nonce[3] = static_cast<uint8_t>((msg_id >> 24) & 0xFF);
  nonce[4] = static_cast<uint8_t>(frag_idx & 0xFF);
  nonce[5] = static_cast<uint8_t>((frag_idx >> 8) & 0xFF);
  nonce[6] = static_cast<uint8_t>(NONCE_SALT & 0xFF);
  nonce[7] = static_cast<uint8_t>((NONCE_SALT >> 8) & 0xFF);
  nonce[8] = static_cast<uint8_t>((NONCE_SALT >> 16) & 0xFF);
  nonce[9] = static_cast<uint8_t>((NONCE_SALT >> 24) & 0xFF);
  nonce[10] = static_cast<uint8_t>(((NONCE_SALT >> 16) & 0xFF) ^ (msg_id & 0xFF));
  nonce[11] = static_cast<uint8_t>(((NONCE_SALT >> 24) & 0xFF) ^ ((msg_id >> 8) & 0xFF));
  return nonce;
}

bool buildFrame(uint32_t msg_id,
                const std::array<uint8_t,32>& public_key,
                const std::array<uint8_t,4>& key_id,
                std::vector<uint8_t>& frame_out) {
  frame_out.clear();
  std::vector<uint8_t> plain;
  plain.reserve(4 + 1 + 3 + key_id.size() + public_key.size());
  plain.insert(plain.end(), std::begin(MAGIC), std::end(MAGIC));
  plain.push_back(VERSION);
  plain.push_back(0);
  plain.push_back(0);
  plain.push_back(0);
  plain.insert(plain.end(), key_id.begin(), key_id.end());
  plain.insert(plain.end(), public_key.begin(), public_key.end());

  auto nonce = makeNonce(msg_id, 0);
  std::vector<uint8_t> cipher;
  std::vector<uint8_t> tag;
  if (!encrypt_ccm(rootKey().data(), rootKey().size(),
                   nonce.data(), nonce.size(),
                   nullptr, 0,
                   plain.data(), plain.size(),
                   cipher, tag, TAG_LEN)) {
    return false;
  }
  cipher.insert(cipher.end(), tag.begin(), tag.end());
  if (cipher.size() > MAX_FRAGMENT_LEN) return false;

  FrameHeader hdr;
  hdr.msg_id = msg_id;
  hdr.frag_idx = 0;
  hdr.frag_cnt = 1;
  hdr.payload_len = static_cast<uint16_t>(cipher.size());
  uint8_t hdr_buf[FrameHeader::SIZE];
  if (!hdr.encode(hdr_buf, sizeof(hdr_buf), cipher.data(), cipher.size())) {
    return false;
  }

  frame_out.reserve(FrameHeader::SIZE * 2 + cipher.size() + cipher.size() / 32);
  frame_out.insert(frame_out.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
  frame_out.insert(frame_out.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
  auto payload = insertPilots(cipher);
  frame_out.insert(frame_out.end(), payload.begin(), payload.end());
  scrambler::scramble(frame_out.data(), frame_out.size());
  return true;
}

bool parseFrame(const uint8_t* frame, size_t len,
                std::array<uint8_t,32>& public_key,
                std::array<uint8_t,4>& key_id,
                uint32_t& msg_id_out) {
  if (!frame || len < FrameHeader::SIZE * 2) return false;
  std::vector<uint8_t> buf(frame, frame + len);
  scrambler::descramble(buf.data(), buf.size());
  FrameHeader hdr;
  bool ok = FrameHeader::decode(buf.data(), buf.size(), hdr);
  if (!ok) {
    ok = FrameHeader::decode(buf.data() + FrameHeader::SIZE,
                             buf.size() - FrameHeader::SIZE, hdr);
  }
  if (!ok) return false;
  if (hdr.frag_cnt != 1 || hdr.frag_idx != 0) return false;

  const uint8_t* payload_p = buf.data() + FrameHeader::SIZE * 2;
  size_t payload_len = buf.size() - FrameHeader::SIZE * 2;
  std::vector<uint8_t> payload;
  removePilots(payload_p, payload_len, payload);
  if (payload.size() != hdr.payload_len) return false;
  if (!hdr.checkFrameCrc(payload.data(), payload.size())) return false;
  if (payload.size() < TAG_LEN) return false;

  size_t cipher_len = payload.size() - TAG_LEN;
  const uint8_t* tag = payload.data() + cipher_len;
  auto nonce = makeNonce(hdr.msg_id, hdr.frag_idx);
  std::vector<uint8_t> plain;
  if (!decrypt_ccm(rootKey().data(), rootKey().size(),
                   nonce.data(), nonce.size(),
                   nullptr, 0,
                   payload.data(), cipher_len,
                   tag, TAG_LEN, plain)) {
    return false;
  }
  if (plain.size() < 4 + 1 + 3 + key_id.size() + public_key.size()) return false;
  if (!std::equal(std::begin(MAGIC), std::end(MAGIC), plain.begin())) return false;
  if (plain[4] != VERSION) return false;

  std::copy_n(plain.data() + 8, key_id.size(), key_id.begin());
  std::copy_n(plain.data() + 12, public_key.size(), public_key.begin());
  msg_id_out = hdr.msg_id;
  return true;
}

} // namespace KeyTransfer
