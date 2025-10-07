#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <RadioLib.h>
#include <vector>
#include <array>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdio>

#include "libs/radio/lora_radiolib_settings.h"     // дефолтные настройки драйвера SX1262

// --- Константы частот банка HOME ---
namespace frequency_tables {

// Локальный набор частот банка HOME, продублированный здесь, чтобы проект был самодостаточным
static constexpr std::size_t HOME_BANK_SIZE = 14;
static constexpr float RX_HOME[HOME_BANK_SIZE] = {
    250.000F,263.450F, 257.700F, 257.200F, 256.450F, 267.250F, 250.090F, 249.850F,
    257.250F, 255.100F, 246.700F, 260.250F, 263.400F, 263.500F};
static constexpr float TX_HOME[HOME_BANK_SIZE] = {
    250.000F,311.400F, 316.150F, 308.800F, 313.850F, 308.250F, 312.600F, 298.830F,
    316.900F, 318.175F, 297.700F, 314.400F, 316.400F, 315.200F};

} // namespace frequency_tables

// --- Проверка наличия обязательных макросов для Wi-Fi ---
#ifndef LOTEST_WIFI_SSID
#  define LOTEST_WIFI_SSID "sat_ap"
#endif
#ifndef LOTEST_WIFI_PASS
#  define LOTEST_WIFI_PASS "12345678"
#endif
#ifndef LOTEST_PROJECT_NAME
#  define LOTEST_PROJECT_NAME "Lotest"
#endif

// --- Глобальные объекты периферии ---
SPIClass radioSPI(VSPI);                          // аппаратный SPI-порт, обслуживающий радиомодуль
SX1262 radio = new Module(5, 26, 27, 25, radioSPI); // используем VSPI сразу при создании объекта Module
WebServer server(80);                             // встроенный HTTP-сервер ESP32

// --- Константы проекта ---
constexpr uint8_t kHomeBankSize = static_cast<uint8_t>(frequency_tables::HOME_BANK_SIZE); // число каналов банка HOME
constexpr uint16_t kImplicitPayloadLength = LoRaRadioLibSettings::DEFAULT_OPTIONS.implicitPayloadLength; // длина implicit-пакета
constexpr size_t kMaxEventHistory = 120;          // ограничение истории событий для веб-чата
constexpr size_t kFullPacketSize = 245;           // максимальная длина пакета SX1262
constexpr std::array<size_t, 5> kFixedPacketOptions = {4, 8, 16, 32, 64}; // доступные размеры фиксированного пакета
constexpr size_t kDefaultFixedPacketSize = kFixedPacketOptions.front();   // размер фиксированного пакета по умолчанию

// --- Структура, описывающая событие в веб-чате ---
struct ChatEvent {
  unsigned long id = 0;   // уникальный идентификатор сообщения
  String text;            // сам текст события
};

// --- Текущее состояние приложения ---
struct AppState {
  uint8_t channelIndex = 0;            // выбранный канал банка HOME
  bool highPower = false;              // признак использования мощности 22 dBm (иначе -5 dBm)
  float currentRxFreq = frequency_tables::RX_HOME[0]; // текущая частота приёма
  float currentTxFreq = frequency_tables::TX_HOME[0]; // текущая частота передачи
  unsigned long nextEventId = 1;       // счётчик идентификаторов для событий
  std::vector<ChatEvent> events;       // журнал событий для веб-интерфейса
  size_t fixedPacketSize = kDefaultFixedPacketSize; // выбранный размер фиксированного пакета
} state;

// --- Флаги приёма LoRa ---
volatile bool packetReceivedFlag = false;   // устанавливается обработчиком DIO1 при приёме
volatile bool packetProcessingEnabled = true; // защита от повторного входа в обработчик

// --- Вспомогательные функции объявления ---
void IRAM_ATTR onRadioDio1Rise();
void addEvent(const String& message);
void appendEventBuffer(const String& message, unsigned long id);
void handleRoot();
void handleLog();
void handleChannelChange();
void handlePowerToggle();
void handleSendFixedPacket();
void handleSendRandomPacket();
void handleSendCustom();
void handleNotFound();
String buildIndexHtml();
String buildChannelOptions(uint8_t selected);
String buildFixedPacketOptions(size_t current);
String escapeJson(const String& value);
String makeAccessPointSsid();
bool applyRadioChannel(uint8_t newIndex);
bool applyRadioPower(bool highPower);
bool ensureReceiveMode();
bool sendBuffer(const std::vector<uint8_t>& buffer, const String& context);
std::vector<uint8_t> parseHexString(const String& raw, bool& ok, String& errorMessage);
String formatByteArray(const std::vector<uint8_t>& data);
void logRadioError(const String& context, int16_t code);
void handleFixedPacketSizeChange();

