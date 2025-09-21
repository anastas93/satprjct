#pragma once
#include <cstdint>
#include <chrono>
#include <array>
#include <deque>
#include <optional>
#include <vector>
#include <string>
#include "radio_interface.h"
#include "message_buffer.h"
#include "libs/packetizer/packet_splitter.h" // подключаем разделитель пакетов из каталога libs
#include "default_settings.h"                  // параметры по умолчанию

// Модуль передачи данных с поддержкой классов QoS
class TxModule {
public:
  // Конструктор принимает радио, размеры очередей по классам QoS и режим пакета
  TxModule(IRadio& radio, const std::array<size_t,4>& capacities, PayloadMode mode = PayloadMode::SMALL);
  // Смена режима размера полезной нагрузки
  void setPayloadMode(PayloadMode mode);
  // Добавляет сообщение в очередь на отправку с указанием класса QoS (0..3)
  uint32_t queue(const uint8_t* data, size_t len, uint8_t qos = 0);
  // Отправляет первое доступное сообщение (если есть)
  // Возвращает true при успешной передаче
  bool loop();
  // Задать минимальную паузу между отправками (мс)
  void setSendPause(uint32_t pause_ms);
  // Получить текущую паузу между отправками (мс)
  uint32_t getSendPause() const;
  // Задать тайм-аут ожидания ACK (мс)
  void setAckTimeout(uint32_t timeout_ms);
  // Получить тайм-аут ожидания ACK (мс)
  uint32_t getAckTimeout() const;
  // Перечитать ключ из хранилища (после смены через веб-интерфейс)
  void reloadKey();
  // Управление режимами подтверждений и шифрования
  void setAckEnabled(bool enabled);
  void setAckRetryLimit(uint8_t retries);
  void onAckReceived();
  void setEncryptionEnabled(bool enabled);
private:
  struct PendingMessage {
    uint32_t id = 0;                         // идентификатор сообщения
    std::vector<uint8_t> data;               // данные сообщения с префиксом
    uint8_t qos = 0;                         // очередь QoS
    uint8_t attempts_left = 0;               // оставшиеся повторы
    bool expect_ack = false;                 // требуется ли подтверждение
    std::string packet_tag;                  // идентификатор пакета для группировки частей
  };

  bool transmit(const PendingMessage& message);
  static bool isAckPayload(const std::vector<uint8_t>& data);
  static std::string extractPacketTag(const std::vector<uint8_t>& data);
  void archiveFollowingParts(uint8_t qos, const std::string& tag);
  void scheduleFromArchive();
  void onSendSuccess();

  IRadio& radio_;
  std::array<MessageBuffer,4> buffers_;             // очереди сообщений по классам QoS
  PacketSplitter splitter_;
  std::array<uint8_t,16> key_{};                    // ключ шифрования
  uint32_t pause_ms_ = DefaultSettings::SEND_PAUSE_MS; // пауза между пакетами
  std::chrono::steady_clock::time_point last_send_; // время последней отправки
  bool ack_enabled_ = DefaultSettings::USE_ACK;     // режим ожидания ACK
  uint8_t ack_retry_limit_ = DefaultSettings::ACK_RETRY_LIMIT; // число повторов
  uint32_t ack_timeout_ms_ = DefaultSettings::ACK_TIMEOUT_MS;  // тайм-аут ожидания
  bool waiting_ack_ = false;                        // ждём ли ACK
  std::chrono::steady_clock::time_point last_attempt_; // момент последней отправки
  std::optional<PendingMessage> inflight_;          // текущий пакет в работе
  std::optional<PendingMessage> delayed_;           // пакет из архива, готовый к отправке
  std::deque<PendingMessage> archive_;              // архив сообщений без ACK
  bool encryption_enabled_ = DefaultSettings::USE_ENCRYPTION; // текущий режим шифрования
};

