#pragma once
#include "qos.h"
#include "../message_cache.h"

// Проверяет, можно ли отправлять данные указанного приоритета.
// Реализация перенесена в заголовок, чтобы Arduino автоматически
// подключал её без отдельного исходника.
inline bool Qos_allow(Qos q) {
  extern MessageCache g_cache;  // Глобальный буфер очередей
  // Блокируем низкий приоритет, если есть более важные кадры
  if (q == Qos::Low && (g_cache.lenH() > 0 || g_cache.lenN() > 0)) return false;
  // Блокируем нормальный приоритет при наличии высоких кадров
  if (q == Qos::Normal && g_cache.lenH() > 0) return false;
  return true;  // Высокий приоритет всегда разрешён
}
