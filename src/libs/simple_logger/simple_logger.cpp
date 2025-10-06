#include "simple_logger.h"
#include <array>
#include "default_settings.h"               // глобальные настройки логирования
#include "logger.h"                         // асинхронная очередь логов

#ifdef ARDUINO
#  include <Arduino.h>
#endif

namespace {
// Максимальный размер кольцевого буфера
constexpr size_t MAX_LOG_RECORDS = 64;

// Кольцевой буфер строк статуса
std::array<std::string, MAX_LOG_RECORDS> g_log{}; // локальное хранилище сообщений
size_t g_head = 0;                                // индекс старейшей записи
size_t g_size = 0;                                // текущее количество строк

// Резервирование следующей позиции в буфере
size_t reserveSlot() {
  if (g_size < g_log.size()) {
    size_t idx = (g_head + g_size) % g_log.size();
    ++g_size;
    return idx;
  }
  size_t idx = g_head;
  g_head = (g_head + 1) % g_log.size();
  return idx;
}

// Единая точка отправки сообщений в системный debug-лог
void dispatchToDebug(const std::string& line) {
  // Все статусы идут через Logger::enqueue, чтобы избежать наложения вывода
  Logger::enqueue(DefaultSettings::LogLevel::INFO, line);
}
} // namespace

namespace SimpleLogger {
void logStatus(const std::string& line) {
  const size_t slot = reserveSlot();             // резервируем позицию в кольцевом буфере
  g_log[slot] = line;                            // сохраняем исходную строку
  dispatchToDebug(line);                         // единый вывод в debug без дублирования
}

std::vector<std::string> getLast(size_t count) {
  if (count > g_size) count = g_size;
  std::vector<std::string> out;
  out.reserve(count);
  size_t start = g_size - count;
  for (size_t i = start; i < g_size; ++i) {
    size_t idx = (g_head + i) % g_log.size();
    out.push_back(g_log[idx]);
  }
  return out;
}
} // namespace SimpleLogger
