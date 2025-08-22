#include <cassert>
#include <iostream>
#include <vector>
#include "message_buffer.h"
#include "packetizer/packet_splitter.h"

// Проверка разделения данных на пакеты
int main() {
  MessageBuffer buf(10);                 // вместимость буфера
  PacketSplitter splitter(PayloadMode::SMALL);
  std::vector<uint8_t> data(100);
  for (int i=0;i<100;++i) data[i]=i;     // заполняем тестовыми данными
  uint32_t first = splitter.splitAndEnqueue(buf, data.data(), data.size());
  assert(first==1);                       // первый ID
  size_t parts=0; uint32_t id; std::vector<uint8_t> out;
  while(buf.pop(id,out)) { parts++; }
  assert(parts==4);                       // 100 байт -> 4 части
  std::cout << "OK" << std::endl;
  return 0;
}
