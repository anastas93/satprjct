#pragma once
#include <cstdint>
#include <string>

// Конечная автоматика ожидания кадра KEYTRANSFER: отслеживает тайм-аут,
// каналы-подписчики (Serial/HTTP) и готовый результат обмена.
class KeyTransferWaiter {
 public:
  // Запуск ожидания. now_ms — текущая отметка времени, timeout_ms — допустимое
  // время ожидания (0 — без тайм-аута). Флаги serial_watcher/http_watcher
  // указывают, какие каналы заинтересованы в результате обмена.
  void start(uint32_t now_ms, uint32_t timeout_ms, bool serial_watcher, bool http_watcher);

  // Установка успешного результата в формате JSON.
  void finalizeSuccess(const std::string& json_result);

  // Установка результата с ошибкой в формате JSON.
  void finalizeError(const std::string& json_result);

  // Полный сброс состояния без уведомления подписчиков.
  void clear();

  // Признак активного ожидания.
  bool isWaiting() const { return waiting_; }

  // Проверка истечения тайм-аута относительно текущего времени.
  bool isExpired(uint32_t now_ms) const;

  // Фактическое значение тайм-аута, указанное при запуске.
  uint32_t timeoutMs() const { return timeout_ms_; }

  // Отметка времени запуска ожидания.
  uint32_t startedAtMs() const { return started_at_ms_; }

  // Есть ли подписчик Serial, ожидающий результат.
  bool serialPending() const { return serial_pending_; }

  // Есть ли подписчик HTTP, ожидающий результат.
  bool httpPending() const { return http_pending_; }

  // Есть ли невостребованный результат хотя бы для одного канала.
  bool hasPendingResult() const { return serial_pending_ || http_pending_; }

  // Изъять результат для Serial (при наличии). Возвращает пустую строку,
  // если результат уже выдан или не был запрошен.
  std::string consumeSerial();

  // Изъять результат для HTTP (при наличии).
  std::string consumeHttp();

 private:
  void finalize(const std::string& json_result);
  void dropResultIfDelivered();

  bool waiting_ = false;
  bool serial_watcher_ = false;
  bool http_watcher_ = false;
  bool serial_pending_ = false;
  bool http_pending_ = false;
  uint32_t started_at_ms_ = 0;
  uint32_t timeout_ms_ = 0;
  std::string result_json_;
};
