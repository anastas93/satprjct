#ifndef ARDUINO
// Тест компилирует встроенный HTML и проверяет, что строка начинается с символа '<'.
#define PROGMEM
#include "web_interface.h"
int main() {
  return (WEB_INTERFACE_HTML[0] == '<') ? 0 : 1;
}
#endif // ARDUINO
