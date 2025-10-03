#include <cassert>
#include <string>
#include "key_safe_mode.h"
#include "libs/log_hook/log_hook.h"

// Глобальные флаги, имитирующие рабочее окружение прошивки.
bool encryptionEnabled = true;
bool keyStorageReady = true;
bool keySafeModeActive = false;

int main() {
  bool appliedEncryption = true; // отражает состояние, переданное в tx/rx.
  configureKeySafeModeController(
      [&](bool enabled) {
        appliedEncryption = enabled;
      },
      &encryptionEnabled,
      &keySafeModeActive,
      &keyStorageReady);

  // Стартовое состояние: шифрование активно, safe mode выключен.
  encryptionEnabled = true;
  keyStorageReady = true;
  keySafeModeActive = false;
  appliedEncryption = true;
  LogHook::clear();
  assert(!keySafeModeHasReason());
  assert(keySafeModeReason().empty());

  // Эмулируем временную недоступность хранилища: активируем safe mode.
  activateKeySafeMode("storage-missing");
  assert(keySafeModeActive);
  assert(!keyStorageReady);
  assert(!encryptionEnabled);
  assert(!appliedEncryption);
  assert(keySafeModeHasReason());
  assert(keySafeModeReason() == "storage-missing");

  // Повторная активация с тем же контекстом не должна порождать CRITICAL-сообщение.
  LogHook::clear();
  activateKeySafeMode("storage-missing");
  auto duplicate = LogHook::getRecent(4);
  for (const auto& entry : duplicate) {
    assert(entry.text.find("повторная ошибка доступа") == std::string::npos);
  }
  assert(keySafeModeReason() == "storage-missing");

  // Новый контекст при активном режиме должен обновляться без критического уровня.
  LogHook::clear();
  activateKeySafeMode("storage-degraded");
  bool contextUpdated = false;
  auto updateLogs = LogHook::getRecent(4);
  for (const auto& entry : updateLogs) {
    if (entry.text.find("KEY SAFE MODE CONTEXT UPDATE") != std::string::npos) {
      contextUpdated = true;
    }
    assert(entry.text.find("повторная ошибка доступа") == std::string::npos);
  }
  assert(contextUpdated);
  assert(keySafeModeReason() == "storage-degraded");

  // Восстанавливаем хранилище: safe mode должен вернуть исходное состояние шифрования.
  encryptionEnabled = false; // имитируем обнулённый глобальный флаг.
  keyStorageReady = false;
  LogHook::clear();
  deactivateKeySafeMode("storage-restored");
  assert(keyStorageReady);
  assert(!keySafeModeActive);
  assert(encryptionEnabled);        // возвращено к значению до активации safe mode.
  assert(appliedEncryption);        // колбэк применил восстановленное значение.
  assert(!keySafeModeHasReason());
  assert(keySafeModeReason().empty());

  // Убеждаемся, что журнал не содержит сообщений о неудачном восстановлении.
  auto recent = LogHook::getRecent(4);
  for (const auto& entry : recent) {
    assert(entry.text.find("RESTORE FAIL") == std::string::npos);
  }
  return 0;
}
