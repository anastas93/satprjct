#pragma once
#include <Arduino.h>

// Текущие собранные данные
extern String programBuffer;

// Флаг начала приёма
extern bool collecting;

// Сброс буфера и подготовка к новому сбору
void resetBuffer();

// Добавление строки в общий буфер (размер ограничен DefaultSettings::SERIAL_BUFFER_LIMIT)
bool appendToBuffer(const String &line);
