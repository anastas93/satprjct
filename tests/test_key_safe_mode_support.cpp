#include "libs/log_hook/log_hook.h"
#include "libs/simple_logger/simple_logger.h"
#include <mutex>
#include <utility>

// Простейшие заглушки для LogHook и SimpleLogger в рамках хостового теста safe mode.

namespace {
std::mutex& logMutex() {
  static std::mutex m;
  return m;
}

std::vector<LogHook::Entry>& logBuffer() {
  static std::vector<LogHook::Entry> buffer;
  return buffer;
}

LogHook::Dispatcher& dispatcherSlot() {
  static LogHook::Dispatcher dispatcher;
  return dispatcher;
}

uint32_t& nextId() {
  static uint32_t counter = 1;
  return counter;
}
}

namespace LogHook {

void setDispatcher(Dispatcher cb) {
  std::lock_guard<std::mutex> lock(logMutex());
  dispatcherSlot() = std::move(cb);
}

void append(const LogString& line) {
  LogHook::Entry entry{};
  entry.id = nextId()++;
  entry.text = line;
  entry.uptime_ms = 0;
  {
    std::lock_guard<std::mutex> lock(logMutex());
    logBuffer().push_back(entry);
    if (dispatcherSlot()) {
      dispatcherSlot()(entry);
    }
  }
}

void append(const char* line) {
  append(LogString(line ? line : ""));
}

std::vector<Entry> getRecent(size_t count) {
  std::lock_guard<std::mutex> lock(logMutex());
  const auto& buffer = logBuffer();
  if (count > buffer.size()) {
    count = buffer.size();
  }
  return std::vector<Entry>(buffer.end() - count, buffer.end());
}

void clear() {
  std::lock_guard<std::mutex> lock(logMutex());
  logBuffer().clear();
  nextId() = 1;
}

size_t size() {
  std::lock_guard<std::mutex> lock(logMutex());
  return logBuffer().size();
}

} // namespace LogHook

namespace SimpleLogger {

void logStatus(const std::string& line) {
  LogHook::append(line);
}

std::vector<std::string> getLast(size_t count) {
  auto recent = LogHook::getRecent(count);
  std::vector<std::string> out;
  out.reserve(recent.size());
  for (const auto& entry : recent) {
    out.push_back(entry.text);
  }
  return out;
}

} // namespace SimpleLogger
