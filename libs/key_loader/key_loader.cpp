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
#if defined(ESP32)
#include <Preferences.h>
#define KEY_LOADER_HAS_NVS 1
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
constexpr uint8_t RECORD_VERSION = 1;
constexpr size_t RECORD_PACKED_SIZE = 4 + 1 + 1 + 2 + 4 + 4 + 16 + 32 + 32 + 32;
constexpr size_t DIGEST_SIZE = crypto::sha256::DIGEST_SIZE;

constexpr uint8_t STORAGE_MAGIC[4] = {'S', 'T', 'B', '1'};
constexpr uint8_t STORAGE_VERSION = 1;
constexpr size_t STORAGE_HEADER_SIZE = 4 + 1 + 1 + 2 + 4 + 4;

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

#ifdef ARDUINO
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

std::vector<uint8_t> serializeRecord(const KeyRecord& rec) {
  std::vector<uint8_t> data(RECORD_PACKED_SIZE + DIGEST_SIZE, 0);
  size_t offset = 0;
  std::memcpy(data.data() + offset, RECORD_MAGIC, sizeof(RECORD_MAGIC)); offset += sizeof(RECORD_MAGIC);
  data[offset++] = RECORD_VERSION;
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
  if (version != RECORD_VERSION) return false;
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

std::vector<uint8_t> serializeSnapshot(const StorageSnapshot& snapshot) {
  std::vector<uint8_t> current_blob =
      (snapshot.current_valid && snapshot.current.valid) ? serializeRecord(snapshot.current) : std::vector<uint8_t>();
  std::vector<uint8_t> previous_blob =
      (snapshot.previous_valid && snapshot.previous.valid) ? serializeRecord(snapshot.previous) : std::vector<uint8_t>();
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
  if (rec.nonce_salt == 0) {
    rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  }
  rec.valid = true;
}

StorageSnapshot makeDefaultSnapshot() {
  StorageSnapshot snapshot;
  KeyRecord rec;
  rec.origin = KeyOrigin::LOCAL;
  rec.session_key = DefaultSettings::DEFAULT_KEY;
  auto digest = crypto::sha256::hash(rec.session_key.data(), rec.session_key.size());
  std::copy_n(digest.begin(), rec.root_private.size(), rec.root_private.begin());
  rec.root_private[0] &= 248;
  rec.root_private[31] &= 127;
  rec.root_private[31] |= 64;
  crypto::x25519::derive_public(rec.root_public, rec.root_private);
  rec.peer_public.fill(0);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.valid = true;
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
  key.valid = true;
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

bool ensurePrefs() {
  static bool opened = false;
  if (!opened) {
    Preferences& prefs = prefsInstance();
    if (!prefs.begin("key_store", false)) {
      Serial.println(F("KeyLoader: не удалось открыть раздел NVS"));
      return false;
    }
    opened = true;
  }
  return true;
}

BlobType readBlob(std::vector<uint8_t>& out) {
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
  if (!ensurePrefs()) return;
  Preferences& prefs = prefsInstance();
  if (prefs.isKey("current")) prefs.remove("current");
  if (prefs.isKey("backup")) prefs.remove("backup");
}

bool writePrimary(const std::vector<uint8_t>& data) {
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
  if (!outcome.loaded || !snapshot.current_valid) {
    snapshot = makeDefaultSnapshot();
    if (backend_ok) {
      writeSnapshot(snapshot);
    }
  } else if (outcome.from_legacy && backend_ok) {
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
  KeyRecord rec;
  crypto::x25519::random_bytes(rec.root_private.data(), rec.root_private.size());
  crypto::x25519::derive_public(rec.root_public, rec.root_private);
  rec.session_key = deriveLocalSession(rec.root_public, rec.root_private);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.origin = KeyOrigin::LOCAL;
  rec.peer_public.fill(0);
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

bool applyRemoteWithBase(StorageSnapshot snapshot,
                         const KeyRecord& base,
                         const std::array<uint8_t,32>& remote_public,
                         KeyRecord* out) {
  std::array<uint8_t,32> shared{};
  if (!crypto::x25519::compute_shared(base.root_private, remote_public, shared)) {
    return false;
  }
  KeyRecord rec = base;
  rec.session_key = deriveSessionFromShared(shared, base.root_public, remote_public);
  rec.nonce_salt = nonceSaltFromKey(rec.session_key);
  rec.origin = KeyOrigin::REMOTE;
  rec.peer_public = remote_public;
  rec.valid = true;
  if (snapshot.current_valid && snapshot.current.valid) {
    snapshot.previous = snapshot.current;
    snapshot.previous_valid = true;
  } else {
    snapshot.previous_valid = false;
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

bool applyRemotePublic(const std::array<uint8_t,32>& remote_public,
                       KeyRecord* out) {
  StorageSnapshot snapshot = ensureSnapshot();
  if (!snapshot.current_valid || !snapshot.current.valid) return false;
  return applyRemoteWithBase(snapshot, snapshot.current, remote_public, out);
}

bool regenerateFromPeer(KeyRecord* out) {
  StorageSnapshot snapshot = ensureSnapshot();
  auto has_peer = [](const std::array<uint8_t,32>& peer) {
    return std::any_of(peer.begin(), peer.end(), [](uint8_t b) { return b != 0; });
  };
  if (snapshot.current_valid && snapshot.current.valid && has_peer(snapshot.current.peer_public)) {
    return applyRemoteWithBase(snapshot, snapshot.current, snapshot.current.peer_public, out);
  }
  if (snapshot.previous_valid && snapshot.previous.valid && has_peer(snapshot.previous.peer_public)) {
    // При откате на предыдущую запись пытаемся использовать старую пару ключей
    return applyRemoteWithBase(snapshot, snapshot.previous, snapshot.previous.peer_public, out);
  }
  return false;
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

std::array<uint8_t,32> getPublicKey() {
  StorageSnapshot& snapshot = ensureSnapshot();
  return snapshot.current.root_public;
}

std::array<uint8_t,12> makeNonce(uint32_t msg_id, uint16_t frag_idx) {
  StorageSnapshot& snapshot = ensureSnapshot();
  std::array<uint8_t,12> nonce{};
  nonce[0] = static_cast<uint8_t>(msg_id & 0xFF);
  nonce[1] = static_cast<uint8_t>((msg_id >> 8) & 0xFF);
  nonce[2] = static_cast<uint8_t>((msg_id >> 16) & 0xFF);
  nonce[3] = static_cast<uint8_t>((msg_id >> 24) & 0xFF);
  nonce[4] = static_cast<uint8_t>(frag_idx & 0xFF);
  nonce[5] = static_cast<uint8_t>((frag_idx >> 8) & 0xFF);
  uint32_t salt = snapshot.current.nonce_salt;
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

StorageBackend getBackend() { return activeBackend(); }

StorageBackend getPreferredBackend() { return g_preferred_backend; }

bool setPreferredBackend(StorageBackend backend) {
#ifdef ARDUINO
#if KEY_LOADER_HAS_NVS
  if (backend == StorageBackend::UNKNOWN || backend == StorageBackend::NVS) {
    g_preferred_backend = backend;
    return true;
  }
  Serial.println(F("KeyLoader: доступен только бэкенд NVS"));
  return false;
#else
  (void)backend;
  Serial.println(F("KeyLoader: на этой платформе нет доступного хранилища"));
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

} // namespace KeyLoader

