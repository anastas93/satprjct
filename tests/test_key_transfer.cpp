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
constexpr std::array<uint8_t,4> MAGIC{{'K','T','R','F'}};                  // сигнатура полезной нагрузки
constexpr size_t COMPACT_HEADER_SIZE = 7;                                  // размер компактного заголовка

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

// Вставка пилотов по той же схеме, что и в прошивке.
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

// Повтор реализации компактного заголовка и AAD для формирования тестовых кадров.
std::array<uint8_t,COMPACT_HEADER_SIZE> makeCompactHeader(uint8_t version,
                                                          uint16_t frag_cnt,
                                                          uint32_t packed_meta) {
  std::array<uint8_t,COMPACT_HEADER_SIZE> compact{};
  compact[0] = version;
  compact[1] = static_cast<uint8_t>(frag_cnt >> 8);
  compact[2] = static_cast<uint8_t>(frag_cnt);
  compact[3] = static_cast<uint8_t>(packed_meta >> 24);
  compact[4] = static_cast<uint8_t>(packed_meta >> 16);
  compact[5] = static_cast<uint8_t>(packed_meta >> 8);
  compact[6] = static_cast<uint8_t>(packed_meta);
  return compact;
}

std::array<uint8_t,COMPACT_HEADER_SIZE + 2> makeAad(uint8_t version,
                                                    uint16_t frag_cnt,
                                                    uint32_t packed_meta,
                                                    uint16_t msg_id) {
  auto compact = makeCompactHeader(version, frag_cnt, packed_meta);
  std::array<uint8_t,COMPACT_HEADER_SIZE + 2> aad{};
  std::copy(compact.begin(), compact.end(), aad.begin());
  aad[COMPACT_HEADER_SIZE] = static_cast<uint8_t>(msg_id >> 8);
  aad[COMPACT_HEADER_SIZE + 1] = static_cast<uint8_t>(msg_id);
  return aad;
}