// --- Формирование имени Wi-Fi сети ---
String makeAccessPointSsid() {
  String base = LOTEST_WIFI_SSID;              // базовый SSID из настроек теста
#if defined(ARDUINO)
  uint32_t suffix = 0;                         // уникальный суффикс для совпадения с основной прошивкой
#  if defined(ESP32)
  uint64_t mac = ESP.getEfuseMac();            // используем eFuse MAC для воспроизведения поведения оригинала
  suffix = static_cast<uint32_t>(mac & 0xFFFFFFULL);
#  elif defined(ESP8266)
  suffix = ESP.getChipId() & 0xFFFFFFU;        // совместимая логика для других платформ
#  else
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);                        // fallback: читаем MAC из Wi-Fi интерфейса
  suffix = (static_cast<uint32_t>(mac[3]) << 16) |
           (static_cast<uint32_t>(mac[4]) << 8) |
           static_cast<uint32_t>(mac[5]);
#  endif
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%06X", static_cast<unsigned>(suffix));
  base += "-";
  base += buf;
#else
  base += "-000000";                           // стабы для хостовых тестов без Arduino
#endif
  return base;
}

// --- Инициализация оборудования ---
void setup() {
  Serial.begin(115200);
  delay(200);
  addEvent("Запуск устройства " LOTEST_PROJECT_NAME);
  randomSeed(esp_random());

  // Настройка SPI для радиомодуля SX1262
  radioSPI.begin(18, 19, 23, 5); // VSPI: SCK=18, MISO=19, MOSI=23, SS=5

  // Подключаем обработчик прерывания по DIO1 для асинхронного приёма
  radio.setDio1Action(onRadioDio1Rise);

  // Стартуем радиомодуль и применяем параметры
  int16_t beginState = radio.begin();
  if (beginState != RADIOLIB_ERR_NONE) {
    logRadioError("radio.begin", beginState);
  } else {
    addEvent("Радиомодуль успешно инициализирован");

    // Применяем настройки LoRa согласно требованиям задачи
    radio.setSpreadingFactor(7);
    radio.setBandwidth(15.63);
    radio.setCodingRate(5);

    const auto& opts = LoRaRadioLibSettings::DEFAULT_OPTIONS;
    radio.setDio2AsRfSwitch(opts.useDio2AsRfSwitch);
    if (opts.useDio3ForTcxo && opts.tcxoVoltage > 0.0f) {
      radio.setTCXO(opts.tcxoVoltage); // включаем внешний TCXO с указанным напряжением
    }
    if (opts.implicitHeader) {
      radio.implicitHeader(kImplicitPayloadLength);
    } else {
      radio.explicitHeader();
    }
    radio.setCRC(opts.enableCrc ? 2 : 0);          // длина CRC в байтах: 2 либо 0
    radio.invertIQ(opts.invertIq);                 // включаем или выключаем инверсию IQ
    radio.setPreambleLength(opts.preambleLength);
    radio.setRxBoostedGainMode(opts.rxBoostedGain); // режим усиленного приёма SX1262
    radio.setSyncWord(opts.syncWord);

    if (!applyRadioChannel(state.channelIndex)) {
      addEvent("Ошибка инициализации канала — проверьте модуль SX1262");
    }
    if (!applyRadioPower(state.highPower)) {
      addEvent("Ошибка установки мощности — проверьте подключение модуля");
    }
  }

  // Поднимаем точку доступа для веб-интерфейса
  WiFi.mode(WIFI_MODE_AP);
  String ssid = makeAccessPointSsid();
  bool apStarted = WiFi.softAP(ssid.c_str(), LOTEST_WIFI_PASS);
  if (apStarted) {
    addEvent(String("Точка доступа запущена: ") + ssid);
    addEvent(String("IP адрес веб-интерфейса: ") + WiFi.softAPIP().toString());
  } else {
    addEvent("Не удалось запустить точку доступа Wi-Fi");
  }

  // Регистрируем HTTP-маршруты
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/channel", HTTP_POST, handleChannelChange);
  server.on("/api/power", HTTP_POST, handlePowerToggle);
  server.on("/api/send/five", HTTP_POST, handleSendFixedPacket); // совместимость с прежним маршрутом
  server.on("/api/send/fixed", HTTP_POST, handleSendFixedPacket);
  server.on("/api/send/random", HTTP_POST, handleSendRandomPacket);
  server.on("/api/send/custom", HTTP_POST, handleSendCustom);
  server.on("/api/fixed-size", HTTP_POST, handleFixedPacketSizeChange);
  server.onNotFound(handleNotFound);
  server.begin();
  addEvent("HTTP-сервер запущен на порту 80");
}

