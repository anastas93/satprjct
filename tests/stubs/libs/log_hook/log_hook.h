#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LogHook {

using LogString = std::string;

struct Entry {
  uint32_t id = 0;
  LogString text;
  uint32_t uptime_ms = 0;
};

using Dispatcher = std::function<void(const Entry&)>;

void setDispatcher(Dispatcher cb);
void append(const LogString& line);
void append(const char* line);
std::vector<Entry> getRecent(size_t count);
void clear();
size_t size();

} // namespace LogHook
