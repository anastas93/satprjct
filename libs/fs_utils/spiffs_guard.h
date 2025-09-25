#pragma once

// Вспомогательный модуль для безопасного монтирования SPIFFS.
// Все комментарии намеренно оставлены на русском языке.

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

namespace fs_utils {

// Обёртка над SPIFFS.begin() с повторной попыткой после форматирования.
// Позволяет гарантированно смонтировать раздел и не выполнять форматирование
// при каждом обращении к файловой системе.
inline bool ensureSpiffsMounted(bool allowFormat = true) {
  static bool mounted = false;
  static bool formatAttempted = false;
  if (mounted) {
    return true;
  }
  if (SPIFFS.begin(false)) {
    mounted = true;
    return true;
  }
  if (!allowFormat) {
    return false;
  }
  if (!formatAttempted) {
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
    } else {
      formatted = ctx.formatted;
    }
#else
    formatted = SPIFFS.format();
#endif
    if (formatted) {
      if (SPIFFS.begin(false)) {
        Serial.println(F("SPIFFS: раздел успешно отформатирован"));
        mounted = true;
        return true;
      }
    } else {
      Serial.println(F("SPIFFS: форматирование не удалось"));
    }
  }
  return mounted;
}

}  // namespace fs_utils

#else  // !ARDUINO

namespace fs_utils {

// На хосте SPIFFS не используется, поэтому всегда возвращаем успех.
inline bool ensureSpiffsMounted(bool = true) { return true; }

}  // namespace fs_utils

#endif
