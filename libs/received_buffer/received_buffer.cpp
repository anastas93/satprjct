#include "received_buffer.h"
#include <cstdio>

// Формирование имени для сырого пакета
std::string ReceivedBuffer::makeRawName(uint32_t id, uint32_t part) const {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "R-%06u|%u", id, part);
  return buf;
}

// Формирование имени для объединённых данных
std::string ReceivedBuffer::makeSplitName(uint32_t id) const {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "SP-%05u", id);
  return buf;
}

// Формирование имени для готовых данных
std::string ReceivedBuffer::makeReadyName(uint32_t id) const {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "GO-%05u", id);
  return buf;
}

// Добавление сырого пакета
std::string ReceivedBuffer::pushRaw(uint32_t id, uint32_t part, const uint8_t* data, size_t len) {
  if (!data || !len) return {};
  raw_.push_back({id, part, std::vector<uint8_t>(data, data + len)});
  return makeRawName(id, part);
}

// Добавление объединённых данных
std::string ReceivedBuffer::pushSplit(uint32_t id, const uint8_t* data, size_t len) {
  if (!data || !len) return {};
  split_.push_back({id, 0, std::vector<uint8_t>(data, data + len)});
  return makeSplitName(id);
}

// Добавление готовых данных
std::string ReceivedBuffer::pushReady(uint32_t id, const uint8_t* data, size_t len) {
  if (!data || !len) return {};
  ready_.push_back({id, 0, std::vector<uint8_t>(data, data + len)});
  return makeReadyName(id);
}

// Извлечение сырого пакета
bool ReceivedBuffer::popRaw(Item& out) {
  if (raw_.empty()) return false;
  out = std::move(raw_.front());
  raw_.pop_front();
  return true;
}

// Извлечение объединённых данных
bool ReceivedBuffer::popSplit(Item& out) {
  if (split_.empty()) return false;
  out = std::move(split_.front());
  split_.pop_front();
  return true;
}

// Извлечение готовых данных
bool ReceivedBuffer::popReady(Item& out) {
  if (ready_.empty()) return false;
  out = std::move(ready_.front());
  ready_.pop_front();
  return true;
}

// Проверки наличия данных
bool ReceivedBuffer::hasRaw() const { return !raw_.empty(); }
bool ReceivedBuffer::hasSplit() const { return !split_.empty(); }
bool ReceivedBuffer::hasReady() const { return !ready_.empty(); }

// Сформировать список имён элементов (ограничение по count)
std::vector<std::string> ReceivedBuffer::list(size_t count) const {
  std::vector<std::string> out;                // результирующий список имён
  size_t produced = 0;                         // сколько имён добавлено
  for (const auto& it : raw_) {                // перебор сырых пакетов
    if (produced >= count) break;              // достигнут лимит
    out.push_back(makeRawName(it.id, it.part));
    ++produced;
  }
  for (const auto& it : split_) {              // перебор объединённых данных
    if (produced >= count) break;              // проверка лимита
    out.push_back(makeSplitName(it.id));
    ++produced;
  }
  for (const auto& it : ready_) {              // перебор готовых данных
    if (produced >= count) break;              // проверка лимита
    out.push_back(makeReadyName(it.id));
    ++produced;
  }
  return out;                                  // возвращаем список имён
}

