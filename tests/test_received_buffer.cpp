#include <cassert>
#include <iostream>
#include "libs/received_buffer/received_buffer.h"

// Проверка буфера принятых сообщений
int main() {
  ReceivedBuffer buf;
  uint8_t a[] = {1,2};
  uint8_t b[] = {3,4,5};
  uint8_t c[] = {6};
  std::string n1 = buf.pushRaw(1, 2, a, 2);
  std::string n2 = buf.pushSplit(7, b, 3);
  std::string n3 = buf.pushReady(5, c, 1);
  assert(n1 == "R-000001|2");
  assert(n2 == "SP-00007");
  assert(n3 == "GO-00005");
  auto names = buf.list(10);                      // проверяем формирование списка имён
  assert(names.size() == 3);                      // ожидаем три элемента
  assert(names[0] == n3 && names[1] == n2 && names[2] == n1);
  auto latest = buf.list(2);                      // проверяем, что берутся самые свежие элементы
  assert(latest.size() == 2);
  assert(latest[0] == n3 && latest[1] == n2);
  auto snap = buf.snapshot(10);                   // проверяем порядок выдачи в снимке
  assert(snap.size() == 3);
  assert(snap[0].item.name == n3 && snap[0].kind == ReceivedBuffer::Kind::Ready);
  assert(snap[1].item.name == n2 && snap[1].kind == ReceivedBuffer::Kind::Split);
  assert(snap[2].item.name == n1 && snap[2].kind == ReceivedBuffer::Kind::Raw);
  auto snapLatest = buf.snapshot(1);              // проверяем ограничение по количеству
  assert(snapLatest.size() == 1);
  assert(snapLatest[0].item.name == n3);
  ReceivedBuffer::Item out;
  assert(buf.popRaw(out) && out.id == 1 && out.part == 2 && out.data.size() == 2 && out.name == n1);
  assert(buf.popSplit(out) && out.id == 7 && out.data.size() == 3 && out.name == n2);
  assert(buf.popReady(out) && out.id == 5 && out.data.size() == 1 && out.name == n3);
  std::cout << "OK" << std::endl;
  return 0;
}

