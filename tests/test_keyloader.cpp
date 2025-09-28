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
  std::array<std::array<uint8_t,32>, 2> ordered_pubs = {rec2.root_public, remote_pub};
  std::sort(ordered_pubs.begin(), ordered_pubs.end());
  // Буфер формируется идентично прошивке: сортируем ключи и конкатенируем.
  std::copy(ordered_pubs[0].begin(), ordered_pubs[0].end(), buf.begin() + shared_local.size());
  std::copy(ordered_pubs[1].begin(), ordered_pubs[1].end(),
            buf.begin() + shared_local.size() + ordered_pubs[0].size());
  auto digest = crypto::sha256::hash(buf.data(), buf.size());
  std::array<uint8_t,16> expected{};
  std::copy_n(digest.begin(), expected.size(), expected.begin());
  std::cout << "expected=";
  for (uint8_t b : expected) std::cout << std::hex << int(b);
  std::cout << " actual=";
  for (uint8_t b : rec2.session_key) std::cout << std::hex << int(b);
  std::cout << std::dec << std::endl;
  assert(expected == rec2.session_key);

  // Сценарий: устройство A генерирует новый ключ, устройство B применяет его,
  // затем устройство A пересчитывает сессию через regenerateFromPeer.
  assert(generateLocalKey());
  KeyRecord rec3;
  assert(loadKeyRecord(rec3));
  assert(rec3.origin == KeyOrigin::LOCAL);
  assert(rec3.peer_public == remote_pub);
  // Проверяем, что новая генерация создаёт отличающийся публичный ключ.
  assert(rec3.root_public != first_public);
  std::array<uint8_t,32> shared_remote_second{};
  crypto::x25519::compute_shared(remote_priv, rec3.root_public, shared_remote_second);
  std::array<uint8_t,96> buf_second{};
  std::copy(shared_remote_second.begin(), shared_remote_second.end(), buf_second.begin());
  std::array<std::array<uint8_t,32>, 2> ordered_pubs_second = {rec3.root_public, remote_pub};
  std::sort(ordered_pubs_second.begin(), ordered_pubs_second.end());
  // Буфер формируется так же, как и в прошивке, чтобы смоделировать сторону B.
  std::copy(ordered_pubs_second[0].begin(), ordered_pubs_second[0].end(),
            buf_second.begin() + shared_remote_second.size());
  std::copy(ordered_pubs_second[1].begin(), ordered_pubs_second[1].end(),
            buf_second.begin() + shared_remote_second.size() + ordered_pubs_second[0].size());
  auto digest_second = crypto::sha256::hash(buf_second.data(), buf_second.size());
  std::array<uint8_t,16> expected_second{};
  std::copy_n(digest_second.begin(), expected_second.size(), expected_second.begin());
  assert(regenerateFromPeer());
  KeyRecord rec4;
  assert(loadKeyRecord(rec4));
  assert(rec4.origin == KeyOrigin::REMOTE);
  assert(rec4.peer_public == remote_pub);
  assert(rec4.session_key == expected_second);
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
