#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <sodium.h>
#include "libs/key_transfer/key_transfer.h"
#include "libs/frame/frame_header.h"          // новый заголовок кадра без CRC
#include "libs/scrambler/scrambler.h"         // скремблер/дескремблер кадра
#include "libs/crypto/chacha20_poly1305.h"    // параметры AEAD для расчётов

namespace {

// Современный пилотный маркер без CRC-проверки заголовка
constexpr size_t PILOT_INTERVAL = 64;                                      // период вставки пилотов
constexpr std::array<uint8_t,7> PILOT_MARKER{{0x7E,'S','A','T','P',0xD6,0x9F}}; // сигнатура маркера
constexpr size_t PILOT_PREFIX_LEN = PILOT_MARKER.size() - 2;               // префикс без CRC
constexpr uint16_t PILOT_MARKER_CRC = 0x9FD6;                              // эталонная CRC префикса

// Очистка полезной нагрузки кадра от вставленных пилотов
void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) return;                                          // защита от пустых входов
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
        if (crc == PILOT_MARKER_CRC && FrameHeader::crc16(data + i, PILOT_PREFIX_LEN) == crc) {
          i += PILOT_MARKER.size();
          continue;                                                       // полностью пропускаем маркер
        }
      }
    }
    out.push_back(data[i]);
    ++count;
    ++i;
  }
}

// Разбор кадра без CRC: учитываем возможный дублированный заголовок и чистим пилоты
bool decodeFrameNoCrc(const std::vector<uint8_t>& frame,
                      FrameHeader& hdr,
                      std::vector<uint8_t>& payload,
                      size_t& header_bytes) {
  if (frame.size() < FrameHeader::SIZE) return false;                      // недостаточно данных для заголовка
  std::vector<uint8_t> descrambled(frame);
  scrambler::descramble(descrambled.data(), descrambled.size());
  FrameHeader primary;
  FrameHeader secondary;
  bool primary_ok = FrameHeader::decode(descrambled.data(), descrambled.size(), primary);
  bool secondary_ok = false;
  if (descrambled.size() >= FrameHeader::SIZE * 2) {
    secondary_ok = FrameHeader::decode(descrambled.data() + FrameHeader::SIZE,
                                       descrambled.size() - FrameHeader::SIZE,
                                       secondary);
  }
  if (!primary_ok && !secondary_ok) return false;                          // заголовок распознать не удалось
  hdr = primary_ok ? primary : secondary;
  header_bytes = primary_ok ? FrameHeader::SIZE : FrameHeader::SIZE * 2;
  std::vector<uint8_t> cleaned;
  removePilots(descrambled.data() + header_bytes,
               descrambled.size() - header_bytes,
               cleaned);
  payload = std::move(cleaned);
  return true;
}

} // namespace

