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
#include <cstdio>
#include <chrono>
#include <type_traits>
#include <utility>
#if !defined(ARDUINO)
#include <thread>
#endif

#include "libs/radio/lora_radiolib_settings.h"     // дефолтные настройки драйвера SX1262

namespace {
constexpr auto kRadioDefaults = LoRaRadioLibSettings::DEFAULT_OPTIONS; // Статический набор настроек RadioLib
constexpr size_t kImplicitPayloadLength = static_cast<size_t>(kRadioDefaults.implicitPayloadLength); // размер implicit-пакета
constexpr float kDefaultBandwidthKhz = kRadioDefaults.bandwidthKhz;    // полоса пропускания LoRa по умолчанию
constexpr uint8_t kDefaultSpreadingFactor = kRadioDefaults.spreadingFactor; // фактор расширения SF
constexpr uint8_t kDefaultCodingRate = kRadioDefaults.codingRateDenom;      // делитель коэффициента кодирования CR
constexpr int8_t kLowPowerDbm = kRadioDefaults.lowPowerDbm;                 // низкий уровень мощности
constexpr int8_t kHighPowerDbm = kRadioDefaults.highPowerDbm;               // высокий уровень мощности
} // namespace

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

// --- Вспомогательная обёртка для доступа к protected-методам SX1262 ---
class PublicSX1262 : public SX1262 {
 public:
  using SX1262::SX1262;             // пробрасываем конструктор базового класса
  using SX1262::clearIrqStatus;     // делаем очистку IRQ-статуса публичной
  using SX1262::getIrqStatus;       // раскрываем оба варианта чтения IRQ-статуса
};

// --- Глобальные объекты периферии ---
SPIClass radioSPI(VSPI);                          // аппаратный SPI-порт, обслуживающий радиомодуль
PublicSX1262 radio = new Module(5, 26, 27, 25, radioSPI); // используем VSPI сразу при создании объекта Module
WebServer server(80);                             // встроенный HTTP-сервер ESP32

// --- Константы проекта ---
constexpr uint8_t kHomeBankSize = static_cast<uint8_t>(frequency_tables::HOME_BANK_SIZE); // число каналов банка HOME
constexpr size_t kMaxEventHistory = 120;          // ограничение истории событий для веб-чата
constexpr size_t kFullPacketSize = 245;           // максимальная длина пакета SX1262
constexpr size_t kFixedFrameSize = 8;             // фиксированная длина кадра LoRa
constexpr size_t kFramePayloadSize = kFixedFrameSize - 1; // полезная часть кадра без управляющего байта
constexpr uint8_t kSingleFrameMarker = 0;         // метка одиночного кадра
constexpr uint8_t kFinalFrameMarker = 1;          // метка завершающего кадра последовательности
constexpr uint8_t kFirstChunkMarker = 2;          // минимальный маркер для кусочных пакетов
constexpr uint8_t kMaxChunkMarker = 253;          // максимальный маркер для кусочных пакетов
constexpr unsigned long kInterFrameDelayMs = 150;  // пауза между кадрами
constexpr size_t kLongPacketSize = 124;           // длина длинного пакета с буквами A-Z
constexpr const char* kIncomingColor = "#5CE16A"; // цвет отображения принятых сообщений

// --- Структура, описывающая событие в веб-чате ---
struct ChatEvent {
  unsigned long id = 0;   // уникальный идентификатор сообщения
  String text;            // сам текст события
  String color;           // цвет текста в веб-интерфейсе
  bool visible = true;    // признак отображения события в веб-журнале
};

// --- Текущее состояние приложения ---
struct AppState {
  uint8_t channelIndex = 0;            // выбранный канал банка HOME
  bool highPower = false;              // признак использования мощности 22 dBm (иначе -5 dBm)
  bool useSf5 = false;                 // признак использования SF5 (false => SF7)
  float currentRxFreq = frequency_tables::RX_HOME[0]; // текущая частота приёма
  float currentTxFreq = frequency_tables::TX_HOME[0]; // текущая частота передачи
  unsigned long nextEventId = 1;       // счётчик идентификаторов для событий
  std::vector<ChatEvent> events;       // журнал событий для веб-интерфейса
  std::vector<uint8_t> rxAssembly;     // буфер сборки принятого сообщения из частей
  bool assemblingMessage = false;      // активен ли режим сборки многочастного сообщения
  uint8_t expectedChunkMarker = kFirstChunkMarker; // ожидаемый маркер следующего куска
} state;

