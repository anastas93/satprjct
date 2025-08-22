#include "serial_program_collector.h"
#include "default_settings.h"

// Ограничение на общий размер буфера задаётся в DefaultSettings::SERIAL_BUFFER_LIMIT

// Текущие собранные данные
String programBuffer;

// Флаг начала приёма
bool collecting = false;

// Сброс буфера и подготовка к новому сбору
void resetBuffer() {
  programBuffer = "";
  collecting = true;
  Serial.println("Начат сбор программы");
}

// Добавление строки в общий буфер
bool appendToBuffer(const String &line) {
  if (programBuffer.length() + line.length() + 1 > DefaultSettings::SERIAL_BUFFER_LIMIT) {
    Serial.println("Буфер переполнен, приём остановлен");
    collecting = false;
    return false;
  }
  programBuffer += line;
  programBuffer += '\n';
  return true;
}
