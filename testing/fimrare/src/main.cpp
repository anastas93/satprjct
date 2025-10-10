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
#include <map>
#include <deque>
#include <numeric>
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

// --- Глобальные объекты периферии ---
SPIClass radioSPI(VSPI);                          // аппаратный SPI-порт, обслуживающий радиомодуль
SX1262 radio = new Module(5, 26, 27, 25, radioSPI); // используем VSPI сразу при создании объекта Module
WebServer server(80);                             // встроенный HTTP-сервер ESP32

// --- Константы проекта ---
constexpr uint8_t kHomeBankSize = static_cast<uint8_t>(frequency_tables::HOME_BANK_SIZE); // число каналов банка HOME
constexpr size_t kMaxEventHistory = 120;          // ограничение истории событий для веб-чата
constexpr size_t kFullPacketSize = 245;           // максимальная длина пакета SX1262
constexpr size_t kFixedFrameSize = 8;             // фиксированная длина кадра LoRa
constexpr size_t kFramePayloadSize = 5;           // полезная часть кадра согласно спецификации ARQ
constexpr unsigned long kInterFrameDelayMs = 25;  // пауза между кадрами в базовом режиме
constexpr size_t kArqWindowSize = 8;              // размер окна подтверждения ACK
constexpr size_t kBitmapWidth = 16;               // ширина BITMAP16 для запроса переотправки
constexpr uint16_t kBitmapFullMask = (1U << kBitmapWidth) - 1U; // полный набор битов BITMAP16
constexpr uint8_t kInterleaverStep = 4;           // шаг интерливера для борьбы с бурстами
constexpr size_t kHarqDataBlock = 11;             // число DATA-пакетов в HARQ-блоке RS(15,11)
constexpr size_t kHarqParityCount = 4;            // число PAR-пакетов в HARQ-блоке
constexpr size_t kLongPacketSize = 124;           // длина длинного пакета с буквами A-Z
constexpr const char* kIncomingColor = "#5CE16A"; // цвет отображения принятых сообщений

// --- Константы формата кадров Lotest ---
constexpr uint8_t kFrameTypeMask = 0xC0;          // верхние два бита определяют тип кадра
constexpr uint8_t kFrameTypeData = 0x00;          // DATA-кадр
constexpr uint8_t kFrameTypeAck = 0x40;           // ACK-кадр
constexpr uint8_t kFrameTypeParity = 0x80;        // PAR-кадр (HARQ)
constexpr uint8_t kFrameTypeFin = 0xC0;           // FIN-кадр с итоговым CRC

constexpr uint8_t kDataFlagAckRequest = 0x01;     // запрашивать ACK после окна
constexpr uint8_t kDataFlagIsParity = 0x02;       // полезная нагрузка содержит паритетные данные
constexpr uint8_t kDataFlagLengthShift = 2;       // смещение длины полезной нагрузки
constexpr uint8_t kDataFlagLengthMask = 0x3C;     // маска длины полезной нагрузки (0..15 байт)

constexpr uint8_t kAckFlagNeedParity = 0x01;      // получателю требуется PAR-передача

constexpr uint8_t kFinFlagHarqUsed = 0x01;        // в ходе передачи использовался HARQ

struct DataBlock {
  std::array<uint8_t, kFramePayloadSize> bytes{}; // полезная нагрузка DATA-пакета
  uint8_t dataLength = 0;                         // фактическое число информационных байт
};

// --- Структура, описывающая событие в веб-чате ---
struct ChatEvent {
  unsigned long id = 0;   // уникальный идентификатор сообщения
  String text;            // сам текст события
  String color;           // цвет текста в веб-интерфейсе
};

// --- Текущее состояние приложения ---
struct ProtocolConfig {
  bool interleaving = false;           // включён ли интерливинг
  bool harq = false;                   // активен ли HARQ (адаптивный FEC)
  bool phyFec = false;                 // использовать ли фиксированный FEC PHY
  bool payloadCrc8 = false;            // добавлять ли CRC-8 к каждому DATA-пакету
};

struct RxWindowState {
  uint16_t baseSeq = 0;                // база текущего окна подтверждения
  uint16_t receivedMask = 0;           // биты успешно принятых пакетов
  uint8_t windowSize = 0;              // фактическое количество пакетов в окне
  bool active = false;                 // инициализировано ли окно
};

struct RxMessageState {
  bool active = false;                 // идёт ли сборка файла
  uint16_t nextExpectedSeq = 0;        // следующий ожидаемый SEQ
  std::map<uint16_t, DataBlock> pending; // буфер ожидания
  std::vector<uint8_t> buffer;         // текущие собранные данные
  uint16_t declaredLength = 0;         // ожидаемая длина файла из FIN
  uint16_t declaredCrc = 0;            // ожидаемый CRC-16 из FIN
  bool finReceived = false;            // получен ли FIN
};

