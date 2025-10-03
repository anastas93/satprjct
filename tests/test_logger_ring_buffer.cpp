#include <cassert>
#include <string>
#include <vector>

#include "../src/logger.h"
#include "../src/default_settings.h"
#include "../src/libs/log_hook/log_hook.h"

int main() {
  // Проверяем обрезку длинных сообщений
  Logger::init();
  LogHook::clear();
  const std::string longMessage(Logger::payloadLimit() + 160, 'A');
  bool accepted = Logger::enqueue(DefaultSettings::LogLevel::INFO, longMessage);
  assert(accepted);
  auto stats = Logger::consumeStats();
  assert(stats.truncated_records == 1);
  assert(stats.truncated_bytes == longMessage.size() - Logger::payloadLimit());
  assert(stats.dropped_records == 0);
  auto recent = LogHook::getRecent(1);
  assert(!recent.empty());
  assert(recent[0].text.find("…") != std::string::npos);

  // Проверяем счётчик потерь при переполнении кольца
  Logger::init();
  LogHook::clear();
  const size_t totalMessages = Logger::queueCapacity() + 72;
  for (size_t i = 0; i < totalMessages; ++i) {
    Logger::enqueue(DefaultSettings::LogLevel::INFO, "MSG " + std::to_string(i));
  }
  auto overflowStats = Logger::consumeStats();
  assert(overflowStats.dropped_records == totalMessages - Logger::queueCapacity());
  assert(overflowStats.truncated_records == 0);
  assert(overflowStats.truncated_bytes == 0);

  return 0;
}
