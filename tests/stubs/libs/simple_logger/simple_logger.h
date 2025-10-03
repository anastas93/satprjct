#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "libs/log_hook/log_hook.h"

namespace SimpleLogger {

void logStatus(const std::string& line);
std::vector<std::string> getLast(size_t count);

} // namespace SimpleLogger
