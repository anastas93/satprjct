#include "simple_logger.h"

// Статический буфер журнала
static std::vector<std::string> g_log;

namespace SimpleLogger {
void logStatus(const std::string& line) {
  // Выделяем префикс до первого пробела
  auto pos = line.find(' ');
  std::string prefix = (pos == std::string::npos) ? line : line.substr(0, pos);
  // Ищем существующую запись с таким же префиксом и заменяем её
  for (auto &entry : g_log) {
    auto epos = entry.find(' ');
    std::string eprefix = (epos == std::string::npos) ? entry : entry.substr(0, epos);
    if (eprefix == prefix) {
      entry = line;
      return;
    }
  }
  // Если не найдено, добавляем новую строку
  g_log.push_back(line);
}

std::vector<std::string> getLast(size_t count) {
  std::vector<std::string> out;
  if (count > g_log.size()) count = g_log.size();
  out.assign(g_log.end() - count, g_log.end());
  return out;
}
} // namespace SimpleLogger
