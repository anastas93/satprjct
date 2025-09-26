#pragma once
#include <functional>
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <unordered_map>
#include <chrono>
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
  bool assembling_ = false;          // активна ли текущая сборка сообщения
  uint32_t active_msg_id_ = 0;       // идентификатор собираемого сообщения
  uint16_t expected_frag_cnt_ = 0;   // сколько фрагментов ожидается
  uint16_t next_frag_idx_ = 0;       // какой индекс должен прийти следующим
  std::chrono::steady_clock::time_point last_conv_cleanup_{}; // момент последней очистки кэша свёртки
  struct PendingConvBlock {
    size_t expected_len = 0;           // ожидаемая длина свёрнутого блока
    std::vector<uint8_t> data;         // накопленные байты для последующего декодирования
    std::chrono::steady_clock::time_point last_update{}; // отметка последнего поступления данных
  };
  std::unordered_map<uint64_t, PendingConvBlock> pending_conv_; // буферизация неполных свёрточных блоков
  struct SplitPrefixInfo {
    bool valid = false;                // удалось ли разобрать префикс
    std::string tag;                   // общий идентификатор группы частей
    size_t part = 0;                   // номер текущей части (начиная с 1)
    size_t total = 0;                  // ожидаемое количество частей
  };
  struct PendingSplit {
    size_t expected_total = 0;         // сколько частей ждём всего
    size_t next_part = 1;              // следующий ожидаемый индекс
    std::vector<uint8_t> data;         // накопленные данные
    std::chrono::steady_clock::time_point last_update{}; // отметка последнего обновления
  };
  std::unordered_map<std::string, PendingSplit> pending_split_; // незавершённые группы частей
  static constexpr std::chrono::seconds PENDING_SPLIT_TTL{30};  // время жизни незавершённых частей
  struct SplitProcessResult {
    bool deliver = false;               // готов ли результат к выдаче
    bool use_original = true;          // можно ли использовать исходный буфер без копирования
    std::vector<uint8_t> data;         // собранные данные, если требуется копия
  };
  std::unordered_map<uint32_t, SplitPrefixInfo> inflight_prefix_; // префиксы, ожидающие завершения
  void cleanupPendingConv(std::chrono::steady_clock::time_point now);
  void cleanupPendingSplits(std::chrono::steady_clock::time_point now);
  SplitPrefixInfo parseSplitPrefix(const std::vector<uint8_t>& data, size_t& prefix_len) const;
  SplitProcessResult handleSplitPart(const SplitPrefixInfo& info, const std::vector<uint8_t>& chunk,
                                     uint32_t msg_id);
};
