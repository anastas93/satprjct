#pragma once
#include <vector>
#include <string>
#include <cstddef>

// Простейший журнал статусов сообщений
namespace SimpleLogger {
// Добавляет строку в журнал
void logStatus(const std::string& line);
// Возвращает последние count строк журнала
std::vector<std::string> getLast(size_t count);
}
