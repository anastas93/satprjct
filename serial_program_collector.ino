#include <Arduino.h>
#include "radio_sx1262.h"  // работа с модулем SX1262

// Максимальный размер буфера (500 КБ)
const size_t MAX_BUFFER = 500UL * 1024UL;

// Текущие собранные данные
String programBuffer;

// Флаг начала приёма
bool collecting = false;

// Радиомодуль
RadioSX1262 radio;

// Сброс буфера и подготовка к новому сбору
void resetBuffer() {
  programBuffer = "";
  collecting = true;
  Serial.println("Начат сбор программы");
}

// Добавление строки в общий буфер
bool appendToBuffer(const String &line) {
  if (programBuffer.length() + line.length() + 1 > MAX_BUFFER) {
    Serial.println("Буфер переполнен, приём остановлен");
    collecting = false;
    return false;
  }
  programBuffer += line;
  programBuffer += '\n';
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  // Инициализация радиомодуля
  if (!radio.begin()) {
    Serial.println("Ошибка инициализации радио");
  }
  Serial.println("Ожидание команд: BEGIN/END, SET_FREQ, SET_BW, SET_SF");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    // Обработка команд управления радиомодулем
    if (line.startsWith("SET_FREQ")) {
      float f = line.substring(8).toFloat();
      Serial.println(radio.setFrequency(f) ? "Частота обновлена" : "Ошибка установки частоты");
      return;
    }
    if (line.startsWith("SET_BW")) {
      float bw = line.substring(7).toFloat();
      Serial.println(radio.setBandwidth(bw) ? "BW обновлена" : "Ошибка установки BW");
      return;
    }
    if (line.startsWith("SET_SF")) {
      int sf = line.substring(7).toInt();
      Serial.println(radio.setSpreadingFactor(sf) ? "SF обновлён" : "Ошибка установки SF");
      return;
    }

    // Команды сбора программы
    if (!collecting) {
      if (line == "BEGIN") {
        resetBuffer();
      }
    } else {
      if (line == "END") {
        collecting = false;
        Serial.print("Сбор завершён. Размер: ");
        Serial.print(programBuffer.length());
        Serial.println(" байт");
        Serial.println(programBuffer);
      } else {
        appendToBuffer(line);
      }
    }
  }
}
