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

// Отдаём стили style.css
void handleStyleCss() {
  server.send_P(200, "text/css", STYLE_CSS);
}

// Отдаём скрипт script.js
void handleScriptJs() {
  server.send_P(200, "application/javascript", SCRIPT_JS);
}

// Отдаём библиотеку SHA-256
void handleSha256Js() {
  server.send_P(200, "application/javascript", SHA256_JS);
}

// Принимаем текст и отправляем его по радио
void handleApiTx() {
  if (server.method() != HTTP_POST) {                       // разрешаем только POST
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String body = server.arg("plain");                       // получаем сырой текст
  body.trim();
  if (body.length() == 0) {                                 // пустое сообщение
    server.send(400, "text/plain", "empty");
    return;
  }
  std::vector<uint8_t> data = utf8ToCp1251(body.c_str());   // конвертация в CP1251
  uint32_t id = tx.queue(data.data(), data.size());         // ставим в очередь
  if (id != 0) {
    tx.loop();                                              // отправляем сразу
    server.send(200, "text/plain", "ok");
  } else {
    server.send(500, "text/plain", "fail");
  }
}

// Преобразование символа в значение перечисления банка каналов
ChannelBank parseBankChar(char b) {
  switch (b) {
    case 'e': case 'E': return ChannelBank::EAST;
    case 'w': case 'W': return ChannelBank::WEST;
    case 't': case 'T': return ChannelBank::TEST;
    case 'a': case 'A': return ChannelBank::ALL;
    default: return radio.getBank();
  }
}

// Выполнение одиночного пинга и получение результата
String cmdPing() {
  // очищаем буфер от прежних пакетов
  ReceivedBuffer::Item dump;
  while (recvBuf.popReady(dump)) {}
  // формируем пинг
  std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> ping{};
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();
  ping[0] = ping[1] ^ ping[2];
  ping[3] = 0;
  ping[4] = 0;
  tx.queue(ping.data(), ping.size());
  while (!tx.loop()) { delay(1); }
  uint32_t t_start = micros();
  bool ok = false;
  ReceivedBuffer::Item resp;
  while (micros() - t_start < DefaultSettings::PING_WAIT_MS * 1000UL) {
    radio.loop();
    if (recvBuf.popReady(resp)) {
      if (resp.data.size() == ping.size() &&
          memcmp(resp.data.data(), ping.data(), ping.size()) == 0) {
        ok = true;
      }
      break;
    }
    delay(1);
  }
  if (ok) {
    uint32_t t_end = micros() - t_start;
    float dist_km = ((t_end * 0.000001f) * 299792458.0f / 2.0f) / 1000.0f;
    String out = "Ping: RSSI ";
    out += String(radio.getLastRssi());
    out += " dBm SNR ";
    out += String(radio.getLastSnr());
    out += " dB distance:~";
    out += String(dist_km);
    out += "km time:";
    out += String(t_end * 0.001f);
    out += "ms";
    return out;
  } else {
    return String("Ping: timeout");
  }
}

// Перебор всех каналов с пингом
String cmdSear() {
  uint8_t prevCh = radio.getChannel();
  String out;
  for (int ch = 0; ch < radio.getBankSize(); ++ch) {
    if (!radio.setChannel(ch)) {
      out += "CH "; out += String(ch); out += ": ошибка\n";
      continue;
    }
    ReceivedBuffer::Item d;
    while (recvBuf.popReady(d)) {}
    std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> pkt{};
    pkt[1] = radio.randomByte();
    pkt[2] = radio.randomByte();
    pkt[0] = pkt[1] ^ pkt[2];
    pkt[3] = 0; pkt[4] = 0;
    tx.queue(pkt.data(), pkt.size());
    while (!tx.loop()) { delay(1); }
    uint32_t t_start = micros();
    bool ok_ch = false;
    ReceivedBuffer::Item res;
    while (micros() - t_start < DefaultSettings::PING_WAIT_MS * 1000UL) {
      radio.loop();
      if (recvBuf.popReady(res)) {
        if (res.data.size() == pkt.size() &&
            memcmp(res.data.data(), pkt.data(), pkt.size()) == 0) {
          ok_ch = true;
        }
        break;
      }
      delay(1);
    }
    if (ok_ch) {
      out += "CH "; out += String(ch); out += ": RSSI ";
      out += String(radio.getLastRssi()); out += " SNR ";
      out += String(radio.getLastSnr()); out += "\n";
    } else {
      out += "CH "; out += String(ch); out += ": тайм-аут\n";
    }
  }
  radio.setChannel(prevCh);
  return out;
}

// Список частот для выбранного банка в формате CSV
String cmdChlist(ChannelBank bank) {
  // Формируем CSV в формате ch,tx,rx,rssi,snr,st,scan
  String out = "ch,tx,rx,rssi,snr,st,scan\n";
  uint16_t n = RadioSX1262::bankSize(bank);
  for (uint16_t i = 0; i < n; ++i) {
    out += String(i);                               // номер канала
    out += ',';
    out += String(RadioSX1262::bankTx(bank, i), 3); // частота передачи
    out += ',';
    out += String(RadioSX1262::bankRx(bank, i), 3); // частота приёма
    out += ",0,0,,\n";                             // пустые поля rssi/snr/st/scan
  }
  return out;
}

// Вывод текущих настроек радиомодуля
String cmdInfo() {
  String s;
  s += "Bank: ";
  switch (radio.getBank()) {
    case ChannelBank::EAST: s += "Восток"; break;
    case ChannelBank::WEST: s += "Запад"; break;
    case ChannelBank::TEST: s += "Тест"; break;
    case ChannelBank::ALL: s += "All"; break;
  }
  s += "\nChannel: "; s += String(radio.getChannel());
  s += "\nRX: "; s += String(radio.getRxFrequency(), 3); s += " MHz";
  s += "\nTX: "; s += String(radio.getTxFrequency(), 3); s += " MHz";
  s += "\nBW: "; s += String(radio.getBandwidth(), 2); s += " kHz";
  s += "\nSF: "; s += String(radio.getSpreadingFactor());
  s += "\nCR: "; s += String(radio.getCodingRate());
  s += "\nPower: "; s += String(radio.getPower()); s += " dBm";
  return s;
}

// Последние записи журнала
String cmdSts(int cnt) {
  if (cnt <= 0) cnt = 10;
  auto logs = SimpleLogger::getLast(cnt);
  String s;
  for (const auto& l : logs) {
    s += l.c_str(); s += '\n';
  }
  return s;
}

// Список имён из буфера приёма
String cmdRsts(int cnt) {
  if (cnt <= 0) cnt = 10;
  auto names = recvBuf.list(cnt);
  String s;
  for (const auto& n : names) {
    s += n.c_str(); s += '\n';
  }
  return s;
}

// Обработка HTTP-команды вида /cmd?c=PI
void handleCmdHttp() {
  String cmd = server.arg("c");
  if (cmd.length() == 0) cmd = server.arg("cmd");
  cmd.trim();
  cmd.toUpperCase();
  String resp;
  if (cmd == "PI") {
    resp = cmdPing();
  } else if (cmd == "SEAR") {
    resp = cmdSear();
  } else if (cmd == "BANK") {
    if (server.hasArg("v")) {
      char b = server.arg("v")[0];
      radio.setBank(parseBankChar(b));
    }
    resp = "OK";
  } else if (cmd == "CH") {
    int ch = server.arg("v").toInt();
    resp = radio.setChannel(ch) ? "OK" : "ERR";
  } else if (cmd == "CHLIST") {
    ChannelBank b = radio.getBank();
    if (server.hasArg("bank")) {
      b = parseBankChar(server.arg("bank")[0]);
    }
    resp = cmdChlist(b);
  } else if (cmd == "INFO") {
    resp = cmdInfo();
  } else if (cmd == "STS") {
    int cnt = server.hasArg("n") ? server.arg("n").toInt() : 10;
    resp = cmdSts(cnt);
  } else if (cmd == "RSTS") {
    int cnt = server.hasArg("n") ? server.arg("n").toInt() : 10;
    resp = cmdRsts(cnt);
  } else {
    resp = "UNKNOWN";
  }
  server.send(200, "text/plain", resp);
}

// Настройка Wi-Fi точки доступа и запуск сервера
void setupWifi() {
  // Задаём статический IP 192.168.4.1 для точки доступа
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(DefaultSettings::WIFI_SSID, DefaultSettings::WIFI_PASS); // создаём AP
  server.on("/", handleRoot);                                         // обработчик страницы
  server.on("/style.css", handleStyleCss);                           // CSS веб-интерфейса
  server.on("/script.js", handleScriptJs);                           // JS логика
  server.on("/libs/sha256.js", handleSha256Js);                      // библиотека SHA-256
  server.on("/api/tx", handleApiTx);                                 // отправка текста по радио
  server.on("/cmd", handleCmdHttp);                                  // обработка команд
  server.on("/api/cmd", handleCmdHttp);                              // совместимый эндпоинт
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
        // очистка буфера от прежних пакетов
        ReceivedBuffer::Item dump;
        while (recvBuf.popReady(dump)) {}

        // формируем 5-байтовый пинг из двух случайных байт
        std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> ping{};
        ping[1] = radio.randomByte();
        ping[2] = radio.randomByte();       // ID пакета
        ping[0] = ping[1] ^ ping[2];        // идентификатор = XOR
        ping[3] = 0;                        // адрес (пока не используется)
        ping[4] = 0;

        tx.queue(ping.data(), ping.size()); // ставим пакет в очередь
        while (!tx.loop()) {                // ждём фактической отправки
          delay(1);                         // небольшая пауза
        }

        uint32_t t_start = micros();       // время отправки
        bool ok = false;                   // флаг получения
        ReceivedBuffer::Item resp;

        // ждём ответ, регулярно вызывая обработчик приёма
        while (micros() - t_start < DefaultSettings::PING_WAIT_MS * 1000UL) {
          radio.loop();
          if (recvBuf.popReady(resp)) {
            if (resp.data.size() == ping.size() &&
                memcmp(resp.data.data(), ping.data(), ping.size()) == 0) {
              ok = true;                   // получено корректное эхо
            }
            break;
          }
          delay(1);
        }

        if (ok) {
          uint32_t t_end = micros() - t_start;               // время прохождения
          float dist_km =
              ((t_end * 0.000001f) * 299792458.0f / 2.0f) / 1000.0f; // оценка дистанции
          Serial.print("Ping: RSSI ");
          Serial.print(radio.getLastRssi());
          Serial.print(" dBm SNR ");
          Serial.print(radio.getLastSnr());
          Serial.print(" dB distance:~");
          Serial.print(dist_km);
          Serial.print("km time:");
          Serial.print(t_end * 0.001f);
          Serial.println("ms");
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
          ReceivedBuffer::Item d;
          while (recvBuf.popReady(d)) {}
          // формируем и отправляем пинг
          std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> pkt2{};
          pkt2[1] = radio.randomByte();
          pkt2[2] = radio.randomByte();         // ID пакета
          pkt2[0] = pkt2[1] ^ pkt2[2];          // идентификатор
          pkt2[3] = 0;
          pkt2[4] = 0;
          tx.queue(pkt2.data(), pkt2.size());
          while (!tx.loop()) {                  // подтверждаем отправку
            delay(1);
          }
          uint32_t t_start = micros();
          bool ok_ch = false;
          ReceivedBuffer::Item res;
          while (micros() - t_start < DefaultSettings::PING_WAIT_MS * 1000UL) {
            radio.loop();
            if (recvBuf.popReady(res)) {
              if (res.data.size() == pkt2.size() &&
                  memcmp(res.data.data(), pkt2.data(), pkt2.size()) == 0) {
                ok_ch = true;
              }
              break;
            }
            delay(1);
          }
          if (ok_ch) {
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
