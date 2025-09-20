#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include "libs/packetizer/packet_gatherer.h" // сборщик пакетов
#include "libs/received_buffer/received_buffer.h" // буфер принятых сообщений

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
  // Привязка внешнего буфера для хранения готовых сообщений
  void setBuffer(ReceivedBuffer* buf);
  // Обновление ключа дешифрования (после смены в хранилище)
  void reloadKey();
private:
  Callback cb_;
  PacketGatherer gatherer_; // внутренний сборщик фрагментов
  ReceivedBuffer* buf_ = nullptr; // внешний буфер готовых данных
  std::array<uint8_t,16> key_{};   // ключ для дешифрования
  std::array<uint8_t,12> nonce_{}; // буфер под вычисленный нонс
  std::vector<uint8_t> frame_buf_;   // рабочий буфер кадра без дополнительного выделения
  std::vector<uint8_t> payload_buf_; // буфер полезной нагрузки после удаления пилотов
  std::vector<uint8_t> work_buf_;    // временный буфер для декодеров
  std::vector<uint8_t> result_buf_;  // буфер результата перед дешифрованием
  std::vector<uint8_t> plain_buf_;   // буфер расшифрованных данных
};
