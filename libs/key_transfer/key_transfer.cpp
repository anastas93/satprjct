#include "key_transfer.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "libs/frame/frame_header.h"
#include "libs/scrambler/scrambler.h"
#include "libs/crypto/aes_ccm.h"
#include "libs/crypto/chacha20_poly1305.h"
#include "libs/crypto/ed25519.h"

namespace KeyTransfer {
namespace {

constexpr size_t MAX_FRAME_SIZE = 245;                   // ограничение SX1262
constexpr size_t PILOT_INTERVAL = 64;                    // период пилотов
constexpr std::array<uint8_t,7> PILOT_MARKER{{
    0x7E, 'S', 'A', 'T', 'P', 0xD6, 0x9F
}};                                                     // маркер с CRC16
constexpr size_t PILOT_PREFIX_LEN = PILOT_MARKER.size() - 2; // часть без CRC
constexpr uint16_t PILOT_MARKER_CRC = 0x9FD6;                // CRC16(prefix)
[[maybe_unused]] const bool PILOT_MARKER_CRC_OK = []() {
  return FrameHeader::crc16(PILOT_MARKER.data(), PILOT_PREFIX_LEN) == PILOT_MARKER_CRC;
}();
constexpr size_t MAX_FRAGMENT_LEN =
    MAX_FRAME_SIZE - FrameHeader::SIZE - (MAX_FRAME_SIZE / PILOT_INTERVAL) * PILOT_MARKER.size();
constexpr uint32_t NONCE_SALT = 0x4B54524E;              // "KTRN" — соль нонса
constexpr uint8_t CERT_MAGIC[4] = {'K','T','C','F'};     // сигнатура сообщения сертификата

// Статический корневой ключ для AEAD (расширяется до 32 байт внутри обёртки)
const std::array<uint8_t,16> ROOT_KEY{
  0x4B, 0x54, 0x52, 0x46, // "KTRF"
  0x2D, 0x52, 0x4F, 0x4F, // "-ROO"
  0x54, 0x2D, 0x4B, 0x45, // "T-KE"
  0x59, 0x21, 0x01, 0x00  // "Y!\x01\x00"
};

// Добавление пилотов каждые 64 байта
std::vector<uint8_t> insertPilots(const std::vector<uint8_t>& in) {
  std::vector<uint8_t> out;
  out.reserve(in.size() + (in.size() / PILOT_INTERVAL) * PILOT_MARKER.size());
  size_t count = 0;
  for (uint8_t b : in) {
    if (count && count % PILOT_INTERVAL == 0) {
      out.insert(out.end(), PILOT_MARKER.begin(), PILOT_MARKER.end());
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
  size_t i = 0;
  while (i < len) {
    if (count && count % PILOT_INTERVAL == 0) {
      size_t remaining = len - i;
      if (remaining >= PILOT_MARKER.size() &&
          std::equal(PILOT_MARKER.begin(), PILOT_MARKER.begin() + PILOT_PREFIX_LEN, data + i)) {
        uint16_t crc = static_cast<uint16_t>(data[i + PILOT_PREFIX_LEN]) |
                       (static_cast<uint16_t>(data[i + PILOT_PREFIX_LEN + 1]) << 8);
        if (crc == PILOT_MARKER_CRC &&
            FrameHeader::crc16(data + i, PILOT_PREFIX_LEN) == crc) {
          i += PILOT_MARKER.size();
          continue;
        }
      }
    }
    out.push_back(data[i]);
    ++count;
    ++i;
  }
}

std::array<uint8_t,32> TRUSTED_ROOT{};                   // доверенный корневой ключ
bool TRUSTED_ROOT_SET = false;
CertificateBundle LOCAL_CERTIFICATE;                     // локальная цепочка для отправки
bool LOCAL_CERTIFICATE_SET = false;

} // namespace

const std::array<uint8_t,16>& rootKey() { return ROOT_KEY; }

static uint32_t packMetadata(uint8_t flags, uint16_t frag_idx, uint16_t payload_len) {
  return (static_cast<uint32_t>(flags) << FrameHeader::FLAGS_SHIFT) |
         ((static_cast<uint32_t>(frag_idx) & 0x0FFF) << FrameHeader::FRAG_SHIFT) |
         (static_cast<uint32_t>(payload_len) & FrameHeader::LEN_MASK);
}

std::array<uint8_t,12> makeNonce(uint32_t packed_meta, uint16_t msg_id) {
  std::array<uint8_t,12> nonce{};
  nonce[0] = static_cast<uint8_t>(packed_meta & 0xFF);
  nonce[1] = static_cast<uint8_t>((packed_meta >> 8) & 0xFF);
  nonce[2] = static_cast<uint8_t>((packed_meta >> 16) & 0xFF);
  nonce[3] = static_cast<uint8_t>((packed_meta >> 24) & 0xFF);
  nonce[4] = static_cast<uint8_t>(msg_id & 0xFF);
  nonce[5] = static_cast<uint8_t>((msg_id >> 8) & 0xFF);
  nonce[6] = static_cast<uint8_t>(NONCE_SALT & 0xFF);
  nonce[7] = static_cast<uint8_t>((NONCE_SALT >> 8) & 0xFF);
  nonce[8] = static_cast<uint8_t>((NONCE_SALT >> 16) & 0xFF);
  nonce[9] = static_cast<uint8_t>((NONCE_SALT >> 24) & 0xFF);
  nonce[10] = static_cast<uint8_t>(((NONCE_SALT >> 16) & 0xFF) ^ (packed_meta & 0xFF));
  nonce[11] = static_cast<uint8_t>(((NONCE_SALT >> 24) & 0xFF) ^ (msg_id & 0xFF));
  return nonce;
}

std::array<uint8_t,8> makeAad(uint8_t version, uint16_t msg_id, uint32_t packed_meta) {
  std::array<uint8_t,8> aad{};
  aad[0] = version;
  aad[1] = static_cast<uint8_t>(msg_id >> 8);
  aad[2] = static_cast<uint8_t>(msg_id);
  aad[3] = static_cast<uint8_t>(packed_meta >> 24);
  aad[4] = static_cast<uint8_t>(packed_meta >> 16);
  aad[5] = static_cast<uint8_t>(packed_meta >> 8);
  aad[6] = static_cast<uint8_t>(packed_meta);
  return aad;
}

bool buildFrame(uint32_t msg_id,
                const std::array<uint8_t,32>& public_key,
                const std::array<uint8_t,4>& key_id,
                std::vector<uint8_t>& frame_out,
                const std::array<uint8_t,32>* ephemeral_public,
                const CertificateBundle* certificate) {
  frame_out.clear();
  std::vector<uint8_t> plain;
  const bool has_ephemeral = (ephemeral_public != nullptr);
  const CertificateBundle* bundle_ptr = certificate;
  const bool has_certificate = (bundle_ptr != nullptr && bundle_ptr->valid && !bundle_ptr->chain.empty());
  uint8_t version = has_ephemeral ? VERSION_EPHEMERAL : VERSION_LEGACY;
  if (has_certificate) {
    version = VERSION_CERTIFICATE;
  }
  uint8_t flags = 0;
  if (has_ephemeral) flags |= FLAG_HAS_EPHEMERAL;
  if (has_certificate) flags |= FLAG_HAS_CERTIFICATE;

  size_t certificate_bytes = 0;
  if (has_certificate) {
    certificate_bytes = 1 + bundle_ptr->chain.size() * (32 + 64);
  }

  plain.reserve(4 + 1 + 3 + key_id.size() + public_key.size() +
                (has_ephemeral ? ephemeral_public->size() : 0) + certificate_bytes);
  plain.insert(plain.end(), std::begin(MAGIC), std::end(MAGIC));
  plain.push_back(version);
  plain.push_back(flags);
  plain.push_back(0);
  plain.push_back(0);
  plain.insert(plain.end(), key_id.begin(), key_id.end());
  plain.insert(plain.end(), public_key.begin(), public_key.end());
  if (has_ephemeral) {
    plain.insert(plain.end(), ephemeral_public->begin(), ephemeral_public->end());
  }
  if (has_certificate) {
    plain.push_back(static_cast<uint8_t>(bundle_ptr->chain.size()));
    for (const auto& record : bundle_ptr->chain) {
      plain.insert(plain.end(), record.issuer_public.begin(), record.issuer_public.end());
      plain.insert(plain.end(), record.signature.begin(), record.signature.end());
    }
  }

  size_t cipher_len_guess = plain.size() + TAG_LEN;
  if (cipher_len_guess > FrameHeader::LEN_MASK) {
    return false; // сообщение слишком длинное для одного кадра
  }
  uint8_t header_flags = FrameHeader::FLAG_ENCRYPTED;
  uint32_t packed_meta = packMetadata(header_flags, 0, static_cast<uint16_t>(cipher_len_guess));

  auto nonce = makeNonce(packed_meta, static_cast<uint16_t>(msg_id));
  std::vector<uint8_t> cipher;
  std::vector<uint8_t> tag;
  auto aad = makeAad(FRAME_VERSION_AEAD, static_cast<uint16_t>(msg_id), packed_meta);
  if (!crypto::chacha20poly1305::encrypt(rootKey().data(), rootKey().size(),
                                         nonce.data(), nonce.size(),
                                         aad.data(), aad.size(),
                                         plain.data(), plain.size(),
                                         cipher, tag)) {
    return false;
  }
  cipher.insert(cipher.end(), tag.begin(), tag.end());
  if (cipher.size() != cipher_len_guess) {
    return false;
  }
  if (cipher.size() > MAX_FRAGMENT_LEN) return false;

  FrameHeader hdr;
  hdr.ver = FRAME_VERSION_AEAD;
  hdr.msg_id = static_cast<uint16_t>(msg_id);
  hdr.setFragIdx(0);
  hdr.frag_cnt = 1;
  hdr.setPayloadLen(static_cast<uint16_t>(cipher.size()));
  hdr.setFlags(header_flags);
  uint8_t hdr_buf[FrameHeader::SIZE];
  if (!hdr.encode(hdr_buf, sizeof(hdr_buf), cipher.data(), cipher.size())) {
    return false;
  }

  frame_out.reserve(FrameHeader::SIZE + cipher.size() + cipher.size() / 32);
  frame_out.insert(frame_out.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
  auto payload = insertPilots(cipher);
  frame_out.insert(frame_out.end(), payload.begin(), payload.end());
  scrambler::scramble(frame_out.data(), frame_out.size());
  return true;
}

bool parseFrame(const uint8_t* frame, size_t len,
                FramePayload& payload,
                uint32_t& msg_id_out) {
  if (!frame || len < FrameHeader::SIZE) return false;
  std::vector<uint8_t> buf(frame, frame + len);
  scrambler::descramble(buf.data(), buf.size());
  FrameHeader hdr;
  size_t header_offset = 0;
  bool ok = FrameHeader::decode(buf.data(), buf.size(), hdr);
  if (!ok) {
    if (buf.size() > FrameHeader::SIZE) {
      ok = FrameHeader::decode(buf.data() + FrameHeader::SIZE,
                               buf.size() - FrameHeader::SIZE, hdr);
      if (ok) header_offset = FrameHeader::SIZE;
    }
  }
  if (!ok) return false;
  if (hdr.frag_cnt != 1 || hdr.getFragIdx() != 0) return false;
  if (hdr.ver != 1 && hdr.ver < FRAME_VERSION_AEAD) return false;

  size_t payload_offset = header_offset + FrameHeader::SIZE;
  if (buf.size() < payload_offset) return false;
  const uint8_t* payload_p = buf.data() + payload_offset;
  size_t payload_len = buf.size() - payload_offset;
  std::vector<uint8_t> payload_bytes;
  removePilots(payload_p, payload_len, payload_bytes);
  if (payload_bytes.size() != hdr.getPayloadLen()) return false;
  size_t tag_len = (hdr.ver >= FRAME_VERSION_AEAD) ? TAG_LEN : TAG_LEN_V1;
  if (payload_bytes.size() < tag_len) return false;

  size_t cipher_len = payload_bytes.size() - tag_len;
  const uint8_t* tag = payload_bytes.data() + cipher_len;
  auto nonce = makeNonce(hdr.packed, hdr.msg_id);
  std::vector<uint8_t> plain;
  if (hdr.ver >= FRAME_VERSION_AEAD) {
    auto aad = makeAad(hdr.ver, hdr.msg_id, hdr.packed);
    if (!crypto::chacha20poly1305::decrypt(rootKey().data(), rootKey().size(),
                                           nonce.data(), nonce.size(),
                                           aad.data(), aad.size(),
                                           payload_bytes.data(), cipher_len,
                                           tag, tag_len, plain)) {
      return false;
    }
  } else {
    if (!decrypt_ccm(rootKey().data(), rootKey().size(),
                     nonce.data(), nonce.size(),
                     nullptr, 0,
                     payload_bytes.data(), cipher_len,
                     tag, tag_len, plain)) {
      return false;
    }
  }
  if (plain.size() < 4 + 1 + 3 + payload.key_id.size() + payload.public_key.size()) return false;
  if (!std::equal(std::begin(MAGIC), std::end(MAGIC), plain.begin())) return false;
  payload.version = plain[4];
  payload.flags = plain[5];
  if (payload.version != VERSION_LEGACY &&
      payload.version != VERSION_EPHEMERAL &&
      payload.version != VERSION_CERTIFICATE) {
    return false;
  }
  std::copy_n(plain.data() + 8, payload.key_id.size(), payload.key_id.begin());
  std::copy_n(plain.data() + 12, payload.public_key.size(), payload.public_key.begin());
  size_t offset = 12 + payload.public_key.size();
  payload.has_ephemeral = false;
  if (payload.version == VERSION_EPHEMERAL || payload.version == VERSION_CERTIFICATE) {
    if ((payload.flags & FLAG_HAS_EPHEMERAL) != 0) {
      if (plain.size() < offset + payload.ephemeral_public.size()) {
        return false;
      }
      std::copy_n(plain.data() + offset, payload.ephemeral_public.size(),
                  payload.ephemeral_public.begin());
      offset += payload.ephemeral_public.size();
      payload.has_ephemeral = true;
    } else {
      payload.ephemeral_public.fill(0);
    }
  } else {
    if (payload.flags != 0) {
      return false;  // для легаси не допускаем дополнительные флаги
    }
    payload.ephemeral_public.fill(0);
  }

  payload.certificate = {};
  if (payload.version == VERSION_CERTIFICATE) {
    if ((payload.flags & FLAG_HAS_CERTIFICATE) == 0) {
      return false;
    }
    if (plain.size() <= offset) {
      return false;
    }
    uint8_t count = plain[offset++];
    size_t need = static_cast<size_t>(count) * (32 + 64);
    if (plain.size() < offset + need) {
      return false;
    }
    payload.certificate.valid = (count > 0);
    payload.certificate.chain.clear();
    payload.certificate.chain.reserve(count);
    for (uint8_t i = 0; i < count; ++i) {
      CertificateRecord rec;
      std::copy_n(plain.data() + offset, rec.issuer_public.size(), rec.issuer_public.begin());
      offset += rec.issuer_public.size();
      std::copy_n(plain.data() + offset, rec.signature.size(), rec.signature.begin());
      offset += rec.signature.size();
      payload.certificate.chain.push_back(rec);
    }
  } else {
    if (payload.flags & FLAG_HAS_CERTIFICATE) {
      return false;  // флаг установлен, но версия не поддерживает
    }
    payload.certificate.valid = false;
    payload.certificate.chain.clear();
  }
  msg_id_out = hdr.msg_id;
  return true;
}

bool parseFrame(const uint8_t* frame, size_t len,
                std::array<uint8_t,32>& public_key,
                std::array<uint8_t,4>& key_id,
                uint32_t& msg_id_out) {
  FramePayload payload;
  if (!parseFrame(frame, len, payload, msg_id_out)) {
    return false;
  }
  public_key = payload.public_key;
  key_id = payload.key_id;
  return true;
}

void setTrustedRoot(const std::array<uint8_t,32>& root_public) {
  TRUSTED_ROOT = root_public;
  TRUSTED_ROOT_SET = true;
}

bool hasTrustedRoot() { return TRUSTED_ROOT_SET; }

const std::array<uint8_t,32>& getTrustedRoot() { return TRUSTED_ROOT; }

bool verifyCertificateChain(const std::array<uint8_t,32>& subject,
                            const CertificateBundle& bundle,
                            std::string* error) {
  if (!bundle.valid || bundle.chain.empty()) {
    if (error) *error = "пустая цепочка";
    return false;
  }
  if (!hasTrustedRoot()) {
    if (error) *error = "не задан доверенный корень";
    return false;
  }
  std::array<uint8_t,32> current = subject;
  for (size_t i = 0; i < bundle.chain.size(); ++i) {
    const auto& rec = bundle.chain[i];
    std::array<uint8_t,36> message{};
    std::copy(std::begin(CERT_MAGIC), std::end(CERT_MAGIC), message.begin());
    std::copy(current.begin(), current.end(), message.begin() + sizeof(CERT_MAGIC));
    if (!crypto::ed25519::verify(message.data(), message.size(), rec.signature, rec.issuer_public)) {
      if (error) *error = "подпись не прошла";
      return false;
    }
    current = rec.issuer_public;
  }
  if (!std::equal(current.begin(), current.end(), TRUSTED_ROOT.begin())) {
    if (error) *error = "корневой ключ не совпал";
    return false;
  }
  return true;
}

void setLocalCertificate(const CertificateBundle& bundle) {
  LOCAL_CERTIFICATE = bundle;
  LOCAL_CERTIFICATE_SET = bundle.valid && !bundle.chain.empty();
}

bool hasLocalCertificate() { return LOCAL_CERTIFICATE_SET; }

const CertificateBundle& getLocalCertificate() { return LOCAL_CERTIFICATE; }

} // namespace KeyTransfer
