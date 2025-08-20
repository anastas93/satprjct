#include "qos_moderator.h"
#include "message_buffer.h"

// Использует глобальный буфер сообщений для оценки очереди
extern MessageBuffer g_buf;

bool Qos_allow(Qos q) {
  // Низкий приоритет блокируется при наличии сообщений высокого или нормального уровня
  if (q == Qos::Low && (g_buf.lenH() > 0 || g_buf.lenN() > 0)) return false;
  // Нормальный приоритет блокируется при наличии сообщений высокого уровня
  if (q == Qos::Normal && g_buf.lenH() > 0) return false;
  return true; // Высокий приоритет всегда проходит
}
