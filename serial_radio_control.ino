#include <Arduino.h>
#include <array>
#include <vector>
#include "radio_sx1262.h"
#include "tx_module.h"
#include "default_settings.h"
#include "libs/text_converter/text_converter.h" // конвертер UTF-8 -> CP1251

// Пример управления радиомодулем через Serial c использованием абстрактного слоя
RadioSX1262 radio;
// Увеличиваем ёмкость очередей до 160 слотов, чтобы помещалось несколько пакетов по 5000 байт
TxModule tx(radio, std::array<size_t,4>{
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY});

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  radio.begin();
  Serial.println("Команды: BF <полоса>, SF <фактор>, CR <код>, BANK <e|w|t>, CH <0-9>, PW <0-9>, TX <строка>, TXL <размер>, BCN, INFO");
}

void loop() {
    radio.loop();                           // обработка входящих пакетов
    tx.loop();                              // обработка очередей передачи
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
      } else if (line.startsWith("TXL ")) {
        int sz = line.substring(4).toInt();                // размер требуемого пакета
        if (sz > 0) {
          // формируем тестовый массив указанного размера с возрастающими байтами
          std::vector<uint8_t> data(sz);
          for (int i = 0; i < sz; ++i) {
            data[i] = static_cast<uint8_t>(i);            // шаблон данных
          }
          tx.setPayloadMode(PayloadMode::LARGE);          // временно включаем крупные пакеты
          uint32_t id = tx.queue(data.data(), data.size());
          tx.setPayloadMode(PayloadMode::SMALL);          // возвращаем режим по умолчанию
          if (id != 0) {
            tx.loop();                                   // отправляем сформированные фрагменты
            Serial.println("Большой пакет отправлен");
          } else {
            Serial.println("Ошибка постановки большого пакета");
          }
        } else {
          Serial.println("Неверный размер");
        }
      }
    }
}