// --- Основной цикл ---
void loop() {
  server.handleClient();

  if (packetReceivedFlag) {
    packetProcessingEnabled = false;
    packetReceivedFlag = false;

    std::vector<uint8_t> buffer(kImplicitPayloadLength, 0);
    int16_t stateCode = radio.readData(buffer.data(), buffer.size());
    if (stateCode == RADIOLIB_ERR_NONE) {
      size_t actualLength = radio.getPacketLength();
      if (actualLength > buffer.size()) {
        actualLength = buffer.size();
      }
      buffer.resize(actualLength);
      addEvent(String("Принят пакет (" + String(buffer.size()) + " байт): ") + formatByteArray(buffer));
    } else {
      logRadioError("readData", stateCode);
    }

    ensureReceiveMode();
    packetProcessingEnabled = true;
  }
}

// --- Обработчик линии DIO1 радиомодуля ---
void IRAM_ATTR onRadioDio1Rise() {
  if (!packetProcessingEnabled) {
    return;
  }
  packetReceivedFlag = true;
}

// --- Добавление события в лог ---
void addEvent(const String& message) {
  appendEventBuffer(message, state.nextEventId++);
  Serial.println(message);
}

// --- Вспомогательная функция: дописываем событие и ограничиваем историю ---
void appendEventBuffer(const String& message, unsigned long id) {
  ChatEvent event;
  event.id = id;
  event.text = message;
  state.events.push_back(event); // явное построение объекта для совместимости со старыми стандартами C++
  if (state.events.size() > kMaxEventHistory) {
    state.events.erase(state.events.begin());
  }
}

// --- HTML главной страницы ---
void handleRoot() {
  server.send(200, "text/html; charset=UTF-8", buildIndexHtml());
}

// --- API: выдача журналов событий ---
void handleLog() {
  unsigned long after = 0;
  if (server.hasArg("after")) {
    after = strtoul(server.arg("after").c_str(), nullptr, 10);
  }

  String payload = "{\"events\":[";
  bool first = true;
  unsigned long lastId = after;
  for (const auto& evt : state.events) {
    if (evt.id <= after) {
      continue;
    }
    if (!first) {
      payload += ',';
    }
    payload += "{\"id\":" + String(evt.id) + ",\"text\":\"" + escapeJson(evt.text) + "\"}";
    first = false;
    lastId = evt.id;
  }
  payload += "],\"lastId\":" + String(lastId) + "}";

  server.send(200, "application/json", payload);
}

// --- API: смена канала ---
void handleChannelChange() {
  if (!server.hasArg("channel")) {
    server.send(400, "application/json", "{\"error\":\"Не указан параметр channel\"}");
    return;
  }
  int channel = server.arg("channel").toInt();
  if (channel < 0 || channel >= kHomeBankSize) {
    server.send(400, "application/json", "{\"error\":\"Недопустимый канал\"}");
    return;
  }
  if (applyRadioChannel(static_cast<uint8_t>(channel))) {
    state.channelIndex = static_cast<uint8_t>(channel);
    addEvent(String("Выбран канал HOME #") + String(channel) + ", RX=" + String(state.currentRxFreq, 3) + " МГц, TX=" + String(state.currentTxFreq, 3) + " МГц");
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Не удалось применить канал\"}");
  }
}

// --- API: переключение мощности ---
void handlePowerToggle() {
  bool newHighPower = server.hasArg("high") && server.arg("high") == "1";
  if (applyRadioPower(newHighPower)) {
    state.highPower = newHighPower;
    addEvent(String("Мощность передачи установлена в ") + (newHighPower ? "22 dBm" : "-5 dBm"));
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Ошибка установки мощности\"}");
  }
}

