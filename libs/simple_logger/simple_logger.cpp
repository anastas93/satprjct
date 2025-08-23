#include "simple_logger.h"

// Статический буфер журнала
static std::vector<std::string> g_log;

namespace SimpleLogger {
void logStatus(const std::string& line) {
  g_log.push_back(line);
}

std::vector<std::string> getLast(size_t count) {
  std::vector<std::string> out;
  if (count > g_log.size()) count = g_log.size();
  out.assign(g_log.end() - count, g_log.end());
  return out;
}
} // namespace SimpleLogger
