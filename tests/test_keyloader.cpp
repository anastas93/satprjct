#include <cassert>
#include <array>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include "libs/key_loader/key_loader.h"
#include "libs/crypto/x25519.h"
#include "libs/crypto/hkdf.h"

int main() {
  using namespace KeyLoader;
  std::filesystem::create_directories("key_storage");
  std::filesystem::remove("key_storage/key.stkey");
  std::filesystem::remove("key_storage/key.stkey.old");

  // Генерация локального ключа и проверка записи
  assert(generateLocalKey());
  assert(!hasPeerPublic());
  // При отсутствии публичного ключа собеседника превью идентификатора недоступно,
  // что соответствует отправке нулевого key_id при первом KEYTRANSFER.
  std::array<uint8_t,4> preview_id{};
  assert(!previewPeerKeyId(preview_id));
  KeyRecord rec;
  assert(loadKeyRecord(rec));
  assert(rec.valid);
  assert(rec.origin == KeyOrigin::LOCAL);
  assert(!rec.session_key.empty());
  const auto first_public = rec.root_public;
  auto id = keyId(rec.session_key);
  std::cout << "local id=" << std::hex << int(id[0]) << int(id[1]) << std::dec << std::endl;

  // Подготовка удалённой стороны для ECDH
  std::array<uint8_t,32> remote_priv{};
  crypto::x25519::random_bytes(remote_priv.data(), remote_priv.size());
  std::array<uint8_t,32> remote_pub{};
  crypto::x25519::derive_public(remote_pub, remote_priv);

  // Применяем внешний ключ и сверяем результат
  assert(applyRemotePublic(remote_pub));
  auto state_after_remote = getState();
  assert(state_after_remote.has_backup);
  // Моделируем первичный KEYTRANSFER: до применения удалённого ключа peer_public отсутствовал,
  // но ключ успешно принят без отката.
  assert(hasPeerPublic());
  KeyRecord rec2;
  assert(loadKeyRecord(rec2));
  assert(rec2.origin == KeyOrigin::REMOTE);
  assert(rec2.peer_public == remote_pub);
  std::array<uint8_t,4> peer_key_id{};
  // После получения ключа без сохранённого peer_public проверяем, что пересчёт идентификатора
  // выполняется по фактическому ECDH-значению.
  assert(previewPeerKeyId(peer_key_id));
  assert(peer_key_id == keyId(rec2.session_key));

  std::array<uint8_t,32> shared_remote{};
  crypto::x25519::compute_shared(remote_priv, rec2.root_public, shared_remote);
  std::array<uint8_t,32> shared_local{};
  crypto::x25519::compute_shared(rec2.root_private, remote_pub, shared_local);
  if (shared_remote != shared_local) {
    std::cout << "shared_remote=";
    for (uint8_t b : shared_remote) std::cout << std::hex << int(b);
    std::cout << " shared_local=";
    for (uint8_t b : shared_local) std::cout << std::hex << int(b);
    std::cout << std::dec << std::endl;
  }
  assert(shared_remote == shared_local);
  std::array<std::array<uint8_t,32>, 2> ordered_pubs = {rec2.root_public, remote_pub};
  std::sort(ordered_pubs.begin(), ordered_pubs.end());
  std::array<uint8_t,64> salt{};
  std::copy(ordered_pubs[0].begin(), ordered_pubs[0].end(), salt.begin());
  std::copy(ordered_pubs[1].begin(), ordered_pubs[1].end(), salt.begin() + ordered_pubs[0].size());
  std::array<uint8_t,16> expected{};
  bool ok = crypto::hkdf::derive(salt.data(),
                                 salt.size(),
                                 shared_local.data(),
                                 shared_local.size(),
                                 reinterpret_cast<const uint8_t*>("KeyLoader remote session v2 static"),
                                 sizeof("KeyLoader remote session v2 static") - 1,
                                 expected.data(),
                                 expected.size());
  assert(ok);
  std::cout << "expected=";
  for (uint8_t b : expected) std::cout << std::hex << int(b);
  std::cout << " actual=";
  for (uint8_t b : rec2.session_key) std::cout << std::hex << int(b);
  std::cout << std::dec << std::endl;
  assert(expected == rec2.session_key);

  std::array<uint8_t,4> nonce_block{};
  bool nonce_ok = crypto::hkdf::derive(nullptr,
                                       0,
                                       rec2.session_key.data(),
                                       rec2.session_key.size(),
                                       reinterpret_cast<const uint8_t*>("KeyLoader nonce v2"),
                                       sizeof("KeyLoader nonce v2") - 1,
                                       nonce_block.data(),
                                       nonce_block.size());
  assert(nonce_ok);
  uint32_t expected_nonce = static_cast<uint32_t>(nonce_block[0]) |
                            (static_cast<uint32_t>(nonce_block[1]) << 8) |
                            (static_cast<uint32_t>(nonce_block[2]) << 16) |
                            (static_cast<uint32_t>(nonce_block[3]) << 24);
  assert(rec2.nonce_salt == expected_nonce);

  // Сценарий: устройство A генерирует новый ключ, устройство B применяет его,
  // затем устройство A пересчитывает сессию через regenerateFromPeer.
  assert(generateLocalKey());
  KeyRecord rec3;
  assert(loadKeyRecord(rec3));
  assert(rec3.origin == KeyOrigin::LOCAL);
  assert(rec3.peer_public == remote_pub);
  std::array<uint8_t,4> peer_key_id_after_local{};
  assert(previewPeerKeyId(peer_key_id_after_local));
  // Проверяем, что новая генерация создаёт отличающийся публичный ключ.
  assert(rec3.root_public != first_public);
  std::array<uint8_t,32> shared_remote_second{};
  crypto::x25519::compute_shared(remote_priv, rec3.root_public, shared_remote_second);
  std::array<std::array<uint8_t,32>, 2> ordered_pubs_second = {rec3.root_public, remote_pub};
  std::sort(ordered_pubs_second.begin(), ordered_pubs_second.end());
  std::array<uint8_t,64> salt_second{};
  std::copy(ordered_pubs_second[0].begin(), ordered_pubs_second[0].end(), salt_second.begin());
  std::copy(ordered_pubs_second[1].begin(), ordered_pubs_second[1].end(),
            salt_second.begin() + ordered_pubs_second[0].size());
  std::array<uint8_t,16> expected_second{};
  bool ok_second = crypto::hkdf::derive(salt_second.data(),
                                        salt_second.size(),
                                        shared_remote_second.data(),
                                        shared_remote_second.size(),
                                        reinterpret_cast<const uint8_t*>("KeyLoader remote session v2 static"),
                                        sizeof("KeyLoader remote session v2 static") - 1,
                                        expected_second.data(),
                                        expected_second.size());
  assert(ok_second);
  assert(peer_key_id_after_local == keyId(expected_second));
  assert(regenerateFromPeer());
  KeyRecord rec4;
  assert(loadKeyRecord(rec4));
  assert(rec4.origin == KeyOrigin::REMOTE);
  assert(rec4.peer_public == remote_pub);
  assert(rec4.session_key == expected_second);
  std::array<uint8_t,4> nonce_second_block{};
  bool nonce_second_ok = crypto::hkdf::derive(nullptr,
                                              0,
                                              rec4.session_key.data(),
                                              rec4.session_key.size(),
                                              reinterpret_cast<const uint8_t*>("KeyLoader nonce v2"),
                                              sizeof("KeyLoader nonce v2") - 1,
                                              nonce_second_block.data(),
                                              nonce_second_block.size());
  assert(nonce_second_ok);
  uint32_t expected_second_nonce = static_cast<uint32_t>(nonce_second_block[0]) |
                                   (static_cast<uint32_t>(nonce_second_block[1]) << 8) |
                                   (static_cast<uint32_t>(nonce_second_block[2]) << 16) |
                                   (static_cast<uint32_t>(nonce_second_block[3]) << 24);
  assert(rec4.nonce_salt == expected_second_nonce);
  std::array<uint8_t,4> peer_key_id_after_regen{};
  assert(previewPeerKeyId(peer_key_id_after_regen));
  assert(peer_key_id_after_regen == keyId(rec4.session_key));
  auto state_after_regen = getState();
  assert(state_after_regen.has_backup);
  // Проверяем восстановление локального ключа после пересчёта сессии.
  assert(restorePreviousKey());
  auto state_after_restore = getState();
  assert(!state_after_restore.has_backup);
  KeyRecord rec5;
  assert(loadKeyRecord(rec5));
  assert(rec5.origin == KeyOrigin::LOCAL);
  assert(rec5.peer_public == remote_pub);
  assert(rec5.root_public == rec3.root_public);

  std::cout << "OK" << std::endl;
  return 0;
}
