#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>
#include <sodium.h>

#include "libs/key_loader/key_loader.h"
#include "libs/key_transfer/key_transfer.h"
#include "libs/crypto/hkdf.h"
#include "libs/crypto/x25519.h"

namespace {

// Вспомогательный расчёт HKDF для удалённой стороны
std::array<uint8_t,16> deriveRemoteSession(const std::array<uint8_t,32>& shared,
                                           const std::array<uint8_t,32>& local_pub,
                                           const std::array<uint8_t,32>& remote_pub,
                                           const char* info) {
  std::array<std::array<uint8_t,32>, 2> ordered{local_pub, remote_pub};
  std::sort(ordered.begin(), ordered.end());
  std::array<uint8_t,64> salt{};
  std::copy(ordered[0].begin(), ordered[0].end(), salt.begin());
  std::copy(ordered[1].begin(), ordered[1].end(), salt.begin() + ordered[0].size());
  std::array<uint8_t,16> session{};
  bool ok = crypto::hkdf::derive(salt.data(),
                                 salt.size(),
                                 shared.data(),
                                 shared.size(),
                                 reinterpret_cast<const uint8_t*>(info),
                                 std::strlen(info),
                                 session.data(),
                                 session.size());
  assert(ok);
  return session;
}

}  // namespace

int main() {
  namespace fs = std::filesystem;

  if (sodium_init() < 0) {
    std::cerr << "sodium init failed" << std::endl;
    return 1;
  }

  // Гарантируем чистое хранилище перед началом проверки.
  fs::create_directories("key_storage");
  fs::remove("key_storage/key.stkey");
  fs::remove("key_storage/key.stkey.old");

  // Генерируем локальную пару и проверяем базовый обмен без эпемерных ключей.
  assert(KeyLoader::generateLocalKey());
  KeyLoader::KeyRecord local{};
  assert(KeyLoader::loadKeyRecord(local));

  std::array<uint8_t,32> remote_priv{};
  crypto::x25519::random_bytes(remote_priv.data(), remote_priv.size());
  std::array<uint8_t,32> remote_pub{};
  crypto::x25519::derive_public(remote_pub, remote_priv);

  std::array<uint8_t,32> shared_static{};
  assert(crypto::x25519::compute_shared(remote_priv, local.root_public, shared_static));
  auto expected_static = deriveRemoteSession(shared_static,
                                             local.root_public,
                                             remote_pub,
                                             "KeyLoader remote session v2 static");
  auto key_id_static = KeyLoader::keyId(expected_static);

  uint32_t static_msg_id = 0xA1B2C3D4;
  std::vector<uint8_t> static_frame;
  assert(KeyTransfer::buildFrame(static_msg_id, remote_pub, key_id_static, static_frame));

  KeyTransfer::FramePayload static_payload{};
  uint32_t parsed_msg = 0;
  assert(KeyTransfer::parseFrame(static_frame.data(), static_frame.size(), static_payload, parsed_msg));
  assert(parsed_msg == static_cast<uint16_t>(static_msg_id));
  assert(static_payload.version == KeyTransfer::VERSION_LEGACY);
  assert(!static_payload.has_ephemeral);
  assert(static_payload.public_key == remote_pub);
  assert(static_payload.key_id == key_id_static);

  KeyLoader::KeyRecord after_static{};
  assert(KeyLoader::applyRemotePublic(static_payload.public_key, nullptr, &after_static));
  assert(after_static.session_key == expected_static);
  assert(after_static.peer_public == remote_pub);
  assert(KeyLoader::keyId(after_static.session_key) == key_id_static);
  assert(!KeyLoader::hasEphemeralSession());

  std::array<uint8_t,32> compat_pub{};
  std::array<uint8_t,4> compat_id{};
  parsed_msg = 0;
  assert(KeyTransfer::parseFrame(static_frame.data(), static_frame.size(), compat_pub, compat_id, parsed_msg));
  assert(compat_pub == remote_pub);
  assert(compat_id == key_id_static);
  assert(parsed_msg == static_cast<uint16_t>(static_msg_id));

  // Подготавливаем новый цикл для проверки эпемерного обмена.
  fs::remove("key_storage/key.stkey");
  fs::remove("key_storage/key.stkey.old");
  KeyLoader::endEphemeralSession();
  assert(KeyLoader::generateLocalKey());
  KeyLoader::KeyRecord local_ephemeral{};
  assert(KeyLoader::loadKeyRecord(local_ephemeral));

  std::array<uint8_t,32> local_ephemeral_pub{};
  assert(KeyLoader::startEphemeralSession(local_ephemeral_pub));

  std::array<uint8_t,32> remote_priv2{};
  crypto::x25519::random_bytes(remote_priv2.data(), remote_priv2.size());
  std::array<uint8_t,32> remote_pub2{};
  crypto::x25519::derive_public(remote_pub2, remote_priv2);

  std::array<uint8_t,32> remote_ephemeral_priv{};
  crypto::x25519::random_bytes(remote_ephemeral_priv.data(), remote_ephemeral_priv.size());
  std::array<uint8_t,32> remote_ephemeral_pub{};
  crypto::x25519::derive_public(remote_ephemeral_pub, remote_ephemeral_priv);

  std::array<uint8_t,32> shared_ephemeral{};
  assert(crypto::x25519::compute_shared(remote_ephemeral_priv, local_ephemeral_pub, shared_ephemeral));
  auto expected_ephemeral = deriveRemoteSession(shared_ephemeral,
                                                local_ephemeral_pub,
                                                remote_ephemeral_pub,
                                                "KeyLoader remote session v2 ephemeral");
  auto key_id_ephemeral = KeyLoader::keyId(expected_ephemeral);

  uint32_t ephemeral_msg_id = 0x0BADBEEF;
  std::vector<uint8_t> ephemeral_frame;
  assert(KeyTransfer::buildFrame(ephemeral_msg_id,
                                 remote_pub2,
                                 key_id_ephemeral,
                                 ephemeral_frame,
                                 &remote_ephemeral_pub));

  KeyTransfer::FramePayload ephemeral_payload{};
  parsed_msg = 0;
  assert(KeyTransfer::parseFrame(ephemeral_frame.data(), ephemeral_frame.size(), ephemeral_payload, parsed_msg));
  assert(parsed_msg == static_cast<uint16_t>(ephemeral_msg_id));
  assert(ephemeral_payload.version == KeyTransfer::VERSION_EPHEMERAL);
  assert(ephemeral_payload.has_ephemeral);
  assert(ephemeral_payload.public_key == remote_pub2);
  assert(ephemeral_payload.ephemeral_public == remote_ephemeral_pub);
  assert(ephemeral_payload.key_id == key_id_ephemeral);

  KeyLoader::KeyRecord after_ephemeral{};
  assert(KeyLoader::applyRemotePublic(ephemeral_payload.public_key,
                                      &ephemeral_payload.ephemeral_public,
                                      &after_ephemeral));
  assert(after_ephemeral.session_key == expected_ephemeral);
  assert(after_ephemeral.peer_public == remote_pub2);
  assert(KeyLoader::keyId(after_ephemeral.session_key) == key_id_ephemeral);
  assert(!KeyLoader::hasEphemeralSession());

  std::cout << "OK" << std::endl;
  return 0;
}