// --- API: изменение длины фиксированного пакета ---
void handleFixedPacketSizeChange() {
  if (!server.hasArg("size")) {
    server.send(400, "application/json", "{\"error\":\"Не указан параметр size\"}");
    return;
  }

  const String raw = server.arg("size");
  char* endPtr = nullptr;
  unsigned long parsed = strtoul(raw.c_str(), &endPtr, 10);
  if (endPtr == raw.c_str() || (endPtr && *endPtr != '\0')) {
    server.send(400, "application/json", "{\"error\":\"Размер должен быть целым числом\"}");
    return;
  }

  size_t requested = static_cast<size_t>(parsed);
  bool allowed = std::find(kFixedPacketOptions.begin(), kFixedPacketOptions.end(), requested) != kFixedPacketOptions.end();
  if (!allowed) {
    server.send(400, "application/json", "{\"error\":\"Недопустимый размер пакета\"}");
    return;
  }

  state.fixedPacketSize = requested;
  addEvent(String("Размер фиксированного пакета установлен: ") + String(static_cast<unsigned long>(requested)) + " байт");

  String response = String("{\"ok\":true,\"size\":") + String(static_cast<unsigned long>(requested)) + "}";
  server.send(200, "application/json", response);
}

// --- API: отправка фиксированного пакета выбранной длины ---
void handleSendFixedPacket() {
  const uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF}; // базовый шаблон содержимого пакета
  std::vector<uint8_t> data(state.fixedPacketSize, 0);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = pattern[i % (sizeof(pattern) / sizeof(pattern[0]))];
  }
  if (!data.empty()) {
    data.back() = 0x01; // завершаем пакет маркером для удобства проверки
  }
  if (sendBuffer(data, String("Фиксированный пакет (") + String(static_cast<unsigned long>(data.size())) + " байт")) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Отправка не удалась\"}");
  }
}

// --- API: отправка полного пакета случайных чередующихся байт ---
void handleSendRandomPacket() {
  std::vector<uint8_t> data(kFullPacketSize, 0);
  uint8_t evenByte = static_cast<uint8_t>(random(0, 256));
  uint8_t oddByte = static_cast<uint8_t>(random(0, 256));
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = (i % 2 == 0) ? evenByte : oddByte;
  }
  if (sendBuffer(data, "Полный пакет с чередованием случайных байт")) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Отправка не удалась\"}");
  }
}

// --- API: отправка пользовательского сообщения ---
void handleSendCustom() {
  if (!server.hasArg("payload")) {
    server.send(400, "application/json", "{\"error\":\"Не передано поле payload\"}");
    return;
  }
  bool ok = false;
  String errorText;
  std::vector<uint8_t> data = parseHexString(server.arg("payload"), ok, errorText);
  if (!ok) {
    server.send(400, "application/json", String("{\"error\":\"") + errorText + "\"}");
    return;
  }
  if (data.empty()) {
    server.send(400, "application/json", "{\"error\":\"Нужно указать хотя бы один байт\"}");
    return;
  }
  if (sendBuffer(data, String("Пользовательский пакет (" + String(data.size()) + " байт)"))) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Отправка не удалась\"}");
  }
}

// --- API: ответ на неизвестный маршрут ---
void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Маршрут не найден\"}");
}

