#include "rx_module.h"

// Передаём данные колбэку, если он установлен
void RxModule::onReceive(const uint8_t* data, size_t len) {
  if (cb_) cb_(data, len);
}

// Установка колбэка для обработки сообщений
void RxModule::setCallback(Callback cb) {
  cb_ = cb;
}
