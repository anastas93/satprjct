#include "key_loader.h"
#ifndef ARDUINO
#include <fstream>
#else
#include <nvs_flash.h>
#include <nvs.h>
#endif

namespace KeyLoader {
#ifndef ARDUINO
// Имя файла с ключом на ПК
static const char* KEY_FILE = "key_storage/key.bin";

// Чтение ключа из файла или возврат значения по умолчанию
std::array<uint8_t,16> loadKey() {
  std::array<uint8_t,16> key{};                   // буфер для ключа
  std::ifstream f(KEY_FILE, std::ios::binary);    // открываем файл
  if (!f.read(reinterpret_cast<char*>(key.data()), key.size())) {
    return DefaultSettings::DEFAULT_KEY;          // при ошибке берём дефолтный ключ
  }
  return key;                                    // возвращаем считанный ключ
}

// Сохранение ключа в файл
bool saveKey(const std::array<uint8_t,16>& key) {
  std::ofstream f(KEY_FILE, std::ios::binary);    // открываем файл на запись
  if (!f.write(reinterpret_cast<const char*>(key.data()), key.size())) {
    return false;                                 // ошибка записи
  }
  return true;                                    // запись успешна
}
#else
// Пространство имён и имя ключа в NVS
static const char* NVS_NAMESPACE = "keystore";   // пространство в NVS
static const char* NVS_KEY = "aes_key";          // имя ключа

// Чтение ключа из NVS ESP32
std::array<uint8_t,16> loadKey() {
  std::array<uint8_t,16> key{};                   // буфер для ключа
  if (nvs_flash_init() != ESP_OK) {
    return DefaultSettings::DEFAULT_KEY;          // инициализация NVS не удалась
  }
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
    return DefaultSettings::DEFAULT_KEY;          // нет сохранённого ключа
  }
  size_t size = key.size();                       // требуемый размер
  esp_err_t err = nvs_get_blob(h, NVS_KEY, key.data(), &size);
  nvs_close(h);
  if (err != ESP_OK || size != key.size()) {
    return DefaultSettings::DEFAULT_KEY;          // возвращаем дефолт при ошибке
  }
  return key;                                     // ключ успешно прочитан
}

// Запись ключа в NVS ESP32
bool saveKey(const std::array<uint8_t,16>& key) {
  if (nvs_flash_init() != ESP_OK) {
    return false;                                 // не удалось инициализировать NVS
  }
  nvs_handle_t h;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
    return false;                                 // не удалось открыть пространство
  }
  esp_err_t err = nvs_set_blob(h, NVS_KEY, key.data(), key.size());
  if (err == ESP_OK) {
    err = nvs_commit(h);                          // фиксируем изменения
  }
  nvs_close(h);
  return err == ESP_OK;                           // успех, если нет ошибки
}
#endif
} // namespace KeyLoader
