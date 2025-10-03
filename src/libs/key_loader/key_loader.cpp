#include "key_loader.h"
#include "../crypto/hkdf.h"
#include "../crypto/sha256.h"
#include "../crypto/x25519.h"
#include "default_settings.h"
#include "libs/config_loader/config_loader.h" // загрузка конфигурации запуска
#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <vector>

#ifndef ARDUINO
#include <filesystem>
#include <fstream>
#include <iostream>
#else
#include <Arduino.h>
#if defined(ESP32)
#include <Preferences.h>
#define KEY_LOADER_HAS_NVS 1
#include <esp_system.h>
#include <esp_flash_encrypt.h>
#else
#define KEY_LOADER_HAS_NVS 0
#endif
#endif

#ifndef KEY_LOADER_HAS_NVS
#define KEY_LOADER_HAS_NVS 0
#endif

namespace KeyLoader {
namespace {

constexpr uint8_t RECORD_MAGIC[4] = {'S', 'T', 'K', '1'};
constexpr uint8_t RECORD_VERSION = 2;
constexpr size_t RECORD_PACKED_SIZE = 4 + 1 + 1 + 2 + 4 + 4 + 16 + 32 + 32 + 32;
constexpr size_t DIGEST_SIZE = crypto::sha256::DIGEST_SIZE;

constexpr uint8_t STORAGE_MAGIC[4] = {'S', 'T', 'B', '1'};
constexpr uint8_t STORAGE_VERSION = 1;
constexpr size_t STORAGE_HEADER_SIZE = 4 + 1 + 1 + 2 + 4 + 4;

constexpr uint8_t RECORD_FLAG_SEALED_PRIV = 0x01;
constexpr char PRIVATE_SEAL_INFO[] = "KeyLoader sealed root v1";

#ifndef ARDUINO
const char* KEY_DIR = "key_storage";
const char* KEY_FILE = "key_storage/key.stkey";
const char* LEGACY_BACKUP_FILE = "key_storage/key.stkey.old";
const char* LEGACY_FILE = "key_storage/key.bin";
#else
const char* LEGACY_FILE = nullptr;
#endif

struct StorageSnapshot {
  KeyRecord current;
  KeyRecord previous;
  bool current_valid = false;
  bool previous_valid = false;
};

StorageSnapshot g_snapshot;
bool g_snapshot_loaded = false;
StorageBackend g_preferred_backend = StorageBackend::UNKNOWN;

struct EphemeralState {
  bool active = false;                                 // есть ли сгенерированная пара
  std::array<uint8_t,32> private_key{};                // эпемерный приватный ключ
  std::array<uint8_t,32> public_key{};                 // эпемерный публичный ключ
};

EphemeralState g_ephemeral;

#ifdef ARDUINO
using FlashMessage = const ::__FlashStringHelper*;

LogCallback g_log_callback = nullptr;
std::vector<FlashMessage> g_buffered_logs;

// Прямая доставка сообщения через пользовательский колбэк либо Serial.
bool emitLogDirect(FlashMessage msg) {
  if (g_log_callback) {
    if (g_log_callback(msg)) {
      return true;
    }
  }
  if (Serial) {
    Serial.println(msg);
    return true;
  }
  return false;
}

// Попытка выгрузить буфер накопленных сообщений.
void flushBufferedLogs() {
  if (g_buffered_logs.empty()) {
    return;
  }
  if (!g_log_callback && !Serial) {
    return;
  }
  std::vector<FlashMessage> pending;
  pending.reserve(g_buffered_logs.size());
  for (FlashMessage msg : g_buffered_logs) {
    if (!emitLogDirect(msg)) {
      pending.push_back(msg);
    }
  }
  g_buffered_logs.swap(pending);
}

// Унифицированный вывод сообщения KeyLoader с безопасной буферизацией до
// готовности Serial.
bool logMessage(FlashMessage msg) {
  flushBufferedLogs();
  if (emitLogDirect(msg)) {
    return true;
  }
  g_buffered_logs.push_back(msg);
  return false;
}

StorageBackend activeBackend() {
#if KEY_LOADER_HAS_NVS
  return StorageBackend::NVS;
#else
  return StorageBackend::UNKNOWN;
#endif
}
#else
StorageBackend activeBackend() { return StorageBackend::FILESYSTEM; }
#endif

constexpr char LOCAL_INFO[] = "KeyLoader local session v2";
constexpr char REMOTE_STATIC_INFO[] = "KeyLoader remote session v2 static";
constexpr char REMOTE_EPHEMERAL_INFO[] = "KeyLoader remote session v2 ephemeral";
constexpr char NONCE_INFO[] = "KeyLoader nonce v2";

#ifndef ARDUINO
bool g_warned_host_key = false;
bool g_warned_hkdf_fallback = false;

// Разбор HEX-ключа из переменной окружения.
bool parseHexKey(const char* value, std::array<uint8_t,32>& key) {
  if (!value) return false;
  size_t len = std::strlen(value);
  if (len != key.size() * 2) return false;
  auto hexVal = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
  };
  for (size_t i = 0; i < key.size(); ++i) {
    int hi = hexVal(value[2 * i]);
    int lo = hexVal(value[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    key[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

// Получение мастер-ключа для шифрования приватного ключа в файловой системе.
bool getHostMasterKey(std::array<uint8_t,32>& key) {
  const char* env = std::getenv("KEYLOADER_HOST_MASTER_KEY");
  if (parseHexKey(env, key)) {
    return true;
  }
  // Фолбэк: используем детерминированное значение и предупреждаем разработчика.
  auto digest = crypto::sha256::hash(reinterpret_cast<const uint8_t*>("KeyLoader host default"),
                                     sizeof("KeyLoader host default") - 1);
  std::copy_n(digest.begin(), key.size(), key.begin());
  if (!g_warned_host_key) {
    g_warned_host_key = true;
    std::cerr << "[KeyLoader] ВНИМАНИЕ: используется предустановленный ключ для эмуляции шифрования."
              << " Задайте переменную KEYLOADER_HOST_MASTER_KEY для повышения безопасности." << std::endl;
  }
  return true;
}
#endif

// Нужна ли программная защита приватного ключа в хранилище.
bool shouldSealPrivateKey() {
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  return false;  // При включённом Flash Encryption данные защищены аппаратно.
#else
  return true;
#endif
#else
  return true;
#endif
}

// Формирование маски для зашифровки приватного ключа.
bool deriveSealMask(uint32_t salt,
                    std::array<uint8_t,32>& mask,
                    bool& used_fallback) {
  std::array<uint8_t,4> salt_bytes{
      static_cast<uint8_t>(salt & 0xFF),
      static_cast<uint8_t>((salt >> 8) & 0xFF),
      static_cast<uint8_t>((salt >> 16) & 0xFF),
      static_cast<uint8_t>((salt >> 24) & 0xFF)};
  used_fallback = false;
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  // Для ESP32 делаем вид, что Flash Encryption обрабатывает ключ на уровне носителя.
  (void)salt_bytes;
  mask.fill(0);
  return true;
#else
  (void)salt_bytes;
  mask.fill(0);
  return false;
#endif
#else
  std::array<uint8_t,32> master{};
  if (!getHostMasterKey(master)) return false;
  bool ok = crypto::hkdf::derive(salt_bytes.data(),
                                 salt_bytes.size(),
                                 master.data(),
                                 master.size(),
                                 reinterpret_cast<const uint8_t*>(PRIVATE_SEAL_INFO),
                                 sizeof(PRIVATE_SEAL_INFO) - 1,
                                 mask.data(),
                                 mask.size());
  if (!ok) {
    used_fallback = true;
    std::array<uint8_t,36> buf{};
    std::copy(master.begin(), master.end(), buf.begin());
    std::copy(salt_bytes.begin(), salt_bytes.end(), buf.begin() + master.size());
    auto digest = crypto::sha256::hash(buf.data(), buf.size());
    std::copy_n(digest.begin(), mask.size(), mask.begin());
  }
  return true;
#endif
}

// Шифрование приватного ключа при записи в файловую систему.
bool sealPrivateKey(std::array<uint8_t,32>& priv, uint32_t& salt_out) {
  if (!shouldSealPrivateKey()) {
    salt_out = 0;
    return true;
  }
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  salt_out = 0;
  return true;
#else
  return false;
#endif
#else
  uint32_t salt = 0;
  std::array<uint8_t,32> mask{};
  bool used_fallback = false;
  // Генерируем ненулевую соль.
  do {
    std::array<uint8_t,4> tmp{};
    crypto::x25519::random_bytes(tmp.data(), tmp.size());
    salt = static_cast<uint32_t>(tmp[0]) |
           (static_cast<uint32_t>(tmp[1]) << 8) |
           (static_cast<uint32_t>(tmp[2]) << 16) |
           (static_cast<uint32_t>(tmp[3]) << 24);
  } while (salt == 0);
  if (!deriveSealMask(salt, mask, used_fallback)) return false;
  for (size_t i = 0; i < priv.size(); ++i) {
    priv[i] ^= mask[i];
  }
  salt_out = salt;
  if (used_fallback && !g_warned_hkdf_fallback) {
    g_warned_hkdf_fallback = true;
    std::cerr << "[KeyLoader] ВНИМАНИЕ: HKDF недоступен, используется SHA-256 для шифрования приватного ключа." << std::endl;
  }
  return true;
#endif
}

// Расшифровка приватного ключа после чтения из хранилища.
bool unsealPrivateKey(std::array<uint8_t,32>& priv, uint32_t salt, bool was_sealed) {
  if (!shouldSealPrivateKey()) {
    return true;
  }
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  (void)priv;
  (void)salt;
  (void)was_sealed;
  return true;
#else
  return false;
#endif
#else
  if (!was_sealed) {
    // Старый формат: приватный ключ не зашифрован, ничего не делаем.
    return true;
  }
  std::array<uint8_t,32> mask{};
  bool used_fallback = false;
  if (!deriveSealMask(salt, mask, used_fallback)) return false;
  for (size_t i = 0; i < priv.size(); ++i) {
    priv[i] ^= mask[i];
  }
  if (used_fallback && !g_warned_hkdf_fallback) {
    g_warned_hkdf_fallback = true;
    std::cerr << "[KeyLoader] ВНИМАНИЕ: HKDF недоступен, используется SHA-256 для расшифровки приватного ключа." << std::endl;
  }
  return true;
#endif
}

uint32_t nonceSaltFromKeyLegacy(const std::array<uint8_t,16>& key) {
  auto digest = crypto::sha256::hash(key.data(), key.size());
  return static_cast<uint32_t>(digest[0]) |
         (static_cast<uint32_t>(digest[1]) << 8) |
         (static_cast<uint32_t>(digest[2]) << 16) |
         (static_cast<uint32_t>(digest[3]) << 24);
}

uint32_t nonceSaltFromKey(const std::array<uint8_t,16>& key) {
  std::array<uint8_t, sizeof(uint32_t)> okm{};
  if (!crypto::hkdf::derive(nullptr,
                             0,
                             key.data(),
                             key.size(),
                             reinterpret_cast<const uint8_t*>(NONCE_INFO),
                             sizeof(NONCE_INFO) - 1,
                             okm.data(),
                             okm.size())) {
    return nonceSaltFromKeyLegacy(key);
  }
  return static_cast<uint32_t>(okm[0]) |
         (static_cast<uint32_t>(okm[1]) << 8) |
         (static_cast<uint32_t>(okm[2]) << 16) |
         (static_cast<uint32_t>(okm[3]) << 24);
}

std::array<uint8_t,16> deriveLocalSession(const std::array<uint8_t,32>& pub,
                                          const std::array<uint8_t,32>& priv) {
  std::array<uint8_t,16> session{};
  bool ok = crypto::hkdf::derive(pub.data(),
                                 pub.size(),
                                 priv.data(),
                                 priv.size(),
                                 reinterpret_cast<const uint8_t*>(LOCAL_INFO),
                                 sizeof(LOCAL_INFO) - 1,
                                 session.data(),
                                 session.size());
  if (!ok) {
    std::array<uint8_t,64> buf{};
    std::copy(pub.begin(), pub.end(), buf.begin());
    std::copy(priv.begin(), priv.end(), buf.begin() + pub.size());
    auto digest = crypto::sha256::hash(buf.data(), buf.size());
    std::copy_n(digest.begin(), session.size(), session.begin());
  }
  return session;
}

std::array<uint8_t,16> deriveSessionFromShared(const std::array<uint8_t,32>& shared,
                                               const std::array<uint8_t,32>& local_pub,
                                               const std::array<uint8_t,32>& remote_pub,
                                               bool use_ephemeral = false) {
  std::array<std::array<uint8_t,32>, 2> ordered_pubs = {local_pub, remote_pub};
  std::sort(ordered_pubs.begin(), ordered_pubs.end());
  std::array<uint8_t,64> salt{};
  std::copy(ordered_pubs[0].begin(), ordered_pubs[0].end(), salt.begin());
  std::copy(ordered_pubs[1].begin(), ordered_pubs[1].end(), salt.begin() + ordered_pubs[0].size());
  const uint8_t* info_ptr = reinterpret_cast<const uint8_t*>(use_ephemeral ? REMOTE_EPHEMERAL_INFO : REMOTE_STATIC_INFO);
  size_t info_len = (use_ephemeral ? sizeof(REMOTE_EPHEMERAL_INFO) : sizeof(REMOTE_STATIC_INFO)) - 1;
  std::array<uint8_t,16> session{};
  bool ok = crypto::hkdf::derive(salt.data(),
                                 salt.size(),
                                 shared.data(),
                                 shared.size(),
                                 info_ptr,
                                 info_len,
                                 session.data(),
                                 session.size());
  if (!ok) {
    std::array<uint8_t,96> buf{};
    std::copy(shared.begin(), shared.end(), buf.begin());
    std::copy(ordered_pubs[0].begin(), ordered_pubs[0].end(), buf.begin() + shared.size());
    std::copy(ordered_pubs[1].begin(), ordered_pubs[1].end(),
              buf.begin() + shared.size() + ordered_pubs[0].size());
    auto digest = crypto::sha256::hash(buf.data(), buf.size());
    std::copy_n(digest.begin(), session.size(), session.begin());
  }
  return session;
}

std::vector<uint8_t> serializeRecord(const KeyRecord& rec) {
  KeyRecord sealed = rec;
  uint8_t flags = 0;
  uint32_t priv_salt = 0;
  if (!sealPrivateKey(sealed.root_private, priv_salt)) {
    return {};
  }
  if (shouldSealPrivateKey() && priv_salt != 0) {
    flags |= RECORD_FLAG_SEALED_PRIV;
  }
  std::vector<uint8_t> data(RECORD_PACKED_SIZE + DIGEST_SIZE, 0);
  size_t offset = 0;
  std::memcpy(data.data() + offset, RECORD_MAGIC, sizeof(RECORD_MAGIC)); offset += sizeof(RECORD_MAGIC);
  data[offset++] = RECORD_VERSION;
  data[offset++] = static_cast<uint8_t>(sealed.origin);
  data[offset++] = flags;
  data[offset++] = 0; // reserved
  uint32_t salt = sealed.nonce_salt;
  data[offset++] = static_cast<uint8_t>(salt & 0xFF);
  data[offset++] = static_cast<uint8_t>((salt >> 8) & 0xFF);
  data[offset++] = static_cast<uint8_t>((salt >> 16) & 0xFF);
  data[offset++] = static_cast<uint8_t>((salt >> 24) & 0xFF);
  data[offset++] = static_cast<uint8_t>(priv_salt & 0xFF);
  data[offset++] = static_cast<uint8_t>((priv_salt >> 8) & 0xFF);
  data[offset++] = static_cast<uint8_t>((priv_salt >> 16) & 0xFF);
  data[offset++] = static_cast<uint8_t>((priv_salt >> 24) & 0xFF);
  std::memcpy(data.data() + offset, sealed.session_key.data(), sealed.session_key.size()); offset += sealed.session_key.size();
  std::memcpy(data.data() + offset, sealed.root_public.data(), sealed.root_public.size()); offset += sealed.root_public.size();
  std::memcpy(data.data() + offset, sealed.root_private.data(), sealed.root_private.size()); offset += sealed.root_private.size();
  std::memcpy(data.data() + offset, sealed.peer_public.data(), sealed.peer_public.size()); offset += sealed.peer_public.size();
  auto digest = crypto::sha256::hash(data.data(), RECORD_PACKED_SIZE);
  std::memcpy(data.data() + offset, digest.data(), digest.size());
  return data;
}

bool deserializeRecord(const std::vector<uint8_t>& raw, KeyRecord& out) {
  if (raw.size() != RECORD_PACKED_SIZE + DIGEST_SIZE) return false;
  auto digest = crypto::sha256::hash(raw.data(), RECORD_PACKED_SIZE);
  if (!std::equal(digest.begin(), digest.end(), raw.data() + RECORD_PACKED_SIZE)) {
    return false;
  }
  size_t offset = 0;
  if (std::memcmp(raw.data(), RECORD_MAGIC, sizeof(RECORD_MAGIC)) != 0) return false;
  offset += sizeof(RECORD_MAGIC);
  uint8_t version = raw[offset++];
  if (version == 0 || version > RECORD_VERSION) return false;
  uint8_t origin = raw[offset++];
  if (origin > static_cast<uint8_t>(KeyOrigin::REMOTE)) return false;
  uint8_t flags = 0;
  if (version >= 2) {
    flags = raw[offset++];
    offset++; // reserved
  } else {
    offset += 2;
  }
  uint32_t salt = static_cast<uint32_t>(raw[offset]) |
                  (static_cast<uint32_t>(raw[offset + 1]) << 8) |
                  (static_cast<uint32_t>(raw[offset + 2]) << 16) |
                  (static_cast<uint32_t>(raw[offset + 3]) << 24);
  offset += 4;
  uint32_t priv_salt = 0;
  if (version >= 2) {
    priv_salt = static_cast<uint32_t>(raw[offset]) |
                (static_cast<uint32_t>(raw[offset + 1]) << 8) |
                (static_cast<uint32_t>(raw[offset + 2]) << 16) |
                (static_cast<uint32_t>(raw[offset + 3]) << 24);
  }
  offset += 4;
  KeyRecord rec;
  rec.origin = static_cast<KeyOrigin>(origin);
  rec.nonce_salt = salt;
  std::copy_n(raw.data() + offset, rec.session_key.size(), rec.session_key.begin()); offset += rec.session_key.size();
  std::copy_n(raw.data() + offset, rec.root_public.size(), rec.root_public.begin()); offset += rec.root_public.size();
  std::copy_n(raw.data() + offset, rec.root_private.size(), rec.root_private.begin()); offset += rec.root_private.size();
  std::copy_n(raw.data() + offset, rec.peer_public.size(), rec.peer_public.begin()); offset += rec.peer_public.size();
  bool sealed = (flags & RECORD_FLAG_SEALED_PRIV) != 0;
  if (version >= 2) {
    if (!unsealPrivateKey(rec.root_private, priv_salt, sealed)) return false;
  }
  rec.valid = true;
  out = rec;
  return true;
}

std::vector<uint8_t> serializeSnapshot(const StorageSnapshot& snapshot) {
  std::vector<uint8_t> current_blob;
  if (snapshot.current_valid && snapshot.current.valid) {
    current_blob = serializeRecord(snapshot.current);
    if (current_blob.empty()) return {};
  }
  std::vector<uint8_t> previous_blob;
  if (snapshot.previous_valid && snapshot.previous.valid) {
    previous_blob = serializeRecord(snapshot.previous);
    if (previous_blob.empty()) return {};
  }
  size_t payload_size = STORAGE_HEADER_SIZE + current_blob.size() + previous_blob.size();
  std::vector<uint8_t> data(payload_size + DIGEST_SIZE, 0);
  size_t offset = 0;
  std::memcpy(data.data() + offset, STORAGE_MAGIC, sizeof(STORAGE_MAGIC)); offset += sizeof(STORAGE_MAGIC);
  data[offset++] = STORAGE_VERSION;
  uint8_t flags = 0;
  if (!current_blob.empty()) flags |= 0x01;
  if (!previous_blob.empty()) flags |= 0x02;
  data[offset++] = flags;
  data[offset++] = 0;
  data[offset++] = 0;
  auto write_u32 = [&](uint32_t value) {
    data[offset++] = static_cast<uint8_t>(value & 0xFF);
    data[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[offset++] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[offset++] = static_cast<uint8_t>((value >> 24) & 0xFF);
  };
  write_u32(static_cast<uint32_t>(current_blob.size()));
  write_u32(static_cast<uint32_t>(previous_blob.size()));
  if (!current_blob.empty()) {
    std::memcpy(data.data() + offset, current_blob.data(), current_blob.size());
    offset += current_blob.size();
  }
  if (!previous_blob.empty()) {
    std::memcpy(data.data() + offset, previous_blob.data(), previous_blob.size());
    offset += previous_blob.size();
  }
  auto digest = crypto::sha256::hash(data.data(), offset);
  std::memcpy(data.data() + offset, digest.data(), digest.size());
  return data;
}

bool deserializeSnapshot(const std::vector<uint8_t>& raw, StorageSnapshot& snapshot) {
  if (raw.size() < STORAGE_HEADER_SIZE + DIGEST_SIZE) return false;
  size_t payload = raw.size() - DIGEST_SIZE;
  auto digest = crypto::sha256::hash(raw.data(), payload);
  if (!std::equal(digest.begin(), digest.end(), raw.data() + payload)) {
    return false;
  }
  size_t offset = 0;
  if (std::memcmp(raw.data(), STORAGE_MAGIC, sizeof(STORAGE_MAGIC)) != 0) return false;
  offset += sizeof(STORAGE_MAGIC);
  uint8_t version = raw[offset++];
  if (version != STORAGE_VERSION) return false;
  uint8_t flags = raw[offset++];
  offset += 2; // reserved
  auto read_u32 = [&](uint32_t& value) {
    value = static_cast<uint32_t>(raw[offset]) |
            (static_cast<uint32_t>(raw[offset + 1]) << 8) |
            (static_cast<uint32_t>(raw[offset + 2]) << 16) |
            (static_cast<uint32_t>(raw[offset + 3]) << 24);
    offset += 4;
  };
  uint32_t current_len = 0;
  uint32_t previous_len = 0;
  read_u32(current_len);
  read_u32(previous_len);
  if (payload != STORAGE_HEADER_SIZE + current_len + previous_len) return false;
  StorageSnapshot result;
  if ((flags & 0x01) != 0) {
    if (current_len == 0) return false;
    std::vector<uint8_t> buf(current_len);
    std::copy_n(raw.data() + offset, current_len, buf.begin());
    if (!deserializeRecord(buf, result.current)) return false;
    result.current_valid = true;
    offset += current_len;
  } else {
    if (current_len != 0) return false;
  }
  if ((flags & 0x02) != 0) {
    if (previous_len == 0) return false;
    std::vector<uint8_t> buf(previous_len);
    std::copy_n(raw.data() + offset, previous_len, buf.begin());
    if (!deserializeRecord(buf, result.previous)) return false;
    result.previous_valid = true;
    offset += previous_len;
  } else {
    if (previous_len != 0) return false;
  }
  snapshot = result;
  return true;
}

void finalizeRecord(KeyRecord& rec) {
  uint32_t derived = nonceSaltFromKey(rec.session_key);
  uint32_t legacy = nonceSaltFromKeyLegacy(rec.session_key);
  if (rec.nonce_salt == 0 || rec.nonce_salt == legacy) {
    rec.nonce_salt = derived;
  }
  rec.valid = true;
}

bool migrateNonceSalt(KeyRecord& rec) {
  if (!rec.valid) return false;
  uint32_t derived = nonceSaltFromKey(rec.session_key);
  uint32_t legacy = nonceSaltFromKeyLegacy(rec.session_key);
  if (rec.nonce_salt == 0 || rec.nonce_salt == legacy) {
    rec.nonce_salt = derived;
    return true;
  }
  return false;
}

StorageSnapshot makeDefaultSnapshot() {
  StorageSnapshot snapshot;
  KeyRecord rec;
  rec.origin = KeyOrigin::LOCAL;
  rec.session_key = ConfigLoader::getConfig().keys.defaultKey; // используем ключ из конфигурации
  auto digest = crypto::sha256::hash(rec.session_key.data(), rec.session_key.size());
  std::copy_n(digest.begin(), rec.root_private.size(), rec.root_private.begin());
  rec.root_private[0] &= 248;
  rec.root_private[31] &= 127;
  rec.root_private[31] |= 64;
  crypto::x25519::derive_public(rec.root_public, rec.root_private);
  rec.peer_public.fill(0);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  finalizeRecord(rec);
  snapshot.current = rec;
  snapshot.current_valid = true;
  snapshot.previous_valid = false;
  return snapshot;
}

#ifndef ARDUINO
bool ensureDir() {
  try {
    std::filesystem::create_directories(KEY_DIR);
    return true;
  } catch (...) {
    return false;
  }
}

bool readPrimary(std::vector<uint8_t>& out) {
  std::ifstream f(KEY_FILE, std::ios::binary);
  if (!f.good()) return false;
  f.seekg(0, std::ios::end);
  auto sz = static_cast<size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  if (sz == 0) return false;
  out.resize(sz);
  f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz));
  return f.good();
}

bool writePrimary(const std::vector<uint8_t>& data) {
  if (!ensureDir()) return false;
  try {
    std::ofstream f(KEY_FILE, std::ios::binary | std::ios::trunc);
    if (!f.good()) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f.good()) return false;
  } catch (...) {
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(LEGACY_BACKUP_FILE, ec);
  return true;
}

bool readLegacyBackup(KeyRecord& out) {
  std::ifstream f(LEGACY_BACKUP_FILE, std::ios::binary);
  if (!f.good()) return false;
  f.seekg(0, std::ios::end);
  auto sz = static_cast<size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  if (sz == 0) return false;
  std::vector<uint8_t> raw(sz);
  f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz));
  if (!f.good()) return false;
  return deserializeRecord(raw, out);
}

bool readLegacyBinary(KeyRecord& key) {
  if (!LEGACY_FILE) return false;
  std::ifstream f(LEGACY_FILE, std::ios::binary);
  if (!f.good()) return false;
  f.read(reinterpret_cast<char*>(key.session_key.data()), key.session_key.size());
  if (!f.good()) return false;
  key.origin = KeyOrigin::LOCAL;
  key.peer_public.fill(0);
  key.nonce_salt = nonceSaltFromKey(key.session_key);
  auto digest = crypto::sha256::hash(key.session_key.data(), key.session_key.size());
  std::copy_n(digest.begin(), key.root_private.size(), key.root_private.begin());
  key.root_private[0] &= 248;
  key.root_private[31] &= 127;
  key.root_private[31] |= 64;
  crypto::x25519::derive_public(key.root_public, key.root_private);
  finalizeRecord(key);
  return true;
}
#else
#if KEY_LOADER_HAS_NVS
enum class BlobType : uint8_t {
  kNone = 0,
  kNewFormat,
  kLegacyCurrent
};

Preferences& prefsInstance() {
  static Preferences prefs;
  return prefs;
}

bool ensureFlashEncryptionEnabled() {
  bool enabled = esp_flash_encryption_enabled();
  static bool warned = false;
  if (!enabled && !warned) {
    // При старте глобальные конструкторы вызывают KeyLoader до инициализации Serial.
    // Чтобы избежать аварии при обращении к неготовому UART, проверяем доступность
    // Serial перед выводом предупреждения и повторяем попытку при следующем вызове.
    if (logMessage(F("KeyLoader: Flash Encryption выключена, доступ к NVS запрещён"))) {
      warned = true;
    }
  }
  return enabled;
}

bool ensurePrefs() {
  static bool opened = false;
  if (!opened) {
    if (!ensureFlashEncryptionEnabled()) {
      return false;
    }
    Preferences& prefs = prefsInstance();
    if (!prefs.begin("key_store", false)) {
      logMessage(F("KeyLoader: не удалось открыть раздел NVS"));
      return false;
    }
    opened = true;
  }
  return true;
}

BlobType readBlob(std::vector<uint8_t>& out) {
  if (!ensureFlashEncryptionEnabled()) return BlobType::kNone;
  if (!ensurePrefs()) return BlobType::kNone;
  Preferences& prefs = prefsInstance();
  size_t len = prefs.getBytesLength("records");
  if (len > 0) {
    out.resize(len);
    size_t read = prefs.getBytes("records", out.data(), len);
    if (read == len) return BlobType::kNewFormat;
    out.clear();
  }
  len = prefs.getBytesLength("current");
  if (len == 0) return BlobType::kNone;
  out.resize(len);
  size_t read = prefs.getBytes("current", out.data(), len);
  if (read != len) {
    out.clear();
    return BlobType::kNone;
  }
  return BlobType::kLegacyCurrent;
}

bool readLegacyBackup(KeyRecord& out) {
  if (!ensureFlashEncryptionEnabled()) return false;
  if (!ensurePrefs()) return false;
  Preferences& prefs = prefsInstance();
  size_t len = prefs.getBytesLength("backup");
  if (len == 0) return false;
  std::vector<uint8_t> raw(len);
  size_t read = prefs.getBytes("backup", raw.data(), len);
  if (read != len) return false;
  return deserializeRecord(raw, out);
}

void clearLegacyBlobs() {
  if (!ensureFlashEncryptionEnabled()) return;
  if (!ensurePrefs()) return;
  Preferences& prefs = prefsInstance();
  if (prefs.isKey("current")) prefs.remove("current");
  if (prefs.isKey("backup")) prefs.remove("backup");
}

bool writePrimary(const std::vector<uint8_t>& data) {
  if (!ensureFlashEncryptionEnabled()) return false;
  if (!ensurePrefs()) return false;
  Preferences& prefs = prefsInstance();
  size_t written = prefs.putBytes("records", data.data(), data.size());
  if (written != data.size()) return false;
  clearLegacyBlobs();
  return true;
}
#else
enum class BlobType : uint8_t { kNone = 0, kNewFormat, kLegacyCurrent };
bool ensurePrefs() { return false; }
BlobType readBlob(std::vector<uint8_t>&, BlobType = BlobType::kNone) { return BlobType::kNone; }
bool readLegacyBackup(KeyRecord&) { return false; }
void clearLegacyBlobs() {}
bool writePrimary(const std::vector<uint8_t>&) { return false; }
#endif
#endif

struct LoadOutcome {
  bool loaded = false;
  bool from_legacy = false;
};

LoadOutcome loadFromBackend(StorageSnapshot& snapshot) {
  LoadOutcome outcome;
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  std::vector<uint8_t> raw;
  BlobType type = readBlob(raw);
  if (type == BlobType::kNewFormat) {
    if (deserializeSnapshot(raw, snapshot)) {
      outcome.loaded = snapshot.current_valid && snapshot.current.valid;
    }
    return outcome;
  }
  if (type == BlobType::kLegacyCurrent) {
    KeyRecord current;
    if (deserializeRecord(raw, current)) {
      snapshot.current = current;
      snapshot.current_valid = current.valid;
      KeyRecord backup;
      if (readLegacyBackup(backup)) {
        snapshot.previous = backup;
        snapshot.previous_valid = backup.valid;
      }
      outcome.loaded = snapshot.current_valid;
      outcome.from_legacy = true;
    }
    return outcome;
  }
#else
  (void)snapshot;
#endif
#else
  std::vector<uint8_t> raw;
  if (readPrimary(raw)) {
    if (deserializeSnapshot(raw, snapshot)) {
      outcome.loaded = snapshot.current_valid && snapshot.current.valid;
      return outcome;
    }
    KeyRecord current;
    if (deserializeRecord(raw, current)) {
      snapshot.current = current;
      snapshot.current_valid = current.valid;
      KeyRecord backup;
      if (readLegacyBackup(backup)) {
        snapshot.previous = backup;
        snapshot.previous_valid = backup.valid;
      }
      outcome.loaded = snapshot.current_valid;
      outcome.from_legacy = true;
      return outcome;
    }
  }
  KeyRecord legacy{};
  if (readLegacyBinary(legacy)) {
    snapshot.current = legacy;
    snapshot.current_valid = legacy.valid;
    snapshot.previous_valid = false;
    outcome.loaded = snapshot.current_valid;
    outcome.from_legacy = true;
  }
#endif
  return outcome;
}

bool writeSnapshot(const StorageSnapshot& snapshot) {
  std::vector<uint8_t> blob = serializeSnapshot(snapshot);
  if (blob.empty()) {
#ifdef ARDUINO
    logMessage(F("KeyLoader: ошибка сериализации snapshot"));
#else
    std::cerr << "[KeyLoader] не удалось сериализовать snapshot" << std::endl;
#endif
    return false;
  }
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  return writePrimary(blob);
#else
  (void)snapshot;
  return false;
#endif
#else
  return writePrimary(blob);
#endif
}

bool ensureBackendReady() {
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  return ensurePrefs();
#else
  return false;
#endif
#else
  return ensureDir();
#endif
}

StorageSnapshot& ensureSnapshot() {
  if (g_snapshot_loaded) return g_snapshot;
  StorageSnapshot snapshot;
  bool backend_ok = ensureBackendReady();
  LoadOutcome outcome;
  if (backend_ok) {
    outcome = loadFromBackend(snapshot);
  }
  bool migrated = false;
  if (outcome.loaded && snapshot.current_valid) {
    if (snapshot.current_valid && snapshot.current.valid) {
      migrated |= migrateNonceSalt(snapshot.current);
    }
    if (snapshot.previous_valid && snapshot.previous.valid) {
      migrated |= migrateNonceSalt(snapshot.previous);
    }
  }
  if (!outcome.loaded || !snapshot.current_valid) {
    snapshot = makeDefaultSnapshot();
    if (backend_ok) {
      writeSnapshot(snapshot);
    }
  } else if ((outcome.from_legacy || migrated) && backend_ok) {
    writeSnapshot(snapshot);
  }
  g_snapshot = snapshot;
  g_snapshot_loaded = true;
  return g_snapshot;
}

bool hasBackup(const StorageSnapshot& snapshot) {
  return snapshot.previous_valid && snapshot.previous.valid;
}

} // namespace

bool ensureStorage() { return ensureBackendReady(); }

std::array<uint8_t,16> loadKey() {
  StorageSnapshot& snapshot = ensureSnapshot();
  return snapshot.current.session_key;
}

bool saveKey(const std::array<uint8_t,16>& key,
             KeyOrigin origin,
             const std::array<uint8_t,32>* peer_public,
             uint32_t nonce_salt) {
  StorageSnapshot snapshot = ensureSnapshot();
  if (snapshot.current_valid && snapshot.current.valid) {
    snapshot.previous = snapshot.current;
    snapshot.previous_valid = true;
  }
  KeyRecord next = snapshot.current;
  next.session_key = key;
  next.origin = origin;
  if (peer_public) next.peer_public = *peer_public;
  else next.peer_public.fill(0);
  if (nonce_salt != 0) {
    next.nonce_salt = nonce_salt;
  } else {
    next.nonce_salt = nonceSaltFromKey(next.session_key);
  }
  finalizeRecord(next);
  snapshot.current = next;
  snapshot.current_valid = true;
  if (!ensureBackendReady()) return false;
  if (!writeSnapshot(snapshot)) return false;
  g_snapshot = snapshot;
  g_snapshot_loaded = true;
  return true;
}

bool generateLocalKey(KeyRecord* out) {
  StorageSnapshot snapshot = ensureSnapshot();
  std::array<uint8_t,32> preserved_peer{};
  bool has_peer = false;
  if (snapshot.current_valid && snapshot.current.valid) {
    const auto& current_peer = snapshot.current.peer_public;
    has_peer = std::any_of(current_peer.begin(), current_peer.end(), [](uint8_t b) {
      return b != 0;
    });
    if (has_peer) {
      preserved_peer = current_peer;
    }
  }
  KeyRecord rec;
  crypto::x25519::random_bytes(rec.root_private.data(), rec.root_private.size());
  crypto::x25519::derive_public(rec.root_public, rec.root_private);
  rec.session_key = deriveLocalSession(rec.root_public, rec.root_private);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.origin = KeyOrigin::LOCAL;
  if (has_peer) {
    rec.peer_public = preserved_peer;
  } else {
    rec.peer_public.fill(0);
  }
  rec.valid = true;
  if (snapshot.current_valid && snapshot.current.valid) {
    snapshot.previous = snapshot.current;
    snapshot.previous_valid = true;
  }
  snapshot.current = rec;
  snapshot.current_valid = true;
  if (!ensureBackendReady()) return false;
  if (!writeSnapshot(snapshot)) return false;
  g_snapshot = snapshot;
  g_snapshot_loaded = true;
  if (out) *out = rec;
  return true;
}

bool restorePreviousKey(KeyRecord* out) {
  StorageSnapshot snapshot = ensureSnapshot();
  if (!hasBackup(snapshot)) return false;
  snapshot.current = snapshot.previous;
  snapshot.current_valid = true;
  snapshot.previous_valid = false;
  if (!ensureBackendReady()) return false;
  if (!writeSnapshot(snapshot)) return false;
  g_snapshot = snapshot;
  g_snapshot_loaded = true;
  if (out) *out = snapshot.current;
  return true;
}

bool applyRemotePublic(const std::array<uint8_t,32>& remote_public,
                       const std::array<uint8_t,32>* remote_ephemeral,
                       KeyRecord* out) {
  StorageSnapshot snapshot = ensureSnapshot();
  if (!snapshot.current_valid || !snapshot.current.valid) {
    if (remote_ephemeral) endEphemeralSession();
    return false;
  }

  if (remote_ephemeral && !g_ephemeral.active) {
    // Удалённая сторона ожидает эпемерный обмен, но локальная сторона не готова.
    endEphemeralSession();
    return false;
  }

  std::array<uint8_t,32> shared{};
  std::array<uint8_t,32> local_pub{};
  std::array<uint8_t,32> remote_pub_for_hash{};
  if (remote_ephemeral) {
    if (!crypto::x25519::compute_shared(g_ephemeral.private_key, *remote_ephemeral, shared)) {
      endEphemeralSession();
      return false;
    }
    local_pub = g_ephemeral.public_key;
    remote_pub_for_hash = *remote_ephemeral;
  } else {
    if (!crypto::x25519::compute_shared(snapshot.current.root_private, remote_public, shared)) {
      if (g_ephemeral.active) endEphemeralSession();
      return false;
    }
    local_pub = snapshot.current.root_public;
    remote_pub_for_hash = remote_public;
  }

  KeyRecord rec = snapshot.current;
  bool use_ephemeral = (remote_ephemeral != nullptr);
  rec.session_key = deriveSessionFromShared(shared, local_pub, remote_pub_for_hash, use_ephemeral);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.origin = KeyOrigin::REMOTE;
  rec.peer_public = remote_public;
  rec.valid = true;
  if (snapshot.current_valid && snapshot.current.valid) {
    snapshot.previous = snapshot.current;
    snapshot.previous_valid = true;
  }
  snapshot.current = rec;
  snapshot.current_valid = true;
  if (!ensureBackendReady()) {
    endEphemeralSession();
    return false;
  }
  if (!writeSnapshot(snapshot)) {
    endEphemeralSession();
    return false;
  }
  g_snapshot = snapshot;
  g_snapshot_loaded = true;
  if (out) *out = rec;
  // Независимо от результата обмена очищаем эпемерный приватный ключ.
  endEphemeralSession();
  return true;
}

bool regenerateFromPeer(const std::array<uint8_t,32>* remote_ephemeral,
                        KeyRecord* out) {
  StorageSnapshot snapshot = ensureSnapshot();
  if (!snapshot.current_valid || !snapshot.current.valid) return false;
  const auto& peer = snapshot.current.peer_public;
  bool has_peer = std::any_of(peer.begin(), peer.end(), [](uint8_t b) { return b != 0; });
  if (!has_peer) return false;
  return applyRemotePublic(peer, remote_ephemeral, out);
}

bool loadKeyRecord(KeyRecord& out) {
  StorageSnapshot& snapshot = ensureSnapshot();
  if (!snapshot.current_valid) return false;
  out = snapshot.current;
  return true;
}

KeyState getState() {
  StorageSnapshot& snapshot = ensureSnapshot();
  KeyState st;
  st.valid = snapshot.current_valid && snapshot.current.valid;
  st.origin = snapshot.current.origin;
  st.nonce_salt = snapshot.current.nonce_salt;
  st.session_key = snapshot.current.session_key;
  st.root_public = snapshot.current.root_public;
  st.peer_public = snapshot.current.peer_public;
  st.has_backup = hasBackup(snapshot);
  st.backend = getBackend();
  return st;
}

bool hasPeerPublic() {
  StorageSnapshot& snapshot = ensureSnapshot();
  if (!snapshot.current_valid || !snapshot.current.valid) return false;
  const auto& peer = snapshot.current.peer_public;
  return std::any_of(peer.begin(), peer.end(), [](uint8_t b) { return b != 0; });
}

bool previewPeerKeyId(std::array<uint8_t,4>& key_id_out) {
  StorageSnapshot& snapshot = ensureSnapshot();
  if (!snapshot.current_valid || !snapshot.current.valid) return false;
  const auto& peer = snapshot.current.peer_public;
  bool has_peer = std::any_of(peer.begin(), peer.end(), [](uint8_t b) {
    return b != 0;
  });
  if (!has_peer) return false;
  std::array<uint8_t,32> shared{};
  if (!crypto::x25519::compute_shared(snapshot.current.root_private, peer, shared)) {
    return false;
  }
  auto session = deriveSessionFromShared(shared, snapshot.current.root_public, peer);
  key_id_out = keyId(session);
  return true;
}

bool hasEphemeralSession() { return g_ephemeral.active; }

void endEphemeralSession() {
  if (!g_ephemeral.active) return;
  std::fill(g_ephemeral.private_key.begin(), g_ephemeral.private_key.end(), 0);
  std::fill(g_ephemeral.public_key.begin(), g_ephemeral.public_key.end(), 0);
  g_ephemeral.active = false;
}

bool startEphemeralSession(std::array<uint8_t,32>& public_out, bool force_new) {
  if (g_ephemeral.active && !force_new) {
    public_out = g_ephemeral.public_key;
    return true;
  }
  endEphemeralSession();
  crypto::x25519::random_bytes(g_ephemeral.private_key.data(), g_ephemeral.private_key.size());
  crypto::x25519::derive_public(g_ephemeral.public_key, g_ephemeral.private_key);
  g_ephemeral.active = true;
  public_out = g_ephemeral.public_key;
  return true;
}

std::array<uint8_t,32> getPublicKey() {
  StorageSnapshot& snapshot = ensureSnapshot();
  return snapshot.current.root_public;
}

static constexpr size_t COMPACT_HEADER_SIZE = 7; // версия, frag_cnt и packed

static std::array<uint8_t,COMPACT_HEADER_SIZE> compactHeaderBytes(uint8_t version,
                                                                  uint16_t frag_cnt,
                                                                  uint32_t packed_meta) {
  std::array<uint8_t,COMPACT_HEADER_SIZE> result{};
  result[0] = version;
  result[1] = static_cast<uint8_t>(frag_cnt >> 8);
  result[2] = static_cast<uint8_t>(frag_cnt);
  result[3] = static_cast<uint8_t>(packed_meta >> 24);
  result[4] = static_cast<uint8_t>(packed_meta >> 16);
  result[5] = static_cast<uint8_t>(packed_meta >> 8);
  result[6] = static_cast<uint8_t>(packed_meta);
  return result;
}

std::array<uint8_t,12> makeNonce(uint8_t version,
                                 uint16_t frag_cnt,
                                 uint32_t packed_meta,
                                 uint16_t msg_id) {
  StorageSnapshot& snapshot = ensureSnapshot();
  std::array<uint8_t,12> nonce{};
  auto compact = compactHeaderBytes(version, frag_cnt, packed_meta);
  std::copy(compact.begin(), compact.end(), nonce.begin());
  nonce[COMPACT_HEADER_SIZE] = static_cast<uint8_t>(msg_id >> 8);
  nonce[COMPACT_HEADER_SIZE + 1] = static_cast<uint8_t>(msg_id);
  uint32_t salt = snapshot.current.nonce_salt;
  uint8_t salt0 = static_cast<uint8_t>(salt);
  uint8_t salt1 = static_cast<uint8_t>(salt >> 8);
  uint8_t salt2 = static_cast<uint8_t>(salt >> 16);
  uint8_t salt3 = static_cast<uint8_t>(salt >> 24);
  nonce[0] ^= salt3; // смешиваем старший байт соли с версией кадра
  nonce[COMPACT_HEADER_SIZE + 2] = static_cast<uint8_t>(salt0 ^ compact.back());
  nonce[COMPACT_HEADER_SIZE + 3] = static_cast<uint8_t>(salt1 ^ nonce[COMPACT_HEADER_SIZE + 1]);
  nonce[COMPACT_HEADER_SIZE + 4] = static_cast<uint8_t>((salt2 ^ salt3) ^ nonce[COMPACT_HEADER_SIZE]);
  return nonce;
}

std::array<uint8_t,4> keyId(const std::array<uint8_t,16>& key) {
  auto digest = crypto::sha256::hash(key.data(), key.size());
  std::array<uint8_t,4> id{};
  std::copy_n(digest.begin(), id.size(), id.begin());
  return id;
}

StorageBackend getBackend() { return activeBackend(); }

StorageBackend getPreferredBackend() { return g_preferred_backend; }

bool setPreferredBackend(StorageBackend backend) {
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  if (backend == StorageBackend::UNKNOWN || backend == StorageBackend::NVS) {
    g_preferred_backend = backend;
    return true;
  }
  logMessage(F("KeyLoader: доступен только бэкенд NVS"));
  return false;
#else
  (void)backend;
  logMessage(F("KeyLoader: на этой платформе нет доступного хранилища"));
  return false;
#endif
#else
  if (backend == StorageBackend::UNKNOWN || backend == StorageBackend::FILESYSTEM) {
    g_preferred_backend = backend;
    return true;
  }
  return false;
#endif
}

const char* backendName(StorageBackend backend) {
  switch (backend) {
    case StorageBackend::NVS: return "nvs";
    case StorageBackend::FILESYSTEM: return "filesystem";
    case StorageBackend::UNKNOWN:
    default: return "unknown";
  }
}

#ifdef ARDUINO
void setLogCallback(LogCallback callback) {
  g_log_callback = callback;
  if (!g_log_callback) {
    return;
  }
  // Сначала пробуем выгрузить буфер через пользовательский обработчик — он
  // может быть готов раньше Serial (например, пересылка в ring-buffer).
  flushBufferedLogs();
  if (!g_buffered_logs.empty() && Serial) {
    // Если что-то осталось (обработчик вернул false), а UART уже инициализован,
    // повторяем попытку прямого вывода через Serial.
    flushBufferedLogs();
  }
}
#else
void setLogCallback(LogCallback) {}
#endif

} // namespace KeyLoader