// --- Построение HTML главной страницы ---
String buildIndexHtml() {
  String html;
  html.reserve(8192);
  html += F("<!DOCTYPE html><html lang=\"ru\"><head><meta charset=\"UTF-8\"><title>Lotest</title><style>");
  html += F("body{font-family:Arial,sans-serif;margin:0;padding:0;background:#10141a;color:#f0f0f0;}");
  html += F("header{background:#1f2a38;padding:16px 24px;font-size:20px;font-weight:bold;}");
  html += F("main{padding:24px;display:flex;gap:24px;flex-wrap:wrap;}");
  html += F("section{background:#1b2330;border-radius:12px;padding:16px;box-shadow:0 4px 12px rgba(0,0,0,0.3);flex:1 1 320px;}");
  html += F("button,select,input[type=text]{background:#2b3648;border:none;border-radius:8px;color:#f0f0f0;padding:8px 12px;margin:4px 0;}");
  html += F("button{cursor:pointer;transition:background 0.2s;}");
  html += F("button:hover{background:#3b4860;}");
  html += F("label{display:block;margin-top:8px;}");
  html += F("#log{height:360px;overflow-y:auto;background:#0f1722;border-radius:8px;padding:12px;font-family:monospace;white-space:pre-wrap;}");
  html += F(".message{margin-bottom:8px;padding-bottom:8px;border-bottom:1px solid rgba(255,255,255,0.1);}");
  html += F(".controls button{width:100%;margin-top:8px;}");
  html += F(".status{font-size:14px;color:#9fb1d1;margin-top:8px;}");
  html += F("</style></head><body><header>Lotest — тестирование LoRa + веб</header><main>");

  html += F("<section><h2>Управление радиомодулем</h2><label>Канал банка HOME:</label><select id=\"channel\">");
  html += buildChannelOptions(state.channelIndex);
  html += F("</select><label><input type=\"checkbox\" id=\"power\"> Мощность 22 dBm (выкл — -5 dBm)</label>");
  html += F("<label>Длина фиксированного пакета:</label><select id=\"fixedSize\">");
  html += buildFixedPacketOptions(state.fixedPacketSize);
  html += F("</select>");
  html += F("<div class=\"status\" id=\"status\"></div><div class=\"controls\">");
  html += F("<button id=\"sendFixed\">Отправить фиксированный пакет</button>");
  html += F("<button id=\"sendRandom\">Отправить полный пакет</button>");
  html += F("<label>Пользовательский пакет (HEX, например \'DE AD BE EF\'):</label><input type=\"text\" id=\"custom\" placeholder=\"Введите байты через пробел\">");
  html += F("<button id=\"sendCustom\">Отправить пользовательский пакет</button>");
  html += F("</div></section>");

  html += F("<section><h2>Журнал событий</h2><div id=\"log\"></div></section></main><script>");
  html += F("const logEl=document.getElementById('log');const channelSel=document.getElementById('channel');const powerCb=document.getElementById('power');const fixedSizeSel=document.getElementById('fixedSize');const statusEl=document.getElementById('status');let lastId=0;");
  html += F("function appendLog(text){const div=document.createElement('div');div.className='message';div.textContent=text;logEl.appendChild(div);logEl.scrollTop=logEl.scrollHeight;}");
  html += F("async function refreshLog(){try{const resp=await fetch(`/api/log?after=${lastId}`);if(!resp.ok)return;const data=await resp.json();data.events.forEach(evt=>{appendLog(evt.text);lastId=evt.id;});}catch(e){console.error(e);}}");
  html += F("async function postForm(url,body){const resp=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)});if(!resp.ok){const err=await resp.json().catch(()=>({error:'Неизвестная ошибка'}));throw new Error(err.error||'Ошибка');}}");
  html += F("channelSel.addEventListener('change',async()=>{try{await postForm('/api/channel',{channel:channelSel.value});statusEl.textContent='Канал применён';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("powerCb.addEventListener('change',async()=>{try{await postForm('/api/power',{high:powerCb.checked?'1':'0'});statusEl.textContent='Мощность обновлена';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("fixedSizeSel.addEventListener('change',async()=>{try{await postForm('/api/fixed-size',{size:fixedSizeSel.value});statusEl.textContent='Размер фиксированного пакета обновлён';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("document.getElementById('sendFixed').addEventListener('click',async()=>{try{await postForm('/api/send/fixed',{});statusEl.textContent='Фиксированный пакет ('+fixedSizeSel.value+' байт) отправлен';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("document.getElementById('sendRandom').addEventListener('click',async()=>{try{await postForm('/api/send/random',{});statusEl.textContent='Полный пакет отправлен';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("document.getElementById('sendCustom').addEventListener('click',async()=>{const payload=document.getElementById('custom').value.trim();try{await postForm('/api/send/custom',{payload});statusEl.textContent='Пользовательский пакет отправлен';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("setInterval(refreshLog,1500);refreshLog();");
  html += F("</script></body></html>");
  return html;
}

// --- Формирование HTML-опций для списка каналов ---
String buildChannelOptions(uint8_t selected) {
  String html;
  for (uint8_t i = 0; i < kHomeBankSize; ++i) {
    html += "<option value=\"" + String(i) + "\"";
    if (i == selected) {
      html += " selected";
    }
    html += ">#" + String(i) + " — RX " + String(frequency_tables::RX_HOME[i], 3) + " МГц / TX " + String(frequency_tables::TX_HOME[i], 3) + " МГц</option>";
  }
  return html;
}

// --- Формирование HTML-опций для выбора длины фиксированного пакета ---
String buildFixedPacketOptions(size_t current) {
  String html;
  for (size_t value : kFixedPacketOptions) {
    html += "<option value=\"" + String(static_cast<unsigned long>(value)) + "\"";
    if (value == current) {
      html += " selected";
    }
    html += ">" + String(static_cast<unsigned long>(value)) + " байт</option>";
  }
  return html;
}

// --- Экранирование строки для JSON ---
String escapeJson(const String& value) {
  String out;
  out.reserve(value.length() + 4);
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<uint8_t>(c) < 32) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

// --- Применение радиоканала ---
bool applyRadioChannel(uint8_t newIndex) {
  if (newIndex >= kHomeBankSize) {
    return false;
  }
  float rx = frequency_tables::RX_HOME[newIndex];
  float tx = frequency_tables::TX_HOME[newIndex];

  int16_t rxState = radio.setFrequency(rx);
  if (rxState != RADIOLIB_ERR_NONE) {
    logRadioError("setFrequency(RX)", rxState);
    return false;
  }

  state.currentRxFreq = rx;
  state.currentTxFreq = tx;

  return ensureReceiveMode();
}

// --- Настройка мощности передачи ---
bool applyRadioPower(bool highPower) {
  int8_t targetDbm = highPower ? 22 : -5;
  int16_t result = radio.setOutputPower(targetDbm);
  if (result != RADIOLIB_ERR_NONE) {
    logRadioError("setOutputPower", result);
    return false;
  }
  return true;
}

// --- Гарантируем, что радио ожидает приём ---
bool ensureReceiveMode() {
  int16_t stateCode = radio.startReceive();
  if (stateCode != RADIOLIB_ERR_NONE) {
    logRadioError("startReceive", stateCode);
    return false;
  }
  return true;
}

// --- Отправка подготовленного буфера ---
bool sendBuffer(const std::vector<uint8_t>& buffer, const String& context) {
  addEvent(context + ": " + formatByteArray(buffer));

  int16_t freqState = radio.setFrequency(state.currentTxFreq);
  if (freqState != RADIOLIB_ERR_NONE) {
    logRadioError("setFrequency(TX)", freqState);
    return false;
  }

  int16_t result = radio.transmit(const_cast<uint8_t*>(buffer.data()), buffer.size());
  if (result != RADIOLIB_ERR_NONE) {
    logRadioError("transmit", result);
    radio.setFrequency(state.currentRxFreq);
    ensureReceiveMode();
    return false;
  }

  int16_t backState = radio.setFrequency(state.currentRxFreq);
  if (backState != RADIOLIB_ERR_NONE) {
    logRadioError("setFrequency(RX restore)", backState);
    return false;
  }

  return ensureReceiveMode();
}

// --- Разбор пользовательского ввода (HEX) ---
std::vector<uint8_t> parseHexString(const String& raw, bool& ok, String& errorMessage) {
  std::vector<uint8_t> result;
  ok = false;
  errorMessage = "";

  String sanitized;
  sanitized.reserve(raw.length());
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw.charAt(i);
    if (isxdigit(static_cast<unsigned char>(c))) {
      sanitized += static_cast<char>(toupper(c));
    } else if (c == ' ' || c == ',' || c == '-') {
      sanitized += ' ';
    } else {
      errorMessage = "Допустимы только шестнадцатеричные символы и пробелы";
      return result;
    }
  }

  std::istringstream iss(sanitized.c_str());
  std::string token;
  while (iss >> token) {
    if (token.length() > 2) {
      errorMessage = "Каждый байт должен состоять из 1-2 HEX символов";
      return result;
    }
    uint8_t value = static_cast<uint8_t>(strtoul(token.c_str(), nullptr, 16));
    result.push_back(value);
    if (result.size() > kFullPacketSize) {
      errorMessage = "Превышен максимальный размер пакета";
      return {};
    }
  }

  if (result.empty()) {
    errorMessage = "Введите хотя бы один байт";
    return result;
  }

  ok = true;
  return result;
}

// --- Форматирование массива байт для вывода ---
String formatByteArray(const std::vector<uint8_t>& data) {
  String out;
  out.reserve(data.size() * 5);
  out += '[';
  for (size_t i = 0; i < data.size(); ++i) {
    if (i > 0) {
      out += ' ';
    }
    char buf[5];
    std::snprintf(buf, sizeof(buf), "0x%02X", data[i]);
    out += buf;
  }
  out += ']';
  return out;
}

// --- Вывод кодов ошибок RadioLib в лог ---
void logRadioError(const String& context, int16_t code) {
  addEvent(String("RadioLib ошибка ") + context + " => " + String(code));
}
