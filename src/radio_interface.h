#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

// Простейший интерфейс радиоканала
class IRadio {
public:
  using RxCallback = std::function<void(const uint8_t*, size_t)>;
  static constexpr int16_t ERR_NONE = 0;                  // успешное завершение операции
  static constexpr int16_t ERR_TIMEOUT = -32000;          // тайм-аут захвата ресурса
  virtual ~IRadio() = default;
  // Отправка данных по радио
  virtual int16_t send(const uint8_t* data, size_t len) = 0;
  // Регистрация колбэка для приёма
  virtual void setReceiveCallback(RxCallback cb) = 0;
  // Гарантируем возврат в режим приёма (по умолчанию ничего не делаем)
  virtual int16_t ensureReceiveMode() { return ERR_NONE; }
};
