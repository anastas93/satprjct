#include <Arduino.h>

// Максимальный размер буфера (500 КБ)
const size_t MAX_BUFFER = 500UL * 1024UL;

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
  Serial.println("Ожидание команд: BEGIN для старта, END для завершения");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

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
