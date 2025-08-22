#pragma once
#include <Arduino.h>

// Максимальный размер буфера (500 КБ)
extern const size_t MAX_BUFFER;

// Текущие собранные данные
extern String programBuffer;

// Флаг начала приёма
extern bool collecting;

// Сброс буфера и подготовка к новому сбору
void resetBuffer();

// Добавление строки в общий буфер
bool appendToBuffer(const String &line);
