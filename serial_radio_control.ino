#include <Arduino.h>
#include "radio_sx1262.h"
#include "message_buffer.h"
#include "tx_module.h"
#include "default_settings.h"
#include "libs/text_converter/text_converter.h" // конвертер UTF-8 -> CP1251

// Пример управления радиомодулем через Serial c использованием абстрактного слоя
RadioSX1262 radio;
MessageBuffer msgBuf(4);        // буфер сообщений
TxModule tx(radio, msgBuf);     // модуль отправки

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  radio.begin();
  Serial.println("Команды: BF <полоса>, SF <фактор>, CR <код>, BANK <e|w|t>, CH <0-9>, PW <0-9>, TX <строка>, BCN, INFO");
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
      } else if (line.startsWith("PW ")) {
        int pw = line.substring(3).toInt();
        if (radio.setPower(pw)) {
          Serial.println("Мощность установлена");
        } else {
          Serial.println("Ошибка установки мощности");
        }
      } else if (line.equalsIgnoreCase("BCN")) {
        radio.sendBeacon();
        Serial.println("Маяк отправлен");
      } else if (line.equalsIgnoreCase("INFO")) {
        // выводим текущие настройки радиомодуля
        Serial.print("Банк: ");
        switch (radio.getBank()) {
          case ChannelBank::EAST: Serial.println("Восток"); break;
          case ChannelBank::WEST: Serial.println("Запад"); break;
          case ChannelBank::TEST: Serial.println("Тест"); break;
        }
        Serial.print("Канал: "); Serial.println(radio.getChannel());
        Serial.print("RX: "); Serial.print(radio.getRxFrequency(), 3); Serial.println(" MHz");
        Serial.print("TX: "); Serial.print(radio.getTxFrequency(), 3); Serial.println(" MHz");
        Serial.print("BW: "); Serial.print(radio.getBandwidth(), 2); Serial.println(" kHz");
        Serial.print("SF: "); Serial.println(radio.getSpreadingFactor());
        Serial.print("CR: "); Serial.println(radio.getCodingRate());
        Serial.print("Power: "); Serial.print(radio.getPower()); Serial.println(" dBm");
      } else if (line.startsWith("TX ")) {
        String msg = line.substring(3);                     // исходный текст
        // конвертация UTF-8 в CP1251 для корректной передачи русских символов
        std::vector<uint8_t> data = utf8ToCp1251(std::string(msg.c_str()));
        // помещаем сообщение в очередь и проверяем успех
        DEBUG_LOG("RC: команда TX");
        uint32_t id = tx.queue(data.data(), data.size());
        if (id != 0) {
          DEBUG_LOG_VAL("RC: сообщение поставлено id=", id);
          tx.loop();                           // отправляем первый пакет
          Serial.println("Пакет отправлен");
        } else {
          Serial.println("Ошибка постановки пакета в очередь");
        }
      }
    }
}
