#include <cstdio>

// Простейшая имитация обмена KEYCHG/KEYACK для тестового скрипта
int main() {
  int kid = 2;
  printf("KEYCHG %d\n", kid);            // рассылка команды смены ключа
  printf("Ожидаем KEYACK %d\n", kid);    // ожидание подтверждения
  printf("KEYACK %d\n", kid);           // ответ от удалённой стороны
  printf("ACTIVATE %d\n", kid);         // активация ключа после подтверждения
  printf("KEY ROTATION OK\n");
  return 0;
}
