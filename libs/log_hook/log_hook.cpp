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

#endif // ARDUINO

