#pragma once
// Вспомогательная функция дампа RX-пакетов в Serial с проверкой переполнения буфера
// Комментарии оставлены на русском согласно требованиям пользователя

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cstring>

#include "libs/log_hook/log_hook.h"

// Шаблонный параметр SerialLike позволяет использовать как реальный HardwareSerial,
// так и заглушки в модульных тестах.
template <typename SerialLike>
bool dumpRxToSerialWithPrefix(SerialLike& serial, const uint8_t* data, size_t len) {
  static const char kPrefix[] = "RX: ";
  static const char kWarning[] = "RX dump truncated";
  const size_t prefixNeed = sizeof(kPrefix) - 1; // длина строки без завершающего нуля

  if (serial.availableForWrite() < static_cast<int>(prefixNeed)) {
    return false; // недостаточно места даже для префикса
  }

  serial.print(kPrefix); // печатаем префикс «RX: » перед данными

  size_t sent = 0;
  while (sent < len) {
    size_t writable = static_cast<size_t>(serial.availableForWrite());
    if (writable == 0) {
      break; // буфер переполнен, прекращаем попытки записи
    }
    size_t chunk = std::min(writable, len - sent);
    size_t written = serial.write(data + sent, chunk);
    if (written == 0) {
      break; // Serial отказался принимать данные, выходим из цикла
    }
    sent += written;
    if (written < chunk) {
      break; // Serial записал меньше, чем просили, вероятно буфер переполнен
    }
  }

  if (sent == len) {
    if (serial.availableForWrite() > 0) {
      serial.write('\n'); // добавляем перевод строки, если осталось место
    }
    return true; // дамп завершён без усечения
  }

  // Фиксируем предупреждение об усечении дампа
  if (serial.availableForWrite() >= static_cast<int>(sizeof(kWarning))) {
    serial.println(kWarning);
  }
  LogHook::append(kWarning);
  return false;
}

