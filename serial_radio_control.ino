#include <Arduino.h>
#include "radio_sx1262.h"
#include "message_buffer.h"
#include "tx_module.h"

// Пример управления радиомодулем через Serial c использованием абстрактного слоя
RadioSX1262 radio;
MessageBuffer msgBuf(4);        // буфер сообщений
TxModule tx(radio, msgBuf);     // модуль отправки

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  radio.begin();
  Serial.println("Команды: BF <полоса>, SF <фактор>, CR <код>, BANK <e|w|t>, CH <0-9>, TX <строка>");
}

void loop() {
    tx.loop();
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.startsWith("BF ") || line.startsWith("BW ")) {
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
      } else if (line.startsWith("CR ")) {
        int cr = line.substring(3).toInt();
        if (radio.setCodingRate(cr)) {
          Serial.println("CR установлен");
        } else {
          Serial.println("Ошибка установки CR");
        }
      } else if (line.startsWith("BANK ")) {
        char b = line.charAt(5);
        if (b == 'e') {
          radio.setBank(ChannelBank::EAST);
          Serial.println("Банк Восток");
        } else if (b == 'w') {
          radio.setBank(ChannelBank::WEST);
          Serial.println("Банк Запад");
        } else if (b == 't') {
          radio.setBank(ChannelBank::TEST);
          Serial.println("Банк Тест");
        }
      } else if (line.startsWith("CH ")) {
        int ch = line.substring(3).toInt();
        if (radio.setChannel(ch)) {
          Serial.print("Канал ");
          Serial.println(ch);
        } else {
          Serial.println("Ошибка выбора канала");
        }
      } else if (line.startsWith("TX ")) {
        String msg = line.substring(3);
        tx.queue((const uint8_t*)msg.c_str(), msg.length());
        tx.loop();
        Serial.println("Пакет отправлен");
      }
    }
}
