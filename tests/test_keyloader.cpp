#include <cassert>
#include <array>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include "libs/key_loader/key_loader.h"
#include "libs/crypto/x25519.h"
#include "libs/crypto/sha256.h"

int main() {
  using namespace KeyLoader;
  std::filesystem::create_directories("key_storage");
  std::filesystem::remove("key_storage/key.stkey");
  std::filesystem::remove("key_storage/key.stkey.old");

  // Генерация локального ключа и проверка записи
  assert(generateLocalKey());
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
  KeyRecord rec2;
  assert(loadKeyRecord(rec2));
  assert(rec2.origin == KeyOrigin::REMOTE);
  assert(rec2.peer_public == remote_pub);

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
  std::array<uint8_t,96> buf{};
  std::copy(shared_local.begin(), shared_local.end(), buf.begin());
  std::copy(rec2.root_public.begin(), rec2.root_public.end(), buf.begin() + shared_local.size());
  std::copy(remote_pub.begin(), remote_pub.end(), buf.begin() + shared_local.size() + rec2.root_public.size());
  auto digest = crypto::sha256::hash(buf.data(), buf.size());
  std::array<uint8_t,16> expected{};
  std::copy_n(digest.begin(), expected.size(), expected.begin());
  std::cout << "expected=";
  for (uint8_t b : expected) std::cout << std::hex << int(b);
  std::cout << " actual=";
  for (uint8_t b : rec2.session_key) std::cout << std::hex << int(b);
  std::cout << std::dec << std::endl;
  assert(expected == rec2.session_key);
  const auto remote_session = rec2.session_key;

  KeyRecord recPeer;
  assert(regenerateFromPeer(&recPeer));
  assert(recPeer.session_key == remote_session);
  auto state_after_peer_regen = getState();
  assert(state_after_peer_regen.session_key == remote_session);

  // Проверяем резервную копию и восстановление
  assert(generateLocalKey());
  auto state_after_regen = getState();
  assert(state_after_regen.has_backup);
  KeyRecord rec3;
  assert(loadKeyRecord(rec3));
  assert(rec3.origin == KeyOrigin::LOCAL);
  // Проверяем, что новая генерация создаёт отличающийся публичный ключ.
  assert(rec3.root_public != first_public);
  // После генерации локального ключа пробуем вернуть удалённый ключ из бэкапа
  KeyRecord recPeer2;
  assert(regenerateFromPeer(&recPeer2));
  assert(recPeer2.session_key == remote_session);
  assert(restorePreviousKey());
  auto state_after_restore = getState();
  assert(!state_after_restore.has_backup);
  KeyRecord rec4;
  assert(loadKeyRecord(rec4));
  assert(rec4.origin == KeyOrigin::LOCAL);
  for (uint8_t b : rec4.peer_public) {
    assert(b == 0);
  }

  std::cout << "OK" << std::endl;
  return 0;
}