// Построение кастомного кадра с контролем флагов/резервов для негативных сценариев.
std::vector<uint8_t> buildCustomFrame(uint8_t payload_version,
                                      uint8_t flags,
                                      uint16_t msg_id,
                                      const std::array<uint8_t,4>& key_id,
                                      const std::array<uint8_t,32>& public_key,
                                      const std::array<uint8_t,32>* ephemeral,
                                      const std::vector<KeyTransfer::CertificateRecord>* cert_chain,
                                      uint8_t reserved0,
                                      uint8_t reserved1) {
  std::vector<uint8_t> plain;
  size_t cert_bytes = cert_chain ? (1 + cert_chain->size() * (32 + 64)) : 0;
  size_t eph_bytes = ephemeral ? ephemeral->size() : 0;
  plain.reserve(4 + 1 + 3 + key_id.size() + public_key.size() + eph_bytes + cert_bytes);
  plain.insert(plain.end(), MAGIC.begin(), MAGIC.end());
  plain.push_back(payload_version);
  plain.push_back(flags);
  plain.push_back(reserved0);
  plain.push_back(reserved1);
  plain.insert(plain.end(), key_id.begin(), key_id.end());
  plain.insert(plain.end(), public_key.begin(), public_key.end());
  if (ephemeral) {
    plain.insert(plain.end(), ephemeral->begin(), ephemeral->end());
  }
  if (cert_chain) {
    plain.push_back(static_cast<uint8_t>(cert_chain->size()));
    for (const auto& rec : *cert_chain) {
      plain.insert(plain.end(), rec.issuer_public.begin(), rec.issuer_public.end());
      plain.insert(plain.end(), rec.signature.begin(), rec.signature.end());
    }
  }

  FrameHeader hdr;
  hdr.ver = KeyTransfer::FRAME_VERSION_AEAD;
  hdr.msg_id = msg_id;
  hdr.frag_cnt = 1;
  hdr.setFlags(FrameHeader::FLAG_ENCRYPTED);
  hdr.setFragIdx(0);
  hdr.setPayloadLen(static_cast<uint16_t>(plain.size() + crypto::chacha20poly1305::TAG_SIZE));
  uint32_t packed_meta = hdr.packed;

  auto nonce = KeyTransfer::makeNonce(hdr.ver, hdr.frag_cnt, packed_meta, hdr.msg_id);
  auto aad = makeAad(hdr.ver, hdr.frag_cnt, packed_meta, hdr.msg_id);
  std::vector<uint8_t> cipher;
  std::vector<uint8_t> tag;
  bool ok = crypto::chacha20poly1305::encrypt(KeyTransfer::rootKey().data(),
                                              KeyTransfer::rootKey().size(),
                                              nonce.data(),
                                              nonce.size(),
                                              aad.data(),
                                              aad.size(),
                                              plain.data(),
                                              plain.size(),
                                              cipher,
                                              tag);
  assert(ok);
  cipher.insert(cipher.end(), tag.begin(), tag.end());

  hdr.setPayloadLen(static_cast<uint16_t>(cipher.size()));
  hdr.setFlags(FrameHeader::FLAG_ENCRYPTED);
  uint8_t hdr_buf[FrameHeader::SIZE];
  ok = hdr.encode(hdr_buf, sizeof(hdr_buf), cipher.data(), cipher.size());
  assert(ok);

  std::vector<uint8_t> frame;
  frame.insert(frame.end(), hdr_buf, hdr_buf + FrameHeader::SIZE);
  auto payload_with_pilots = insertPilots(cipher);
  frame.insert(frame.end(), payload_with_pilots.begin(), payload_with_pilots.end());
  scrambler::scramble(frame.data(), frame.size());
  return frame;
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

    // Проверяем защиту от переполнения счётчика сертификатов.
    KeyTransfer::CertificateBundle huge_bundle;
    huge_bundle.valid = true;
    huge_bundle.chain.resize(256);
    std::vector<uint8_t> unused_frame;
    assert(!KeyTransfer::buildFrame(msg_id, pub, id, unused_frame, &eph, &huge_bundle));

    // Проверяем отказ при неизвестных флагах в полезной нагрузке.
    {
      auto bad_frame = buildCustomFrame(KeyTransfer::VERSION_EPHEMERAL,
                                        KeyTransfer::FLAG_HAS_EPHEMERAL | 0x80,
                                        0x2222,
                                        id,
                                        pub,
                                        &eph,
                                        nullptr,
                                        0,
                                        0);
      KeyTransfer::FramePayload bad_payload{};
      uint32_t bad_msg = 0;
      assert(!KeyTransfer::parseFrame(bad_frame.data(), bad_frame.size(), bad_payload, bad_msg));
    }

    // Проверяем отказ при ненулевых резервных байтах.
    {
      auto bad_frame = buildCustomFrame(KeyTransfer::VERSION_EPHEMERAL,
                                        KeyTransfer::FLAG_HAS_EPHEMERAL,
                                        0x3333,
                                        id,
                                        pub,
                                        &eph,
                                        nullptr,
                                        0x01,
                                        0x00);
      KeyTransfer::FramePayload bad_payload{};
      uint32_t bad_msg = 0;
      assert(!KeyTransfer::parseFrame(bad_frame.data(), bad_frame.size(), bad_payload, bad_msg));
    }

    // Проверяем, что версия 2 без флага эпемерного ключа отвергается.
    {
      auto bad_frame = buildCustomFrame(KeyTransfer::VERSION_EPHEMERAL,
                                        0x00,
                                        0x4444,
                                        id,
                                        pub,
                                        nullptr,
                                        nullptr,
                                        0,
                                        0);
      KeyTransfer::FramePayload bad_payload{};
      uint32_t bad_msg = 0;
      assert(!KeyTransfer::parseFrame(bad_frame.data(), bad_frame.size(), bad_payload, bad_msg));
    }

    // Проверяем отказ при пустой цепочке сертификатов.
    {
      std::vector<KeyTransfer::CertificateRecord> empty_chain;
      auto bad_frame = buildCustomFrame(KeyTransfer::VERSION_CERTIFICATE,
                                        KeyTransfer::FLAG_HAS_CERTIFICATE,
                                        0x5555,
                                        id,
                                        pub,
                                        nullptr,
                                        &empty_chain,
                                        0,
                                        0);
      KeyTransfer::FramePayload bad_payload{};
      uint32_t bad_msg = 0;
      assert(!KeyTransfer::parseFrame(bad_frame.data(), bad_frame.size(), bad_payload, bad_msg));
    }

    std::cout << "OK" << std::endl;
    return 0;
  }