struct AckNotification {
  bool hasValue = false;               // получен ли ACK от удалённой стороны
  uint16_t baseSeq = 0;                // база окна
  uint16_t missingBitmap = 0;          // биты недостающих пакетов (1 = требуется повтор)
  bool needParity = false;             // требуется ли PAR-передача
  uint8_t reportedWindow = 0;          // размер окна, который видел получатель
};

struct AppState {
  uint8_t channelIndex = 0;            // выбранный канал банка HOME
  bool highPower = false;              // признак использования мощности 22 dBm (иначе -5 dBm)
  bool useSf5 = false;                 // признак использования SF5 (false => SF7)
  float currentRxFreq = frequency_tables::RX_HOME[0]; // текущая частота приёма
  float currentTxFreq = frequency_tables::TX_HOME[0]; // текущая частота передачи
  unsigned long nextEventId = 1;       // счётчик идентификаторов для событий
  std::vector<ChatEvent> events;       // журнал событий для веб-интерфейса
  ProtocolConfig protocol;             // параметры протокола Lotest
  uint16_t nextTxSequence = 0;         // следующий SEQ для передачи DATA
  RxWindowState rxWindow;              // состояние окна приёма
  RxMessageState rxMessage;            // состояние сборки сообщения
  AckNotification pendingAck;          // последнее полученное подтверждение
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
void addEvent(const String& message, const String& color = String());
void appendEventBuffer(const String& message, unsigned long id, const String& color = String());
void handleRoot();
void handleLog();
void handleChannelChange();
void handlePowerToggle();
void handleSendLongPacket();
void handleSendRandomPacket();
void handleSendCustom();
void handleNotFound();
void handleProtocolToggle();
String buildIndexHtml();
String buildChannelOptions(uint8_t selected);
String escapeJson(const String& value);
String makeAccessPointSsid();
bool applyRadioChannel(uint8_t newIndex);
bool applyRadioPower(bool highPower);
bool applySpreadingFactor(bool useSf5);
bool applyPhyFec(bool enable);
bool ensureReceiveMode();
void processRadioEvents();
bool sendPayload(const std::vector<uint8_t>& payload, const String& context);
bool transmitFrame(const std::array<uint8_t, kFixedFrameSize>& frame, const String& context);
void processIncomingFrame(const std::vector<uint8_t>& frame);
void processIncomingDataFrame(const std::vector<uint8_t>& frame);
void processIncomingAckFrame(const std::vector<uint8_t>& frame);
void processIncomingParityFrame(const std::vector<uint8_t>& frame);
void processIncomingFinFrame(const std::vector<uint8_t>& frame);
void prepareAck(uint16_t seq, uint8_t windowSize, bool forceSend);
void sendAck(uint16_t baseSeq, uint16_t missingBitmap, bool needParity, uint8_t windowSize);
void flushPendingData();
bool sendDataWindow(const std::vector<DataBlock>& blocks,
                    size_t offset,
                    uint16_t baseSeq,
                    uint8_t windowSize);
bool waitForAck(uint16_t baseSeq, uint8_t windowSize, uint16_t& missingBitmap, bool& needParity);
void retransmitMissing(const std::vector<DataBlock>& blocks,
                       size_t offset,
                       uint16_t baseSeq,
                       uint16_t missingBitmap);
std::vector<DataBlock> splitPayloadIntoBlocks(const std::vector<uint8_t>& payload,
                                              bool appendCrc8);
std::array<uint8_t, kFixedFrameSize> buildDataFrame(uint16_t seq,
                                                    const DataBlock& block,
                                                    bool ackRequest,
                                                    bool isParity);
std::array<uint8_t, kFixedFrameSize> buildAckFrame(uint16_t baseSeq,
                                                   uint16_t missingBitmap,
                                                   bool needParity,
                                                   uint8_t windowSize);
std::array<uint8_t, kFixedFrameSize> buildFinFrame(uint16_t length,
                                                   uint16_t crc,
                                                   bool harqUsed);
uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t crc = 0xFFFF);
uint8_t crc8Dallas(const uint8_t* data, size_t length);
void resetReceiveState();
void logReceivedMessage(const std::vector<uint8_t>& payload);
void logRadioError(const String& context, int16_t code);
void handleSpreadingFactorToggle();
void waitInterFrameDelay();
String formatByteArray(const std::vector<uint8_t>& data);
String formatTextPayload(const std::vector<uint8_t>& data);

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
  server.on("/api/protocol", HTTP_POST, handleProtocolToggle);
  server.onNotFound(handleNotFound);
  server.begin();
  addEvent("HTTP-сервер запущен на порту 80");
}

// --- Основной цикл ---
void loop() {
  server.handleClient();
  processRadioEvents();
}

