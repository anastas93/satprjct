#include <Arduino.h>
#include "radio_sx1262.h"

// Пример управления параметрами радиомодуля через Serial
RadioSX1262 radio;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  radio.begin();
  Serial.println("Команды: F <частота>, BW <полоса>, SF <фактор>");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("F ")) {
      float freq = line.substring(2).toFloat();
      if (radio.setFrequency(freq)) {
        Serial.println("Частота установлена");
      } else {
        Serial.println("Ошибка установки частоты");
      }
    } else if (line.startsWith("BW ")) {
      float bw = line.substring(3).toFloat();
      if (radio.setBandwidth(bw)) {
        Serial.println("Полоса установлена");
      } else {
        Serial.println("Ошибка установки BW");
      }
    } else if (line.startsWith("SF ")) {
      int sf = line.substring(3).toInt();
      if (radio.setSpreadingFactor(sf)) {
        Serial.println("SF установлен");
      } else {
        Serial.println("Ошибка установки SF");
      }
    }
  }
}
