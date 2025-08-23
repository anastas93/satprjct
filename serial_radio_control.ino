#include <Arduino.h>
#include <array>
#include <vector>
#include <cstring> // для strlen

// --- Радио и модули ---
#include "radio_sx1262.h"
#include "tx_module.h"
#include "rx_module.h" // модуль приёма
#include "default_settings.h"

// --- Вспомогательные библиотеки ---
#include "libs/text_converter/text_converter.h"   // конвертер UTF-8 -> CP1251
#include "libs/simple_logger/simple_logger.h"     // журнал статусов
#include "libs/received_buffer/received_buffer.h" // буфер принятых сообщений
#include "libs/crypto/aes_ccm.h"                  // AES-CCM шифрование

// --- Сеть и веб-интерфейс ---
#include <WiFi.h>        // работа с Wi-Fi
#include <WebServer.h>   // встроенный HTTP-сервер
#include "web/web_content.h" // встроенные файлы веб-интерфейса

// Пример управления радиомодулем через Serial c использованием абстрактного слоя
RadioSX1262 radio;
// Увеличиваем ёмкость очередей до 160 слотов, чтобы помещалось несколько пакетов по 5000 байт
TxModule tx(radio, std::array<size_t,4>{
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY});
RxModule rx;                // модуль приёма
ReceivedBuffer recvBuf;     // буфер полученных сообщений
bool ackEnabled = DefaultSettings::USE_ACK; // флаг автоматической отправки ACK

WebServer server(80);       // HTTP-сервер для веб-интерфейса

// Отдаём страницу index.html
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// Отдаём статический файл app.js
void handleAppJs() {
  server.send_P(200, "application/javascript", APP_JS);
}

// Отдаём стили style.css
void handleStyleCss() {
  server.send_P(200, "text/css", STYLE_CSS);
}

// Настройка Wi-Fi точки доступа и запуск сервера
void setupWifi() {
  WiFi.softAP(DefaultSettings::WIFI_SSID, DefaultSettings::WIFI_PASS); // создаём AP
  server.on("/", handleRoot);                                         // обработчик страницы
  server.on("/app.js", handleAppJs);                                 // JS веб-интерфейса
  server.on("/style.css", handleStyleCss);                           // CSS веб-интерфейса
  server.begin();                                                      // старт сервера
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());                                     // выводим адрес
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  setupWifi();                                       // запускаем точку доступа
  radio.begin();
  rx.setBuffer(&recvBuf);                                   // сохраняем принятые пакеты
  // обработка входящих данных с учётом ACK
  rx.setCallback([&](const uint8_t* d, size_t l){
    if (l == 3 && d[0]=='A' && d[1]=='C' && d[2]=='K') { // пришёл ACK
      Serial.println("ACK: получен");
      return;
    }
    Serial.print("RX: ");
    for (size_t i = 0; i < l; ++i) Serial.write(d[i]);
    Serial.println();
    if (ackEnabled) {                                     // отправляем подтверждение
      const uint8_t ack_msg[3] = {'A','C','K'};
      tx.queue(ack_msg, sizeof(ack_msg));
      tx.loop();
    }
  });
  radio.setReceiveCallback([&](const uint8_t* d, size_t l){  // привязка приёма
    rx.onReceive(d, l);
  });
  Serial.println("Команды: BF <полоса>, SF <фактор>, CR <код>, BANK <e|w|t|a>, CH <номер>, PW <0-9>, TX <строка>, TXL <размер>, BCN, INFO, STS <n>, RSTS <n>, ACK [0|1], PI, SEAR");
}

