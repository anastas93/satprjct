#pragma once

// Вспомогательный модуль для безопасного монтирования SPIFFS.
// Все комментарии намеренно оставлены на русском языке.

#include <cstdint>
#include "default_settings.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#if defined(ESP32) && __has_include("freertos/FreeRTOS.h") && __has_include("freertos/task.h")
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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

  LOG_ERROR("SPIFFS: первичное монтирование не удалось");
  if (!allowFormat) {
    result.status = SpiffsMountStatus::kMountFailed;
    return result;
  }

  bool shouldRetryFormat = !formatAttempted;
  if (shouldRetryFormat) {
    formatAttempted = true;
    LOG_WARN("SPIFFS: требуется форматирование, пробуем очистить раздел");
    bool formatted = false;
    bool formatPerformed = false;
#if FS_UTILS_HAS_ESP_IPC
    // Форматируем раздел в отдельной задаче на ядре PRO с увеличенным стеком, чтобы избежать переполнения стека ipc0.
    auto formatOnCpu0 = [&]() -> bool {
      struct FormatTaskContext {
        TaskHandle_t waiter = nullptr;
        bool formatted = false;
      } ctx;
      ctx.waiter = xTaskGetCurrentTaskHandle();
      constexpr uint32_t kStackSizeWords = 4096;  // 16 КБ стека для задачи форматирования
      constexpr UBaseType_t kPriority = tskIDLE_PRIORITY + 1;
      TaskHandle_t formatTaskHandle = nullptr;
      BaseType_t createResult = xTaskCreatePinnedToCore(
          [](void* arg) {
            auto* context = static_cast<FormatTaskContext*>(arg);
            context->formatted = SPIFFS.format();
            xTaskNotifyGive(context->waiter);
            vTaskDelete(nullptr);
          },
          "spiffs_fmt", kStackSizeWords, &ctx, kPriority, &formatTaskHandle, 0);
      if (createResult != pdPASS) {
        LOG_ERROR("SPIFFS: не удалось создать задачу форматирования, пробуем прямой вызов");
        return false;
      }
      uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(60000));
      if (notified == 0) {
        LOG_ERROR("SPIFFS: форматирование не завершилось за отведённое время");
        vTaskDelete(formatTaskHandle);
        return false;
      }
      formatted = ctx.formatted;
      return true;
    };

    formatPerformed = formatOnCpu0();
    if (!formatPerformed) {
      LOG_WARN("SPIFFS: выполняем форматирование напрямую (без IPC)");
      formatted = SPIFFS.format();
      formatPerformed = true;
    }
#else
    formatted = SPIFFS.format();
    formatPerformed = true;
#endif

    if (formatted) {
      if (SPIFFS.begin(false)) {
        LOG_INFO("SPIFFS: раздел успешно отформатирован");
        mounted = true;
        result.mounted = true;
        result.status = SpiffsMountStatus::kSuccess;
        return result;
      }
      LOG_ERROR("SPIFFS: монтирование после форматирования не удалось");
      result.status = SpiffsMountStatus::kPostFormatMountFailed;
      formatAttempted = false;  // разрешаем повторное форматирование при следующем вызове
    } else {
      LOG_ERROR("SPIFFS: форматирование не удалось");
      result.status = SpiffsMountStatus::kFormatFailed;
      formatAttempted = false;  // даём шанс повторить попытку форматирования
    }
  }

  LOG_WARN("SPIFFS: пробуем монтировать с автоформатированием (begin(true))");
  if (SPIFFS.begin(true)) {
    LOG_INFO("SPIFFS: begin(true) успешно смонтировал раздел");
    mounted = true;
    formatAttempted = true;
    result.mounted = true;
    result.status = SpiffsMountStatus::kSuccess;
    return result;
  }

  LOG_ERROR("SPIFFS: begin(true) тоже не смог смонтировать раздел");
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

