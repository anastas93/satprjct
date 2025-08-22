#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include "libs/packetizer/packet_gatherer.h" // сборщик пакетов

// Модуль приёма данных
class RxModule {
public:
  using Callback = std::function<void(const uint8_t*, size_t)>;
  // Конструктор по умолчанию
  RxModule();
  // Обработка входящего пакета
  void onReceive(const uint8_t* data, size_t len);
  // Установка пользовательского колбэка
  void setCallback(Callback cb);
private:
  Callback cb_;
  PacketGatherer gatherer_; // внутренний сборщик фрагментов
};