void loop() {
    server.handleClient();                  // обработка HTTP-запросов
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
        } else if (b == 'a') {
          radio.setBank(ChannelBank::ALL);
          Serial.println("Банк All");
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
          case ChannelBank::ALL: Serial.println("All"); break;
        }
        Serial.print("Канал: "); Serial.println(radio.getChannel());
        Serial.print("RX: "); Serial.print(radio.getRxFrequency(), 3); Serial.println(" MHz");
        Serial.print("TX: "); Serial.print(radio.getTxFrequency(), 3); Serial.println(" MHz");
        Serial.print("BW: "); Serial.print(radio.getBandwidth(), 2); Serial.println(" kHz");
        Serial.print("SF: "); Serial.println(radio.getSpreadingFactor());
        Serial.print("CR: "); Serial.println(radio.getCodingRate());
        Serial.print("Power: "); Serial.print(radio.getPower()); Serial.println(" dBm");
      } else if (line.startsWith("STS")) {
        int cnt = line.length() > 3 ? line.substring(4).toInt() : 10;
        if (cnt <= 0) cnt = 10;                       // значение по умолчанию
        auto logs = SimpleLogger::getLast(cnt);
        for (const auto& s : logs) {
          Serial.println(s.c_str());
        }
      } else if (line.startsWith("RSTS")) {
        int cnt = line.length() > 4 ? line.substring(5).toInt() : 10; // ограничение выводимых имён
        if (cnt <= 0) cnt = 10;                                       // значение по умолчанию
        auto names = recvBuf.list(cnt);                               // получаем список имён из буфера
        for (const auto& n : names) {
          Serial.println(n.c_str());
        }
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
      } else if (line.equalsIgnoreCase("ENCT")) {
        // тест шифрования: создаём сообщение, шифруем и расшифровываем
        const uint8_t key[16]   = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        const uint8_t nonce[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
        const char* text = "Test ENCT";                    // исходное сообщение
        size_t len = strlen(text);
        std::vector<uint8_t> cipher, tag, plain;
        bool enc = encrypt_ccm(key, sizeof(key), nonce, sizeof(nonce),
                               nullptr, 0,
                               reinterpret_cast<const uint8_t*>(text), len,
                               cipher, tag, 8);
        bool dec = false;
        if (enc) {
          dec = decrypt_ccm(key, sizeof(key), nonce, sizeof(nonce),
                            nullptr, 0,
                            cipher.data(), cipher.size(),
                            tag.data(), tag.size(), plain);
        }
        if (enc && dec && plain.size() == len &&
            std::equal(plain.begin(), plain.end(),
                       reinterpret_cast<const uint8_t*>(text))) {
          Serial.println("ENCT: успех");
        } else {
          Serial.println("ENCT: ошибка");
        }
      } else if (line.startsWith("ACK")) {
        if (line.length() > 3) {                          // установка явного значения
          ackEnabled = line.substring(4).toInt() != 0;
        } else {
          ackEnabled = !ackEnabled;                       // переключение
        }
        Serial.print("ACK: ");
        Serial.println(ackEnabled ? "включён" : "выключен");
      } else if (line.equalsIgnoreCase("PI")) {
        // отправляем пинг случайными байтами и ждём эха
        std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> pkt;
        for (auto &b : pkt) {
          b = radio.randomByte();              // заполняем случайностями
        }
        tx.queue(pkt.data(), pkt.size());      // ставим пакет в очередь
        while (!tx.loop()) {                   // ждём подтверждения отправки
          delay(1);                            // короткая пауза
        }
        delay(DefaultSettings::PING_WAIT_MS);  // отсчёт после отправки
        radio.loop();                          // обрабатываем возможное получение
        ReceivedBuffer::Item item;
        if (recvBuf.popRaw(item)) {
          Serial.print("RSSI: ");
          Serial.print(radio.getLastRssi());
          Serial.print(" SNR: ");
          Serial.println(radio.getLastSnr());
        } else {
          Serial.println("Пинг: тайм-аут");
        }
      } else if (line.equalsIgnoreCase("SEAR")) {
        // перебор каналов с пингом и возвратом исходного канала
        uint8_t prevCh = radio.getChannel();         // сохраняем текущий канал
        for (int ch = 0; ch < radio.getBankSize(); ++ch) {
          if (!radio.setChannel(ch)) {               // переключаемся
            Serial.print("CH ");
            Serial.print(ch);
            Serial.println(": ошибка");
            continue;
          }
          // очищаем буфер перед новым запросом
          ReceivedBuffer::Item dump;
          while (recvBuf.popRaw(dump)) {}
          // формируем и отправляем пинг
          std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> pkt2;
          for (auto &b : pkt2) {
            b = radio.randomByte();                 // случайные данные
          }
          tx.queue(pkt2.data(), pkt2.size());
          while (!tx.loop()) {                     // подтверждаем отправку
            delay(1);                               // короткая пауза
          }
          delay(DefaultSettings::PING_WAIT_MS);     // отсчёт после отправки
          radio.loop();                             // обрабатываем приём
          ReceivedBuffer::Item res;
          if (recvBuf.popRaw(res)) {
            Serial.print("CH ");
            Serial.print(ch);
            Serial.print(": RSSI ");
            Serial.print(radio.getLastRssi());
            Serial.print(" SNR ");
            Serial.println(radio.getLastSnr());
          } else {
            Serial.print("CH ");
            Serial.print(ch);
            Serial.println(": тайм-аут");
          }
        }
        radio.setChannel(prevCh);                    // возвращаем исходный канал
      }
    }
}
