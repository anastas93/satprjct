#include "packet_splitter.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <random>
#include <string>
#include "default_settings.h"
#include "libs/simple_logger/simple_logger.h" // журнал статусов

namespace {

// Монотонный генератор идентификаторов для префиксов [TAG|...]
class TagGenerator {
public:
  // Получить следующее значение счётчика
  uint32_t next() {
    // Увеличиваем счётчик и избегаем нулевого значения
    uint32_t value = counter_.fetch_add(1, std::memory_order_relaxed);
    if (value == 0) value = counter_.fetch_add(1, std::memory_order_relaxed);
    return value;
  }

  // Доступ к единственному экземпляру
  static TagGenerator& instance() {
    static TagGenerator generator;
    return generator;
  }

private:
  // Инициализация счётчика случайным стартовым смещением
  TagGenerator() : counter_(initialSeed()) {}

  // Сбор энтропии для стартового значения
  static uint32_t initialSeed() {
    std::random_device rd;
    uint32_t hi = rd();
    uint32_t lo = rd();
    uint32_t seed = (hi << 16) ^ lo;
    return seed ? seed : 1u;  // избегаем нулевого запуска
  }

  std::atomic<uint32_t> counter_;
};

// Удобная обёртка для получения свежего тега
uint32_t nextTagValue() {
  return TagGenerator::instance().next();
}

}  // namespace

// Конструктор
PacketSplitter::PacketSplitter(PayloadMode mode, size_t custom)
    : mode_(mode), custom_(custom) {}

// Смена режима
void PacketSplitter::setMode(PayloadMode mode) { mode_ = mode; }

// Установка произвольного размера
void PacketSplitter::setCustomSize(size_t custom) { custom_ = custom; }

// Размер полезной нагрузки
size_t PacketSplitter::payloadSize() const {
  if (custom_) return custom_;
  switch (mode_) {
    case PayloadMode::SMALL:  return 32;
    case PayloadMode::MEDIUM: return 64;
    case PayloadMode::LARGE:  return 128;
  }
  return 32;
}

// Разделение данных и помещение в буфер
uint32_t PacketSplitter::splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len, bool with_id) const {
  if (!data || len == 0) {                              // проверка входных данных
    DEBUG_LOG("PacketSplitter: пустой ввод");
    return 0;
  }
  size_t chunk = payloadSize();
  size_t parts = (len + chunk - 1) / chunk;             // сколько частей потребуется
  if (buf.freeSlots() < parts) {                        // недостаточно места в буфере
    DEBUG_LOG("PacketSplitter: нет места в буфере");
    return 0;
  }
  DEBUG_LOG_VAL("PacketSplitter: частей=", parts);
  size_t offset = 0;
  uint32_t first_id = 0;
  size_t added = 0;                                     // количество успешно добавленных частей
  uint32_t tag = 0;
  std::string base;
  if (with_id) {
    tag = nextTagValue();                              // новый уникальный ID
    char hex[9];
    std::snprintf(hex, sizeof(hex), "%08X", tag);
    base = hex;
  }
  size_t part_idx = 1;
  while (offset < len) {
    size_t part = std::min(chunk, len - offset);
    std::string prefix;
    std::vector<uint8_t> tmp;
    if (with_id) {
      prefix = "[" + base + "|" + std::to_string(part_idx);
      if (parts > 1) {
        prefix += "/";
        prefix += std::to_string(parts);
      }
      prefix += "]";
      tmp.assign(prefix.begin(), prefix.end());
    }
    tmp.insert(tmp.end(), data + offset, data + offset + part);
    uint32_t id = buf.enqueue(tmp.data(), tmp.size());
    if (id == 0) {                                      // ошибка добавления
      if (with_id) SimpleLogger::logStatus(prefix + " ERR");
      DEBUG_LOG("PacketSplitter: ошибка добавления, откат");
      while (added--) buf.dropLast();                   // откат всех ранее добавленных частей
      return 0;
    }
    if (with_id) SimpleLogger::logStatus(prefix + " PROG");
    if (first_id == 0) first_id = id;
    offset += part;
    ++added;
    ++part_idx;
  }
  DEBUG_LOG_VAL("PacketSplitter: первый id=", first_id);
  return first_id;
}

