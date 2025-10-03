#include "simple_logger.h"
#include <array>
#include <string_view>
#include "default_settings.h"               // глобальные настройки логирования
#include "logger.h"                         // асинхронная очередь логов

#ifdef ARDUINO
#  include <Arduino.h>
#endif

namespace {
// Максимальный размер кольцевого буфера
constexpr size_t MAX_LOG_RECORDS = 64;

// Запись журнала с сохранённым префиксом
struct LogEntry {
  std::string prefix;   // префикс до первого пробела
  std::string line;     // полная строка журнала
};

std::array<LogEntry, MAX_LOG_RECORDS> g_log{}; // кольцевой буфер записей
size_t g_head = 0;                             // индекс старейшей записи
size_t g_size = 0;                             // текущее количество строк

// Поиск записи по префиксу для замены
LogEntry* findByPrefix(std::string_view prefix) {
  for (size_t i = 0; i < g_size; ++i) {
    size_t idx = (g_head + i) % g_log.size();
    if (g_log[idx].prefix == prefix) {
      return &g_log[idx];
    }
  }
  return nullptr;
}
} // namespace

namespace SimpleLogger {
void logStatus(const std::string& line) {
  // Выделяем префикс до первого пробела
  auto pos = line.find(' ');
  std::string_view prefix_view = (pos == std::string::npos)
                                   ? std::string_view(line)
                                   : std::string_view(line.data(), pos);
  if (LogEntry* existing = findByPrefix(prefix_view)) {
    existing->line = line;                       // обновляем строку без перевыделения
    Logger::enqueue(DefaultSettings::LogLevel::INFO, line); // отправляем обновление в очередь логов
    return;
  }

  size_t index;
  if (g_size < g_log.size()) {
    index = (g_head + g_size) % g_log.size();    // используем свободный слот
    ++g_size;
  } else {
    index = g_head;                              // перезаписываем самую старую запись
    g_head = (g_head + 1) % g_log.size();
  }
  LogEntry& entry = g_log[index];
  entry.prefix.assign(prefix_view.data(), prefix_view.size());
  entry.line = line;
  Logger::enqueue(DefaultSettings::LogLevel::INFO, line);   // передаём запись в асинхронный лог
}

std::vector<std::string> getLast(size_t count) {
  if (count > g_size) count = g_size;
  std::vector<std::string> out;
  out.reserve(count);
  size_t start = g_size - count;
  for (size_t i = start; i < g_size; ++i) {
    size_t idx = (g_head + i) % g_log.size();
    out.push_back(g_log[idx].line);
  }
  return out;
}
} // namespace SimpleLogger