// --- Флаги приёма LoRa ---
volatile bool packetReceivedFlag = false;   // устанавливается обработчиком DIO1 при приёме
volatile bool packetProcessingEnabled = true; // защита от повторного входа в обработчик
volatile bool irqStatusPending = false;     // есть ли необработанные IRQ-флаги SX1262

// --- Совместимость с различными версиями API RadioLib ---
namespace radiolib_compat {

// Проверяем доступность старого API getIrqFlags()/clearIrqFlags()
template <typename T, typename = void>
struct HasIrqFlagsApi : std::false_type {};

template <typename T>
struct HasIrqFlagsApi<
    T,
    std::void_t<decltype(std::declval<T&>().getIrqFlags()),
                decltype(std::declval<T&>().clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL))>>
    : std::true_type {};

// Проверяем наличие современного варианта getIrqStatus() без аргументов
template <typename T, typename = void>
struct HasZeroArgIrqStatusApi : std::false_type {};

template <typename T>
struct HasZeroArgIrqStatusApi<
    T,
    std::void_t<decltype(std::declval<T&>().getIrqStatus())>> : std::true_type {};

// Проверяем наличие варианта getIrqStatus(uint16_t*)
template <typename T, typename = void>
struct HasPointerIrqStatusApi : std::false_type {};

template <typename T>
struct HasPointerIrqStatusApi<
    T,
    std::void_t<decltype(
        std::declval<T&>().getIrqStatus(static_cast<uint16_t*>(nullptr)))>>
    : std::true_type {};

// Унифицированное чтение IRQ-флагов SX1262
template <typename Radio>
uint32_t readIrqFlags(Radio& radio) {
  if constexpr (HasIrqFlagsApi<Radio>::value) {
    return radio.getIrqFlags();                                  // старый API RadioLib
  } else if constexpr (HasZeroArgIrqStatusApi<Radio>::value) {
    return static_cast<uint32_t>(radio.getIrqStatus());          // новый API без аргументов
  } else if constexpr (HasPointerIrqStatusApi<Radio>::value) {
    uint16_t legacyFlags = 0;
    const int16_t state = radio.getIrqStatus(&legacyFlags);      // fallback через указатель
    return (state == RADIOLIB_ERR_NONE) ? legacyFlags : 0U;
  } else {
    return 0U;                                                   // неподдерживаемый вариант API
  }
}

// Унифицированная очистка IRQ-флагов SX1262
template <typename Radio>
int16_t clearIrqFlags(Radio& radio, uint32_t mask) {
  if constexpr (HasIrqFlagsApi<Radio>::value) {
    return radio.clearIrqFlags(mask);                            // старый API
  } else {
    (void)mask;
    return radio.clearIrqStatus();                               // современный API без аргументов
  }
}

} // namespace radiolib_compat

