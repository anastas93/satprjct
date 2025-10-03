#pragma once
// Подсистема асинхронного журналирования с кольцевым буфером
// Все комментарии на русском в соответствии с пользовательскими требованиями

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include "default_settings.h"

namespace Logger {

// Тип записи в очереди журнала
enum class RecordType : uint8_t {
  Text,   // обычное текстовое сообщение
  Csv     // запись с результатами измерений в формате CSV
};

// Статистика по потерям и обрезкам
struct Stats {
  uint32_t dropped_records = 0;      // количество потерянных записей при переполнении
  uint32_t truncated_records = 0;    // количество обрезанных сообщений
  uint32_t truncated_bytes = 0;      // количество усечённых байтов
};

// Инициализация подсистемы; должна быть вызвана до старта задач
void init();

// Добавление готовой строки в очередь (не блокирует поток выполнения)
bool enqueue(DefaultSettings::LogLevel level, const std::string& line);

// Добавление строки в ISR-контексте
bool enqueueFromISR(DefaultSettings::LogLevel level, const std::string& line);

// Форматирование по printf-подобному шаблону и помещение в очередь
bool enqueuef(DefaultSettings::LogLevel level, const char* fmt, ...);

// Вариант для ISR
bool enqueuefFromISR(DefaultSettings::LogLevel level, const char* fmt, ...);

// Обёртка для LOG_*_VAL макросов
bool enqueueValue(DefaultSettings::LogLevel level, const char* prefix, long long value);

// Задача вывода журнала (должна быть запущена в FreeRTOS)
void task(void*);

// Возвращает текущую статистику (копию) и обнуляет счётчики
Stats consumeStats();

// Формирование бинарного фрейма с CRC16 для передачи по Serial
std::string makeFrame(RecordType type,
                      DefaultSettings::LogLevel level,
                      uint32_t timestamp_us,
                      const std::string& payload);

// Валидация и разбор фрейма, используется в тестах
bool parseFrame(const std::string& frame,
                RecordType& type,
                DefaultSettings::LogLevel& level,
                uint32_t& timestamp_us,
                std::string& payload);

// Получение отметки времени (микросекунды с момента старта)
uint32_t nowMicros();

// Добавление CSV-записи для LOG_WRAP
void enqueueWrapSample(const char* tag,
                       uint32_t start_us,
                       uint32_t end_us);

// Предоставление параметров буфера для тестов
size_t queueCapacity();
size_t payloadLimit();

} // namespace Logger

// Универсальная обёртка для измерения времени выполнения выражений
namespace LoggerDetail {

template <typename F>
auto wrap(const char* tag, F&& func) -> decltype(func()) {
  const uint32_t start = Logger::nowMicros();
  if constexpr (std::is_void_v<decltype(func())>) {
    func();
    const uint32_t end = Logger::nowMicros();
    Logger::enqueueWrapSample(tag, start, end);
  } else {
    auto result = func();
    const uint32_t end = Logger::nowMicros();
    Logger::enqueueWrapSample(tag, start, end);
    return result;
  }
}

} // namespace LoggerDetail

#define LOG_WRAP(tag, expr) \
  LoggerDetail::wrap(tag, [&]() -> decltype(expr) { return (expr); })

