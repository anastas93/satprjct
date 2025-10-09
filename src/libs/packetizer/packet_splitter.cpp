#include "packet_splitter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
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
uint16_t PacketSplitter::splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len, bool with_id) const {
  if (!data || len == 0) {                                 // проверка входных данных
    DEBUG_LOG("PacketSplitter: пустой ввод");
    return 0;
  }

  const size_t chunk = payloadSize();                      // базовый размер полезной нагрузки
  if (chunk == 0) {
    DEBUG_LOG("PacketSplitter: нулевой размер блока");
    return 0;
  }

  const size_t parts = (len + chunk - 1) / chunk;          // количество необходимых частей
  if (parts == 0) {
    return 0;
  }
  if (buf.freeSlots() < parts) {                           // проверяем, хватит ли свободных слотов
    DEBUG_LOG("PacketSplitter: нет места в буфере");
    return 0;
  }
  DEBUG_LOG_VAL("PacketSplitter: частей=", parts);

  uint32_t tag_value = 0;
  std::array<char, 9> tag_text{};                          // HEX-представление тега + терминатор
  std::array<char, 48> prefix_buf{};                       // запас под префикс вида [TAG|N/M]

  if (with_id) {
    tag_value = nextTagValue();                            // фиксируем уникальный тег
    static constexpr char kHex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {                         // формируем строку из восьми шестнадцатеричных символов
      tag_text[static_cast<size_t>(i)] = kHex[tag_value & 0x0F];
      tag_value >>= 4;
    }
    tag_text.back() = '\0';
  }

  std::vector<uint8_t> scratch;                            // временный буфер для сборки части
  scratch.reserve(buf.slotSize());
  size_t offset = 0;
  size_t added = 0;                                        // число уже добавленных частей
  uint16_t first_id = 0;                                   // идентификатор первой добавленной записи
  size_t part_index = 0;

  auto rollback = [&]() {
    while (added > 0) {                                    // по одному удаляем ранее добавленные элементы
      buf.dropLast();
      --added;
    }
  };

  while (offset < len) {
    const size_t part = std::min(chunk, len - offset);     // реальный размер очередного фрагмента
    size_t prefix_len = 0;

    if (with_id) {
      ++part_index;
      int printed = 0;
      if (parts > 1) {
        printed = std::snprintf(prefix_buf.data(), prefix_buf.size(), "[%s|%zu/%zu]", tag_text.data(), part_index, parts);
      } else {
        printed = std::snprintf(prefix_buf.data(), prefix_buf.size(), "[%s|%zu]", tag_text.data(), part_index);
      }
      if (printed <= 0 || static_cast<size_t>(printed) >= prefix_buf.size()) {
        DEBUG_LOG("PacketSplitter: префикс не сформирован");
        rollback();
        return 0;
      }
      prefix_len = static_cast<size_t>(printed);
      if (prefix_len >= buf.slotSize()) {                  // проверяем, помещается ли префикс в слот
        DEBUG_LOG("PacketSplitter: префикс не помещается в слот");
        rollback();
        return 0;
      }
    }

    if (prefix_len + part > buf.slotSize()) {              // суммарный размер части с префиксом
      DEBUG_LOG("PacketSplitter: фрагмент превышает размер слота");
      rollback();
      return 0;
    }

    scratch.resize(prefix_len + part);
    if (prefix_len > 0) {                                  // копируем префикс в начало буфера
      std::memcpy(scratch.data(), prefix_buf.data(), prefix_len);
    }
    std::memcpy(scratch.data() + prefix_len, data + offset, part);

    uint16_t id = buf.enqueue(scratch.data(), scratch.size());
    if (id == 0) {                                         // ошибка постановки в очередь
      if (with_id) {
        std::string log_line(prefix_buf.data(), prefix_len);
        log_line += " ERR";
        SimpleLogger::logStatus(log_line);
      }
      DEBUG_LOG("PacketSplitter: ошибка добавления, откат");
      rollback();                                          // возвращаем очередь к исходному состоянию
      return 0;
    }

    if (with_id) {                                         // фиксируем успех в журнале
      std::string log_line(prefix_buf.data(), prefix_len);
      log_line += " PROG";
      SimpleLogger::logStatus(log_line);
    }

    if (first_id == 0) first_id = id;
    offset += part;
    ++added;
  }

  DEBUG_LOG_VAL("PacketSplitter: первый id=", first_id);
  return first_id;
}

