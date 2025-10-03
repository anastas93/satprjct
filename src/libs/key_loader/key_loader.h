#pragma once
#include <array>
#include <cstdint>
#include <optional>

#ifdef ARDUINO
class __FlashStringHelper;
#endif

namespace KeyLoader {

#ifdef ARDUINO
// Тип обработчика логов KeyLoader для Arduino: принимает строку во флеше и
// возвращает true при успешной доставке сообщения.
using LogCallback = bool (*)(const ::__FlashStringHelper* message);
#else
// Хостовый обработчик логов KeyLoader, работающий со строками в обычной памяти.
using LogCallback = bool (*)(const char* message);
#endif

// Доступные варианты хранилища ключей
enum class StorageBackend : uint8_t {
  UNKNOWN = 0,  // режим автоопределения или отсутствие хранилища
  NVS = 1,      // Preferences/NVS (ESP32)
  FILESYSTEM = 2, // обычная файловая система (хостовые сборки)
};

// Происхождение активного ключа
enum class KeyOrigin : uint8_t {
  LOCAL = 0,   // локально сгенерированный ключ
  REMOTE = 1 // получен после обмена по ECDH
};

// Полная запись ключей для хранения на диске/в NVS
struct KeyRecord {
  bool valid = false;                                  // удалось ли прочитать запись
  KeyOrigin origin = KeyOrigin::LOCAL;                 // происхождение активного ключа
  uint32_t nonce_salt = 0;                             // соль для генерации нонсов
  std::array<uint8_t,16> session_key{};                // симметричный ключ AES-CCM
  std::array<uint8_t,32> root_private{};               // приватный корневой ключ Curve25519
  std::array<uint8_t,32> root_public{};                // публичный корневой ключ
  std::array<uint8_t,32> peer_public{};                // публичный ключ удалённой стороны (если есть)
};

// Сокращённое состояние для отображения в UI
struct KeyState {
  bool valid = false;                                  // есть ли сохранённый ключ
  KeyOrigin origin = KeyOrigin::LOCAL;                 // тип текущего ключа
  uint32_t nonce_salt = 0;                             // соль для нонсов
  std::array<uint8_t,16> session_key{};                // активный симметричный ключ
  std::array<uint8_t,32> root_public{};                // публичный корневой ключ устройства
  std::array<uint8_t,32> peer_public{};                // последний известный ключ удалённой стороны
  bool has_backup = false;                             // есть ли резервная копия предыдущего ключа
  StorageBackend backend = StorageBackend::UNKNOWN;    // используемое хранилище
};

// Создание и проверка хранилища (папки/директории). Выполняется автоматически при чтении.
bool ensureStorage();

// Загрузка симметричного ключа шифрования (AES-CCM). При отсутствии возвращает значение по умолчанию.
std::array<uint8_t,16> loadKey();

// Сохранение симметричного ключа (например, при получении внешнего значения).
// Можно указать происхождение и публичный ключ удалённой стороны.
bool saveKey(const std::array<uint8_t,16>& key,
             KeyOrigin origin = KeyOrigin::LOCAL,
             const std::array<uint8_t,32>* peer_public = nullptr,
             uint32_t nonce_salt = 0);

// Запись нового локального ключа (генерация пары Curve25519 и симметричного ключа из неё).
// Предыдущее состояние переносится в секцию `previous`.
bool generateLocalKey(KeyRecord* out = nullptr);

// Восстановление предыдущего ключа (если сохранена секция `previous`).
bool restorePreviousKey(KeyRecord* out = nullptr);

// Обновление активного ключа по публичному ключу удалённой стороны (ECDH + SHA-256).
// Возвращает false при недопустимом публичном ключе или ошибке записи.
bool applyRemotePublic(const std::array<uint8_t,32>& remote_public,
                       const std::array<uint8_t,32>* remote_ephemeral = nullptr,
                       KeyRecord* out = nullptr);

// Повторное применение сохранённого публичного ключа удалённой стороны.
bool regenerateFromPeer(const std::array<uint8_t,32>* remote_ephemeral = nullptr,
                        KeyRecord* out = nullptr);

// Получение полного содержимого файла (для тестов и служебных задач).
bool loadKeyRecord(KeyRecord& out);

// Краткая информация для UI/логов.
KeyState getState();

// Публичный корневой ключ устройства.
std::array<uint8_t,32> getPublicKey();

// Генерация нонса для AEAD по компактному заголовку и 16-битному идентификатору сообщения.
std::array<uint8_t,12> makeNonce(uint8_t version,
                                 uint16_t frag_cnt,
                                 uint32_t packed_meta,
                                 uint16_t msg_id);

// 4-байтовый идентификатор ключа (первые байты SHA-256 от симметричного ключа).
std::array<uint8_t,4> keyId(const std::array<uint8_t,16>& key);

// Есть ли сохранённый публичный ключ удалённой стороны.
bool hasPeerPublic();

// Предпросчёт идентификатора ключа для текущего удалённого партнёра без изменения состояния.
bool previewPeerKeyId(std::array<uint8_t,4>& key_id_out);

// Активный бэкенд хранения (NVS/файловая система).
StorageBackend getBackend();

// Предпочитаемый бэкенд (UNKNOWN означает автоматический выбор).
StorageBackend getPreferredBackend();

// Установка предпочтения бэкенда (UNKNOWN включает автоматический выбор).
bool setPreferredBackend(StorageBackend backend);

// Короткое текстовое имя бэкенда для логов/UI.
const char* backendName(StorageBackend backend);

// Запуск нового эпемерного сеанса: генерирует пару X25519 и возвращает публичный ключ.
bool startEphemeralSession(std::array<uint8_t,32>& public_out, bool force_new = true);

// Проверка наличия активной эпемерной пары.
bool hasEphemeralSession();

// Сброс активной эпемерной пары (зануление приватного ключа).
void endEphemeralSession();

// Установка пользовательского обработчика логов KeyLoader.
void setLogCallback(LogCallback callback);

}  // namespace KeyLoader


