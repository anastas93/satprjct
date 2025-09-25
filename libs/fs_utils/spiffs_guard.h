#pragma once

// Вспомогательный модуль для безопасного монтирования SPIFFS.
// Все комментарии намеренно оставлены на русском языке.

#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

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
    if (SPIFFS.format()) {
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