// Проверка формирования и разбора кадра KEYTRANSFER с сертификатами
int main() {
  if (sodium_init() < 0) {
    std::cerr << "sodium init failed" << std::endl;
    return 1;
  }

  std::array<uint8_t,32> pub{};
  std::array<uint8_t,32> eph{};
  for (size_t i = 0; i < pub.size(); ++i) {
    pub[i] = static_cast<uint8_t>(i + 1);
    eph[i] = static_cast<uint8_t>(0x80 + i);
  }

  std::array<uint8_t,32> root_seed{};
  for (size_t i = 0; i < root_seed.size(); ++i) {
    root_seed[i] = static_cast<uint8_t>(0x33 + i);
  }
  std::array<uint8_t,32> root_public{};
  std::array<uint8_t,64> root_private{};
  crypto_sign_ed25519_seed_keypair(root_public.data(), root_private.data(), root_seed.data());
  KeyTransfer::setTrustedRoot(root_public);

  KeyTransfer::CertificateRecord record;
  record.issuer_public = root_public;
  std::array<uint8_t,36> cert_message{};
  cert_message[0] = 'K';
  cert_message[1] = 'T';
  cert_message[2] = 'C';
  cert_message[3] = 'F';
  std::copy(pub.begin(), pub.end(), cert_message.begin() + 4);
  crypto_sign_ed25519_detached(record.signature.data(), nullptr,
                               cert_message.data(), cert_message.size(),
                               root_private.data());

  KeyTransfer::CertificateBundle bundle;
  bundle.valid = true;
  bundle.chain.push_back(record);
  KeyTransfer::setLocalCertificate(bundle);
  const auto& stored_bundle = KeyTransfer::getLocalCertificate();

  std::array<uint8_t,4> id{0xAA, 0xBB, 0xCC, 0xDD};
  std::vector<uint8_t> frame;
  uint32_t msg_id = 0x11223344;
  assert(KeyTransfer::buildFrame(msg_id, pub, id, frame, &eph, &stored_bundle));
  assert(!frame.empty());
  FrameHeader frame_hdr;
  std::vector<uint8_t> frame_payload;
  size_t frame_header_bytes = 0;
  assert(decodeFrameNoCrc(frame, frame_hdr, frame_payload, frame_header_bytes));
  assert(frame_header_bytes == FrameHeader::SIZE);
  assert(frame_hdr.ver == 2);
  assert(frame_hdr.frag_cnt == 1);
  assert((frame_hdr.getFlags() & FrameHeader::FLAG_ENCRYPTED) == FrameHeader::FLAG_ENCRYPTED);
  const size_t plain_len = 4 /*MAGIC*/ + 1 /*version*/ + 1 /*flags*/ + 2 /*reserved*/ +
                           id.size() + pub.size() + eph.size() + 1 /*cert count*/ +
                           stored_bundle.chain.size() * (32 + 64);
  const size_t expected_payload_len = plain_len + crypto::chacha20poly1305::TAG_SIZE;
  const size_t expected_pilot_count = expected_payload_len / PILOT_INTERVAL;
  const size_t expected_frame_size = FrameHeader::SIZE + expected_payload_len +
                                     expected_pilot_count * PILOT_MARKER.size();
  assert(frame_hdr.getPayloadLen() == expected_payload_len);
  assert(frame_payload.size() == expected_payload_len);
  assert(frame.size() == expected_frame_size);
  assert(frame_hdr.msg_id == static_cast<uint16_t>(msg_id));

  KeyTransfer::FramePayload decoded_payload;
  uint32_t decoded_msg = 0;
  assert(KeyTransfer::parseFrame(frame.data(), frame.size(), decoded_payload, decoded_msg));
  assert(decoded_payload.public_key == pub);
  assert(decoded_payload.key_id == id);
  assert(decoded_payload.has_ephemeral);
  assert(decoded_payload.ephemeral_public == eph);
  assert(decoded_payload.version == KeyTransfer::VERSION_CERTIFICATE);
  assert(decoded_payload.certificate.valid);
  assert(decoded_payload.certificate.chain.size() == 1);
  std::string cert_error;
  assert(KeyTransfer::verifyCertificateChain(decoded_payload.public_key, decoded_payload.certificate, &cert_error));
  assert(decoded_msg == static_cast<uint16_t>(msg_id));

  // Проверяем совместимость с обёрткой без доступа к эпемерному ключу
  std::array<uint8_t,32> decoded_pub{};
  std::array<uint8_t,4> decoded_id{};
  decoded_msg = 0;
  assert(KeyTransfer::parseFrame(frame.data(), frame.size(), decoded_pub, decoded_id, decoded_msg));
  assert(decoded_pub == pub);
  assert(decoded_id == id);
  assert(decoded_msg == static_cast<uint16_t>(msg_id));

  // Нарушаем подпись и убеждаемся, что проверка падает
  auto tampered = decoded_payload;
  tampered.certificate.chain[0].signature[0] ^= 0xFF;
  std::string broken_error;
  assert(!KeyTransfer::verifyCertificateChain(tampered.public_key, tampered.certificate, &broken_error));

  // Формируем и разбираем кадр в легаси-формате
  std::vector<uint8_t> legacy_frame;
  assert(KeyTransfer::buildFrame(msg_id, pub, id, legacy_frame, nullptr, nullptr));
  KeyTransfer::FramePayload legacy_payload;
  assert(KeyTransfer::parseFrame(legacy_frame.data(), legacy_frame.size(), legacy_payload, decoded_msg));
  assert(!legacy_payload.has_ephemeral);
  assert(legacy_payload.version == KeyTransfer::VERSION_LEGACY);
  std::cout << "OK" << std::endl;
  return 0;
}

