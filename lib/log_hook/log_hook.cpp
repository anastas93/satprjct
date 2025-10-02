#include "log_hook.h"

#ifdef ARDUINO

#include <deque>

namespace LogHook {
  // Максимальная ёмкость буфера журналирования
  static constexpr size_t kMaxEntries = 200;

  namespace {
    std::deque<Entry> gBuffer;            // кольцевой буфер записей
    Dispatcher gDispatcher = nullptr;     // текущий обработчик push-уведомлений
    uint32_t gNextId = 1;                 // последовательный идентификатор
  } // namespace

  // Приватная вспомогательная функция добавления записи
  static void pushEntry(const LogString& text) {
    Entry entry;
    entry.id = gNextId++;
    entry.text = text;
    entry.uptime_ms = millis();
    gBuffer.push_back(entry);
    while (gBuffer.size() > kMaxEntries) {
      gBuffer.pop_front();
    }
    if (gDispatcher) {
      gDispatcher(entry);
    }
  }

  void setDispatcher(Dispatcher cb) {
    gDispatcher = cb;
  }

  void append(const LogString& line) {
    pushEntry(line);
  }

  void append(const char* line) {
    if (!line) {
      pushEntry(String());
      return;
    }
    pushEntry(String(line));
  }

  std::vector<Entry> getRecent(size_t count) {
    std::vector<Entry> out;
    if (count == 0 || gBuffer.empty()) {
      return out;
    }
    const size_t available = gBuffer.size();
    const size_t start = (count >= available) ? 0 : (available - count);
    out.reserve(available - start);
    for (size_t i = start; i < available; ++i) {
      out.push_back(gBuffer[i]);
    }
    return out;
  }

  void clear() {
    gBuffer.clear();
    gNextId = 1;
  }

  size_t size() {
    return gBuffer.size();
}

} // namespace LogHook

#else  // !ARDUINO

#include <deque>
#include <mutex>
#include <chrono>

namespace {
  // Максимальная ёмкость буфера журналирования для хостовой сборки
  constexpr size_t kMaxEntries = 200;

  std::deque<LogHook::Entry> gBuffer;       // кольцевой буфер последних строк
  LogHook::Dispatcher gDispatcher = nullptr; // обработчик push-уведомлений
  std::mutex gMutex;                         // защита общей памяти
  uint32_t gNextId = 1;                      // последовательный идентификатор записей
  const auto gStart = std::chrono::steady_clock::now(); // отметка старта процесса

  // Расчёт значения uptime в миллисекундах от старта программы
  uint32_t uptimeMs() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto diff = duration_cast<milliseconds>(now - gStart);
    return static_cast<uint32_t>(diff.count());
  }

  // Добавление строки в буфер с учётом ограничений по размеру
  void pushEntry(const LogHook::LogString& text) {
    LogHook::Entry entry{};
    entry.id = gNextId++;
    entry.text = text;
    entry.uptime_ms = uptimeMs();

    LogHook::Dispatcher dispatcherCopy;
    {
      std::lock_guard<std::mutex> lock(gMutex);
      gBuffer.push_back(entry);
      while (gBuffer.size() > kMaxEntries) {
        gBuffer.pop_front();
      }
      dispatcherCopy = gDispatcher;
    }

    if (dispatcherCopy) {
      dispatcherCopy(entry); // уведомляем подписчика вне блокировки
    }
  }
} // namespace

namespace LogHook {

void setDispatcher(Dispatcher cb) {
  std::lock_guard<std::mutex> lock(gMutex);
  gDispatcher = std::move(cb);
}

void append(const LogString& line) {
  pushEntry(line);
}

void append(const char* line) {
  if (!line) {
    pushEntry(LogString());
  } else {
    pushEntry(LogString(line));
  }
}

std::vector<Entry> getRecent(size_t count) {
  std::vector<Entry> out;
  std::lock_guard<std::mutex> lock(gMutex);
  if (count == 0 || gBuffer.empty()) {
    return out;
  }
  if (count > gBuffer.size()) {
    count = gBuffer.size();
  }
  out.reserve(count);
  const size_t start = gBuffer.size() - count;
  for (size_t i = start; i < gBuffer.size(); ++i) {
    out.push_back(gBuffer[i]);
  }
  return out;
}

void clear() {
  std::lock_guard<std::mutex> lock(gMutex);
  gBuffer.clear();
  gNextId = 1;
}

size_t size() {
  std::lock_guard<std::mutex> lock(gMutex);
  return gBuffer.size();
}

} // namespace LogHook

#endif // ARDUINO

