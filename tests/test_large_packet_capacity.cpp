#include <cassert>
#include <iostream>
#include <vector>
#include "default_settings.h"
#include "message_buffer.h"
#include "packetizer/packet_splitter.h"

// Проверка, что буфер размещает несколько сообщений по 5000 байт
int main() {
  MessageBuffer buf(DefaultSettings::TX_QUEUE_CAPACITY); // увеличенный буфер
  PacketSplitter splitter(PayloadMode::LARGE);          // крупные пакеты ~128 байт
  std::vector<uint8_t> data(5000, 0xAA);                // тестовые данные
  const size_t parts = (5000 + 127) / 128;              // частей в одном сообщении
  for (int i = 0; i < 3; ++i) {
    uint32_t id = splitter.splitAndEnqueue(buf, data.data(), data.size());
    assert(id != 0);                                   // каждое сообщение добавлено
  }
  // после трёх сообщений должно остаться место ещё под одно
  assert(buf.freeSlots() == DefaultSettings::TX_QUEUE_CAPACITY - 3 * parts);
  std::cout << "OK" << std::endl;
  return 0;
}