// --- Вспомогательные функции объявления ---
void IRAM_ATTR onRadioDio1Rise();
String formatSx1262IrqFlags(uint32_t flags);
void addEvent(const String& message, const String& color = String(), bool visible = true);
void appendEventBuffer(const String& message, unsigned long id, const String& color = String(), bool visible = true);
void handleRoot();
void handleLog();
void handleChannelChange();
void handlePowerToggle();
void handleSendLongPacket();
void handleSendRandomPacket();
void handleSendCustom();
void handleNotFound();
String buildIndexHtml();
String buildChannelOptions(uint8_t selected);
String escapeJson(const String& value);
String makeAccessPointSsid();
bool applyRadioChannel(uint8_t newIndex);
bool applyRadioPower(bool highPower);
bool applySpreadingFactor(bool useSf5);
bool ensureReceiveMode();
bool sendPayload(const std::vector<uint8_t>& payload, const String& context);
bool transmitFrame(const std::array<uint8_t, kFixedFrameSize>& frame, size_t index, size_t total);
std::vector<std::array<uint8_t, kFixedFrameSize>> splitPayloadIntoFrames(const std::vector<uint8_t>& payload);
void processIncomingFrame(const std::vector<uint8_t>& frame);
void resetReceiveAssembly();
void appendReceiveChunk(const std::vector<uint8_t>& chunk, bool finalChunk);
String formatByteArray(const std::vector<uint8_t>& data);
String formatTextPayload(const std::vector<uint8_t>& data);
String describeFrameMarker(uint8_t marker);
void logReceivedMessage(const std::vector<uint8_t>& payload);
void logRadioError(const String& context, int16_t code);
void handleSpreadingFactorToggle();
void waitInterFrameDelay();
void trimTrailingZeros(std::vector<uint8_t>& buffer);

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

  // Стартуем радиомодуль и применяем параметры, аналогичные основной прошивке
  const float initialRxFreq = frequency_tables::RX_HOME[state.channelIndex];
  const uint8_t syncWord = static_cast<uint8_t>(kRadioDefaults.syncWord & 0xFFU);
  const float tcxoVoltage = (kRadioDefaults.useDio3ForTcxo && kRadioDefaults.tcxoVoltage > 0.0f)
                               ? kRadioDefaults.tcxoVoltage
                               : 0.0f;
  const int8_t initialPowerDbm = state.highPower ? kHighPowerDbm : kLowPowerDbm; // стартовая мощность
  int16_t beginState = radio.begin(initialRxFreq,
                                   kDefaultBandwidthKhz,
                                   kDefaultSpreadingFactor,
                                   kDefaultCodingRate,
                                   syncWord,
                                   initialPowerDbm,
                                   kRadioDefaults.preambleLength,
                                   tcxoVoltage,
                                   kRadioDefaults.enableRegulatorDCDC);
  if (beginState != RADIOLIB_ERR_NONE) {
    logRadioError("radio.begin", beginState);
  } else {
    addEvent("Радиомодуль успешно инициализирован");

    // Применяем настройки LoRa согласно требованиям задачи
    state.useSf5 = (kDefaultSpreadingFactor == 5);
    applySpreadingFactor(state.useSf5);
    radio.setBandwidth(kDefaultBandwidthKhz);
    radio.setCodingRate(kDefaultCodingRate);

    radio.setDio2AsRfSwitch(kRadioDefaults.useDio2AsRfSwitch);
    if (kRadioDefaults.useDio3ForTcxo && kRadioDefaults.tcxoVoltage > 0.0f) {
      radio.setTCXO(kRadioDefaults.tcxoVoltage); // включаем внешний TCXO с указанным напряжением
    }
    if (kRadioDefaults.implicitHeader) {
      radio.implicitHeader(kImplicitPayloadLength);
    } else {
      radio.explicitHeader();
    }
    radio.setCRC(kRadioDefaults.enableCrc ? 2 : 0);          // длина CRC в байтах: 2 либо 0
    radio.invertIQ(kRadioDefaults.invertIq);                 // включаем или выключаем инверсию IQ
    radio.setPreambleLength(kRadioDefaults.preambleLength);
    radio.setRxBoostedGainMode(kRadioDefaults.rxBoostedGain); // режим усиленного приёма SX1262
    radio.setSyncWord(kRadioDefaults.syncWord);

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
  server.on("/api/send/five", HTTP_POST, handleSendLongPacket); // совместимость с прежним маршрутом
  server.on("/api/send/fixed", HTTP_POST, handleSendLongPacket);
  server.on("/api/send/long", HTTP_POST, handleSendLongPacket);
  server.on("/api/send/random", HTTP_POST, handleSendRandomPacket);
  server.on("/api/send/custom", HTTP_POST, handleSendCustom);
  server.on("/api/sf", HTTP_POST, handleSpreadingFactorToggle);
  server.onNotFound(handleNotFound);
  server.begin();
  addEvent("HTTP-сервер запущен на порту 80");
}

