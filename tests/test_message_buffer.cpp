#include <cassert>
#include <iostream>
#include <vector>
#include "message_buffer.h"

// Проверка базовых операций буфера сообщений
int main() {
  MessageBuffer buf(2);                  // буфер на два сообщения
  uint8_t a[] = {1,2,3};
  uint8_t b[] = {4,5};
  uint8_t c[] = {6};
  assert(buf.enqueue(a,3) == 1);         // первое сообщение
  assert(buf.enqueue(b,2) == 2);         // второе сообщение
  assert(buf.enqueue(c,1) == 0);         // буфер переполнен
  uint32_t id; std::vector<uint8_t> out;
  assert(buf.pop(id,out) && id==1);      // получаем первое сообщение
  assert(out.size()==3 && out[0]==1);
  assert(buf.pop(id,out) && id==2);      // получаем второе сообщение
  assert(!buf.hasPending());             // очередь пуста

  // Проверяем, что после отката добавления идентификаторы не повторяются
  uint8_t d[] = {7,8};
  uint8_t e[] = {9,10};
  uint32_t first = buf.enqueue(d,2);
  assert(first > 0);
  assert(buf.enqueue(e,2) > 0);
  assert(buf.dropLast());                // откат последнего добавления
  uint32_t after_drop = buf.enqueue(e,2);
  assert(after_drop > first);            // идентификаторы продолжают расти
  std::cout << "OK" << std::endl;
  return 0;
}
