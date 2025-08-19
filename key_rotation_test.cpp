#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "crypto_spec.h"

// Тест ротации ключа с подтверждением
int main() {
  std::srand((unsigned)std::time(nullptr));
  uint8_t newKey[16];
  for (int i = 0; i < 16; ++i) newKey[i] = std::rand() & 0xFF;
  uint8_t kid = 2;
  printf("Генерация ключа для KID=%u\n", kid);
  if (crypto_spec::rotateKeyWithAck(newKey, kid, 3)) {
    printf("KEY ROTATION OK\n");
    return 0;
  }
  printf("KEY ROTATION FAIL\n");
  return 1;
}
