#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <unordered_map>
#include "libs/packetizer/packet_gatherer.h" // сборщик пакетов
#include "libs/received_buffer/received_buffer.h" // буфер принятых сообщений
#include "default_settings.h"

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
  // Управление режимом шифрования (fallback для старых кадров)
  void setEncryptionEnabled(bool enabled);
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
  uint32_t raw_counter_ = 0;         // счётчик сырых пакетов без заголовка
  bool encryption_forced_ = DefaultSettings::USE_ENCRYPTION; // ожидание шифрования по умолчанию
  struct PendingConvBlock {
    size_t expected_len = 0;           // ожидаемая длина свёрнутого блока
    std::vector<uint8_t> data;         // накопленные байты для последующего декодирования
  };
  std::unordered_map<uint64_t, PendingConvBlock> pending_conv_; // буферизация неполных свёрточных блоков
};
