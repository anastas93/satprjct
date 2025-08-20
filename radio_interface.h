#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

// Простейший интерфейс радиоканала
class IRadio {
public:
  using RxCallback = std::function<void(const uint8_t*, size_t)>;
  virtual ~IRadio() = default;
  // Отправка данных по радио
  virtual void send(const uint8_t* data, size_t len) = 0;
  // Регистрация колбэка для приёма
  virtual void setReceiveCallback(RxCallback cb) = 0;
};