// --- Основной цикл ---
void loop() {
  server.handleClient();

  bool processIrq = false;                    // требуется ли перенос IRQ-статуса в основной поток
#if defined(ARDUINO)
  noInterrupts();                              // защищаем флаг от изменения в ISR
#endif
  if (irqStatusPending) {                     // фиксируем запрос на логирование IRQ
    irqStatusPending = false;
    processIrq = true;
  }
#if defined(ARDUINO)
  interrupts();                               // возвращаем обработку прерываний
#endif

  if (processIrq) {
    const uint32_t irqFlags = radiolib_compat::readIrqFlags(radio);   // считываем активные флаги независимо от версии API
    radiolib_compat::clearIrqFlags(radio, RADIOLIB_SX126X_IRQ_ALL);   // сбрасываем регистр IRQ в доступном варианте
    addEvent(formatSx1262IrqFlags(irqFlags));                         // публикуем расшифровку событий
  }

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
      processIncomingFrame(buffer);
    } else {
      logRadioError("readData", stateCode);
    }

    ensureReceiveMode();
    packetProcessingEnabled = true;
  }
}

// --- Обработчик линии DIO1 радиомодуля ---
void IRAM_ATTR onRadioDio1Rise() {
  irqStatusPending = true;                   // отмечаем необходимость чтения IRQ-статуса
  if (!packetProcessingEnabled) {
    return;
  }
  packetReceivedFlag = true;
}

// --- Формирование человекочитаемого описания IRQ SX1262 ---
String formatSx1262IrqFlags(uint32_t flags) {
  const uint32_t effectiveMask = flags & 0xFFFFU;                  // интересуют только младшие 16 бит
  if (effectiveMask == RADIOLIB_SX126X_IRQ_NONE) {
    return F("SX1262 IRQ: флаги отсутствуют (маска=0x0000)");
  }

  struct IrqEntry {
    uint32_t mask;                                                  // битовая маска события
    const char* name;                                               // краткое имя IRQ
    const char* description;                                        // пояснение для оператора
  };

  static const IrqEntry kIrqMap[] = {
      {RADIOLIB_SX126X_IRQ_TX_DONE, "IRQ_TX_DONE", "передача завершена"},
      {RADIOLIB_SX126X_IRQ_RX_DONE, "IRQ_RX_DONE", "приём завершён"},
      {RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED, "IRQ_PREAMBLE_DETECTED", "обнаружена преамбула"},
      {RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID, "IRQ_SYNCWORD_VALID", "сошёлся sync word"},
      {RADIOLIB_SX126X_IRQ_HEADER_VALID, "IRQ_HEADER_VALID", "валидный заголовок"},
      {RADIOLIB_SX126X_IRQ_HEADER_ERR, "IRQ_HEADER_ERR", "ошибка заголовка"},
      {RADIOLIB_SX126X_IRQ_CRC_ERR, "IRQ_CRC_ERR", "ошибка CRC"},
#if defined(RADIOLIB_IRQ_RX_TIMEOUT)
      {RADIOLIB_IRQ_RX_TIMEOUT, "IRQ_RX_TIMEOUT", "тайм-аут приёма"},
#endif
#if defined(RADIOLIB_IRQ_TX_TIMEOUT)
      {RADIOLIB_IRQ_TX_TIMEOUT, "IRQ_TX_TIMEOUT", "тайм-аут передачи"},
#endif
#if !defined(RADIOLIB_IRQ_RX_TIMEOUT) && !defined(RADIOLIB_IRQ_TX_TIMEOUT)
      {RADIOLIB_SX126X_IRQ_TIMEOUT, "IRQ_RX_TX_TIMEOUT", "тайм-аут RX/TX"},
#endif
      {RADIOLIB_SX126X_IRQ_CAD_DONE, "IRQ_CAD_DONE", "CAD завершён"},
      {RADIOLIB_SX126X_IRQ_CAD_DETECTED, "IRQ_CAD_DETECTED", "CAD обнаружил передачу"},
#ifdef RADIOLIB_SX126X_IRQ_LR_FHSS_HOP
      {RADIOLIB_SX126X_IRQ_LR_FHSS_HOP, "IRQ_LR_FHSS_HOP", "LR-FHSS hop"},
#endif
  };

  String decoded;
  decoded.reserve(192);                                             // предотвращаем повторные аллокации
  uint32_t knownMask = 0U;                                          // известные биты
  bool first = true;
  for (const auto& entry : kIrqMap) {
    if ((effectiveMask & entry.mask) == 0U) {
      continue;                                                     // пропускаем неактивные события
    }
    if (!first) {
      decoded += F(" | ");
    }
    decoded += entry.name;
    decoded += F(" — ");
    decoded += entry.description;
    first = false;
    knownMask |= entry.mask;
  }

  String result;
  result.reserve(224);
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "SX1262 IRQ=0x%04lX", static_cast<unsigned long>(effectiveMask));
  result += buffer;

  if (decoded.length() > 0) {
    result += F(", расшифровка: [");
    result += decoded;
    result += ']';
  }

  const uint32_t unknownMask = effectiveMask & ~knownMask;
  if (unknownMask != 0U) {
    std::snprintf(buffer, sizeof(buffer), ", неизвестные биты=0x%04lX", static_cast<unsigned long>(unknownMask));
    result += buffer;
  }

  return result;
}

