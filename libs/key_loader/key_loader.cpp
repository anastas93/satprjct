#include "key_loader.h"
#include "../crypto/sha256.h"
#include "../crypto/x25519.h"
#include "default_settings.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef ARDUINO
#include <filesystem>
#include <fstream>
#else
#include <SPIFFS.h>
#include <FS.h>
#include "../fs_utils/spiffs_guard.h"
#endif

namespace KeyLoader {
namespace {

constexpr uint8_t MAGIC[4] = {'S', 'T', 'K', '1'};
constexpr uint8_t VERSION = 1;
constexpr size_t PACKED_SIZE = 4 + 1 + 1 + 2 + 4 + 4 + 16 + 32 + 32 + 32;
constexpr size_t DIGEST_SIZE = crypto::sha256::DIGEST_SIZE;

#ifndef ARDUINO
const char* KEY_DIR = "key_storage";
const char* KEY_FILE = "key_storage/key.stkey";
const char* KEY_FILE_OLD = "key_storage/key.stkey.old";
const char* LEGACY_FILE = "key_storage/key.bin";
#else
const char* KEY_DIR = "/keys";
const char* KEY_FILE = "/keys/key.stkey";
const char* KEY_FILE_OLD = "/keys/key.stkey.old";
#endif

KeyRecord g_cache;
bool g_cache_valid = false;

std::array<uint8_t,16> truncateDigest(const std::array<uint8_t, crypto::sha256::DIGEST_SIZE>& digest) {
  std::array<uint8_t,16> out{};
  std::copy_n(digest.begin(), out.size(), out.begin());
  return out;
}

uint32_t nonceSaltFromKey(const std::array<uint8_t,16>& key) {
  auto digest = crypto::sha256::hash(key.data(), key.size());
  return static_cast<uint32_t>(digest[0]) |
         (static_cast<uint32_t>(digest[1]) << 8) |
         (static_cast<uint32_t>(digest[2]) << 16) |
         (static_cast<uint32_t>(digest[3]) << 24);
}

std::array<uint8_t,16> deriveLocalSession(const std::array<uint8_t,32>& pub,
                                          const std::array<uint8_t,32>& priv) {
  std::array<uint8_t,64> buf{};
  std::copy(pub.begin(), pub.end(), buf.begin());
  std::copy(priv.begin(), priv.end(), buf.begin() + pub.size());
  auto digest = crypto::sha256::hash(buf.data(), buf.size());
  return truncateDigest(digest);
}

std::array<uint8_t,16> deriveSessionFromShared(const std::array<uint8_t,32>& shared,
                                               const std::array<uint8_t,32>& local_pub,
                                               const std::array<uint8_t,32>& remote_pub) {
  std::array<uint8_t,96> buf{};
  std::copy(shared.begin(), shared.end(), buf.begin());
  std::copy(local_pub.begin(), local_pub.end(), buf.begin() + shared.size());
  std::copy(remote_pub.begin(), remote_pub.end(), buf.begin() + shared.size() + local_pub.size());
  auto digest = crypto::sha256::hash(buf.data(), buf.size());
  return truncateDigest(digest);
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

bool readFile(std::vector<uint8_t>& out) {
  std::ifstream f(KEY_FILE, std::ios::binary);
  if (!f.good()) return false;
  f.seekg(0, std::ios::end);
  auto sz = static_cast<size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  out.resize(sz);
  if (sz == 0) return false;
  f.read(reinterpret_cast<char*>(out.data()), sz);
  return f.good();
}

bool writeFile(const std::vector<uint8_t>& data) {
  if (!ensureDir()) return false;
  try {
    std::filesystem::path current(KEY_FILE);
    std::filesystem::path backup(KEY_FILE_OLD);
    if (std::filesystem::exists(backup)) {
      std::filesystem::remove(backup);
    }
    if (std::filesystem::exists(current)) {
      std::filesystem::rename(current, backup);
    }
    std::ofstream f(KEY_FILE, std::ios::binary | std::ios::trunc);
    if (!f.good()) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return f.good();
  } catch (...) {
    return false;
  }
}

bool hasBackup() {
  return std::filesystem::exists(KEY_FILE_OLD);
}

bool restoreBackup() {
  try {
    if (!std::filesystem::exists(KEY_FILE_OLD)) return false;
    if (std::filesystem::exists(KEY_FILE)) {
      std::filesystem::remove(KEY_FILE);
    }
    std::filesystem::rename(KEY_FILE_OLD, KEY_FILE);
    return true;
  } catch (...) {
    return false;
  }
}

bool readLegacy(std::array<uint8_t,16>& key) {
  std::ifstream f(LEGACY_FILE, std::ios::binary);
  if (!f.good()) return false;
  f.read(reinterpret_cast<char*>(key.data()), key.size());
  return f.good();
}
#else
bool ensureDir() {
  if (!fs_utils::ensureSpiffsMounted()) return false;
  if (!SPIFFS.exists(KEY_DIR)) {
    return SPIFFS.mkdir(KEY_DIR);
  }
  return true;
}

bool readFile(std::vector<uint8_t>& out) {
  if (!fs_utils::ensureSpiffsMounted()) return false;
  File f = SPIFFS.open(KEY_FILE, "r");
  if (!f) return false;
  size_t sz = f.size();
  if (sz == 0) { f.close(); return false; }
  out.resize(sz);
  size_t read = f.read(out.data(), sz);
  f.close();
  return read == sz;
}

bool writeFile(const std::vector<uint8_t>& data) {
  if (!fs_utils::ensureSpiffsMounted()) return false;
  if (SPIFFS.exists(KEY_FILE_OLD)) SPIFFS.remove(KEY_FILE_OLD);
  if (SPIFFS.exists(KEY_FILE)) SPIFFS.rename(KEY_FILE, KEY_FILE_OLD);
  File f = SPIFFS.open(KEY_FILE, "w");
  if (!f) return false;
  size_t written = f.write(data.data(), data.size());
  f.close();
  return written == data.size();
}

bool hasBackup() {
  if (!fs_utils::ensureSpiffsMounted(false)) return false;
  return SPIFFS.exists(KEY_FILE_OLD);
}

bool restoreBackup() {
  if (!fs_utils::ensureSpiffsMounted()) return false;
  if (!SPIFFS.exists(KEY_FILE_OLD)) return false;
  if (SPIFFS.exists(KEY_FILE)) SPIFFS.remove(KEY_FILE);
  return SPIFFS.rename(KEY_FILE_OLD, KEY_FILE);
}

bool readLegacy(std::array<uint8_t,16>&) { return false; }
#endif

std::vector<uint8_t> serialize(const KeyRecord& rec) {
  std::vector<uint8_t> data(PACKED_SIZE, 0);
  size_t offset = 0;
  std::memcpy(data.data() + offset, MAGIC, sizeof(MAGIC)); offset += sizeof(MAGIC);
  data[offset++] = VERSION;
  data[offset++] = static_cast<uint8_t>(rec.origin);
  data[offset++] = 0; data[offset++] = 0; // reserved
  uint32_t salt = rec.nonce_salt;
  data[offset++] = static_cast<uint8_t>(salt & 0xFF);
  data[offset++] = static_cast<uint8_t>((salt >> 8) & 0xFF);
  data[offset++] = static_cast<uint8_t>((salt >> 16) & 0xFF);
  data[offset++] = static_cast<uint8_t>((salt >> 24) & 0xFF);
  offset += 4; // reserved2
  std::memcpy(data.data() + offset, rec.session_key.data(), rec.session_key.size()); offset += rec.session_key.size();
  std::memcpy(data.data() + offset, rec.root_public.data(), rec.root_public.size()); offset += rec.root_public.size();
  std::memcpy(data.data() + offset, rec.root_private.data(), rec.root_private.size()); offset += rec.root_private.size();
  std::memcpy(data.data() + offset, rec.peer_public.data(), rec.peer_public.size()); offset += rec.peer_public.size();
  auto digest = crypto::sha256::hash(data.data(), data.size());
  data.insert(data.end(), digest.begin(), digest.end());
  return data;
}

bool deserialize(const std::vector<uint8_t>& raw, KeyRecord& out) {
  if (raw.size() < PACKED_SIZE + DIGEST_SIZE) return false;
  auto digest = crypto::sha256::hash(raw.data(), PACKED_SIZE);
  if (!std::equal(digest.begin(), digest.end(), raw.data() + PACKED_SIZE)) {
    return false;
  }
  size_t offset = 0;
  if (std::memcmp(raw.data(), MAGIC, sizeof(MAGIC)) != 0) return false;
  offset += sizeof(MAGIC);
  uint8_t version = raw[offset++];
  if (version != VERSION) return false;
  uint8_t origin = raw[offset++];
  if (origin > static_cast<uint8_t>(KeyOrigin::REMOTE)) return false;
  offset += 2; // reserved
  uint32_t salt = static_cast<uint32_t>(raw[offset]) |
                  (static_cast<uint32_t>(raw[offset + 1]) << 8) |
                  (static_cast<uint32_t>(raw[offset + 2]) << 16) |
                  (static_cast<uint32_t>(raw[offset + 3]) << 24);
  offset += 4;
  offset += 4; // reserved2
  KeyRecord rec;
  rec.origin = static_cast<KeyOrigin>(origin);
  rec.nonce_salt = salt;
  std::copy_n(raw.data() + offset, rec.session_key.size(), rec.session_key.begin()); offset += rec.session_key.size();
  std::copy_n(raw.data() + offset, rec.root_public.size(), rec.root_public.begin()); offset += rec.root_public.size();
  std::copy_n(raw.data() + offset, rec.root_private.size(), rec.root_private.begin()); offset += rec.root_private.size();
  std::copy_n(raw.data() + offset, rec.peer_public.size(), rec.peer_public.begin()); offset += rec.peer_public.size();
  rec.valid = true;
  out = rec;
  return true;
}

KeyRecord makeDefaultRecord() {
  KeyRecord rec;
  rec.origin = KeyOrigin::LOCAL;
  rec.session_key = DefaultSettings::DEFAULT_KEY;
  auto digest = crypto::sha256::hash(rec.session_key.data(), rec.session_key.size());
  std::copy_n(digest.begin(), rec.root_private.size(), rec.root_private.begin());
  rec.root_private[0] &= 248;
  rec.root_private[31] &= 127;
  rec.root_private[31] |= 64;
  crypto::x25519::derive_public(rec.root_public, rec.root_private);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.valid = true;
  rec.peer_public.fill(0);
  return rec;
}

bool writeRecord(const KeyRecord& rec) {
  KeyRecord tmp = rec;
  if (tmp.nonce_salt == 0) {
    tmp.nonce_salt = nonceSaltFromKey(tmp.session_key);
  }
  auto data = serialize(tmp);
  if (!writeFile(data)) return false;
  g_cache = tmp;
  g_cache_valid = true;
  return true;
}

bool loadRecordFromDisk(KeyRecord& out) {
  std::vector<uint8_t> raw;
  if (!readFile(raw)) return false;
  if (!deserialize(raw, out)) return false;
  g_cache = out;
  g_cache_valid = true;
  return true;
}

bool tryLoad(KeyRecord& out) {
  if (g_cache_valid) {
    out = g_cache;
    return true;
  }
  if (loadRecordFromDisk(out)) return true;
  // fallback: legacy key
  std::array<uint8_t,16> legacy{};
  if (readLegacy(legacy)) {
    KeyRecord rec = makeDefaultRecord();
    rec.session_key = legacy;
    rec.nonce_salt = nonceSaltFromKey(rec.session_key);
    writeRecord(rec);
    out = rec;
    return true;
  }
  return false;
}

KeyRecord ensureRecord() {
  KeyRecord rec;
  if (tryLoad(rec)) return rec;
  KeyRecord def = makeDefaultRecord();
  if (!writeRecord(def)) {
    g_cache = def;
    g_cache_valid = false;
  }
  return def;
}

} // namespace

bool ensureStorage() { return ensureDir(); }

std::array<uint8_t,16> loadKey() {
  KeyRecord rec = ensureRecord();
  return rec.session_key;
}

bool saveKey(const std::array<uint8_t,16>& key,
             KeyOrigin origin,
             const std::array<uint8_t,32>* peer_public,
             uint32_t nonce_salt) {
  KeyRecord rec = ensureRecord();
  rec.session_key = key;
  rec.origin = origin;
  if (peer_public) rec.peer_public = *peer_public;
  if (nonce_salt != 0) {
    rec.nonce_salt = nonce_salt;
  } else {
    rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  }
  return writeRecord(rec);
}

bool generateLocalKey(KeyRecord* out) {
  if (!ensureDir()) return false;
  KeyRecord rec;
  crypto::x25519::random_bytes(rec.root_private.data(), rec.root_private.size());
  crypto::x25519::derive_public(rec.root_public, rec.root_private);
  rec.session_key = deriveLocalSession(rec.root_public, rec.root_private);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.origin = KeyOrigin::LOCAL;
  rec.peer_public.fill(0);
  rec.valid = true;
  if (!writeRecord(rec)) return false;
  if (out) *out = rec;
  return true;
}

bool restorePreviousKey(KeyRecord* out) {
  if (!restoreBackup()) return false;
  g_cache_valid = false;
  KeyRecord rec;
  if (!loadRecordFromDisk(rec)) return false;
  if (out) *out = rec;
  return true;
}

bool applyRemotePublic(const std::array<uint8_t,32>& remote_public,
                       KeyRecord* out) {
  KeyRecord rec = ensureRecord();
  std::array<uint8_t,32> shared{};
  if (!crypto::x25519::compute_shared(rec.root_private, remote_public, shared)) {
    return false;
  }
  rec.session_key = deriveSessionFromShared(shared, rec.root_public, remote_public);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.origin = KeyOrigin::REMOTE;
  rec.peer_public = remote_public;
  rec.valid = true;
  if (!writeRecord(rec)) return false;
  if (out) *out = rec;
  return true;
}

bool loadKeyRecord(KeyRecord& out) {
  if (tryLoad(out)) return true;
  out = makeDefaultRecord();
  out.valid = false;
  return false;
}

KeyState getState() {
  KeyRecord rec = ensureRecord();
  KeyState st;
  st.valid = rec.valid;
  st.origin = rec.origin;
  st.nonce_salt = rec.nonce_salt;
  st.session_key = rec.session_key;
  st.root_public = rec.root_public;
  st.peer_public = rec.peer_public;
  st.has_backup = hasBackup();
  return st;
}

std::array<uint8_t,32> getPublicKey() {
  KeyRecord rec = ensureRecord();
  return rec.root_public;
}

std::array<uint8_t,12> makeNonce(uint32_t msg_id, uint16_t frag_idx) {
  KeyRecord rec = ensureRecord();
  std::array<uint8_t,12> nonce{};
  nonce[0] = static_cast<uint8_t>(msg_id & 0xFF);
  nonce[1] = static_cast<uint8_t>((msg_id >> 8) & 0xFF);
  nonce[2] = static_cast<uint8_t>((msg_id >> 16) & 0xFF);
  nonce[3] = static_cast<uint8_t>((msg_id >> 24) & 0xFF);
  nonce[4] = static_cast<uint8_t>(frag_idx & 0xFF);
  nonce[5] = static_cast<uint8_t>((frag_idx >> 8) & 0xFF);
  uint32_t salt = rec.nonce_salt;
  nonce[6] = static_cast<uint8_t>(salt & 0xFF);
  nonce[7] = static_cast<uint8_t>((salt >> 8) & 0xFF);
  nonce[8] = static_cast<uint8_t>((salt >> 16) & 0xFF);
  nonce[9] = static_cast<uint8_t>((salt >> 24) & 0xFF);
  nonce[10] = static_cast<uint8_t>(((salt >> 16) & 0xFF) ^ (msg_id & 0xFF));
  nonce[11] = static_cast<uint8_t>(((salt >> 24) & 0xFF) ^ ((msg_id >> 8) & 0xFF));
  return nonce;
}

std::array<uint8_t,4> keyId(const std::array<uint8_t,16>& key) {
  auto digest = crypto::sha256::hash(key.data(), key.size());
  std::array<uint8_t,4> id{};
  std::copy_n(digest.begin(), id.size(), id.begin());
  return id;
}

} // namespace KeyLoader

