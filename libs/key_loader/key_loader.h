#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace KeyLoader {

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
  bool has_backup = false;                             // есть ли резервная копия key.stkey.old
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
// При сохранении предыдущая версия переименовывается в key.stkey.old (если была).
bool generateLocalKey(KeyRecord* out = nullptr);

// Восстановление предыдущего ключа (если существует key.stkey.old).
bool restorePreviousKey(KeyRecord* out = nullptr);

// Обновление активного ключа по публичному ключу удалённой стороны (ECDH + SHA-256).
// Возвращает false при недопустимом публичном ключе или ошибке записи.
bool applyRemotePublic(const std::array<uint8_t,32>& remote_public,
                       KeyRecord* out = nullptr);

// Получение полного содержимого файла (для тестов и служебных задач).
bool loadKeyRecord(KeyRecord& out);

// Краткая информация для UI/логов.
KeyState getState();

// Публичный корневой ключ устройства.
std::array<uint8_t,32> getPublicKey();

// Генерация нонса для AES-CCM на основе идентификатора сообщения и индекса фрагмента.
std::array<uint8_t,12> makeNonce(uint32_t msg_id, uint16_t frag_idx);

// 4-байтовый идентификатор ключа (первые байты SHA-256 от симметричного ключа).
std::array<uint8_t,4> keyId(const std::array<uint8_t,16>& key);

}  // namespace KeyLoader

