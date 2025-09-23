#include "received_buffer.h"
#include <cstdio>
#include <algorithm>

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
  raw_.emplace_back();
  auto& item = raw_.back();
  item.id = id;
  item.part = part;
  item.data.assign(data, data + len);
  item.name = makeRawName(id, part);
  return item.name;
}

// Добавление объединённых данных
std::string ReceivedBuffer::pushSplit(uint32_t id, const uint8_t* data, size_t len) {
  if (!data || !len) return {};
  split_.emplace_back();
  auto& item = split_.back();
  item.id = id;
  item.part = 0;
  item.data.assign(data, data + len);
  item.name = makeSplitName(id);
  return item.name;
}

// Добавление готовых данных
std::string ReceivedBuffer::pushReady(uint32_t id, const uint8_t* data, size_t len) {
  if (!data || !len) return {};
  ready_.emplace_back();
  auto& item = ready_.back();
  item.id = id;
  item.part = 0;
  item.data.assign(data, data + len);
  item.name = makeReadyName(id);
  return item.name;
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
  if (count == 0) return out;                  // сразу выходим при нулевом запросе
  const size_t total = std::min(count, raw_.size() + split_.size() + ready_.size());
  out.reserve(total);                          // резервируем место под итоговый объём
  size_t produced = 0;                         // сколько имён добавлено
  auto append = [&](const std::deque<Item>& queue) {
    for (const auto& it : queue) {             // перебор элементов очереди
      if (produced >= count) break;            // достигнут лимит выдачи
      out.push_back(it.name);                  // используем готовое имя без форматирования
      ++produced;
    }
  };
  append(ready_);
  append(split_);
  append(raw_);
  return out;                                  // возвращаем список имён
}

// Сформировать копию элементов с типом
std::vector<ReceivedBuffer::SnapshotEntry> ReceivedBuffer::snapshot(size_t count) const {
  std::vector<SnapshotEntry> out;
  if (count == 0) return out;                                  // пустой запрос
  const size_t total = std::min(count, raw_.size() + split_.size() + ready_.size());
  out.reserve(total);
  size_t produced = 0;
  auto append = [&](const std::deque<Item>& queue, Kind kind) {
    for (const auto& it : queue) {
      if (produced >= count) break;                             // достигнут лимит
      SnapshotEntry entry;
      entry.item = it;                                          // копируем данные
      entry.kind = kind;
      out.push_back(std::move(entry));
      ++produced;
    }
  };
  append(ready_, Kind::Ready);
  append(split_, Kind::Split);
  append(raw_, Kind::Raw);
  return out;
}

