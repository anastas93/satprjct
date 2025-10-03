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

  // Эмулируем временную недоступность хранилища: активируем safe mode.
  activateKeySafeMode("storage-missing");
  assert(keySafeModeActive);
  assert(!keyStorageReady);
  assert(!encryptionEnabled);
  assert(!appliedEncryption);

  // Восстанавливаем хранилище: safe mode должен вернуть исходное состояние шифрования.
  encryptionEnabled = false; // имитируем обнулённый глобальный флаг.
  keyStorageReady = false;
  LogHook::clear();
  deactivateKeySafeMode("storage-restored");
  assert(keyStorageReady);
  assert(!keySafeModeActive);
  assert(encryptionEnabled);        // возвращено к значению до активации safe mode.
  assert(appliedEncryption);        // колбэк применил восстановленное значение.

  // Убеждаемся, что журнал не содержит сообщений о неудачном восстановлении.
  auto recent = LogHook::getRecent(4);
  for (const auto& entry : recent) {
    assert(entry.text.find("RESTORE FAIL") == std::string::npos);
  }
  return 0;
}