// --- Обработка радиособытий вне основного цикла ---
void processRadioEvents() {
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

  if (!packetReceivedFlag) {
    return;
  }

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
void addEvent(const String& message, const String& color) {
  appendEventBuffer(message, state.nextEventId++, color);
  Serial.println(message);
}

// --- Вспомогательная функция: дописываем событие и ограничиваем историю ---
void appendEventBuffer(const String& message, unsigned long id, const String& color) {
  ChatEvent event;
  event.id = id;
  event.text = message;
  event.color = color;
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
    payload += "{\"id\":" + String(evt.id) + ",\"text\":\"" + escapeJson(evt.text) + "\"";
    if (evt.color.length() > 0) {
      payload += ",\"color\":\"" + escapeJson(evt.color) + "\"";
    }
    payload += "}";
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

// --- API: переключение параметров протокола ---
void handleProtocolToggle() {
  bool handled = false;

  if (server.hasArg("interleaving")) {
    bool enabled = server.arg("interleaving") == "1";
    state.protocol.interleaving = enabled;
    addEvent(String("Интерливинг ") + (enabled ? "включён" : "выключен"));
    handled = true;
  }

  if (server.hasArg("harq")) {
    bool enabled = server.arg("harq") == "1";
    state.protocol.harq = enabled;
    addEvent(String("HARQ ") + (enabled ? "включён" : "выключен"));
    handled = true;
  }

  if (server.hasArg("crc8")) {
    bool enabled = server.arg("crc8") == "1";
    state.protocol.payloadCrc8 = enabled;
    addEvent(String("CRC-8 для DATA ") + (enabled ? "включён (payload=4 байта)" : "выключен"));
    handled = true;
  }

  if (server.hasArg("phyfec")) {
    bool enabled = server.arg("phyfec") == "1";
    if (applyPhyFec(enabled)) {
      state.protocol.phyFec = enabled;
      addEvent(String("PHY FEC ") + (enabled ? "включён (CR=4/7)" : "выключен"));
    } else {
      server.send(500, "application/json", "{\"error\":\"Не удалось переключить FEC\"}");
      return;
    }
    handled = true;
  }

  if (!handled) {
    server.send(400, "application/json", "{\"error\":\"Параметры не переданы\"}");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
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
  html += F("fieldset{border:1px solid #2b3648;border-radius:8px;margin-top:12px;padding:12px;}");
  html += F("legend{padding:0 8px;color:#9fb1d1;}");
  html += F("#log{height:360px;overflow-y:auto;background:#0f1722;border-radius:8px;padding:12px;font-family:monospace;white-space:pre-wrap;}");
  html += F(".message{margin-bottom:8px;padding-bottom:8px;border-bottom:1px solid rgba(255,255,255,0.1);}");
  html += F(".controls button{width:100%;margin-top:8px;}");
  html += F(".status{font-size:14px;color:#9fb1d1;margin-top:8px;}");
  html += F("</style></head><body><header>Lotest — тестирование LoRa + веб</header><main>");

  html += F("<section><h2>Управление радиомодулем</h2><label>Канал банка HOME:</label><select id=\"channel\">");
  html += buildChannelOptions(state.channelIndex);
  html += F("</select>");
  html += "<label><input type=\\\"checkbox\\\" id=\\\"power\\\"";
  if (state.highPower) {
    html += " checked";
  }
  html += "> Мощность 22 dBm (выкл — -5 dBm)</label>";
  html += "<label><input type=\\\"checkbox\\\" id=\\\"sf5\\\"";
  if (state.useSf5) {
    html += " checked";
  }
  html += "> Фактор расширения SF5 (выкл — SF7)</label>";
  html += F("<fieldset><legend>Надёжность</legend>");
  html += "<label><input type=\\\"checkbox\\\" id=\\\"interleaving\\\"";
  if (state.protocol.interleaving) {
    html += " checked";
  }
  html += "> Интерливинг (шаг 4)</label>";
  html += "<label><input type=\\\"checkbox\\\" id=\\\"harq\\\"";
  if (state.protocol.harq) {
    html += " checked";
  }
  html += "> HARQ (адаптивный RS(15,11))</label>";
  html += "<label><input type=\\\"checkbox\\\" id=\\\"phyfec\\\"";
  if (state.protocol.phyFec) {
    html += " checked";
  }
  html += "> PHY FEC (LoRa CR=4/7)</label>";
  html += "<label><input type=\\\"checkbox\\\" id=\\\"crc8\\\"";
  if (state.protocol.payloadCrc8) {
    html += " checked";
  }
  html += "> CRC-8 на DATA (payload=4 байта)</label>";
  html += F("</fieldset>");
  html += F("<div class=\"status\" id=\"status\"></div><div class=\"controls\">");
  html += F("<button id=\"sendLong\">Отправить длинный пакет 124 байта</button>");
  html += F("<button id=\"sendRandom\">Отправить полный пакет</button>");
  html += F("<label>Пользовательский пакет (текст):</label><input type=\"text\" id=\"custom\" placeholder=\"Введите сообщение\">");
  html += F("<button id=\"sendCustom\">Отправить пользовательский пакет</button>");
  html += F("</div></section>");

  html += F("<section><h2>Журнал событий</h2><div id=\"log\"></div></section></main><script>");
  html += F("const logEl=document.getElementById('log');const channelSel=document.getElementById('channel');const powerCb=document.getElementById('power');const sfCb=document.getElementById('sf5');const interCb=document.getElementById('interleaving');const harqCb=document.getElementById('harq');const phyFecCb=document.getElementById('phyfec');const crc8Cb=document.getElementById('crc8');const statusEl=document.getElementById('status');let lastId=0;");
  html += F("function appendLog(entry){const div=document.createElement('div');div.className='message';div.textContent=entry.text;if(entry.color){div.style.color=entry.color;}logEl.appendChild(div);logEl.scrollTop=logEl.scrollHeight;}");
  html += F("async function refreshLog(){try{const resp=await fetch(`/api/log?after=${lastId}`);if(!resp.ok)return;const data=await resp.json();data.events.forEach(evt=>{appendLog(evt);lastId=evt.id;});}catch(e){console.error(e);}}");
  html += F("async function postForm(url,body){const resp=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)});if(!resp.ok){const err=await resp.json().catch(()=>({error:'Неизвестная ошибка'}));throw new Error(err.error||'Ошибка');}}");
  html += F("async function updateProtocol(field,value){const payload={};payload[field]=value?'1':'0';try{await postForm('/api/protocol',payload);statusEl.textContent='Настройки протокола обновлены';refreshLog();}catch(e){statusEl.textContent=e.message;}}");
  html += F("channelSel.addEventListener('change',async()=>{try{await postForm('/api/channel',{channel:channelSel.value});statusEl.textContent='Канал применён';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("powerCb.addEventListener('change',async()=>{try{await postForm('/api/power',{high:powerCb.checked?'1':'0'});statusEl.textContent='Мощность обновлена';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("sfCb.addEventListener('change',async()=>{try{await postForm('/api/sf',{sf5:sfCb.checked?'1':'0'});statusEl.textContent='Фактор расширения обновлён';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("interCb.addEventListener('change',()=>{updateProtocol('interleaving',interCb.checked);});");
  html += F("harqCb.addEventListener('change',()=>{updateProtocol('harq',harqCb.checked);});");
  html += F("phyFecCb.addEventListener('change',()=>{updateProtocol('phyfec',phyFecCb.checked);});");
  html += F("crc8Cb.addEventListener('change',()=>{updateProtocol('crc8',crc8Cb.checked);});");
  html += F("document.getElementById('sendLong').addEventListener('click',async()=>{try{await postForm('/api/send/long',{});statusEl.textContent='Длинный пакет отправлен';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("document.getElementById('sendRandom').addEventListener('click',async()=>{try{await postForm('/api/send/random',{});statusEl.textContent='Полный пакет отправлен';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
  html += F("document.getElementById('sendCustom').addEventListener('click',async()=>{const input=document.getElementById('custom');const payload=input.value;if(!payload.trim()){statusEl.textContent='Введите сообщение';return;}try{await postForm('/api/send/custom',{payload});statusEl.textContent='Пользовательский пакет отправлен';refreshLog();}catch(e){statusEl.textContent=e.message;}});");
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

// --- Переключение PHY FEC ---
bool applyPhyFec(bool enable) {
  uint8_t targetCr = enable ? 7 : kDefaultCodingRate;
  int16_t result = radio.setCodingRate(targetCr);
  if (result != RADIOLIB_ERR_NONE) {
    logRadioError("setCodingRate", result);
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
// --- Отправка подготовленного буфера ---
bool sendPayload(const std::vector<uint8_t>& payload, const String& context) {
  if (payload.empty()) {
    addEvent(context + ": пустой буфер");
    return false;
  }

  addEvent(context + ": " + formatByteArray(payload) + " | \"" + formatTextPayload(payload) + "\"");

  const uint16_t crc = crc16Ccitt(payload.data(), payload.size());
  auto blocks = splitPayloadIntoBlocks(payload, state.protocol.payloadCrc8);
  if (blocks.empty()) {
    addEvent("Не удалось подготовить DATA-пакеты для передачи");
    return false;
  }

  size_t totalBlocks = blocks.size();
  size_t offset = 0;
  uint16_t windowBaseSeq = state.nextTxSequence;
  bool harqUsed = false;

  while (offset < totalBlocks) {
    const uint8_t windowSize = static_cast<uint8_t>(std::min(kArqWindowSize, totalBlocks - offset));
    if (!sendDataWindow(blocks, offset, windowBaseSeq, windowSize)) {
      return false;
    }

    uint16_t missingBitmap = 0;
    bool needParity = false;
    if (!waitForAck(windowBaseSeq, windowSize, missingBitmap, needParity)) {
      addEvent("ACK не получен — считаем потерянным всё окно");
      missingBitmap = static_cast<uint16_t>((1U << windowSize) - 1U);
    }

    uint8_t retries = 0;
    while (missingBitmap != 0 && retries < 3) {
      const uint8_t missingCount = static_cast<uint8_t>(__builtin_popcount(missingBitmap));
      addEvent(String("Повторная отправка ") + String(static_cast<unsigned long>(missingCount)) +
               " пакетов по BITMAP16 0x" + String(missingBitmap, 16));
      retransmitMissing(blocks, offset, windowBaseSeq, missingBitmap);
      if (!waitForAck(windowBaseSeq, windowSize, missingBitmap, needParity)) {
        addEvent("ACK после переотправки не пришёл — считаем окно потерянным");
        missingBitmap = static_cast<uint16_t>((1U << windowSize) - 1U);
      }
      ++retries;
    }

    if (missingBitmap != 0) {
      addEvent("Не удалось доставить все DATA-пакеты окна — остановка передачи");
      return false;
    }

    if (needParity) {
      if (state.protocol.harq) {
        harqUsed = true;
        addEvent("Получен запрос HARQ: генерация PAR пока не реализована (TODO)");
      } else {
        addEvent("Получен запрос HARQ, но HARQ отключён");
      }
    }

    offset += windowSize;
    windowBaseSeq = static_cast<uint16_t>(windowBaseSeq + windowSize);
  }

  state.nextTxSequence = static_cast<uint16_t>(state.nextTxSequence + totalBlocks);

  auto finFrame = buildFinFrame(static_cast<uint16_t>(payload.size()), crc, harqUsed);
  if (!transmitFrame(finFrame, F("FIN"))) {
    return false;
  }

  waitInterFrameDelay();
  return true;
}

// --- Передача окна DATA-пакетов ---
bool sendDataWindow(const std::vector<DataBlock>& blocks,
                    size_t offset,
                    uint16_t baseSeq,
                    uint8_t windowSize) {
  std::vector<size_t> order(windowSize);
  std::iota(order.begin(), order.end(), 0U);

  if (state.protocol.interleaving && windowSize > 1) {
    std::vector<size_t> interleaved;
    for (size_t start = 0; start < kInterleaverStep && start < windowSize; ++start) {
      for (size_t pos = start; pos < windowSize; pos += kInterleaverStep) {
        interleaved.push_back(pos);
      }
    }
    auto it = std::find(interleaved.begin(), interleaved.end(), windowSize - 1);
    if (it != interleaved.end()) {
      interleaved.erase(it);
      interleaved.push_back(windowSize - 1);
    }
    order = std::move(interleaved);
  }

  for (size_t idx : order) {
    if (idx >= windowSize) {
      continue;
    }
    const size_t blockIndex = offset + idx;
    if (blockIndex >= blocks.size()) {
      break;
    }
    const bool ackRequest = (idx == windowSize - 1);
    auto frame = buildDataFrame(static_cast<uint16_t>(baseSeq + idx), blocks[blockIndex], ackRequest, false);
    if (!transmitFrame(frame, ackRequest ? F("DATA (ACK)") : F("DATA"))) {
      return false;
    }
    waitInterFrameDelay();
  }

  return true;
}

// --- Ожидание ACK ---
bool waitForAck(uint16_t baseSeq, uint8_t windowSize, uint16_t& missingBitmap, bool& needParity) {
  const unsigned long timeoutMs = 1000;
  const unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    processRadioEvents();
    if (state.pendingAck.hasValue && state.pendingAck.baseSeq == baseSeq) {
      missingBitmap = state.pendingAck.missingBitmap;
      needParity = state.pendingAck.needParity;
      if (state.pendingAck.reportedWindow != windowSize) {
        addEvent(String("ACK сообщил окно ") +
                 String(static_cast<unsigned long>(state.pendingAck.reportedWindow)) +
                 " вместо ожидаемых " + String(windowSize));
      }
      state.pendingAck.hasValue = false;
      return true;
    }
#if defined(ARDUINO)
    delay(5);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
#endif
  }
  return false;
}

// --- Переотправка потерянных пакетов ---
void retransmitMissing(const std::vector<DataBlock>& blocks,
                       size_t offset,
                       uint16_t baseSeq,
                       uint16_t missingBitmap) {
  for (uint8_t bit = 0; bit < kBitmapWidth; ++bit) {
    if ((missingBitmap & (1U << bit)) == 0U) {
      continue;
    }
    const size_t blockIndex = offset + bit;
    if (blockIndex >= blocks.size()) {
      continue;
    }
    auto frame = buildDataFrame(static_cast<uint16_t>(baseSeq + bit), blocks[blockIndex], (bit == 0), false);
    transmitFrame(frame, F("DATA retry"));
    waitInterFrameDelay();
  }
}

// --- Непосредственная передача одного кадра ---
bool transmitFrame(const std::array<uint8_t, kFixedFrameSize>& frame, const String& context) {
  std::vector<uint8_t> bytes(frame.begin(), frame.end());
  addEvent(String("→ ") + context + ": " + formatByteArray(bytes));

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

// --- Построение DATA-кадра ---
std::array<uint8_t, kFixedFrameSize> buildDataFrame(uint16_t seq,
                                                    const DataBlock& block,
                                                    bool ackRequest,
                                                    bool isParity) {
  std::array<uint8_t, kFixedFrameSize> frame{};

  const bool hasCrc = state.protocol.payloadCrc8;
  uint8_t length = block.dataLength;
  if (hasCrc && length < kFramePayloadSize) {
    ++length; // добавляем байт CRC-8
  }
  uint8_t flags = static_cast<uint8_t>((length << kDataFlagLengthShift) & kDataFlagLengthMask);
  if (ackRequest) {
    flags |= kDataFlagAckRequest;
  }
  if (isParity) {
    flags |= kDataFlagIsParity;
  }

  frame[0] = static_cast<uint8_t>(kFrameTypeData | flags);
  frame[1] = static_cast<uint8_t>(seq & 0xFFU);
  frame[2] = static_cast<uint8_t>((seq >> 8) & 0xFFU);
  std::copy(block.bytes.begin(), block.bytes.end(), frame.begin() + 3);
  return frame;
}

// --- Построение ACK-кадра ---
std::array<uint8_t, kFixedFrameSize> buildAckFrame(uint16_t baseSeq,
                                                   uint16_t missingBitmap,
                                                   bool needParity,
                                                   uint8_t windowSize) {
  std::array<uint8_t, kFixedFrameSize> frame{};
  frame[0] = static_cast<uint8_t>(kFrameTypeAck | (needParity ? kAckFlagNeedParity : 0));
  frame[1] = static_cast<uint8_t>(baseSeq & 0xFFU);
  frame[2] = static_cast<uint8_t>((baseSeq >> 8) & 0xFFU);
  frame[3] = static_cast<uint8_t>(missingBitmap & 0xFFU);
  frame[4] = static_cast<uint8_t>((missingBitmap >> 8) & 0xFFU);
  frame[5] = windowSize;
  return frame;
}

// --- Построение FIN-кадра ---
std::array<uint8_t, kFixedFrameSize> buildFinFrame(uint16_t length,
                                                   uint16_t crc,
                                                   bool harqUsed) {
  std::array<uint8_t, kFixedFrameSize> frame{};
  frame[0] = static_cast<uint8_t>(kFrameTypeFin | (harqUsed ? kFinFlagHarqUsed : 0));
  frame[1] = static_cast<uint8_t>(length & 0xFFU);
  frame[2] = static_cast<uint8_t>((length >> 8) & 0xFFU);
  frame[3] = static_cast<uint8_t>(crc & 0xFFU);
  frame[4] = static_cast<uint8_t>((crc >> 8) & 0xFFU);
  return frame;
}

// --- Разбиение сообщения на DATA-блоки ---
std::vector<DataBlock> splitPayloadIntoBlocks(const std::vector<uint8_t>& payload,
                                              bool appendCrc8) {
  std::vector<DataBlock> blocks;
  const size_t dataBytesPerBlock = appendCrc8 ? (kFramePayloadSize - 1) : kFramePayloadSize;
  size_t offset = 0;
  while (offset < payload.size()) {
    DataBlock block;
    const size_t chunk = std::min(dataBytesPerBlock, payload.size() - offset);
    std::copy_n(payload.begin() + offset, chunk, block.bytes.begin());
    block.dataLength = static_cast<uint8_t>(chunk);
    if (appendCrc8) {
      block.bytes[dataBytesPerBlock] = crc8Dallas(block.bytes.data(), chunk);
    }
    blocks.push_back(block);
    offset += chunk;
  }
  return blocks;
}

// --- CRC-16 CCITT ---
uint16_t crc16Ccitt(const uint8_t* data, size_t length, uint16_t crc) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000U) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021U);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// --- CRC-8 Dallas/Maxim ---
uint8_t crc8Dallas(const uint8_t* data, size_t length) {
  uint8_t crc = 0;
  for (size_t i = 0; i < length; ++i) {
    uint8_t byte = data ? data[i] : 0;
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x80U) {
        crc = static_cast<uint8_t>((crc << 1) ^ 0x31U);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// --- Пауза между кадрами ---
void waitInterFrameDelay() {
#if defined(ARDUINO)
  delay(kInterFrameDelayMs);
#else
  std::this_thread::sleep_for(std::chrono::milliseconds(kInterFrameDelayMs));
#endif
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

// --- Логирование принятого сообщения ---
void logReceivedMessage(const std::vector<uint8_t>& payload) {
  addEvent(String("Принято сообщение (") + String(static_cast<unsigned long>(payload.size())) + " байт): " +
           formatByteArray(payload) + " | \"" + formatTextPayload(payload) + "\"", kIncomingColor);
}

// --- Полный сброс состояния приёмника ---
void resetReceiveState() {
  state.rxWindow = {};
  state.rxMessage = {};
  state.pendingAck = {};
}

// --- Обработка принятого кадра ---
void processIncomingFrame(const std::vector<uint8_t>& frame) {
  if (frame.empty()) {
    return;
  }

  addEvent(String("← Кадр: ") + formatByteArray(frame));

  const uint8_t type = frame[0] & kFrameTypeMask;
  switch (type) {
    case kFrameTypeData:
      processIncomingDataFrame(frame);
      break;
    case kFrameTypeAck:
      processIncomingAckFrame(frame);
      break;
    case kFrameTypeParity:
      processIncomingParityFrame(frame);
      break;
    case kFrameTypeFin:
      processIncomingFinFrame(frame);
      break;
    default:
      addEvent("Неизвестный тип кадра");
      break;
  }
}

// --- Обработка DATA-кадра ---
void processIncomingDataFrame(const std::vector<uint8_t>& frame) {
  if (frame.size() < 3) {
    addEvent("Получен усечённый DATA-кадр");
    return;
  }

  const uint8_t flags = frame[0] & static_cast<uint8_t>(~kFrameTypeMask);
  const uint16_t seq = static_cast<uint16_t>(frame[1]) | (static_cast<uint16_t>(frame[2]) << 8);
  const uint8_t lengthField = static_cast<uint8_t>((flags & kDataFlagLengthMask) >> kDataFlagLengthShift);
  const bool ackRequest = (flags & kDataFlagAckRequest) != 0;
  const bool isParity = (flags & kDataFlagIsParity) != 0;

  if (isParity) {
    processIncomingParityFrame(frame);
    return;
  }

  if (!state.rxWindow.active || seq < state.rxWindow.baseSeq ||
      seq >= static_cast<uint16_t>(state.rxWindow.baseSeq + kBitmapWidth)) {
    state.rxWindow.active = true;
    state.rxWindow.baseSeq = static_cast<uint16_t>(seq - (seq % kArqWindowSize));
    state.rxWindow.receivedMask = 0;
    state.rxWindow.windowSize = 0;
  }

  const uint8_t bit = static_cast<uint8_t>(seq - state.rxWindow.baseSeq);
  if (bit < kBitmapWidth) {
    state.rxWindow.receivedMask |= static_cast<uint16_t>(1U << bit);
    state.rxWindow.windowSize = static_cast<uint8_t>(std::max<uint8_t>(state.rxWindow.windowSize, bit + 1));
  }

  if (!state.rxMessage.active) {
    state.rxMessage.active = true;
    state.rxMessage.nextExpectedSeq = seq;
    state.rxMessage.buffer.clear();
    state.rxMessage.pending.clear();
    state.rxMessage.declaredLength = 0;
    state.rxMessage.declaredCrc = 0;
    state.rxMessage.finReceived = false;
  }

  DataBlock block;
  const size_t payloadAvailable = std::min<size_t>(kFramePayloadSize, frame.size() - 3);
  std::copy_n(frame.begin() + 3, payloadAvailable, block.bytes.begin());
  block.dataLength = std::min<uint8_t>(lengthField, kFramePayloadSize);

  if (state.protocol.payloadCrc8 && block.dataLength > 0) {
    if (block.dataLength == 0) {
      addEvent("DATA-кадр содержит некорректную длину");
      return;
    }
    if (block.dataLength > 0) {
      const uint8_t crcOffset = block.dataLength - 1;
      const uint8_t expected = block.bytes[crcOffset];
      const uint8_t actualLength = crcOffset;
      const uint8_t computed = crc8Dallas(block.bytes.data(), actualLength);
      if (computed != expected) {
        addEvent("CRC-8 DATA не сошёлся, кадр отброшен");
        return;
      }
      block.dataLength = actualLength;
    }
  }

  state.rxMessage.pending[seq] = block;
  flushPendingData();

  if (ackRequest) {
    const uint8_t windowCount = static_cast<uint8_t>(std::max<uint8_t>(state.rxWindow.windowSize, bit + 1));
    prepareAck(seq, windowCount, true);
  }
}

// --- Обработка ACK-кадра ---
void processIncomingAckFrame(const std::vector<uint8_t>& frame) {
  if (frame.size() < 6) {
    addEvent("Получен усечённый ACK");
    return;
  }
  AckNotification note;
  note.hasValue = true;
  note.baseSeq = static_cast<uint16_t>(frame[1]) | (static_cast<uint16_t>(frame[2]) << 8);
  note.missingBitmap = static_cast<uint16_t>(frame[3]) | (static_cast<uint16_t>(frame[4]) << 8);
  note.needParity = (frame[0] & kAckFlagNeedParity) != 0;
  note.reportedWindow = frame[5];
  state.pendingAck = note;
  addEvent(String("Принят ACK: base=") + String(note.baseSeq) +
           ", missing=0x" + String(note.missingBitmap, 16) +
           ", window=" + String(note.reportedWindow));
}

// --- Обработка PAR-кадра (пока заглушка) ---
void processIncomingParityFrame(const std::vector<uint8_t>& frame) {
  (void)frame;
  addEvent("Получен PAR-кадр — HARQ пока не реализован (TODO)");
}

// --- Обработка FIN-кадра ---
void processIncomingFinFrame(const std::vector<uint8_t>& frame) {
  if (frame.size() < 5) {
    addEvent("Получен усечённый FIN");
    return;
  }
  if (!state.rxMessage.active) {
    state.rxMessage.active = true;
    state.rxMessage.nextExpectedSeq = state.rxWindow.baseSeq;
    state.rxMessage.buffer.clear();
    state.rxMessage.pending.clear();
  }
  uint16_t length = static_cast<uint16_t>(frame[1]) | (static_cast<uint16_t>(frame[2]) << 8);
  uint16_t crc = static_cast<uint16_t>(frame[3]) | (static_cast<uint16_t>(frame[4]) << 8);
  state.rxMessage.declaredLength = length;
  state.rxMessage.declaredCrc = crc;
  state.rxMessage.finReceived = true;
  flushPendingData();
}

// --- Подготовка и отправка ACK ---
void prepareAck(uint16_t /*seq*/, uint8_t windowSize, bool forceSend) {
  if (!state.rxWindow.active) {
    return;
  }
  const uint8_t effectiveWindow = std::max<uint8_t>(state.rxWindow.windowSize, windowSize);
  uint16_t mask = static_cast<uint16_t>((effectiveWindow >= kBitmapWidth) ? kBitmapFullMask
                                                                         : ((1U << effectiveWindow) - 1U));
  const uint16_t missing = static_cast<uint16_t>((~state.rxWindow.receivedMask) & mask);
  const bool needParity = false; // TODO: оценка необходимости HARQ на приёме
  if (!forceSend && missing == 0) {
    return;
  }
  const uint16_t base = state.rxWindow.baseSeq;
  sendAck(base, missing, needParity, effectiveWindow);
  state.rxWindow.baseSeq = static_cast<uint16_t>(base + effectiveWindow);
  state.rxWindow.receivedMask = 0;
  state.rxWindow.windowSize = 0;
}

void sendAck(uint16_t baseSeq, uint16_t missingBitmap, bool needParity, uint8_t windowSize) {
  auto frame = buildAckFrame(baseSeq, missingBitmap, needParity, windowSize);
  transmitFrame(frame, F("ACK"));
}

// --- Сборка последовательных блоков ---
void flushPendingData() {
  if (!state.rxMessage.active) {
    return;
  }

  bool progressed = true;
  while (progressed) {
    progressed = false;
    auto it = state.rxMessage.pending.find(state.rxMessage.nextExpectedSeq);
    if (it == state.rxMessage.pending.end()) {
      break;
    }
    const DataBlock& block = it->second;
    state.rxMessage.buffer.insert(state.rxMessage.buffer.end(),
                                  block.bytes.begin(),
                                  block.bytes.begin() + block.dataLength);
    state.rxMessage.pending.erase(it);
    state.rxMessage.nextExpectedSeq = static_cast<uint16_t>(state.rxMessage.nextExpectedSeq + 1);
    progressed = true;
  }

  if (state.rxMessage.finReceived && state.rxMessage.pending.empty()) {
    if (state.rxMessage.declaredLength <= state.rxMessage.buffer.size()) {
      state.rxMessage.buffer.resize(state.rxMessage.declaredLength);
      const uint16_t crc = crc16Ccitt(state.rxMessage.buffer.data(), state.rxMessage.buffer.size());
      if (crc == state.rxMessage.declaredCrc) {
        logReceivedMessage(state.rxMessage.buffer);
      } else {
        addEvent("CRC-16 FIN не сошёлся — сообщение отброшено");
      }
    } else {
      addEvent("FIN сообщил длину больше собранной — сообщение отброшено");
    }
    state.rxMessage.active = false;
    state.rxMessage.pending.clear();
    state.rxMessage.buffer.clear();
    state.rxMessage.finReceived = false;
    state.rxWindow.active = false;
    state.rxWindow.receivedMask = 0;
    state.rxWindow.windowSize = 0;
  }
}
