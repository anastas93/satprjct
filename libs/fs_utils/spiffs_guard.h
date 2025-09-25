#pragma once

// Вспомогательный модуль для безопасного монтирования SPIFFS.
// Все комментарии намеренно оставлены на русском языке.

#include <cstdint>

#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#if defined(ESP32) && __has_include("esp_ipc.h")
#include <esp_err.h>
#include <esp_ipc.h>
#define FS_UTILS_HAS_ESP_IPC 1
#else
#define FS_UTILS_HAS_ESP_IPC 0
#endif
#endif

#ifndef FS_UTILS_HAS_ESP_IPC
#define FS_UTILS_HAS_ESP_IPC 0
#endif

namespace fs_utils {

// Детальное состояние операции монтирования SPIFFS.
enum class SpiffsMountStatus : uint8_t {
  kSuccess = 0,               // раздел успешно смонтирован (или уже был смонтирован)
  kMountFailed,               // SPIFFS.begin(false) не смог смонтировать раздел
  kFormatFailed,              // SPIFFS.format() вернул false
  kFormatIpcError,            // esp_ipc_call_blocking завершился с ошибкой
  kPostFormatMountFailed,     // после форматирования не удалось смонтировать раздел
  kBeginTrueFailed            // SPIFFS.begin(true) также не помог смонтировать раздел
};

// Структура-результат, содержащая признак успеха и код причины.
struct SpiffsMountResult {
  bool mounted = false;                       // true, если раздел смонтирован
  SpiffsMountStatus status = SpiffsMountStatus::kMountFailed;  // итоговый статус операции
  int32_t error_code = 0;                     // дополнительный код ошибки (esp_err_t или 0)

  bool ok() const { return mounted; }         // удобная проверка успеха
};

// Текстовое описание статуса для отображения пользователю.
inline const char* describeStatus(SpiffsMountStatus status) {
  switch (status) {
    case SpiffsMountStatus::kSuccess: return "успешное монтирование";
    case SpiffsMountStatus::kMountFailed: return "не удалось смонтировать SPIFFS";
    case SpiffsMountStatus::kFormatFailed: return "форматирование SPIFFS не завершилось";
    case SpiffsMountStatus::kFormatIpcError: return "ошибка IPC при форматировании SPIFFS";
    case SpiffsMountStatus::kPostFormatMountFailed: return "после форматирования раздел не монтируется";
    case SpiffsMountStatus::kBeginTrueFailed: return "повторная попытка SPIFFS.begin(true) не помогла";
  }
  return "неизвестная ошибка SPIFFS";
}

#ifdef ARDUINO
// Обёртка над SPIFFS.begin() с повторными попытками форматирования и монтирования.
inline SpiffsMountResult ensureSpiffsMounted(bool allowFormat = true) {
  static bool mounted = false;
  static bool formatAttempted = false;
  SpiffsMountResult result{};
  if (mounted) {
    result.mounted = true;
    result.status = SpiffsMountStatus::kSuccess;
    return result;
  }

  if (SPIFFS.begin(false)) {
    mounted = true;
    result.mounted = true;
    result.status = SpiffsMountStatus::kSuccess;
    return result;
  }

  Serial.println(F("SPIFFS: первичное монтирование не удалось"));
  if (!allowFormat) {
    result.status = SpiffsMountStatus::kMountFailed;
    return result;
  }

  bool shouldRetryFormat = !formatAttempted;
  if (shouldRetryFormat) {
    formatAttempted = true;
    Serial.println(F("SPIFFS: требуется форматирование, пробуем очистить раздел"));
    bool formatted = false;
#if FS_UTILS_HAS_ESP_IPC
    // Форматируем раздел на основном ядре через IPC, чтобы избежать assert внутри FreeRTOS.
    struct FormatContext {
      bool formatted = false;
    } ctx;
    constexpr int kFormatCpu = 0;  // основной (PRO) процессор ESP32
    esp_err_t ipcResult = esp_ipc_call_blocking(kFormatCpu, [](void* arg) {
      auto* context = static_cast<FormatContext*>(arg);
      context->formatted = SPIFFS.format();
    }, &ctx);
    if (ipcResult != ESP_OK) {
      Serial.print(F("SPIFFS: ошибка IPC при форматировании, код="));
      Serial.println(static_cast<int>(ipcResult));
      result.status = SpiffsMountStatus::kFormatIpcError;
      result.error_code = static_cast<int32_t>(ipcResult);
      formatAttempted = false;  // позволяем повторить попытку позже
      return result;
    }
    formatted = ctx.formatted;
#else
    formatted = SPIFFS.format();
#endif

    if (formatted) {
      if (SPIFFS.begin(false)) {
        Serial.println(F("SPIFFS: раздел успешно отформатирован"));
        mounted = true;
        result.mounted = true;
        result.status = SpiffsMountStatus::kSuccess;
        return result;
      }
      Serial.println(F("SPIFFS: монтирование после форматирования не удалось"));
      result.status = SpiffsMountStatus::kPostFormatMountFailed;
      formatAttempted = false;  // разрешаем повторное форматирование при следующем вызове
    } else {
      Serial.println(F("SPIFFS: форматирование не удалось"));
      result.status = SpiffsMountStatus::kFormatFailed;
      formatAttempted = false;  // даём шанс повторить попытку форматирования
    }
  }

  Serial.println(F("SPIFFS: пробуем монтировать с автоформатированием (begin(true))"));
  if (SPIFFS.begin(true)) {
    Serial.println(F("SPIFFS: begin(true) успешно смонтировал раздел"));
    mounted = true;
    formatAttempted = true;
    result.mounted = true;
    result.status = SpiffsMountStatus::kSuccess;
    return result;
  }

  Serial.println(F("SPIFFS: begin(true) тоже не смог смонтировать раздел"));
  result.status = SpiffsMountStatus::kBeginTrueFailed;
  return result;
}

#else  // !ARDUINO

// На хосте SPIFFS не используется, поэтому всегда возвращаем успех.
inline SpiffsMountResult ensureSpiffsMounted(bool = true) {
  SpiffsMountResult result{};
  result.mounted = true;
  result.status = SpiffsMountStatus::kSuccess;
  result.error_code = 0;
  return result;
}

#endif

}  // namespace fs_utils

