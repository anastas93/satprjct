#include "key_safe_mode.h"
#include <string>
#include <utility>
#include "default_settings.h"
#include "libs/simple_logger/simple_logger.h"

namespace {
// Колбэк применения состояния шифрования к рабочим модулям.
std::function<void(bool)> gApplyEncryption;
// Глобальные флаги, которыми управляет основной код.
bool* gEncryptionFlag = nullptr;
bool* gSafeModeFlag = nullptr;
bool* gStorageReadyFlag = nullptr;
// Резерв предыдущего состояния шифрования на момент входа в safe mode.
bool gEncryptionBackup = false;
bool gEncryptionBackupValid = false;

// Возвращает строку контекста с подстановкой значения по умолчанию.
std::string contextOrDefault(const char* reason) {
  return std::string(reason ? reason : "без контекста");
}

// Локальный помощник для логирования невозможности восстановления.
void logRestoreFailure(const std::string& context) {
  SimpleLogger::logStatus("KEY SAFE MODE RESTORE FAIL: " + context);
  LOG_ERROR("Key safe mode: не удалось восстановить исходный режим шифрования (%s)",
            context.c_str());
}

// Проверяем готовность контроллера и сообщаем о проблеме в лог.
bool ensureControllerConfigured(const char* action) {
  if (gApplyEncryption && gEncryptionFlag && gSafeModeFlag && gStorageReadyFlag) {
    return true;
  }
  LOG_ERROR("Key safe mode: контроллер не инициализирован, операция %s недоступна", action);
  return false;
}
} // namespace

void configureKeySafeModeController(std::function<void(bool)> applyEncryption,
                                    bool* encryptionFlag,
                                    bool* safeModeFlag,
                                    bool* storageReadyFlag) {
  gApplyEncryption = std::move(applyEncryption);
  gEncryptionFlag = encryptionFlag;
  gSafeModeFlag = safeModeFlag;
  gStorageReadyFlag = storageReadyFlag;
  gEncryptionBackupValid = false; // сбрасываем резерв на случай повторной конфигурации
}

void disableEncryptionForSafeMode() {
  if (!ensureControllerConfigured("disable")) {
    return;
  }
  // При первом входе в safe mode фиксируем текущее состояние шифрования.
  if (!gEncryptionBackupValid && gEncryptionFlag) {
    gEncryptionBackup = *gEncryptionFlag;
    gEncryptionBackupValid = true;
  }
  if (gEncryptionFlag && *gEncryptionFlag) {
    *gEncryptionFlag = false;
  }
  if (gApplyEncryption) {
    gApplyEncryption(false);
  }
}

void activateKeySafeMode(const char* reason) {
  if (!ensureControllerConfigured("activate")) {
    return;
  }
  const std::string context = contextOrDefault(reason);
  if (gStorageReadyFlag) {
    *gStorageReadyFlag = false;
  }
  // Сохраняем предыдущее состояние только при первом входе.
  if (gSafeModeFlag && !*gSafeModeFlag && gEncryptionFlag) {
    gEncryptionBackup = *gEncryptionFlag;
    gEncryptionBackupValid = true;
  }
  disableEncryptionForSafeMode();
  if (gSafeModeFlag && !*gSafeModeFlag) {
    *gSafeModeFlag = true;
    SimpleLogger::logStatus("KEY SAFE MODE ON: " + context);
    LOG_ERROR("CRITICAL: хранилище ключей недоступно (%s), шифрование отключено и ключевые команды заблокированы",
              context.c_str());
  } else {
    LOG_ERROR("CRITICAL: повторная ошибка доступа к хранилищу ключей (%s), защищённый режим уже активен",
              context.c_str());
  }
}

void deactivateKeySafeMode(const char* reason) {
  if (!ensureControllerConfigured("deactivate")) {
    return;
  }
  const std::string context = contextOrDefault(reason);
  if (gStorageReadyFlag) {
    *gStorageReadyFlag = true;
  }
  if (!gSafeModeFlag || !*gSafeModeFlag) {
    return; // режим уже снят, дополнительных действий не требуется
  }
  *gSafeModeFlag = false;
  bool restored = false;
  if (gEncryptionBackupValid && gEncryptionFlag) {
    *gEncryptionFlag = gEncryptionBackup;
    if (gApplyEncryption) {
      gApplyEncryption(gEncryptionBackup);
    }
    restored = true;
  } else {
    if (gApplyEncryption && gEncryptionFlag) {
      gApplyEncryption(*gEncryptionFlag);
    }
    logRestoreFailure(context);
  }
  gEncryptionBackupValid = false;
  SimpleLogger::logStatus("KEY SAFE MODE OFF: " + context);
  LOG_INFO("Key: хранилище ключей восстановлено (%s), защищённый режим снят", context.c_str());
  if (!restored) {
    LOG_ERROR("Key safe mode: автоматическое восстановление шифрования не выполнено (%s)",
              context.c_str());
  }
}
