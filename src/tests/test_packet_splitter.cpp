#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include "message_buffer.h"
#include "packetizer/packet_splitter.h"

// Проверка разделения данных на пакеты
int main() {
  MessageBuffer buf(10);                 // вместимость буфера
  PacketSplitter splitter(PayloadMode::SMALL);
  std::vector<uint8_t> data(100);
  for (int i=0;i<100;++i) data[i]=i;     // заполняем тестовыми данными
  uint16_t first = splitter.splitAndEnqueue(buf, data.data(), data.size());
  assert(first==1);                       // первый ID
  size_t parts=0; uint16_t id; std::vector<uint8_t> out;
  while(buf.pop(id,out)) { parts++; }
  assert(parts==4);                       // 100 байт -> 4 части

  // Проверяем, что последовательные вызовы формируют разные теги
  {
    MessageBuffer tag_buf(5);
    PacketSplitter tag_splitter(PayloadMode::SMALL);
    std::vector<uint8_t> payload(10, 0xAA);
    std::set<std::string> seen;
    for (int i = 0; i < 3; ++i) {
      uint16_t first_id = tag_splitter.splitAndEnqueue(tag_buf, payload.data(), payload.size(), true);
      assert(first_id != 0);              // тег добавлен в буфер
      uint16_t part_id = 0;
      std::vector<uint8_t> part;
      bool ok = tag_buf.pop(part_id, part);
      assert(ok);
      auto closing = std::find(part.begin(), part.end(), ']');
      assert(closing != part.end());
      std::string prefix(part.begin(), closing + 1);
      auto pipe = std::find(prefix.begin(), prefix.end(), '|');
      assert(pipe != prefix.end());
      std::string tag(prefix.begin() + 1, pipe);
      assert(tag.size() == 8);            // теги сериализуются восемью символами
      bool inserted = seen.insert(tag).second;
      assert(inserted);                   // повторов быть не должно
    }
    assert(seen.size() == 3);
  }

  std::cout << "OK" << std::endl;
  return 0;
}
