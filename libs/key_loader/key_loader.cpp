#include "key_loader.h"
#include <fstream>

namespace KeyLoader {
// Имя файла с ключом
static const char* KEY_FILE = "key_storage/key.bin";

std::array<uint8_t,16> loadKey() {
  std::array<uint8_t,16> key{};                   // буфер для ключа
  std::ifstream f(KEY_FILE, std::ios::binary);    // открываем файл
  if (!f.read(reinterpret_cast<char*>(key.data()), key.size())) {
    return DefaultSettings::DEFAULT_KEY;          // при ошибке берём дефолтный ключ
  }
  return key;                                    // возвращаем считанный ключ
}
} // namespace KeyLoader