// --- Добавление события в лог ---
void addEvent(const String& message, const String& color, bool visible) {
  appendEventBuffer(message, state.nextEventId++, color, visible);
  Serial.println(message);
}

// --- Вспомогательная функция: дописываем событие и ограничиваем историю ---
void appendEventBuffer(const String& message, unsigned long id, const String& color, bool visible) {
  ChatEvent event;
  event.id = id;
  event.text = message;
  event.color = color;
  event.visible = visible;
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
    lastId = evt.id;
    if (!evt.visible) {
      continue;                                                // пропускаем события, скрытые для веб-лога
    }
    if (!first) {
      payload += ',';
    }
    payload += "{\"id\":" + String(evt.id) + ",\"text\":\"" + escapeJson(evt.text) + "\"";
    if (evt.color.length() > 0) {
      payload += ",\"color\":\"" + escapeJson(evt.color) + "\"";
    }
    payload += "}";
    first = false;
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

// --- API: переключение фактора расширения ---
void handleSpreadingFactorToggle() {
  bool newSf5 = server.hasArg("sf5") && server.arg("sf5") == "1";
  if (applySpreadingFactor(newSf5)) {
    addEvent(String("Фактор расширения установлен: SF") + String(static_cast<unsigned long>(newSf5 ? 5 : kDefaultSpreadingFactor)));
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(500, "application/json", "{\"error\":\"Ошибка установки SF\"}");
  }
}

// --- API: отправка длинного пакета с буквами A-Z ---
void handleSendLongPacket() {
  std::vector<uint8_t> data(kLongPacketSize, 0);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>('A' + (i % 26));
  }
  if (sendPayload(data, String("Длинный пакет (") + String(static_cast<unsigned long>(data.size())) + " байт)")) {
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
  if (sendPayload(data, "Полный пакет с чередованием случайных байт")) {
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
  const String text = server.arg("payload");
  if (text.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Введите сообщение\"}");
    return;
  }
  std::vector<uint8_t> data(text.length());
  std::memcpy(data.data(), text.c_str(), text.length());
  if (sendPayload(data, String("Пользовательский пакет (" + String(data.size()) + " байт)"))) {
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
  html += F("</select>");
  html += "<label><input type='checkbox' id='power'";
  if (state.highPower) {
    html += " checked";
  }
  html += "> Мощность 22 dBm (выкл — -5 dBm)</label>";
  html += "<label><input type='checkbox' id='sf5'";
  if (state.useSf5) {
    html += " checked";
  }
  html += "> Фактор расширения SF5 (выкл — SF7)</label>";
  html += F("<div class=\"status\" id=\"status\"></div><div class=\"controls\">");
  html += F("<button id=\"sendLong\">Отправить длинный пакет 124 байта</button>");
  html += F("<button id=\"sendRandom\">Отправить полный пакет</button>");
  html += F("<label>Пользовательский пакет (текст):</label><input type=\"text\" id=\"custom\" placeholder=\"Введите сообщение\">");
  html += F("<button id=\"sendCustom\">Отправить пользовательский пакет</button>");
  html += F("</div></section>");

  html += F("<section><h2>Журнал событий</h2><div id=\"log\"></div></section></main><script>");
  // Используем максимально совместимый JavaScript без современных конструкций, чтобы UI работал в старых браузерах.
  html += F("var logEl=document.getElementById('log');var channelSel=document.getElementById('channel');var powerCb=document.getElementById('power');var sfCb=document.getElementById('sf5');var statusEl=document.getElementById('status');var lastId=0;");
  html += F("function appendLog(entry){var div=document.createElement('div');div.className='message';div.textContent=entry.text||'';if(entry.color){div.style.color=entry.color;}logEl.appendChild(div);logEl.scrollTop=logEl.scrollHeight;}");
  html += F("function encodeForm(body){var pairs=[];for(var key in body){if(Object.prototype.hasOwnProperty.call(body,key)){pairs.push(encodeURIComponent(key)+'='+encodeURIComponent(body[key]));}}return pairs.join('&');}");
  html += F("function postForm(url,body,onOk,onError){var xhr=new XMLHttpRequest();xhr.open('POST',url,true);xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded');xhr.onreadystatechange=function(){if(xhr.readyState!==4){return;}if(xhr.status>=200&&xhr.status<300){if(onOk){onOk();}}else{var message='Ошибка';try{var resp=JSON.parse(xhr.responseText||'{}');if(resp&&resp.error){message=resp.error;}}catch(err){}if(onError){onError(message);}else{console.error(message);}}};xhr.send(encodeForm(body||{}));}");
  html += F("function refreshLog(){var xhr=new XMLHttpRequest();xhr.open('GET','/api/log?after='+encodeURIComponent(lastId),true);xhr.onreadystatechange=function(){if(xhr.readyState!==4){return;}if(xhr.status>=200&&xhr.status<300){try{var data=JSON.parse(xhr.responseText||'{}');if(data&&data.events){for(var i=0;i<data.events.length;i++){appendLog(data.events[i]);if(data.events[i].id){lastId=data.events[i].id;}}}}catch(err){console.error(err);}}};xhr.send();}");
  html += F("function handleError(message){statusEl.textContent=message||'Ошибка';}");
  html += F("channelSel.addEventListener('change',function(){postForm('/api/channel',{channel:channelSel.value},function(){statusEl.textContent='Канал применён';refreshLog();},handleError);});");
  html += F("powerCb.addEventListener('change',function(){postForm('/api/power',{high:powerCb.checked?'1':'0'},function(){statusEl.textContent='Мощность обновлена';refreshLog();},handleError);});");
  html += F("sfCb.addEventListener('change',function(){postForm('/api/sf',{sf5:sfCb.checked?'1':'0'},function(){statusEl.textContent='Фактор расширения обновлён';refreshLog();},handleError);});");
  html += F("document.getElementById('sendLong').addEventListener('click',function(){postForm('/api/send/long',{},function(){statusEl.textContent='Длинный пакет отправлен';refreshLog();},handleError);});");
  html += F("document.getElementById('sendRandom').addEventListener('click',function(){postForm('/api/send/random',{},function(){statusEl.textContent='Полный пакет отправлен';refreshLog();},handleError);});");
  html += F("var customInput=document.getElementById('custom');");
  html += F("function sendCustom(){var payload=customInput.value||'';if(!payload.replace(/\\s+/g,'').length){statusEl.textContent='Введите сообщение';return;}postForm('/api/send/custom',{payload:payload},function(){statusEl.textContent='Пользовательский пакет отправлен';refreshLog();},handleError);}");
  html += F("document.getElementById('sendCustom').addEventListener('click',sendCustom);");
  html += F("customInput.addEventListener('keydown',function(evt){evt=evt||window.event;if(evt.keyCode===13){evt.preventDefault();sendCustom();}});");
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
  int8_t targetDbm = highPower ? kHighPowerDbm : kLowPowerDbm;
  int16_t result = radio.setOutputPower(targetDbm);
  if (result != RADIOLIB_ERR_NONE) {
    logRadioError("setOutputPower", result);
    return false;
  }
  return true;
}

// --- Настройка фактора расширения SF ---
bool applySpreadingFactor(bool useSf5) {
  uint8_t targetSf = useSf5 ? 5 : kDefaultSpreadingFactor;
  int16_t result = radio.setSpreadingFactor(targetSf);
  if (result != RADIOLIB_ERR_NONE) {
    logRadioError("setSpreadingFactor", result);
    return false;
  }
  state.useSf5 = useSf5;
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
bool sendPayload(const std::vector<uint8_t>& payload, const String& context) {
  if (payload.empty()) {
    addEvent(context + ": пустой буфер");
    return false;
  }

  addEvent(context + ": " + formatByteArray(payload) + " | \"" + formatTextPayload(payload) + "\"");

  auto frames = splitPayloadIntoFrames(payload);
  if (frames.empty()) {
    addEvent("Не удалось подготовить кадры для отправки");
    return false;
  }

  if (frames.size() > 1) {
    addEvent(String("Сообщение разбито на ") + String(static_cast<unsigned long>(frames.size())) + " кадров");
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    const auto& frame = frames[i];
    std::vector<uint8_t> frameVec(frame.begin(), frame.end());
    addEvent(String("→ Кадр #") + String(static_cast<unsigned long>(i + 1)) + " (" + describeFrameMarker(frame[0]) + "): " + formatByteArray(frameVec));
    if (!transmitFrame(frame, i, frames.size())) {
      return false;
    }
    if (i + 1 < frames.size()) {
      waitInterFrameDelay();
    }
  }

  return true;
}

// --- Непосредственная передача одного кадра ---
bool transmitFrame(const std::array<uint8_t, kFixedFrameSize>& frame, size_t /*index*/, size_t /*total*/) {
  int16_t freqState = radio.setFrequency(state.currentTxFreq);
  if (freqState != RADIOLIB_ERR_NONE) {
    logRadioError("setFrequency(TX)", freqState);
    return false;
  }

  int16_t result = radio.transmit(const_cast<uint8_t*>(frame.data()), kFixedFrameSize);
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

// --- Разбиение сообщения на кадры по 8 байт ---
std::vector<std::array<uint8_t, kFixedFrameSize>> splitPayloadIntoFrames(const std::vector<uint8_t>& payload) {
  std::vector<std::array<uint8_t, kFixedFrameSize>> frames;
  if (payload.empty()) {
    return frames;
  }

  if (payload.size() <= kFramePayloadSize) {
    std::array<uint8_t, kFixedFrameSize> frame{};
    frame[0] = kSingleFrameMarker;
    std::copy(payload.begin(), payload.end(), frame.begin() + 1);
    frames.push_back(frame);
    return frames;
  }

  size_t offset = 0;
  uint8_t marker = kFirstChunkMarker;
  while (offset < payload.size()) {
    std::array<uint8_t, kFixedFrameSize> frame{};
    size_t chunk = std::min(kFramePayloadSize, payload.size() - offset);
    bool last = (offset + chunk) >= payload.size();
    frame[0] = last ? kFinalFrameMarker : marker;
    std::copy_n(payload.begin() + offset, chunk, frame.begin() + 1);
    frames.push_back(frame);
    offset += chunk;
    if (!last && marker < kMaxChunkMarker) {
      ++marker;
    }
  }

  return frames;
}

// --- Пауза между кадрами ---
void waitInterFrameDelay() {
#if defined(ARDUINO)
  delay(kInterFrameDelayMs);
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(kInterFrameDelayMs));
#endif
}

// --- Обрезка завершающих нулей (для последнего кадра) ---
void trimTrailingZeros(std::vector<uint8_t>& buffer) {
  while (!buffer.empty() && buffer.back() == 0) {
    buffer.pop_back();
  }
}

// --- Формирование текстового представления полезной нагрузки ---
String formatTextPayload(const std::vector<uint8_t>& data) {
  String out;
  out.reserve(data.size() + 8);
  for (uint8_t byte : data) {
    if (byte == '\n') {
      out += "\\n";
    } else if (byte == '\r') {
      out += "\\r";
    } else if (byte == '\t') {
      out += "\\t";
    } else if (byte >= 0x20 && byte <= 0x7E) {
      out += static_cast<char>(byte);
    } else {
      char buf[5];
      std::snprintf(buf, sizeof(buf), "\\x%02X", static_cast<unsigned>(byte));
      out += buf;
    }
  }
  return out;
}

// --- Человекочитаемое описание маркера кадра ---
String describeFrameMarker(uint8_t marker) {
  if (marker == kSingleFrameMarker) {
    return F("одиночный");
  }
  if (marker == kFinalFrameMarker) {
    return F("последний");
  }
  if (marker >= kFirstChunkMarker && marker <= kMaxChunkMarker) {
    return String("часть #") + String(static_cast<unsigned long>(marker - 1));
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "маркер 0x%02X", static_cast<unsigned>(marker));
  return String(buf);
}

// --- Добавление части в буфер сборки ---
void appendReceiveChunk(const std::vector<uint8_t>& chunk, bool finalChunk) {
  state.rxAssembly.insert(state.rxAssembly.end(), chunk.begin(), chunk.end());
  if (finalChunk) {
    logReceivedMessage(state.rxAssembly);
    resetReceiveAssembly();
  } else {
    state.assemblingMessage = true;
  }
}

// --- Сброс состояния сборки ---
void resetReceiveAssembly() {
  state.rxAssembly.clear();
  state.assemblingMessage = false;
  state.expectedChunkMarker = kFirstChunkMarker;
}

// --- Логирование принятого сообщения ---
void logReceivedMessage(const std::vector<uint8_t>& payload) {
  addEvent(String("Принято сообщение (") + String(static_cast<unsigned long>(payload.size())) + " байт): " + formatByteArray(payload) + " | \"" + formatTextPayload(payload) + "\"", kIncomingColor);
}

// --- Обработка принятого кадра ---
void processIncomingFrame(const std::vector<uint8_t>& frame) {
  if (frame.empty()) {
    return;
  }

  const uint8_t marker = frame[0];
  const bool showFrameInWebLog = (marker == kSingleFrameMarker);  // отображаем только одиночные кадры

  addEvent(String("Принят кадр ") + describeFrameMarker(marker) + ": " + formatByteArray(frame), String(), showFrameInWebLog);

  std::vector<uint8_t> payload;
  if (frame.size() > 1) {
    payload.assign(frame.begin() + 1, frame.end());
  }

  if (marker == kSingleFrameMarker) {
    trimTrailingZeros(payload);
    resetReceiveAssembly();
    appendReceiveChunk(payload, true);
    return;
  }

  if (marker == kFinalFrameMarker) {
    trimTrailingZeros(payload);
    if (!state.assemblingMessage) {
      resetReceiveAssembly();
    }
    appendReceiveChunk(payload, true);
    return;
  }

  if (!state.assemblingMessage) {
    resetReceiveAssembly();
  } else if (marker != state.expectedChunkMarker) {
    addEvent("Получен неожиданный маркер последовательности, буфер сборки сброшен");
    resetReceiveAssembly();
  }

  appendReceiveChunk(payload, false);
  state.expectedChunkMarker = (marker < kMaxChunkMarker) ? static_cast<uint8_t>(marker + 1) : kMaxChunkMarker;
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
  String message = String("RadioLib ошибка ") + context + " => " + String(code);
  switch (code) {
    case RADIOLIB_ERR_SPI_CMD_FAILED:
      message += " (SPI команда не выполнена — проверьте питание и линии CS/CLK/MISO/MOSI/BUSY)";
      break;
    default:
      break;
  }
  addEvent(message);
}
