#pragma once
// Управление защищённым режимом хранилища ключей и состоянием шифрования.
// Все комментарии на русском согласно пользовательским требованиям.

#include <functional>

// Конфигурация контроллера защищённого режима: задаёт флаги состояния и
// колбэк, который применяет новое значение шифрования к модулям передачи/приёма.
void configureKeySafeModeController(std::function<void(bool)> applyEncryption,
                                    bool* encryptionFlag,
                                    bool* safeModeFlag,
                                    bool* storageReadyFlag);

// Принудительное отключение шифрования при переходе в защищённый режим.
void disableEncryptionForSafeMode();

// Активация защищённого режима при временной недоступности хранилища ключей.
void activateKeySafeMode(const char* reason);

// Деактивация защищённого режима после восстановления хранилища ключей.
void deactivateKeySafeMode(const char* reason);
