#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <sodium.h>
#include "libs/key_transfer/key_transfer.h"

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
  assert(decoded_msg == msg_id);

  // Проверяем совместимость с обёрткой без доступа к эпемерному ключу
  std::array<uint8_t,32> decoded_pub{};
  std::array<uint8_t,4> decoded_id{};
  decoded_msg = 0;
  assert(KeyTransfer::parseFrame(frame.data(), frame.size(), decoded_pub, decoded_id, decoded_msg));
  assert(decoded_pub == pub);
  assert(decoded_id == id);
  assert(decoded_msg == msg_id);

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

