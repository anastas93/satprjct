#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include "packet_splitter.h"

// Сборщик пакетов в исходное сообщение
class PacketGatherer {
public:
  // Конструктор с указанием режима и опциональным произвольным размером
  explicit PacketGatherer(PayloadMode mode = PayloadMode::SMALL, size_t custom = 0);
  // Сброс текущего состояния
  void reset();
  // Добавить часть сообщения
  void add(const uint8_t* data, size_t len);
  // Проверка завершения
  bool isComplete() const;
  // Получить собранное сообщение
  const std::vector<uint8_t>& get() const;
private:
  size_t payloadSize() const;
  PayloadMode mode_;
  size_t custom_ = 0; // произвольный размер, если задан
  bool complete_ = false;
  std::vector<uint8_t> data_;
};

