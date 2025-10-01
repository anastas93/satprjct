#pragma once
#ifndef LOG_HOOK_LOG_H
#define LOG_HOOK_LOG_H

// Единый хук журналирования с буферизацией сообщений и нотификацией подписчиков
// Все комментарии и документация на русском согласно требованиям пользователя

#include <stdint.h>
#include <stddef.h>

#ifdef ARDUINO
#  include <Arduino.h>
#else
#  include <string>
#  include <vector>
#  include <utility>
#  include <functional>
#endif

#include <vector>

namespace LogHook {

#ifdef ARDUINO
  using LogString = String;  // На Arduino используем класс String
#else
  using LogString = std::string;                // Для хостовых сборок применяем std::string
#endif

  struct Entry;  // предварительное объявление для типа диспетчера

#ifdef ARDUINO
  using Dispatcher = void(*)(const Entry&); // колбэк доставки новых сообщений
#else
  using Dispatcher = std::function<void(const Entry&)>; // диспетчер для push-уведомлений
#endif

  // Структура одной записи журнала
  struct Entry {
    uint32_t id;        // последовательный идентификатор строки
    LogString text;     // текст сообщения
    uint32_t uptime_ms; // время с момента старта устройства (millis)
  };

  // Установка обработчика, который будет вызван при появлении новых записей
  void setDispatcher(Dispatcher cb);

  // Добавление строки в буфер и запуск уведомления
  void append(const LogString& line);
  void append(const char* line);

  // Получение последних N записей (не более ёмкости буфера)
  std::vector<Entry> getRecent(size_t count);

  // Очистка буфера и сброс счётчика идентификаторов
  void clear();

  // Возвращает текущий размер буфера
  size_t size();

} // namespace LogHook
#endif // LOG_HOOK_LOG_H

