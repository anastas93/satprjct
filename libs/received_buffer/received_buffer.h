#pragma once
#include <deque>
#include <vector>
#include <string>
#include <cstdint>

// Буфер принятых сообщений с разделением по стадиям обработки
class ReceivedBuffer {
public:
  // Элемент буфера
  struct Item {
    uint32_t id;                 // идентификатор сообщения
    uint32_t part;               // номер части для сырого пакета
    std::vector<uint8_t> data;   // содержимое пакета
  };

  // Добавить сырой пакет и получить имя вида R-000000|номер
  std::string pushRaw(uint32_t id, uint32_t part, const uint8_t* data, size_t len);
  // Добавить объединённые данные и получить имя SP-00000
  std::string pushSplit(uint32_t id, const uint8_t* data, size_t len);
  // Добавить готовые данные и получить имя GO-00000
  std::string pushReady(uint32_t id, const uint8_t* data, size_t len);

  // Извлечь следующий элемент из очереди
  bool popRaw(Item& out);   // сырой пакет
  bool popSplit(Item& out); // объединённые данные
  bool popReady(Item& out); // готовые данные

  // Проверка наличия элементов
  bool hasRaw() const;
  bool hasSplit() const;
  bool hasReady() const;

private:
  std::string makeRawName(uint32_t id, uint32_t part) const;   // генерация имени R-
  std::string makeSplitName(uint32_t id) const;                // генерация имени SP-
  std::string makeReadyName(uint32_t id) const;                // генерация имени GO-

  std::deque<Item> raw_;    // очередь сырых пакетов
  std::deque<Item> split_;  // очередь объединённых данных
  std::deque<Item> ready_;  // очередь готовых данных
};

