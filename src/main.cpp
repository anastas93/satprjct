#include <Arduino.h>
#include <array>
#include <vector>
#include <deque>
#include <string>
#include <cstring> // для strlen
#include <algorithm> // для std::equal
#include <cstdio>    // для snprintf при подготовке JSON
#include <cstdlib>   // для strtol/strtof при разборе HTTP-параметров
#include <cerrno>    // для анализа ошибок strtol/strtof
#include <cctype>    // для std::isspace при валидации остатка строки

// --- Радио и модули ---
#include "radio_sx1262.h"
#include "tx_module.h"
#include "rx_module.h" // модуль приёма
#include "default_settings.h"
#include "libs/config_loader/config_loader.h" // загрузка конфигурации из файла

// --- Вспомогательные библиотеки ---
#include "libs/text_converter/text_converter.h"   // конвертер UTF-8 -> CP1251
#include "libs/simple_logger/simple_logger.h"     // журнал статусов
#include "logger.h"                               // асинхронный вывод логов
#include "libs/received_buffer/received_buffer.h" // буфер принятых сообщений
#include "libs/packetizer/packet_gatherer.h"      // собиратель пакетов для теста
#include "libs/packetizer/packet_splitter.h"      // параметры делителя пакетов
#include "libs/crypto/chacha20_poly1305.h"        // AEAD ChaCha20-Poly1305
#include "libs/key_loader/key_loader.h"           // управление ключами и ECDH
#include "libs/key_transfer/key_transfer.h"       // обмен корневым ключом по LoRa
#include "libs/key_transfer_waiter/key_transfer_waiter.h" // конечная автоматика ожидания KEYTRANSFER
#include "libs/protocol/ack_utils.h"              // обработка ACK-пакетов
#include "key_safe_mode.h"                        // управление защищённым режимом и шифрованием
#include "sse_buffered_writer.h"                  // буферизированная отправка SSE-кадров
#include "rx_serial_dump.h"                       // безопасный дамп RX-данных в Serial

// --- Сеть и веб-интерфейс ---
#include <WiFi.h>        // работа с Wi-Fi
#include <WebServer.h>   // встроенный HTTP-сервер
#if defined(ARDUINO) && defined(ESP32)
#include <esp_system.h>  // доступ к уникальному идентификатору ESP32
#endif
#include "web/web_content.h"      // встроенные файлы веб-интерфейса
#include "http_body_reader.h"     // потоковое чтение HTTP-тела
#ifndef ARDUINO
#include <fstream>
#include <chrono>
#else
#include <FS.h>
using NetworkClient = WiFiClient;
#endif

#if defined(__has_include)
#define SR_HAS_INCLUDE(path) __has_include(path)
#else
#define SR_HAS_INCLUDE(path) 0
#endif

#if defined(ESP32) && SR_HAS_INCLUDE("esp_partition.h")
#if SR_HAS_INCLUDE("esp_core_dump.h")
#include "esp_core_dump.h"      // управление core dump на ESP32 (если заголовок доступен)
#define SR_HAS_COREDUMP_IMAGE_CHECK 1
#else
#define SR_HAS_COREDUMP_IMAGE_CHECK 0
#endif
#include "esp_partition.h"      // прямой доступ к разделам флеша
#if SR_HAS_INCLUDE("spi_flash_mmap.h")
#include "spi_flash_mmap.h"     // современный заголовок для работы с флешем
#elif SR_HAS_INCLUDE("esp_spi_flash.h")
#include "esp_spi_flash.h"      // совместимость с устаревшим заголовком
#endif
#ifndef SPI_FLASH_SEC_SIZE
#define SPI_FLASH_SEC_SIZE 4096  // резервное определение размера сектора флеша
#endif
#if SR_HAS_INCLUDE("esp_ipc.h")
#include "esp_ipc.h"            // выполнение критичных операций на ядре 0
#define SR_HAS_ESP_IPC 0
#else
#define SR_HAS_ESP_IPC 0
#endif
#if SR_HAS_INCLUDE("esp_idf_version.h")
#include "esp_idf_version.h"    // сведения о версии ESP-IDF
#endif
#include "freertos/FreeRTOS.h"
#ifndef ESP_IPC_CPU_PRO
#define ESP_IPC_CPU_PRO 0
#endif
#include "freertos/task.h"
#define SR_HAS_ESP_COREDUMP 1
#else
#define SR_HAS_ESP_COREDUMP 0
#define SR_HAS_COREDUMP_IMAGE_CHECK 0
#endif

#if defined(SR_HAS_INCLUDE)
#undef SR_HAS_INCLUDE
#endif

#ifdef ARDUINO
// Резервная версия прошивки, чтобы /ver отвечал даже без внешнего файла
static const char kEmbeddedVersion[] PROGMEM =
#include "ver"
;
#else
// Аналогичный резерв для тестовых сборок на хосте
static const char kEmbeddedVersion[] =
#include "ver"
;
#endif

// Предварительные объявления структур, чтобы прототипы Arduino
// не нарушали компиляцию при автоматическом добавлении заголовков.
struct LegacyTestPacket;
struct TestRxmSpec;

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
static const ConfigLoader::Config& gConfig = ConfigLoader::getConfig(); // глобальная конфигурация
bool ackEnabled = gConfig.radio.useAck; // флаг автоматической отправки ACK
bool encryptionEnabled = gConfig.radio.useEncryption; // режим шифрования
bool keyStorageReady = false;               // флаг успешной инициализации хранилища ключей
bool keySafeModeActive = false;             // признак защищённого режима работы без доступа к ключам
uint8_t ackRetryLimit = gConfig.radio.ackRetryLimit; // число повторов при ожидании ACK
static constexpr uint32_t kAckDelayMinMs = 0;             // минимально допустимая задержка ответа ACK
static constexpr uint32_t kAckDelayMaxMs = 5000;          // максимально допустимая задержка ответа ACK
uint32_t ackResponseDelayMs = gConfig.radio.ackResponseDelayMs; // текущая задержка перед ACK
bool lightPackMode = false;             // режим прямой передачи текста без префикса
bool testModeEnabled = false;           // флаг тестового режима SendMsg_BR/received_msg
uint8_t testModeLocalCounter = 0;       // локальный счётчик пакетов для тестового режима
bool rxSerialDumpEnabled = DefaultSettings::RX_SERIAL_DUMP_ENABLED; // режим дампа RX в Serial

WebServer server(80);       // HTTP-сервер для веб-интерфейса

template <typename T, size_t N>
constexpr size_t progmemLength(const T (&)[N]) {
  return N;
}

template <size_t N>
constexpr size_t progmemLength(const char (&array)[N]) {
  return N ? (N - 1) : 0;
}

// Простейший экранировщик для подстановки строк в JSON-ответы API.
static String escapeJson(const std::string& text) {
  String out;
  out.reserve(text.size() + 4);
  for (unsigned char ch : text) {
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (ch < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(ch));
          out += buf;
        } else {
          out += static_cast<char>(ch);
        }
        break;
    }
  }
  return out;
}

static void sendProgmemAsset(const char* contentType,
                             const uint8_t* data,
                             size_t length,
                             bool cacheable = true) {
  if (cacheable) {
    server.sendHeader("Cache-Control", "public, max-age=86400, immutable");
  } else {
    server.sendHeader("Cache-Control", "no-cache");
  }
  server.send_P(200, contentType, reinterpret_cast<PGM_P>(data), length);
}

// Ожидание инициализации Serial с ограничением по времени, чтобы не блокировать запуск Wi-Fi
bool waitForSerial(unsigned long timeout_ms) {
#if defined(ARDUINO)
  const unsigned long start = millis();
  while (!Serial) {
    if (timeout_ms > 0 && (millis() - start) >= timeout_ms) {
      return false;  // истёк тайм-аут ожидания подключения к USB
    }
    delay(10);  // предотвращаем срабатывание сторожевого таймера
  }
#else
  (void)timeout_ms;  // в хостовой сборке Serial отсутствует
#endif
  return true;
}

// Предварительные объявления для блоков SSE, чтобы Arduino-процессор прототипов
// корректно обрабатывал пользовательские типы.
struct PushClientSession;
bool sendSseFrame(PushClientSession& session, const String& event, const String& data, uint32_t id);
String buildLogPayload(const LogHook::Entry& entry);
void enqueueLogEntry(const LogHook::Entry& entry);
bool broadcastLogEntry(const LogHook::Entry& entry);
void flushPendingLogEntries();
struct LastIrqStatus;
void updateLastIrqStatus(const char* message, uint32_t uptimeMs);
void flushPendingIrqStatus(bool force = false);
String buildIrqStatusPayload(const LastIrqStatus& status);
bool broadcastIrqStatus(const LastIrqStatus& status);
static void onRadioIrqLog(const char* message, uint32_t uptimeMs);
void maintainPushSessions(bool forceKeepAlive = false);
String cmdKeyTransferSendLora();
bool enqueueTextMessage(const String& payload, uint32_t& outId, String& err);
bool enqueueBinaryMessage(const uint8_t* data, size_t len, uint32_t& outId, String& err);
void processKeyTransferReceiveState();

// Состояние генератора тестовых входящих сообщений
struct TestRxmState {
  bool active = false;          // идёт ли генерация
  uint8_t produced = 0;         // сколько сообщений создано
  uint32_t nextAt = 0;          // когда запланировано следующее сообщение (millis)
} testRxmState;

// Описание шаблона тестовых сообщений TESTRXM
struct TestRxmSpec {
  uint8_t percent;  // доля исходного текста в процентах
  bool useGatherer; // требуется ли эмуляция поступления частями
};

// Количество сообщений и шаблоны (последний проверяет работу PacketGatherer)
static constexpr size_t kTestRxmCount = 5;
static constexpr TestRxmSpec kTestRxmSpecs[kTestRxmCount] = {
  {100, false},
  {50,  false},
  {30,  false},
  {20,  false},
  {100, true},
};

// Эталонный текст для генерации тестовых сообщений
static const char kTestRxmLorem[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed eros arcu, "
    "ultricies et maximus vitae, porttitor condimentum erat. Nam commodo porttitor.";

// Пользовательский шаблон для TESTRXM и ограничение длины строки
static constexpr size_t kTestRxmMaxSourceLength = 2048; // соответствует лимиту интерфейса (~2 КиБ)
String testRxmSourceText;

// Состояние процесса обмена корневым ключом по LoRa
struct KeyTransferRuntime {
  bool waiting = false;                 // ожидаем ли приём специального кадра
  bool completed = false;               // удалось ли применить ключ
  String error;                         // код ошибки для ответа в HTTP/Serial
  uint32_t last_msg_id = 0;             // идентификатор последнего пакета
  bool legacy_peer = false;             // последний принятый кадр был в легаси-формате
  bool ephemeral_active = false;        // активна ли подготовленная эпемерная пара
  uint32_t waitStartedAt = 0;           // отметка запуска ожидания (millis)
  uint32_t waitDeadlineAt = 0;          // дедлайн завершения ожидания (millis, 0 = без тайм-аута)
} keyTransferRuntime;

KeyTransferWaiter keyTransferWaiter;    // автомат ожидания результатов KEYTRANSFER

// Сброс временных меток ожидания KEYTRANSFER, чтобы новый цикл начинался "с чистого листа"
static void resetKeyTransferWaitTiming() {
  keyTransferRuntime.waitStartedAt = 0;
  keyTransferRuntime.waitDeadlineAt = 0;
}

// --- Состояние буферизации команд из Serial ---
static String serialLineBuffer;                         // накапливаемая строка команды
static bool serialLineOverflow = false;                 // флаг переполнения буфера
static unsigned long serialLastByteAtMs = 0;            // время последнего принятого символа
static constexpr size_t kSerialLineMaxLength = 1024;    // максимальная длина текстовой команды
static constexpr unsigned long kSerialLineTimeoutMs = 2000; // тайм-аут ожидания завершения строки (увеличен для ручного ввода)
static bool serialWasReady = false;                     // отслеживаем переход готовности Serial

// Преобразование массива байтов в hex-строку (верхний регистр)
template <size_t N>
String toHex(const std::array<uint8_t, N>& data) {
  static const char kHex[] = "0123456789ABCDEF";
  String out;
  out.reserve(N * 2);
  for (size_t i = 0; i < N; ++i) {
    out += kHex[data[i] >> 4];
    out += kHex[data[i] & 0x0F];
  }
  return out;
}

// Парсинг hex-строки в массив байтов; допускаются строчные/прописные символы
template <size_t N>
bool parseHex(const String& text, std::array<uint8_t, N>& out) {
  if (text.length() != static_cast<int>(N * 2)) return false;
  auto hexVal = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
  };
  for (size_t i = 0; i < N; ++i) {
    int hi = hexVal(text[static_cast<int>(i * 2)]);
    int lo = hexVal(text[static_cast<int>(i * 2 + 1)]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

// Преобразование вектора байтов в hex-строку
String toHex(const std::vector<uint8_t>& data) {
  static const char kHex[] = "0123456789ABCDEF";
  String out;
  out.reserve(data.size() * 2);
  for (uint8_t b : data) {
    out += kHex[(b >> 4) & 0x0F];
    out += kHex[b & 0x0F];
  }
  return out;
}

// Вспомогательная конвертация Arduino String -> std::string
static std::string toStdString(const String& value) {
  return std::string(value.c_str());
}

// Экранирование строки для включения в JSON
String jsonEscape(const String& value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(static_cast<int>(i));
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04X", static_cast<unsigned>(static_cast<unsigned char>(c)));
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

#if SR_HAS_ESP_COREDUMP
// Унифицированные макросы совместимости для получения Idle-задачи и выбора ядра IPC
#ifndef ESP_IDF_VERSION_VAL
#define ESP_IDF_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#endif

#if defined(ESP_IDF_VERSION_MAJOR) && defined(ESP_IDF_VERSION_MINOR) && \
    defined(ESP_IDF_VERSION_PATCH) && !defined(ESP_IDF_VERSION)
#define ESP_IDF_VERSION \
  ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH)
#endif

#if defined(ESP_IDF_VERSION_MAJOR) && defined(ESP_IDF_VERSION) && \
    (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0))
#define SR_IDLE_TASK_HANDLE_FOR_CORE(core) xTaskGetIdleTaskHandleForCore(core)
static constexpr uint32_t kCoreDumpIpcTargetCore = 0; // ядро PRO в новых версиях ESP-IDF
#else
#define SR_IDLE_TASK_HANDLE_FOR_CORE(core) xTaskGetIdleTaskHandleForCPU(core)
static constexpr uint32_t kCoreDumpIpcTargetCore = ESP_IPC_CPU_PRO; // ядро PRO в старых версиях
#endif

// Флаги, управляющие отложенной очисткой core dump после старта FreeRTOS.
bool gCoreDumpClearPending = true;
uint32_t gCoreDumpClearAfterMs = 0;

// Контекст для IPC-вызова очистки core dump
struct CoreDumpClearContext {
  const esp_partition_t* part = nullptr;      // раздел core dump
  uint8_t zeros[32] = {};                     // буфер нулей для записи
  esp_err_t eraseErr = ESP_OK;                // результат стирания
  esp_err_t writeErr = ESP_OK;                // результат записи
};

// Выполняем стирание/запись core dump на ядре PRO через IPC
static void coreDumpClearIpc(void* arg) {
  CoreDumpClearContext* ctx = static_cast<CoreDumpClearContext*>(arg);
  if (!ctx || !ctx->part) {
    return;
  }
  // Стираем только первый сектор, чтобы не нарушать выравнивание по 4 КБ
  size_t eraseSize = std::min<size_t>(ctx->part->size, SPI_FLASH_SEC_SIZE);
  ctx->eraseErr = esp_partition_erase_range(ctx->part, 0, eraseSize);
  if (ctx->eraseErr != ESP_OK) {
    return;
  }
  ctx->writeErr = esp_partition_write(ctx->part, 0, ctx->zeros, sizeof(ctx->zeros));
}

// Пытаемся очистить повреждённую конфигурацию core dump, чтобы убрать перезагрузки с ошибкой CRC
bool clearCorruptedCoreDumpConfig() {
#if SR_HAS_COREDUMP_IMAGE_CHECK
  // Перед очисткой проверяем состояние core dump штатной функцией, чтобы не стирать
  // исправную конфигурацию при каждом запуске.
  esp_err_t checkErr = esp_core_dump_image_check();
  if (checkErr == ESP_OK) {
    Serial.println("Core dump: конфигурация корректна, очистка не требуется");
    return true;
  }
  if (checkErr == ESP_ERR_NOT_FOUND) {
    Serial.println("Core dump: данные отсутствуют, конфигурация считается чистой");
    return true;
  }
  if (checkErr != ESP_ERR_INVALID_CRC && checkErr != ESP_ERR_INVALID_SIZE) {
    Serial.print("Core dump: image_check вернул код=");
    Serial.println(static_cast<int>(checkErr));
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  Serial.print("Core dump: image_check сообщает о повреждении (код=");
  Serial.print(static_cast<int>(checkErr));
  Serial.println(") — выполняем очистку");
#endif
  TaskHandle_t idle0 = SR_IDLE_TASK_HANDLE_FOR_CORE(0);
#if (portNUM_PROCESSORS > 1)
  TaskHandle_t idle1 = SR_IDLE_TASK_HANDLE_FOR_CORE(1);
#else
  TaskHandle_t idle1 = idle0;
#endif
  if (idle0 == nullptr || idle1 == nullptr) {
    Serial.println("Core dump: Idle-задачи ещё не запущены, повторим позже");
    gCoreDumpClearAfterMs = millis() + 500;
    return false;
  }
  // На некоторых сборках Arduino+ESP32 вызов esp_core_dump_image_erase()
  // приводит к assert внутри FreeRTOS (нет Idle-задачи на втором ядре).
  // Поэтому стираем раздел core dump напрямую через API разделов флеша.
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  if (!part) {
    Serial.println("Core dump: раздел не найден");
    return true;
  }
  // Сначала читаем начало раздела и проверяем, не сброшена ли конфигурация уже
  static constexpr size_t kProbeSize = 32; // достаточно для структуры esp_core_dump_flash_config_t
  uint8_t probe[kProbeSize];
  esp_err_t err = esp_partition_read(part, 0, probe, sizeof(probe));
  if (err != ESP_OK) {
    Serial.print("Core dump: ошибка чтения, код=");
    Serial.println(static_cast<int>(err));
    // Дадим флешу и подсистеме core dump возможность освободить ресурсы и попробуем позже.
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  bool allZero = true;
  for (uint8_t b : probe) {
    if (b != 0) {
      allZero = false;
    }
  }
  if (allZero) {
    Serial.println("Core dump: конфигурация уже чистая");
    return true;
  }
  // После стирания флеш содержит 0xFF, а esp_core_dump ожидает нули в конфигурации,
  // поэтому записываем явные нули в начале раздела через IPC на ядре 0.
  CoreDumpClearContext ctx;
  ctx.part = part;
  std::fill_n(ctx.zeros, sizeof(ctx.zeros), 0);
  esp_err_t ipcErr = ESP_OK;
#if SR_HAS_ESP_IPC
  ipcErr = esp_ipc_call_blocking(kCoreDumpIpcTargetCore, &coreDumpClearIpc, &ctx);
#else
  // На платформах без esp_ipc выполняем очистку на текущем ядре, так как выбор ядра недоступен.
  coreDumpClearIpc(&ctx);
#endif
  if (ipcErr != ESP_OK) {
    Serial.print("Core dump: IPC-вызов завершился ошибкой, код=");
    Serial.println(static_cast<int>(ipcErr));
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  if (ctx.eraseErr != ESP_OK) {
    Serial.print("Core dump: ошибка стирания, код=");
    Serial.println(static_cast<int>(ctx.eraseErr));
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  if (ctx.writeErr == ESP_OK) {
    Serial.println("Core dump: конфигурация сброшена");
    return true;
  }
  Serial.print("Core dump: ошибка записи, код=");
  Serial.println(static_cast<int>(ctx.writeErr));
  gCoreDumpClearAfterMs = millis() + 1000;
  return false;
}
#endif


// Преобразование буфера байтов в строку ASCII
String vectorToString(const std::vector<uint8_t>& data) {
  String out;
  out.reserve(data.size());
  for (uint8_t b : data) {
    out += static_cast<char>(b);
  }
  return out;
}

// Проверяем, похож ли буфер на JPEG по сигнатуре FF D8 FF
bool isLikelyJpeg(const uint8_t* data, size_t len) {
  if (!data || len < 3) {
    return false;
  }
  return data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

// Декодирование CP1251 в UTF-8 для отправки в веб-интерфейс
String decodeCp1251ToString(const std::vector<uint8_t>& data) {
  if (data.empty()) return String();
  std::vector<uint8_t> trimmed(data);
  while (!trimmed.empty() && trimmed.back() == 0) {
    trimmed.pop_back();
  }
  if (trimmed.empty()) return String();
  std::string utf8 = cp1251ToUtf8(trimmed);
  String out(utf8.c_str());
  out.trim();
  return out;
}

// Текстовое представление очереди буфера
String receivedKindToString(ReceivedBuffer::Kind kind) {
  switch (kind) {
    case ReceivedBuffer::Kind::Raw:   return String("raw");
    case ReceivedBuffer::Kind::Split: return String("split");
    case ReceivedBuffer::Kind::Ready: return String("ready");
  }
  return String("unknown");
}

// Формирование JSON-объекта для элемента буфера приёма
String receivedItemToJson(const ReceivedBuffer::SnapshotEntry& entry) {
  String json = "{";
  String name = jsonEscape(String(entry.item.name.c_str()));
  json += "\"name\":\"";
  json += name;
  json += "\"";
  json += ",\"type\":\"";
  json += receivedKindToString(entry.kind);
  json += "\"";
  json += ",\"len\":";
  json += String(entry.item.data.size());
  String decoded = decodeCp1251ToString(entry.item.data);
  if (decoded.length() > 0) {
    json += ",\"text\":\"";
    json += jsonEscape(decoded);
    json += "\"";
  }
  String hex = toHex(entry.item.data);
  if (hex.length() > 0) {
    json += ",\"hex\":\"";
    json += hex;
    json += "\"";
  }
  json += "}";
  return json;
}

// --- Поддержка push-уведомлений через SSE ---
static constexpr uint32_t kPushKeepAliveMs = 15000;   // интервал keep-alive для SSE клиентов
static constexpr size_t kPushMaxClients = 4;          // максимум одновременных подключений
static constexpr uint32_t kPushSendTimeoutMs = 1500;   // максимум ожидания освобождения окна записи
static constexpr size_t kPushSendChunkLimit = 512;     // верхняя граница размера одного куска отправки

struct PushClientSession {
  NetworkClient client;       // активное соединение SSE
  uint32_t lastActivityMs;    // время последнего успешного события
  uint32_t lastKeepAliveMs;   // время последнего keep-alive
  SseBufferedWriter<NetworkClient, String> writer; // очередь частичных кадров для неблокирующей отправки
};

static std::vector<PushClientSession> gPushSessions;  // список подписчиков SSE
static uint32_t gPushNextEventId = 1;                 // глобальный счётчик событий
static std::deque<LogHook::Entry> gPendingLogEntries; // очередь строк журнала для отложенной отправки
static constexpr size_t kMaxPendingLogEntries = 64;   // ограничение размера очереди журнала

#if defined(ARDUINO)
static uint32_t systemMillis() {
  return millis();
}
#else
static uint32_t systemMillis() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}
#endif

struct LastIrqStatus {
  String message;          // последняя строка журнала с расшифровкой IRQ
  uint32_t uptimeMs = 0;   // uptime устройства на момент обработки IRQ
  uint32_t capturedAtMs = 0; // локальное время фиксации (millis)
  bool hasValue = false;   // получали ли мы когда-либо IRQ
  bool dirty = false;      // требуется ли отправка SSE-сообщения
};

static LastIrqStatus gLastIrqStatus;                  // актуальный статус IRQ для SSE

// Сохраняем запись журнала в локальной очереди, чтобы не блокировать Serial при отправке SSE
void enqueueLogEntry(const LogHook::Entry& entry) {
  if (gPendingLogEntries.size() >= kMaxPendingLogEntries) {
    gPendingLogEntries.pop_front(); // отбрасываем самую старую запись при переполнении
  }
  gPendingLogEntries.push_back(entry);
}

void updateLastIrqStatus(const char* message, uint32_t uptimeMs) {
  if (!message) {
    return; // некорректный указатель, пропускаем обновление
  }
  gLastIrqStatus.message = String(message);
  gLastIrqStatus.uptimeMs = uptimeMs;
  gLastIrqStatus.capturedAtMs = systemMillis();
  gLastIrqStatus.hasValue = true;
  gLastIrqStatus.dirty = true;
}

String buildIrqStatusPayload(const LastIrqStatus& status) {
  String payload = "{";
  payload += "\"message\":\"";
  payload += jsonEscape(status.message);
  payload += "\",\"uptime\":";
  payload += String(static_cast<unsigned long>(status.uptimeMs));
  payload += ",\"timestamp\":";
  payload += String(static_cast<unsigned long>(status.capturedAtMs));
  payload += "}";
  return payload;
}

bool broadcastIrqStatus(const LastIrqStatus& status) {
  if (gPushSessions.empty()) {
    return false;
  }
  maintainPushSessions();
  String payload = buildIrqStatusPayload(status);
  String eventName = F("irq");
  uint32_t eventId = gPushNextEventId++;
  bool delivered = false;
  for (auto it = gPushSessions.begin(); it != gPushSessions.end(); ) {
    if (!sendSseFrame(*it, eventName, payload, eventId)) {
      it->client.stop();
      it = gPushSessions.erase(it);
    } else {
      delivered = true;
      ++it;
    }
  }
  return delivered;
}

void flushPendingIrqStatus(bool force) {
  if (!gLastIrqStatus.hasValue) {
    return; // ещё не получали ни одного IRQ
  }
  if (!force && !gLastIrqStatus.dirty) {
    return; // актуальное состояние уже доставлено подписчикам
  }
  if (!broadcastIrqStatus(gLastIrqStatus)) {
    return; // пока нет активных клиентов — попробуем позже
  }
  gLastIrqStatus.dirty = false;
}

static void onRadioIrqLog(const char* message, uint32_t uptimeMs) {
  updateLastIrqStatus(message, uptimeMs);
}

// Формируем JSON-представление для уведомления о новом элементе буфера
String buildReceivedPushPayload(ReceivedBuffer::Kind kind, const ReceivedBuffer::Item& item) {
  String payload = "{";
  payload += "\"kind\":\"";
  payload += receivedKindToString(kind);
  payload += "\",";
  payload += "\"name\":\"";
  payload += jsonEscape(String(item.name.c_str()));
  payload += "\",";
  payload += "\"id\":";
  payload += String(item.id);
  payload += ",\"part\":";
  payload += String(item.part);
  payload += ",\"len\":";
  payload += String(static_cast<unsigned long>(item.data.size()));
  payload += "}";
  return payload;
}

// Формируем JSON-представление строки журнала для SSE
String buildLogPayload(const LogHook::Entry& entry) {
  String payload = "{";
  payload += "\"id\":";
  payload += String(static_cast<unsigned long>(entry.id));
  payload += ",\"uptime\":";
  payload += String(static_cast<unsigned long>(entry.uptime_ms));
  payload += ",\"text\":\"";
  payload += jsonEscape(String(entry.text));
  payload += "\"";
  payload += "}";
  return payload;
}

// Отправка одного SSE-сообщения; возвращает false, если запись не удалась
bool sendSseFrame(PushClientSession& session, const String& event, const String& data, uint32_t id) {
  if (!session.client.connected()) {
    return false;
  }
  String frame;
  frame.reserve(event.length() + data.length() + 32);
  if (id > 0) {
    frame += F("id: ");
    frame += String(id);
    frame += '\n';
  }
  if (event.length() > 0) {
    frame += F("event: ");
    frame += event;
    frame += '\n';
  }
  if (data.length() > 0) {
    frame += F("data: ");
    frame += data;
    frame += '\n';
  }
  frame += '\n';
  if (!session.writer.enqueueFrame(session.client,
                                   session.lastActivityMs,
                                   session.lastKeepAliveMs,
                                   std::move(frame),
                                   true,
                                   true,
                                   kPushSendTimeoutMs,
                                   kPushSendChunkLimit)) {
    return false; // зафиксирован тайм-аут ожидания окна — клиент больше не обслуживаем
  }
  return true;
}

// Очистка и keep-alive для всех активных клиентов
void maintainPushSessions(bool forceKeepAlive) {
  if (gPushSessions.empty()) return;
  for (auto it = gPushSessions.begin(); it != gPushSessions.end(); ) {
    PushClientSession& session = *it;
    if (!session.client.connected()) {
      it = gPushSessions.erase(it);
      continue;
    }
    if (!session.writer.flush(session.client,
                              session.lastActivityMs,
                              session.lastKeepAliveMs,
                              kPushSendTimeoutMs,
                              kPushSendChunkLimit)) {
      session.client.stop();
      it = gPushSessions.erase(it);
      continue;
    }
    if (!session.writer.empty()) {
      ++it;
      continue; // дожидаемся, пока дозавершится предыдущая отправка
    }
    const uint32_t now = millis();
    const uint32_t elapsed = now - session.lastKeepAliveMs;
    if (forceKeepAlive || elapsed > kPushKeepAliveMs) {
      String keepAliveFrame = String(F(": keep-alive\n\n"));
      if (!session.writer.enqueueFrame(session.client,
                                       session.lastActivityMs,
                                       session.lastKeepAliveMs,
                                       std::move(keepAliveFrame),
                                       false,
                                       true,
                                       kPushSendTimeoutMs,
                                       kPushSendChunkLimit)) {
        session.client.stop();
        it = gPushSessions.erase(it);
        continue;
      }
    }
    ++it;
  }
}

// Рассылка уведомления всем подключённым клиентам
void broadcastReceivedPush(ReceivedBuffer::Kind kind, const ReceivedBuffer::Item& item) {
  if (gPushSessions.empty()) return;
  maintainPushSessions();
  String payload = buildReceivedPushPayload(kind, item);
  String eventName = F("received");
  uint32_t eventId = gPushNextEventId++;
  for (auto it = gPushSessions.begin(); it != gPushSessions.end(); ) {
    if (!sendSseFrame(*it, eventName, payload, eventId)) {
      it->client.stop();
      it = gPushSessions.erase(it);
    } else {
      ++it;
    }
  }
}

// Рассылка строки журнала всем подписчикам SSE
bool broadcastLogEntry(const LogHook::Entry& entry) {
  if (gPushSessions.empty()) return false;
  maintainPushSessions();
  String payload = buildLogPayload(entry);
  String eventName = F("log");
  uint32_t eventId = gPushNextEventId++;
  bool delivered = false;
  for (auto it = gPushSessions.begin(); it != gPushSessions.end(); ) {
    if (!sendSseFrame(*it, eventName, payload, eventId)) {
      it->client.stop();
      it = gPushSessions.erase(it);
    } else {
      ++it;
      delivered = true;
    }
  }
  return delivered;
}

// Пытаемся передать накопленные строки журнала активным SSE-клиентам
void flushPendingLogEntries() {
  if (gPendingLogEntries.empty() || gPushSessions.empty()) {
    return; // либо нечего отправлять, либо получателей пока нет
  }
  while (!gPendingLogEntries.empty() && !gPushSessions.empty()) {
    LogHook::Entry entry = gPendingLogEntries.front();
    gPendingLogEntries.pop_front();
    if (!broadcastLogEntry(entry)) {
      // Никто не получил запись (клиенты отключились) — вернём строку и подождём новых подключений
      gPendingLogEntries.push_front(entry);
      break;
    }
  }
}

// Обработчик подключения SSE на /events
void handleSseConnect() {
  NetworkClient baseClient = server.client();
  if (!baseClient) {
    return;
  }
  baseClient.setTimeout(0);
  baseClient.setNoDelay(true);
  NetworkClient client = baseClient; // сохраняем копию для хранения в сессии
  static const char kHeader[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/event-stream\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: keep-alive\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "\r\n";
  const size_t headerLen = sizeof(kHeader) - 1;
  if (client.write(reinterpret_cast<const uint8_t*>(kHeader), headerLen) != headerLen) {
    client.stop();
    return;
  }
  static const char kRetryLine[] = "retry: 4000\n\n";
  if (client.write(reinterpret_cast<const uint8_t*>(kRetryLine), sizeof(kRetryLine) - 1) != sizeof(kRetryLine) - 1) {
    client.stop();
    return;
  }
  PushClientSession session;
  session.client = client;
  uint32_t now = millis();
  session.lastActivityMs = now;
  session.lastKeepAliveMs = now;
  if (gPushSessions.size() >= kPushMaxClients) {
    gPushSessions.front().client.stop();
    gPushSessions.erase(gPushSessions.begin());
  }
  gPushSessions.push_back(session);
  PushClientSession& stored = gPushSessions.back();
  String helloEvent = F("hello");
  String helloPayload = F("{\"status\":\"ready\"}");
  if (!sendSseFrame(stored, helloEvent, helloPayload, gPushNextEventId++)) {
    stored.client.stop();
    gPushSessions.pop_back();
    return;
  }
  flushPendingLogEntries(); // сразу отдаём накопленные строки журнала новому подписчику
  flushPendingIrqStatus(true); // синхронизируем статус IRQ при подключении клиента
  static uint32_t lastPushLogMs = 0;
  const int32_t sinceLastLog = static_cast<int32_t>(now - lastPushLogMs);
  if (lastPushLogMs == 0 || sinceLastLog < 0 || sinceLastLog >= 30000) {
    LOG_INFO("HTTP push: новое подключение");
    lastPushLogMs = now;
  }
}

// Выдача нового идентификатора для тестовых сообщений
uint32_t nextTestRxmId() {
  static uint32_t counter = 60000;
  counter = (counter + 1) % 100000;
  if (counter == 0) counter = 1;
  return counter;
}

// Генерация идентификатора для сообщений тестового режима SendMsg_BR/received_msg
uint32_t nextTestModeMessageId() {
  static uint32_t counter = 90000;
  counter = (counter + 1) % 100000;
  if (counter == 0) counter = 1;
  return counter;
}

// Разбор текстовых значений 0/1/on/off для команды TESTMODE
bool parseOnOffToken(const String& value, bool& out) {
  String token = value;
  token.trim();
  if (token.length() == 0) return false;
  String lower = token;
  lower.toLowerCase();
  if (lower == "1" || lower == "on" || lower == "true" || lower == "вкл" || lower == "включ" || lower == "да") {
    out = true;
    return true;
  }
  if (lower == "0" || lower == "off" || lower == "false" || lower == "выкл" || lower == "выключ" || lower == "нет") {
    out = false;
    return true;
  }
  if (lower == "toggle" || lower == "swap" || lower == "перекл" || lower == "сменить") {
    out = !testModeEnabled;
    return true;
  }
  return false;
}

// Параметры «унаследованного» формата пакета из SendMsg_BR
static constexpr size_t kLegacyPacketSize = 112;
static constexpr size_t kLegacyHeaderSize = 9;
static constexpr size_t kLegacyPayloadMax = 96;
static constexpr uint8_t kLegacySourceAddress = 1;
static constexpr uint8_t kLegacyBroadcastAddress = 0;

// Представление тестового пакета в формате исторического проекта
struct LegacyTestPacket {
  std::array<uint8_t, kLegacyPacketSize> data{}; // полный буфер пакета
  size_t length = kLegacyHeaderSize;             // фактическая длина
  uint8_t source = kLegacySourceAddress;         // адрес источника
  uint8_t destination = kLegacyBroadcastAddress; // адрес назначения (широковещание)
  bool encrypted = false;                        // признак шифрования
  uint8_t keyIndex = 0;                          // индекс ключа (для шифрования)
};

// Формирование пакета по правилам SendMsg_BR
LegacyTestPacket buildLegacyTestPacket(const std::vector<uint8_t>& payload) {
  LegacyTestPacket packet;
  packet.data.fill(0);
  static uint8_t seqA = 0x2A; // псевдослучайные байты идентификатора пакета
  static uint8_t seqB = 0x7C;
  seqA = static_cast<uint8_t>(seqA + 1);
  seqB = static_cast<uint8_t>(seqB + 3);
  packet.data[1] = seqA;
  packet.data[2] = seqB;
  packet.data[0] = packet.data[1] ^ packet.data[2];
  packet.data[3] = packet.source;
  packet.data[4] = packet.destination;
  packet.data[5] = 0; // флаг подтверждения не используется
  packet.data[6] = packet.encrypted ? 1 : 0;
  packet.data[7] = packet.keyIndex;
  packet.data[8] = testModeLocalCounter;
  testModeLocalCounter = static_cast<uint8_t>(testModeLocalCounter + 1);
  const size_t limit = std::min(payload.size(), static_cast<size_t>(kLegacyPayloadMax));
  if (limit > 0) {
    std::copy(payload.begin(), payload.begin() + limit, packet.data.begin() + kLegacyHeaderSize);
  }
  packet.length = kLegacyHeaderSize + limit;
  return packet;
}

// Формирование строки для чата в стиле received_msg
String formatLegacyDisplayString(const LegacyTestPacket& packet, const String& decoded) {
  String text = decoded;
  if (text.length() == 0) text = String("—");
  String out = String("    ");
  out += text;
  out += "*";
  out += String(static_cast<unsigned int>(packet.source));
  if (packet.encrypted) {
    out += " ~ucr~";
  }
  out += "*  #";
  out += text;
  return out;
}

// Эмуляция полного цикла SendMsg_BR → received_msg с сохранением в буфер
bool testModeProcessMessage(const String& payload, uint32_t& outId, String& err) {
  String trimmed = payload;
  trimmed.trim();
  if (trimmed.length() == 0) {
    err = "пустое сообщение";
    return false;
  }
  std::vector<uint8_t> cp = utf8ToCp1251(trimmed.c_str());
  if (cp.empty()) {
    err = "пустое сообщение";
    return false;
  }
  if (cp.size() > kLegacyPayloadMax) {
    cp.resize(kLegacyPayloadMax);
  }
  LegacyTestPacket packet = buildLegacyTestPacket(cp);
  size_t payloadLen = packet.length > kLegacyHeaderSize ? packet.length - kLegacyHeaderSize : 0;
  std::vector<uint8_t> decodedSource;
  if (payloadLen > 0) {
    decodedSource.assign(packet.data.begin() + kLegacyHeaderSize,
                        packet.data.begin() + kLegacyHeaderSize + payloadLen);
  }
  String decoded = decodeCp1251ToString(decodedSource);
  if (decoded.length() == 0) decoded = trimmed;
  String display = formatLegacyDisplayString(packet, decoded);
  std::vector<uint8_t> readyBytes = utf8ToCp1251(display.c_str());
  if (readyBytes.empty()) {
    const char* raw = display.c_str();
    size_t rawLen = static_cast<size_t>(display.length());
    readyBytes.resize(rawLen);
    if (rawLen > 0) {
      std::memcpy(readyBytes.data(), raw, rawLen);
    }
  }
  outId = nextTestModeMessageId();
  recvBuf.pushRaw(outId, 0, packet.data.data(), packet.length);
  recvBuf.pushReady(outId, readyBytes.data(), readyBytes.size());
  String log = String("TESTMODE id=");
  log += String(outId);
  log += " len=";
  log += String(static_cast<unsigned int>(packet.length));
  SimpleLogger::logStatus(std::string(log.c_str()));
  Serial.print("TESTMODE RX: ");
  Serial.println(display);
  return true;
}

// Формирование тестового текста с указанным шаблоном
void setTestRxmSourceText(const String& text) {
  String normalized = text;
  normalized.trim();
  if (normalized.length() == 0) {
    testRxmSourceText = "";
    DEBUG_LOG("TESTRXM: используется стандартный шаблон");
    return;
  }
  const size_t limit = kTestRxmMaxSourceLength;
  const size_t normalizedLen = static_cast<size_t>(normalized.length());
  if (normalizedLen > limit) {
    normalized.remove(static_cast<unsigned int>(limit));
  }
  testRxmSourceText = normalized;
  DEBUG_LOG_VAL("TESTRXM: пользовательский шаблон, длина=", static_cast<int>(testRxmSourceText.length()));
}
String makeTestRxmPayload(uint8_t index, const TestRxmSpec& spec) {
  const char* sourcePtr = nullptr;
  size_t sourceLen = 0;
  if (testRxmSourceText.length() > 0) {                           // есть пользовательский текст
    sourcePtr = testRxmSourceText.c_str();
    sourceLen = static_cast<size_t>(testRxmSourceText.length());
  } else {                                                         // иначе используем стандартный Lorem ipsum
    sourcePtr = kTestRxmLorem;
    sourceLen = strlen(kTestRxmLorem);
  }
  if (sourceLen == 0) {                                           // страховка от пустого шаблона
    sourcePtr = kTestRxmLorem;
    sourceLen = strlen(kTestRxmLorem);
  }
  size_t take = (sourceLen * spec.percent + 99) / 100;             // округляем вверх
  if (take == 0) take = sourceLen;                                 // страховка на случай нуля
  if (take > sourceLen) take = sourceLen;                          // ограничение длиной источника
  String payload;
  const size_t prefixReserve = spec.useGatherer ? 96 : 64;         // запас под префикс и UTF-8
  payload.reserve(static_cast<unsigned int>(prefixReserve + take)); // резерв памяти под строку
  payload += "RXM Lorem ";
  payload += String(static_cast<int>(index + 1));
  payload += " (";
  payload += String(static_cast<int>(spec.percent));
  payload += "%";
  if (spec.useGatherer) payload += ", сборщик";
  payload += "): ";
  for (size_t i = 0; i < take; ++i) {                              // добавляем нужную долю текста
    payload += sourcePtr[i];
  }
  return payload;
}

// Добавление тестового сообщения в буфер с опциональной эмуляцией разбиения
void enqueueTestRxmMessage(uint32_t id, const TestRxmSpec& spec, const String& payload) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(payload.c_str());
  const size_t len = static_cast<size_t>(payload.length());
  if (len == 0) return;                                            // пустые сообщения не добавляем

  if (!spec.useGatherer) {                                         // простое готовое сообщение
    recvBuf.pushReady(id, data, len);
    return;
  }

  PacketGatherer gatherer(PayloadMode::SMALL, DefaultSettings::GATHER_BLOCK_SIZE);
  gatherer.reset();                                                // сбрасываем внутреннее состояние
  const size_t chunk = DefaultSettings::GATHER_BLOCK_SIZE;         // размер части для "эфира"
  size_t offset = 0;
  uint32_t part = 0;
  while (offset < len) {                                           // нарезаем на части
    size_t take = std::min(chunk, len - offset);
    recvBuf.pushRaw(id, part, data + offset, take);                // сырые пакеты R-xxxxxx
    gatherer.add(data + offset, take);                             // передаём части сборщику
    offset += take;
    ++part;
  }
  const auto& combined = gatherer.get();                           // собранный результат SP-xxxxx
  if (!combined.empty()) {
    recvBuf.pushSplit(id, combined.data(), combined.size());
    recvBuf.pushReady(id, combined.data(), combined.size());       // финальный GO-xxxxx
  } else {
    recvBuf.pushReady(id, data, len);                              // fallback на случай пустой сборки
  }
}

// Формирование JSON с состоянием ключа
String makeKeyStateJson() {
  KeyLoader::KeyState st = KeyLoader::getState();
  auto idBytes = KeyLoader::keyId(st.session_key);
  String json = "{";
  json += "\"valid\":";
  json += st.valid ? "true" : "false";
  json += ",\"type\":\"";
  json += (st.origin == KeyLoader::KeyOrigin::REMOTE ? "external" : "local");
  json += "\",\"id\":\"";
  json += toHex(idBytes);
  json += "\",\"public\":\"";
  json += toHex(st.root_public);
  json += ",\"hasBackup\":";
  json += st.has_backup ? "true" : "false";
  json += ",\"storage\":\"";
  json += KeyLoader::backendName(st.backend);
  json += "\",\"preferred\":\"";
  auto preferredBackend = KeyLoader::getPreferredBackend();
  if (preferredBackend == KeyLoader::StorageBackend::UNKNOWN) {
    json += "auto";
  } else {
    json += KeyLoader::backendName(preferredBackend);
  }
  json += "\"";
  if (st.origin == KeyLoader::KeyOrigin::REMOTE) {
    json += ",\"peer\":\"";
    json += toHex(st.peer_public);
    json += "\"";
  }
  json += ",\"baseKey\":\"";
  json += toHex(gConfig.keys.defaultKey);
  json += "\"";
  json += ",\"safeMode\":";
  json += keySafeModeActive ? "true" : "false";
  json += ",\"storageReady\":";
  json += keyStorageReady ? "true" : "false";
  if (keySafeModeActive && keySafeModeHasReason()) {
    json += ",\"safeModeContext\":\"";
    json += escapeJson(keySafeModeReason());
    json += "\"";
  }
  json += "}";
  return json;
}

// Короткий ответ о бэкенде хранения ключей для UI/CLI
String makeKeyStorageStatusJson() {
  auto active = KeyLoader::getBackend();
  auto preferred = KeyLoader::getPreferredBackend();
  String json = "{";
  json += "\"storage\":\"";
  json += KeyLoader::backendName(active);
  json += "\",\"preferred\":\"";
  if (preferred == KeyLoader::StorageBackend::UNKNOWN) {
    json += "auto";
  } else {
    json += KeyLoader::backendName(preferred);
  }
  json += "\"";
  json += ",\"safeMode\":";
  json += keySafeModeActive ? "true" : "false";
  json += ",\"ready\":";
  json += keyStorageReady ? "true" : "false";
  if (keySafeModeActive && keySafeModeHasReason()) {
    json += ",\"context\":\"";
    json += escapeJson(keySafeModeReason());
    json += "\"";
  }
  json += "}";
  return json;
}

void reloadCryptoModules() {
  tx.reloadKey();
  rx.reloadKey();
}

// Унифицированный JSON-ответ при заблокированных операциях с ключами
String keySafeModeErrorJson() {
  String json = "{\"error\":\"key-safe-mode\",\"message\":\"хранилище ключей недоступно\",\"safeMode\":true";
  if (keySafeModeHasReason()) {
    json += ",\"context\":\"";
    json += escapeJson(keySafeModeReason());
    json += "\"";
  }
  json += "}";
  return json;
}

// Проверка доступности операций с ключами (true — можно продолжать)
bool keyOperationsAllowed() {
  return keyStorageReady && !keySafeModeActive;
}

// Переинициализация хранилища ключей с учётом защищённого режима
bool ensureKeyStorageReady(const char* reason) {
  bool ok = KeyLoader::ensureStorage();
  if (ok) {
    deactivateKeySafeMode(reason);
  } else {
    activateKeySafeMode(reason);
  }
  return ok;
}

// Пытаемся автоматически восстановить доступ к хранилищу перед выполнением команд с ключами
bool ensureKeyOperationsAvailable(const char* reason) {
  if (keyOperationsAllowed()) {
    return true;
  }
  return ensureKeyStorageReady(reason);
}

// Генерация случайного идентификатора сообщения для обмена ключами
uint32_t generateKeyTransferMsgId() {
  uint32_t id = 0;
  id |= static_cast<uint32_t>(radio.randomByte());
  id |= static_cast<uint32_t>(radio.randomByte()) << 8;
  id |= static_cast<uint32_t>(radio.randomByte()) << 16;
  id |= static_cast<uint32_t>(radio.randomByte()) << 24;
  return id;
}

String readVersionFile() {
#ifndef ARDUINO
  const char* candidates[] = {"ver", "ver/ver"};
  for (const char* path : candidates) {
    std::ifstream f(path);
    if (!f.good()) continue;
    std::string line;
    if (!std::getline(f, line)) continue;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.empty()) continue;
    String text(line.c_str());
    text.trim();
    if (text.length() >= 2) {
      char first = text.charAt(0);
      char last = text.charAt(text.length() - 1);
      if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
        text = text.substring(1, text.length() - 1);
        text.trim();
      }
    }
    if (text.length()) return text;
  }
  String fallback(kEmbeddedVersion);
  fallback.trim();
  if (fallback.length() >= 2) {
    char first = fallback.charAt(0);
    char last = fallback.charAt(fallback.length() - 1);
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
      fallback = fallback.substring(1, fallback.length() - 1);
      fallback.trim();
    }
  }
  if (fallback.length()) return fallback;
  return String("unknown");
#else
  String fallback(FPSTR(kEmbeddedVersion));
  fallback.trim();
  if (fallback.length() >= 2) {
    char first = fallback.charAt(0);
    char last = fallback.charAt(fallback.length() - 1);
    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
      fallback = fallback.substring(1, fallback.length() - 1);
      fallback.trim();
    }
  }
  if (fallback.length()) return fallback;
  return String("unknown");
#endif
}

enum class KeyTransferCommandOrigin {
  Http,
  Serial,
};

static constexpr uint32_t kKeyTransferReceiveTimeoutMs = 8000; // тайм-аут ожидания кадра KEYTRANSFER

// Формирование JSON с ошибкой обмена ключами
String makeKeyTransferErrorJson(const String& code) {
  String response = String(F("{\"error\":\"")) + code + F("\"");
  if (code.equalsIgnoreCase("verify")) {
    response += F(",\"message\":\"несовпадение идентификаторов ключей\"");
  } else if (code.equalsIgnoreCase("timeout")) {
    response += F(",\"message\":\"истёк тайм-аут ожидания кадра\"");
  } else if (code.equalsIgnoreCase("busy")) {
    response += F(",\"message\":\"предыдущий результат KEYTRANSFER ещё не получен\"");
  } else if (code.equalsIgnoreCase("ephemeral")) {
    response += F(",\"message\":\"не удалось подготовить эпемерную пару\"");
  }
  response += F("}");
  return response;
}

// Формирование JSON для состояния ожидания
String makeKeyTransferWaitingJson() {
  String response = F("{\"status\":\"waiting\"");
  uint32_t timeout_ms = keyTransferWaiter.timeoutMs();
  if (timeout_ms > 0) {
    response += F(",\"timeout\":");
    response += String(static_cast<unsigned long>(timeout_ms));
  }
  uint32_t now = millis();
  uint32_t elapsed_ms = 0;
  bool have_elapsed = false;
  if (keyTransferWaiter.isWaiting()) {
    uint32_t started = keyTransferWaiter.startedAtMs();
    if (started != 0) {
      elapsed_ms = static_cast<uint32_t>(now - started);
      have_elapsed = true;
      response += F(",\"elapsed\":");
      response += String(static_cast<unsigned long>(elapsed_ms));
    }
    if (timeout_ms == 0 && keyTransferRuntime.waitDeadlineAt != 0) {
      uint32_t remaining = keyTransferWaiter.isExpired(now)
                               ? 0
                               : static_cast<uint32_t>(keyTransferRuntime.waitDeadlineAt - now);
      response += F(",\"remaining\":");
      response += String(static_cast<unsigned long>(remaining));
    }
  }
  if (timeout_ms > 0) {
    uint32_t remaining = timeout_ms;
    if (have_elapsed) {
      remaining = (elapsed_ms >= timeout_ms) ? 0 : static_cast<uint32_t>(timeout_ms - elapsed_ms);
    }
    response += F(",\"remaining\":");
    response += String(static_cast<unsigned long>(remaining));
  }
  response += F("}");
  return response;
}

// Обработка специального кадра KEYTRANSFER; возвращает true, если пакет потреблён
bool handleKeyTransferFrame(const uint8_t* data, size_t len) {
  uint32_t msg_id = 0;
  KeyTransfer::FramePayload payload;
  if (!KeyTransfer::parseFrame(data, len, payload, msg_id)) {
    return false;                                  // кадр не относится к обмену ключами
  }
  keyTransferRuntime.last_msg_id = msg_id;
  keyTransferRuntime.legacy_peer = (payload.version == KeyTransfer::VERSION_LEGACY);
  if (!keyTransferRuntime.waiting) {
    SimpleLogger::logStatus("KEYTRANSFER IGN");   // пришёл неожиданный ключ
    return true;                                   // не передаём дальше в RxModule
  }
  if ((payload.flags & KeyTransfer::FLAG_HAS_CERTIFICATE) != 0 ||
      payload.version == KeyTransfer::VERSION_CERTIFICATE) {
    std::string cert_error;
    if (!KeyTransfer::verifyCertificateChain(payload.public_key, payload.certificate, &cert_error)) {
      keyTransferRuntime.error = String("cert");
      keyTransferRuntime.waiting = false;
      keyTransferRuntime.completed = false;
      if (keyTransferRuntime.ephemeral_active) {
        KeyLoader::endEphemeralSession();
        keyTransferRuntime.ephemeral_active = false;
      }
      resetKeyTransferWaitTiming();
      keyTransferWaiter.finalizeError(toStdString(makeKeyTransferErrorJson(keyTransferRuntime.error)));
      SimpleLogger::logStatus("KEYTRANSFER CERT ERR");
      Serial.print("KEYTRANSFER: ошибка проверки сертификата: ");
      Serial.println(cert_error.c_str());
      return true;
    }
  } else if (KeyTransfer::hasTrustedRoot()) {
    Serial.println("KEYTRANSFER: предупреждение — нет сертификата при наличии доверенного корня");
  }

  KeyLoader::KeyRecord previous_snapshot;          // сохраняем снимок до применения удалённого ключа
  bool has_previous_snapshot = KeyLoader::loadKeyRecord(previous_snapshot);
  const std::array<uint8_t,32>* remote_ephemeral = payload.has_ephemeral ? &payload.ephemeral_public : nullptr;
  if (!KeyLoader::applyRemotePublic(payload.public_key, remote_ephemeral)) {
    keyTransferRuntime.error = String("apply");
    keyTransferRuntime.waiting = false;
    keyTransferRuntime.completed = false;
    if (keyTransferRuntime.ephemeral_active) {
      KeyLoader::endEphemeralSession();
      keyTransferRuntime.ephemeral_active = false;
    }
    resetKeyTransferWaitTiming();
    keyTransferWaiter.finalizeError(toStdString(makeKeyTransferErrorJson(keyTransferRuntime.error)));
    SimpleLogger::logStatus("KEYTRANSFER ERR");
    Serial.println("KEYTRANSFER: ошибка применения удалённого ключа");
    return true;
  }
  bool skip_remote_check = (payload.version == KeyTransfer::VERSION_EPHEMERAL) ||
                           std::all_of(payload.key_id.begin(), payload.key_id.end(), [](uint8_t b) {
                             return b == 0;
                           });
  if (!skip_remote_check) {
    auto local_id = KeyLoader::keyId(KeyLoader::loadKey());
    bool ids_match = std::equal(local_id.begin(), local_id.end(), payload.key_id.begin());
    if (!ids_match) {
      bool restored = KeyLoader::restorePreviousKey();
      if (!restored && has_previous_snapshot) {
        restored = KeyLoader::saveKey(previous_snapshot.session_key,
                                      previous_snapshot.origin,
                                      &previous_snapshot.peer_public,
                                      previous_snapshot.nonce_salt);
      }
      if (restored) {
        reloadCryptoModules();                        // возвращаемся к предыдущему ключу
      }
      keyTransferRuntime.completed = false;
      keyTransferRuntime.waiting = false;
      keyTransferRuntime.error = String("verify");
      if (keyTransferRuntime.ephemeral_active) {
        KeyLoader::endEphemeralSession();
        keyTransferRuntime.ephemeral_active = false;
      }
      resetKeyTransferWaitTiming();
      keyTransferWaiter.finalizeError(toStdString(makeKeyTransferErrorJson(keyTransferRuntime.error)));
      std::string log = "KEYTRANSFER MISMATCH ";
      log += toHex(local_id).c_str();
      log += "/";
      log += toHex(payload.key_id).c_str();
      SimpleLogger::logStatus(log);
      Serial.print("KEYTRANSFER: несовпадение идентификаторов (local=");
      Serial.print(toHex(local_id));
      Serial.print(", remote=");
      Serial.print(toHex(payload.key_id));
      Serial.println(")");
      Serial.println("KEYTRANSFER: откатываемся на предыдущий ключ");
      return true;
    }
  }
  reloadCryptoModules();                          // обновляем Tx/Rx новым ключом
  keyTransferRuntime.completed = true;
  keyTransferRuntime.waiting = false;
  keyTransferRuntime.error = String();
  if (keyTransferRuntime.ephemeral_active) {
    KeyLoader::endEphemeralSession();
    keyTransferRuntime.ephemeral_active = false;
  }
  resetKeyTransferWaitTiming();
  keyTransferWaiter.finalizeSuccess(toStdString(makeKeyStateJson()));
  SimpleLogger::logStatus("KEYTRANSFER OK");
  Serial.println("KEYTRANSFER: получен корневой ключ по LoRa");
  return true;
}

String cmdKeyState() {
  DEBUG_LOG("Key: запрос состояния");
  return makeKeyStateJson();
}

// Управление бэкендом хранилища ключей (NVS/auto)
String cmdKeyStorage(const String& mode) {
  String trimmed = mode;
  trimmed.trim();
  if (trimmed.length() == 0) {
    return makeKeyStorageStatusJson();
  }
  if (trimmed.equalsIgnoreCase("RETRY")) {
    LOG_INFO("Key: запрошена повторная проверка хранилища через команду RETRY");
    ensureKeyStorageReady("manual retry");
    return makeKeyStorageStatusJson();
  }
  KeyLoader::StorageBackend backend = KeyLoader::StorageBackend::UNKNOWN;
  if (trimmed.equalsIgnoreCase("AUTO")) {
    backend = KeyLoader::StorageBackend::UNKNOWN;
  } else if (trimmed.equalsIgnoreCase("NVS")) {
    backend = KeyLoader::StorageBackend::NVS;
  } else if (trimmed.equalsIgnoreCase("FS") || trimmed.equalsIgnoreCase("FILESYSTEM")) {
    backend = KeyLoader::StorageBackend::FILESYSTEM;
  } else {
    return String("{\"error\":\"mode\"}");
  }
  if (!KeyLoader::setPreferredBackend(backend)) {
    return String("{\"error\":\"unsupported\"}");
  }
  auto active = KeyLoader::getBackend();
  if (active == KeyLoader::StorageBackend::UNKNOWN) {
    return String("{\"error\":\"unavailable\"}");
  }
  return makeKeyStorageStatusJson();
}

String cmdKeyGenSecure() {
  DEBUG_LOG("Key: генерация нового ключа");
  if (!ensureKeyOperationsAvailable("command KEYGEN")) {
    return keySafeModeErrorJson();
  }
  if (KeyLoader::generateLocalKey()) {
    const bool synced_with_peer = KeyLoader::regenerateFromPeer();
    // Если есть сохранённый ключ удалённой стороны, пересчитываем сессию сразу.
    if (synced_with_peer) {
      DEBUG_LOG("Key: сессия пересчитана по сохранённому ключу удалённой стороны");
    }
    reloadCryptoModules();
    return makeKeyStateJson();
  }
  return String("{\"error\":\"keygen\"}");
}

String cmdKeyGenPeer() {
  DEBUG_LOG("Key: повторное применение удалённого ключа");
  if (!ensureKeyOperationsAvailable("command KEYGEN PEER")) {
    return keySafeModeErrorJson();
  }
  if (KeyLoader::regenerateFromPeer()) {
    reloadCryptoModules();
    return makeKeyStateJson();
  }
  return String("{\"error\":\"peer\"}");
}

String cmdKeyRestoreSecure() {
  DEBUG_LOG("Key: восстановление ключа из резервной копии");
  if (!ensureKeyOperationsAvailable("command KEYRESTORE")) {
    return keySafeModeErrorJson();
  }
  if (KeyLoader::restorePreviousKey()) {
    reloadCryptoModules();
    return makeKeyStateJson();
  }
  return String("{\"error\":\"restore\"}");
}

String cmdKeySendSecure() {
  return cmdKeyTransferSendLora();                   // совместимый вызов через новый механизм
}

String cmdKeyReceiveSecure(const String& hex) {
  DEBUG_LOG("Key: применение удалённого ключа");
  if (!ensureKeyOperationsAvailable("command KEYRECV")) {
    return keySafeModeErrorJson();
  }
  std::array<uint8_t,32> remote{};
  if (!parseHex(hex, remote)) {
    return String("{\"error\":\"format\"}");
  }
  if (!KeyLoader::applyRemotePublic(remote)) {
    return String("{\"error\":\"apply\"}");
  }
  reloadCryptoModules();
  return makeKeyStateJson();
}

// Отправка корневого ключа через LoRa с использованием спецключа
String cmdKeyTransferSendLora() {
  DEBUG_LOG("Key: отправка ключа по LoRa");
  if (!ensureKeyOperationsAvailable("command KEYTRANSFER SEND")) {
    return keySafeModeErrorJson();
  }
  auto state = KeyLoader::getState();
  std::array<uint8_t,4> key_id{};
  std::array<uint8_t,32> ephemeral_public{};
  const std::array<uint8_t,32>* ephemeral_ptr = nullptr;
  bool use_legacy = keyTransferRuntime.legacy_peer;
  if (use_legacy) {
    // При выявленном легаси-пире отправляем старый формат без эпемерного ключа
    KeyLoader::endEphemeralSession();
    bool has_peer = KeyLoader::hasPeerPublic();
    if (has_peer) {
      if (!KeyLoader::previewPeerKeyId(key_id)) {
        SimpleLogger::logStatus("KEYTRANSFER ERR PREVIEW");
        Serial.println("KEYTRANSFER: ошибка расчёта идентификатора ключа");
        return String("{\"error\":\"peer\"}");
      }
    } else {
      key_id.fill(0);
    }
  } else {
    // Для нового обмена формируем эпемерную пару X25519
    if (!KeyLoader::startEphemeralSession(ephemeral_public, true)) {
      SimpleLogger::logStatus("KEYTRANSFER ERR EPHEM");
      Serial.println("KEYTRANSFER: не удалось подготовить эпемерный ключ");
      return String("{\"error\":\"ephemeral\"}");
    }
    key_id.fill(0);  // удалённая сторона проверяет результат после применения кадра
    ephemeral_ptr = &ephemeral_public;
  }
  std::vector<uint8_t> frame;
  uint32_t msg_id = generateKeyTransferMsgId();
  const KeyTransfer::CertificateBundle* cert_ptr = nullptr;
  if (KeyTransfer::hasLocalCertificate()) {
    cert_ptr = &KeyTransfer::getLocalCertificate();
  }
  if (!KeyTransfer::buildFrame(msg_id, state.root_public, key_id, frame, ephemeral_ptr, cert_ptr)) {
    return String("{\"error\":\"build\"}");
  }
  tx.prepareExternalSend();
  int16_t sendState = radio.send(frame.data(), frame.size());
  tx.completeExternalSend();
  if (sendState == RadioSX1262::ERR_TIMEOUT) {
    SimpleLogger::logStatus("KEYTRANSFER ERR RADIO BUSY");
    Serial.println("KEYTRANSFER: радио занято, отправка отклонена");
    return String("{\"error\":\"radio_busy\"}");
  }
  if (sendState != IRadio::ERR_NONE) {
    SimpleLogger::logStatus("KEYTRANSFER ERR RADIO");
    Serial.print("KEYTRANSFER: ошибка отправки, код=");
    Serial.println(sendState);
    return String("{\"error\":\"radio\"}");
  }
  keyTransferRuntime.last_msg_id = msg_id;
  SimpleLogger::logStatus("KEYTRANSFER SEND");
  Serial.println("KEYTRANSFER: отправлен публичный корневой ключ");
  return makeKeyStateJson();
}

// Ожидание и приём корневого ключа через LoRa
String cmdKeyTransferReceiveLora(KeyTransferCommandOrigin origin) {
  DEBUG_LOG("Key: ожидание ключа по LoRa");
  if (!ensureKeyOperationsAvailable("command KEYTRANSFER RECEIVE")) {
    return keySafeModeErrorJson();
  }

  // Если для текущего канала уже подготовлен результат — возвращаем его.
  if (origin == KeyTransferCommandOrigin::Http && keyTransferWaiter.httpPending()) {
    String ready = String(keyTransferWaiter.consumeHttp().c_str());
    return ready.length() ? ready : makeKeyTransferWaitingJson();
  }
  if (origin == KeyTransferCommandOrigin::Serial && keyTransferWaiter.serialPending()) {
    String ready = String(keyTransferWaiter.consumeSerial().c_str());
    return ready.length() ? ready : makeKeyTransferWaitingJson();
  }

  // Если ожидание уже идёт, просто сообщаем статус.
  if (keyTransferRuntime.waiting) {
    return makeKeyTransferWaitingJson();
  }

  // Защита от перезапуска, пока другой канал не забрал результат.
  if (keyTransferWaiter.hasPendingResult()) {
    return makeKeyTransferErrorJson(String("busy"));
  }

  std::array<uint8_t,32> prepared_ephemeral{};
  if (!KeyLoader::startEphemeralSession(prepared_ephemeral, false)) {
    SimpleLogger::logStatus("KEYTRANSFER ERR EPHEM");
    Serial.println("KEYTRANSFER: не удалось подготовить эпемерную пару для ожидания");
    return makeKeyTransferErrorJson(String("ephemeral"));
  }

  // Настраиваем новое ожидание: сохраняем отметки времени и подключаем наблюдателей.
  const uint32_t now = millis();
  keyTransferRuntime.waiting = true;
  keyTransferRuntime.completed = false;
  keyTransferRuntime.error = String();
  keyTransferRuntime.last_msg_id = 0;
  keyTransferRuntime.ephemeral_active = true;
  keyTransferRuntime.waitStartedAt = now;
  keyTransferRuntime.waitDeadlineAt = (kKeyTransferReceiveTimeoutMs > 0)
                                          ? static_cast<uint32_t>(now + kKeyTransferReceiveTimeoutMs)
                                          : 0;
  bool serialWatcher = (origin == KeyTransferCommandOrigin::Serial);
  bool httpWatcher = (origin == KeyTransferCommandOrigin::Http);
  keyTransferWaiter.start(now, kKeyTransferReceiveTimeoutMs, serialWatcher, httpWatcher);
  SimpleLogger::logStatus("KEYTRANSFER WAIT");
  if (origin == KeyTransferCommandOrigin::Serial) {
    Serial.println("KEYTRANSFER: ожидание кадра запущено");
  }
  return makeKeyTransferWaitingJson();
}

// Периодический опрос автомата ожидания KEYTRANSFER из loop()
void processKeyTransferReceiveState() {
  if (keyTransferRuntime.waiting) {
    // При активном ожидании крутим радио, TX и push-сессии, чтобы интерфейс оставался отзывчивым.
    radio.loop();
    rx.tickCleanup();
    tx.loop();
    maintainPushSessions();
    flushPendingLogEntries();
    flushPendingIrqStatus();

    if (keyTransferWaiter.isWaiting()) {
      uint32_t now = millis();
      if (keyTransferWaiter.isExpired(now)) {
        keyTransferRuntime.waiting = false;
        keyTransferRuntime.completed = false;
        keyTransferRuntime.error = String("timeout");
        if (keyTransferRuntime.ephemeral_active) {
          KeyLoader::endEphemeralSession();
          keyTransferRuntime.ephemeral_active = false;
        }
        resetKeyTransferWaitTiming();
        String timeoutJson = makeKeyTransferErrorJson(keyTransferRuntime.error);
        keyTransferWaiter.finalizeError(toStdString(timeoutJson));
        SimpleLogger::logStatus("KEYTRANSFER TIMEOUT");
        Serial.println("KEYTRANSFER: истёк тайм-аут ожидания кадра");
      }
    }
  }

  // После завершения ожидания чистим отметки, чтобы JSON не сообщал устаревшие дедлайны.
  if (!keyTransferRuntime.waiting && keyTransferRuntime.waitDeadlineAt != 0 && !keyTransferWaiter.isWaiting()) {
    resetKeyTransferWaitTiming();
  }

  if (keyTransferWaiter.serialPending()) {
    String ready = String(keyTransferWaiter.consumeSerial().c_str());
    if (ready.length()) {
      Serial.println(ready);
    }
  }
}

// Отдаём страницу index.html
void handleRoot() {
  sendProgmemAsset("text/html; charset=utf-8", INDEX_HTML, progmemLength(INDEX_HTML), false);
}

// Отдаём стили style.css
void handleStyleCss() {
  sendProgmemAsset("text/css; charset=utf-8", STYLE_CSS, progmemLength(STYLE_CSS));
}

// Отдаём скрипт script.js
void handleScriptJs() {
  sendProgmemAsset("application/javascript; charset=utf-8", SCRIPT_JS, progmemLength(SCRIPT_JS));
}

// Отдаём библиотеку SHA-256
void handleSha256Js() {
  sendProgmemAsset("application/javascript; charset=utf-8", SHA256_JS, progmemLength(SHA256_JS));
}

// Отдаём библиотеку преобразования MGRS → широта/долгота
void handleMgrsJs() {
  sendProgmemAsset("application/javascript; charset=utf-8", MGRS_JS, progmemLength(MGRS_JS));
}

// Отдаём CSV-справочник частот каналов
void handleFreqInfoCsv() {
  sendProgmemAsset("text/csv; charset=utf-8", FREQ_INFO_CSV, progmemLength(FREQ_INFO_CSV));
}

// Отдаём постоянный набор TLE для вкладки Pointing
void handleGeostatTleJs() {
  sendProgmemAsset("application/javascript; charset=utf-8", GEOSTAT_TLE_JS, progmemLength(GEOSTAT_TLE_JS));
}

// Принимаем текст и отправляем его по радио
void handleApiTx() {
  if (server.method() != HTTP_POST) {                       // разрешаем только POST
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // Проверяем, что клиент отправил ожидаемый заголовок Content-Type
  String reqContentType = server.hasHeader("Content-Type") ? server.header("Content-Type") : String();
  reqContentType.trim();
  String loweredType = reqContentType;
  loweredType.toLowerCase();
  int delimPos = loweredType.indexOf(';');
  String baseType = delimPos >= 0 ? loweredType.substring(0, delimPos) : loweredType;
  baseType.trim();
  // Relaxed content-type handling: accept common types used by browsers/tools
  bool typeAllowed = (baseType.length() == 0) || (baseType == "text/plain") ||
                     (baseType == "application/json") ||
                     (baseType == "application/x-www-form-urlencoded");
  if (!typeAllowed) {
    server.send(415, "text/plain", "unsupported content-type");
    std::string log = "API TX reject content-type=";
    if (reqContentType.length() > 0) {
      log += reqContentType.c_str();
    } else {
      log += "<missing>";
    }
    SimpleLogger::logStatus(log);
    return;
  }
  // Disable the old strict check below
#if 0
  if (baseType != "text/plain") {
    server.send(415, "text/plain", "unsupported content-type");
    std::string log = "API TX reject content-type=";
    if (reqContentType.length() > 0) {
      log += reqContentType.c_str();
    } else {
      log += "<missing>";
    }
    SimpleLogger::logStatus(log);
    return;
  }
#endif
  String body = server.arg("plain");                       // получаем сырой текст
  uint32_t testId = 0;                                      // идентификатор сообщения эмуляции
  String testErr;                                           // текст ошибки эмуляции
  bool simulated = false;                                   // выполнена ли эмуляция
  if (testModeEnabled) {
    simulated = testModeProcessMessage(body, testId, testErr);
    if (!simulated && testErr.length() > 0) {               // записываем сбой в журнал
      std::string log = "TESTMODE: ошибка эмуляции — ";
      log += testErr.c_str();
      SimpleLogger::logStatus(log);
    }
  }
  uint32_t id = 0;                                          // идентификатор реальной очереди
  String err;                                               // текст ошибки постановки
  if (enqueueTextMessage(body, id, err)) {                  // повторно используем логику Serial-команды
    String resp = "ok:";
    resp += String(id);
    server.send(200, "text/plain", resp);
  } else {
    if (testModeEnabled) {                                  // добавляем информацию о тестовом режиме
      if (simulated) {
        err += " (test id=";
        err += String(testId);
        err += ")";
      } else if (testErr.length() > 0) {
        err += " (testmode: ";
        err += testErr;
        err += ")";
      }
    }
    server.send(400, "text/plain", err);                   // читаемая причина ошибки
  }
}

// Приём JPEG-изображения и постановка его в очередь передачи
void handleApiTxImage() {
  if (server.method() != HTTP_POST) {                       // поддерживаем только POST
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // Контролируем MIME-типы ещё до чтения тела запроса
  String reqContentType = server.hasHeader("Content-Type") ? server.header("Content-Type") : String();
  reqContentType.trim();
  String loweredType = reqContentType;
  loweredType.toLowerCase();
  int delimPos = loweredType.indexOf(';');
  String baseType = delimPos >= 0 ? loweredType.substring(0, delimPos) : loweredType;
  baseType.trim();
  // Accept broader set of image content-types; exact validation is done later by isLikelyJpeg
  static const char* kAllowedImageTypes[] = {"image/jpeg", "image/pjpeg", "image/jpg", "image/jfif"};
  bool typeAllowed = false;
  for (const char* allowed : kAllowedImageTypes) {
    if (baseType == allowed) {
      typeAllowed = true;
      break;
    }
  }
  if (!typeAllowed) {
    if (baseType == String("application/octet-stream") || baseType == String("binary/octet-stream") || baseType.startsWith("image/")) {
      std::string log = "IMG TX unexpected content-type=";
      if (reqContentType.length() > 0) {
        log += reqContentType.c_str();
      } else {
        log += "<missing>";
      }
      SimpleLogger::logStatus(log);
      // proceed; payload will be validated by signature check
    } else {
      server.send(415, "text/plain", "unsupported content-type");
      std::string log = "IMG TX reject content-type=";
      if (reqContentType.length() > 0) {
        log += reqContentType.c_str();
      } else {
        log += "<missing>";
      }
      SimpleLogger::logStatus(log);
      return;
    }
  }
  constexpr size_t kContentLengthUnknown = static_cast<size_t>(-1);
  size_t contentLength = kContentLengthUnknown;               // Body size from Content-Length header if provided
  if (server.hasHeader("Content-Length")) {
    long headerValue = server.header("Content-Length").toInt();
    if (headerValue >= 0) {
      contentLength = static_cast<size_t>(headerValue);
    }
  }
  static const size_t kImageMaxPayload = []() {
    PacketSplitter splitter(PayloadMode::LARGE);
    return DefaultSettings::TX_QUEUE_CAPACITY * splitter.payloadSize();
  }();                                                     // реальный потолок для очереди
  if (contentLength != kContentLengthUnknown && contentLength == 0) {
    server.send(400, "text/plain", "empty");
    return;
  }
  if (contentLength != kContentLengthUnknown && contentLength > kImageMaxPayload) {
    String reason = "image exceeds queue capacity (max ";
    reason += String(static_cast<unsigned long>(kImageMaxPayload));
    reason += " bytes)";
    server.send(413, "text/plain", reason);
    std::string log = "IMG TX reject bytes=" + std::to_string(contentLength) +
                      " limit=" + std::to_string(kImageMaxPayload);
    SimpleLogger::logStatus(log);
    return;
  }
  auto client = server.client();                            // поток байтов HTTP-запроса
  std::vector<uint8_t> payload;
  bool lengthExceeded = false;
  bool incomplete = false;
  auto delayFn = []() { delay(1); };                        // пауза между чтениями
  if (!HttpBodyReader::readBodyToVector(client,
                                        contentLength,
                                        kImageMaxPayload,
                                        payload,
                                        delayFn,
                                        lengthExceeded,
                                        incomplete)) {
    if (lengthExceeded) {
      String reason = "image exceeds queue capacity (max ";
      reason += String(static_cast<unsigned long>(kImageMaxPayload));
      reason += " bytes)";
      server.send(413, "text/plain", reason);
      size_t reportedSize = (contentLength != kContentLengthUnknown)
                                ? contentLength
                                : payload.size();
      std::string log = "IMG TX reject bytes=" + std::to_string(reportedSize) +
                        " limit=" + std::to_string(kImageMaxPayload);
      SimpleLogger::logStatus(log);
      return;
    }
    if (payload.empty()) {
      server.send(400, "text/plain", "empty");
      return;
    }
    server.send(400, "text/plain", "short read");
    std::string log = "IMG TX short-read bytes=" + std::to_string(payload.size());
    if (incomplete) {
      log += " incomplete";
    }
    SimpleLogger::logStatus(log);
    return;
  }
  if (!isLikelyJpeg(payload.data(), payload.size())) {
    server.send(415, "text/plain", "not jpeg");
    std::string log = "IMG TX reject body-sig bytes=" + std::to_string(payload.size());
    SimpleLogger::logStatus(log);
    return;
  }
  // Читаем метаданные изображения из заголовков (необязательные поля)
  String profile = server.hasHeader("X-Image-Profile") ? server.header("X-Image-Profile") : String();
  profile.trim();
  profile.toUpperCase();
  String frameW = server.hasHeader("X-Image-Frame-Width") ? server.header("X-Image-Frame-Width") : String();
  String frameH = server.hasHeader("X-Image-Frame-Height") ? server.header("X-Image-Frame-Height") : String();
  String origSize = server.hasHeader("X-Image-Original-Size") ? server.header("X-Image-Original-Size") : String();
  uint32_t id = 0;
  String err;
  if (enqueueBinaryMessage(payload.data(), payload.size(), id, err)) {
    String resp = "IMG:OK id=";
    resp += String(id);
    resp += " bytes=";
    resp += String(static_cast<unsigned long>(payload.size()));
    if (profile.length() > 0) {
      resp += " profile=";
      resp += profile;
    }
    server.send(200, "text/plain", resp);
    std::string log = "IMG TX id=" + std::to_string(id) + " bytes=" + std::to_string(payload.size());
    if (profile.length() > 0) {
      log += " profile=";
      log += profile.c_str();
    }
    if (origSize.length() > 0) {
      log += " orig=";
      log += origSize.c_str();
    }
    if (frameW.length() > 0 && frameH.length() > 0) {
      log += " frame=";
      log += frameW.c_str();
      log += "x";
      log += frameH.c_str();
    }
    SimpleLogger::logStatus(log);                            // фиксируем отправку в журнале
  } else {
    server.send(400, "text/plain", err);
  }
}

// Преобразование символа в значение перечисления банка каналов
ChannelBank parseBankChar(char b) {
  switch (b) {
    case 'e': case 'E': return ChannelBank::EAST;
    case 'w': case 'W': return ChannelBank::WEST;
    case 't': case 'T': return ChannelBank::TEST;
    case 'a': case 'A': return ChannelBank::ALL;
    case 'h': case 'H': return ChannelBank::HOME;
    default: return radio.getBank();
  }
}

// Генерация сообщения для теста входящего буфера
void processTestRxm() {
  if (!testRxmState.active) return;                        // генерация не активна
  uint32_t now = millis();
  int32_t delta = static_cast<int32_t>(now - testRxmState.nextAt);
  if (delta < 0) return;                                   // ещё рано

  const uint8_t index = testRxmState.produced;
  if (index >= kTestRxmCount) {                            // защита от выхода за границы
    testRxmState.active = false;
    testRxmState.nextAt = 0;
    return;
  }

  const TestRxmSpec& spec = kTestRxmSpecs[index];          // выбираем сценарий
  String payload = makeTestRxmPayload(index, spec);        // формируем полезную нагрузку
  const uint32_t id = nextTestRxmId();                     // выделяем идентификатор
  enqueueTestRxmMessage(id, spec, payload);                // добавляем в буфер

  DEBUG_LOG("TESTRXM: создано входящее сообщение");
  DEBUG_LOG_VAL("TESTRXM: id=", id);
  DEBUG_LOG_VAL("TESTRXM: bytes=", static_cast<int>(payload.length()));
  if (spec.useGatherer) {
    DEBUG_LOG("TESTRXM: сообщение имитирует поступление частями");
  }

  testRxmState.produced++;
  if (testRxmState.produced >= kTestRxmCount) {
    testRxmState.active = false;
    testRxmState.nextAt = 0;
    DEBUG_LOG("TESTRXM: серия завершена");
  } else {
    testRxmState.nextAt = now + 500;                       // следующее сообщение через 0.5 с
  }
}

// Команда TESTRXM — запускает генерацию пяти тестовых входящих сообщений
String cmdTestRxm(const String* overrideText) {
  if (overrideText != nullptr) {
    setTestRxmSourceText(*overrideText);
  }
  if (testRxmState.active) {
    return String("{\"status\":\"busy\"}");
  }
  testRxmState.active = true;
  testRxmState.produced = 0;
  testRxmState.nextAt = millis();
  DEBUG_LOG("TESTRXM: запуск генерации серии сообщений");
  String json = "{\"status\":\"scheduled\",\"count\":";
  json += String(static_cast<int>(kTestRxmCount));
  json += ",\"intervalMs\":500}";
  return json;
}

String cmdTestRxm() {
  return cmdTestRxm(nullptr);
}

// Выполнение одиночного пинга и получение результата
String cmdPing() {
  std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> ping{};
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();
  ping[0] = ping[1] ^ ping[2];
  ping[3] = 0;
  ping[4] = 0;
  std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> resp{};
  size_t respLen = 0;
  uint32_t elapsed = 0;
  tx.prepareExternalSend();
  int16_t pingState = radio.ping(ping.data(), ping.size(),
                                 resp.data(), resp.size(),
                                 respLen, DefaultSettings::PING_WAIT_MS * 1000UL,
                                 elapsed);
  tx.completeExternalSend();
  if (pingState == RadioSX1262::ERR_TIMEOUT) {
    return String("Ping: radio busy");
  }
  if (pingState == RadioSX1262::ERR_PING_TIMEOUT) {
    return String("Ping: timeout");
  }
  if (pingState != IRadio::ERR_NONE) {
    String out = "Ping: error=";
    out += String(pingState);
    return out;
  }
  if (respLen == ping.size() &&
      memcmp(resp.data(), ping.data(), ping.size()) == 0) {
    float dist_km = ((elapsed * 0.000001f) * 299792458.0f / 2.0f) / 1000.0f;
    String out = "Ping: RSSI ";
    out += String(radio.getLastRssi());
    out += " dBm SNR ";
    out += String(radio.getLastSnr());
    out += " dB distance:~";
    out += String(dist_km);
    out += "km time:";
    out += String(elapsed * 0.001f);
    out += "ms";
    return out;
  } else {
    return String("Ping: mismatch");
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
    std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> resp{};
    size_t respLen = 0;
    uint32_t elapsed = 0;
    tx.prepareExternalSend();
    int16_t pingState = radio.ping(pkt.data(), pkt.size(),
                                   resp.data(), resp.size(),
                                   respLen, DefaultSettings::PING_WAIT_MS * 1000UL,
                                   elapsed);
    tx.completeExternalSend();
    (void)respLen; // длина не нужна при проверке ок
    (void)elapsed; // время в поиске не используется
    if (pingState == IRadio::ERR_NONE) {
      out += "CH "; out += String(ch); out += ": RSSI ";
      out += String(radio.getLastRssi()); out += " SNR ";
      out += String(radio.getLastSnr()); out += "\n";
    } else if (pingState == RadioSX1262::ERR_TIMEOUT) {
      out += "CH "; out += String(ch); out += ": радио занято\n";
    } else if (pingState == RadioSX1262::ERR_PING_TIMEOUT) {
      out += "CH "; out += String(ch); out += ": тайм-аут\n";
    } else {
      out += "CH "; out += String(ch); out += ": err=";
      out += String(pingState); out += "\n";
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

// Преобразование текущего банка в букву для ответов HTTP
String bankToLetter(ChannelBank bank) {
  switch (bank) {
    case ChannelBank::EAST: return "e";
    case ChannelBank::WEST: return "w";
    case ChannelBank::TEST: return "t";
    case ChannelBank::ALL:  return "a";
    case ChannelBank::HOME: return "h";
    default:                return "";
  }
}

// Краткое текстовое представление состояния ACK
String ackStateText() {
  return ackEnabled ? String("ACK:1") : String("ACK:0");
}

// Текущее состояние режима Light pack
String lightPackStateText() {
  return lightPackMode ? String("LIGHT:1") : String("LIGHT:0");
}

// Общая функция постановки текстового сообщения в очередь
bool enqueueTextMessage(const String& payload, uint32_t& outId, String& err) {
  String trimmed = payload;
  trimmed.trim();
  if (trimmed.length() == 0) {
    err = "пустое сообщение";
    return false;
  }
  std::vector<uint8_t> data = utf8ToCp1251(trimmed.c_str());
  if (data.empty()) {
    err = "пустое сообщение";
    return false;
  }
  uint32_t id = 0;
  if (lightPackMode) {                                   // Light pack пытается отправить сообщение без префикса
    id = tx.queuePlain(data.data(), data.size());
    if (id == 0) {                                       // если не удалось (слишком длинно или очередь занята)
      id = tx.queue(data.data(), data.size(), 0, true);  // откатываемся к стандартному режиму с префиксом
    }
  } else {
    id = tx.queue(data.data(), data.size(), 0, true);
  }
  if (id == 0) {
    err = "очередь заполнена";
    return false;
  }
  tx.loop();
  outId = id;
  return true;
}

// Постановка бинарного буфера (например, JPEG) в очередь передачи
bool enqueueBinaryMessage(const uint8_t* data, size_t len, uint32_t& outId, String& err) {
  if (!data || len == 0) {
    err = "пустой буфер";
    return false;
  }
  tx.setPayloadMode(PayloadMode::LARGE);                    // даём больше места под кадр
  uint32_t id = tx.queue(data, len);
  tx.setPayloadMode(PayloadMode::SMALL);                    // возвращаем стандартный режим
  if (id == 0) {
    err = "очередь заполнена";
    return false;
  }
  tx.loop();
  outId = id;
  return true;
}

// Команда TX с возвратом результата для HTTP-интерфейса
String cmdTx(const String& payload) {
  uint32_t testId = 0;                               // идентификатор эмуляции тестового режима
  String testErr;                                    // текст ошибки тестового режима
  bool simulated = false;                            // удалось ли выполнить эмуляцию
  if (testModeEnabled) {
    simulated = testModeProcessMessage(payload, testId, testErr);
    if (!simulated && testErr.length() > 0) {        // логируем сбой эмуляции
      std::string log = "TESTMODE: ошибка эмуляции — ";
      log += testErr.c_str();
      SimpleLogger::logStatus(log);
    }
  }

  uint32_t id = 0;                                   // идентификатор реальной отправки
  String err;                                        // причина ошибки очереди
  if (enqueueTextMessage(payload, id, err)) {
    String ok = "TX:OK id=";                         // сохраняем исходный формат ответа
    ok += String(id);
    return ok;
  }

  String out = "TX:ERR ";
  out += err;                                        // сообщаем о реальной ошибке отправки
  if (testModeEnabled) {
    if (simulated) {                                 // добавляем информацию об эмуляции
      out += " (test id=";
      out += String(testId);
      out += ")";
    } else if (testErr.length() > 0) {
      out += " (testmode: ";
      out += testErr;
      out += ")";
    }
  }
  return out;
}

// Команда TXL формирует тестовый пакет указанного размера
String cmdTxl(size_t sz) {
  if (sz == 0 || sz > 8192) {
    return String("TXL: неверный размер");
  }
  std::vector<uint8_t> data(sz);
  for (size_t i = 0; i < sz; ++i) {
    data[i] = static_cast<uint8_t>(i & 0xFF);
  }
  tx.setPayloadMode(PayloadMode::LARGE);
  uint32_t id = tx.queue(data.data(), data.size());
  tx.setPayloadMode(PayloadMode::SMALL);
  if (id == 0) {
    return String("TXL: очередь занята");
  }
  tx.loop();
  String out = "TXL:OK id=";
  out += String(id);
  out += " size=";
  out += String(sz);
  return out;
}

// Управление тестовым режимом эмуляции SendMsg_BR/received_msg
String cmdTestMode(const String& arg) {
  String token = arg;
  token.trim();
  if (token.length() == 0) {
    String resp = "TESTMODE:";
    resp += testModeEnabled ? "1" : "0";
    return resp;
  }
  bool desired = testModeEnabled;
  if (!parseOnOffToken(token, desired)) {
    return String("TESTMODE:ERR");
  }
  bool changed = desired != testModeEnabled;
  testModeEnabled = desired;
  if (changed) {
    testModeLocalCounter = 0;                                  // сбрасываем локальный счётчик
    SimpleLogger::logStatus(testModeEnabled ? "TESTMODE ON" : "TESTMODE OFF");
    Serial.print("TESTMODE: ");
    Serial.println(testModeEnabled ? "включён" : "выключен");
  }
  String resp = "TESTMODE:";
  resp += testModeEnabled ? "1" : "0";
  return resp;
}

// Команда BCN отправляет служебный маяк
String cmdBcn() {
  tx.prepareExternalSend();
  int16_t state = radio.sendBeacon();
  tx.completeExternalSend();
  if (state == RadioSX1262::ERR_TIMEOUT) {
    return String("BCN:BUSY");
  }
  if (state != IRadio::ERR_NONE) {
    String resp = "BCN:ERR";
    resp += state;
    return resp;
  }
  return String("BCN:OK");
}

// Команда ENCT повторяет тест шифрования и возвращает подробности
String cmdEnct() {
  const uint8_t key[16]   = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  auto nonce = KeyLoader::makeNonce(0, 0, 0, 0);
  const char* text = "Test ENCT";
  size_t len = strlen(text);
  std::vector<uint8_t> cipher, tag, plain;
  bool enc = crypto::chacha20poly1305::encrypt(key, sizeof(key), nonce.data(), nonce.size(),
                                               nullptr, 0,
                                               reinterpret_cast<const uint8_t*>(text), len,
                                               cipher, tag);
  bool dec = false;
  if (enc) {
    dec = crypto::chacha20poly1305::decrypt(key, sizeof(key), nonce.data(), nonce.size(),
                                            nullptr, 0,
                                            cipher.data(), cipher.size(),
                                            tag.data(), tag.size(), plain);
  }
  if (enc && dec && plain.size() == len &&
      std::equal(plain.begin(), plain.end(),
                 reinterpret_cast<const uint8_t*>(text))) {
    String cipherHex = toHex(cipher);
    String tagHex = toHex(tag);
    String decoded = vectorToString(plain);
    String json = "{\"status\":\"ok\",\"plain\":\"";
    json += jsonEscape(String(text));
    json += "\",\"cipher\":\"";
    json += cipherHex;
    json += "\",\"decoded\":\"";
    json += jsonEscape(decoded);
    json += "\",\"tag\":\"";
    json += tagHex;
    json += "\",\"nonce\":\"";
    json += jsonEscape(toHex(nonce));
    json += "\"}";

    String logPlain = String("ENCT: plain=") + text;
    String logCipher = String("ENCT: cipher=") + cipherHex;
    String logDecoded = String("ENCT: decoded=") + decoded;
    DEBUG_LOG(logPlain.c_str());
    DEBUG_LOG(logCipher.c_str());
    DEBUG_LOG(logDecoded.c_str());
    return json;
  }
  DEBUG_LOG("ENCT: ошибка теста шифрования");
  return String("{\"status\":\"error\"}");
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
    case ChannelBank::HOME: s += "Home"; break;
  }
  s += "\nChannel: "; s += String(radio.getChannel());
  s += "\nRX: "; s += String(radio.getRxFrequency(), 3); s += " MHz";
  s += "\nTX: "; s += String(radio.getTxFrequency(), 3); s += " MHz";
  s += "\nBW: "; s += String(radio.getBandwidth(), 2); s += " kHz";
  s += "\nSF: "; s += String(radio.getSpreadingFactor());
  s += "\nCR: "; s += String(radio.getCodingRate());
  s += "\nPower: "; s += String(radio.getPower()); s += " dBm";
  s += "\nPause: "; s += String(tx.getSendPause()); s += " ms";
  s += "\nACK timeout: "; s += String(tx.getAckTimeout()); s += " ms";
  s += "\nACK delay: "; s += String(ackResponseDelayMs); s += " ms";
  s += "\nACK: "; s += ackEnabled ? "включён" : "выключен";
  s += "\nRX boosted gain: ";
  s += radio.isRxBoostedGainEnabled() ? "включён" : "выключен";
  s += "\nLight pack: "; s += lightPackMode ? "включён" : "выключен";
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

// JSON-версия списка принятых сообщений
String cmdRstsJson(int cnt) {
  if (cnt <= 0) cnt = 10;
  auto snapshot = recvBuf.snapshot(cnt);
  String out = "{\"items\":[";
  for (size_t i = 0; i < snapshot.size(); ++i) {
    if (i > 0) out += ',';
    out += receivedItemToJson(snapshot[i]);
  }
  out += "]}";
  return out;
}

// Универсальный разбор числа с плавающей запятой из HTTP-параметра с проверкой границ
static bool parseFloatArgument(const String& rawInput,
                               float minValue,
                               float maxValue,
                               float& outValue,
                               String& errorMessage,
                               const char* valueName,
                               const char* units = nullptr,
                               uint8_t precision = 2) {
  String trimmed = rawInput;
  trimmed.trim();
  if (trimmed.length() == 0) {
    errorMessage = String("Параметр ") + valueName + " не может быть пустым.";
    return false;
  }

  errno = 0;
  const char* start = trimmed.c_str();
  char* end = nullptr;
  float parsed = strtof(start, &end);
  if (start == end) {
    errorMessage = String(u8"\u041f\u0430\u0440\u0430\043c\0435\0442\0440 ");
    errorMessage += valueName;
    errorMessage += u8" \u0434\u043e\u043b\u0436\u0435\u043d \u0441\u043e\u0434\u0435\u0440\u0436\u0430\0442\044c \u0447\u0438\u0441\043b\043e (\u043f\u043e\u043b\u0443\0447\u0435\u043d\u043e \"";
    errorMessage += rawInput;
    errorMessage += u8"\").";
    return false;
  }
  if (errno == ERANGE) {
    errorMessage = String("Значение параметра ") + valueName + " выходит за пределы допустимого диапазона.";
    return false;
  }
  while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  if (*end != '\0') {
    errorMessage = String("Лишние символы в параметре ") + valueName + " (\"" + rawInput + "\").";
    return false;
  }
  if (parsed < minValue || parsed > maxValue) {
    String range = String(minValue, static_cast<unsigned int>(precision));
    range += "–";
    range += String(maxValue, static_cast<unsigned int>(precision));
    if (units && units[0] != '\0') {
      range += " ";
      range += units;
    }
    errorMessage = String("Недопустимое значение ") + valueName + " \"" + trimmed + "\" — допустимый диапазон " + range + ".";
    return false;
  }
  outValue = parsed;
  return true;
}

// Универсальный разбор целого числа из HTTP-параметра с проверкой остатка и диапазона
static bool parseIntArgument(const String& rawInput,
                             long minValue,
                             long maxValue,
                             long& outValue,
                             String& errorMessage,
                             const char* valueName) {
  String trimmed = rawInput;
  trimmed.trim();
  if (trimmed.length() == 0) {
    errorMessage = String("Параметр ") + valueName + " не может быть пустым.";
    return false;
  }

  errno = 0;
  const char* start = trimmed.c_str();
  char* end = nullptr;
  long parsed = strtol(start, &end, 10);
  if (start == end) {
    errorMessage = String(u8"\u041f\u0430\u0440\u0430\043c\0435\0442\0440 ");
    errorMessage += valueName;
    errorMessage += u8" \u0434\u043e\u043b\u0436\u0435\u043d \u0441\u043e\u0434\u0435\u0440\u0436\0430\0442\044c \u0446\u0435\u043b\043e\0435 \u0447\u0438\0441\043b\043e (\u043f\u043e\u043b\0443\u0447\0435\043d\043e \"";
    errorMessage += rawInput;
    errorMessage += u8"\").";
    return false;
  }
  if (errno == ERANGE) {
    errorMessage = String("Число в параметре ") + valueName + " выходит за пределы диапазона типа long.";
    return false;
  }
  while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  if (*end != '\0') {
    errorMessage = String("Лишние символы в параметре ") + valueName + " (\"" + rawInput + "\").";
    return false;
  }
  if (parsed < minValue || parsed > maxValue) {
    errorMessage = String("Недопустимое значение ") + valueName + " \"" + trimmed + "\" — допустимый диапазон "
                 + String(minValue) + "–" + String(maxValue) + ".";
    return false;
  }
  outValue = parsed;
  return true;
}

// Специализация разбора ширины полосы пропускания (BW/BF)
static bool parseBandwidthArgument(const String& rawInput, float& outValue, String& errorMessage) {
  static constexpr float kBandwidthMinKHz = 7.81f;
  static constexpr float kBandwidthMaxKHz = 31.25f;
  return parseFloatArgument(rawInput, kBandwidthMinKHz, kBandwidthMaxKHz, outValue, errorMessage, "BW", "кГц");
}

// Специализация разбора spreading factor
static bool parseSpreadingFactorArgument(const String& rawInput, int& outValue, String& errorMessage) {
  static constexpr long kSfMin = 5;
  static constexpr long kSfMax = 12;
  long parsed = 0;
  if (!parseIntArgument(rawInput, kSfMin, kSfMax, parsed, errorMessage, "SF")) {
    return false;
  }
  outValue = static_cast<int>(parsed);
  return true;
}

// Специализация разбора coding rate
static bool parseCodingRateArgument(const String& rawInput, int& outValue, String& errorMessage) {
  static constexpr long kCrMin = 5;
  static constexpr long kCrMax = 8;
  long parsed = 0;
  if (!parseIntArgument(rawInput, kCrMin, kCrMax, parsed, errorMessage, "CR")) {
    return false;
  }
  outValue = static_cast<int>(parsed);
  return true;
}

// Специализация разбора индекса канала для текущего банка
static bool parseChannelArgument(const String& rawInput,
                                 uint16_t bankSize,
                                 uint16_t& outValue,
                                 String& errorMessage) {
  if (bankSize == 0) {
    errorMessage = "Текущий банк не содержит каналов.";
    return false;
  }
  long parsed = 0;
  if (!parseIntArgument(rawInput, 0, static_cast<long>(bankSize) - 1, parsed, errorMessage, "канала")) {
    return false;
  }
  outValue = static_cast<uint16_t>(parsed);
  return true;
}

// Специализация разбора индекса мощности
static bool parsePowerPresetArgument(const String& rawInput, uint8_t& outValue, String& errorMessage) {
  static constexpr long kPowerPresetMin = 0;
  static constexpr long kPowerPresetMax = 9;
  long parsed = 0;
  if (!parseIntArgument(rawInput, kPowerPresetMin, kPowerPresetMax, parsed, errorMessage, "предустановки мощности")) {
    return false;
  }
  outValue = static_cast<uint8_t>(parsed);
  return true;
}

// Обработка HTTP-команды вида /cmd?c=PI
void handleCmdHttp() {
  String cmd = server.arg("c");
  if (cmd.length() == 0) cmd = server.arg("cmd");
  cmd.trim();
  cmd.toUpperCase();
  String cmdArg;
  int spacePos = cmd.indexOf(' ');
  if (spacePos > 0) {
    cmdArg = cmd.substring(spacePos + 1);
    cmdArg.trim();
    cmd = cmd.substring(0, spacePos);
  }
  String resp;
  String contentType = "text/plain";
  int statusCode = 200;
  auto respondBadRequest = [&](const String& message) {
    statusCode = 400;
    resp = message;
    LOG_WARN("HTTP /cmd %s: %s", cmd.c_str(), message.c_str());
    std::string line = "ERR HTTP ";
    line += cmd.c_str();
    line += ": ";
    line += message.c_str();
    SimpleLogger::logStatus(line);
  };
  if (cmd == "PI") {
    resp = cmdPing();
  } else if (cmd == "SEAR") {
    resp = cmdSear();
  } else if (cmd == "BF" || cmd == "BW") {
    if (server.hasArg("v")) {
      float bwValue = 0.0f;
      String error;
      const String raw = server.arg("v");
      if (!parseBandwidthArgument(raw, bwValue, error)) {
        respondBadRequest(error);
      } else if (!radio.setBandwidth(bwValue)) {
        String message = String("Не удалось применить BW=") + String(bwValue, 2) + " кГц — значение не поддерживается устройством.";
        respondBadRequest(message);
      } else {
        resp = String("BF:OK");
      }
    } else {
      resp = String(radio.getBandwidth(), 2);
    }
  } else if (cmd == "SF") {
    if (server.hasArg("v")) {
      int sfValue = 0;
      String error;
      const String raw = server.arg("v");
      if (!parseSpreadingFactorArgument(raw, sfValue, error)) {
        respondBadRequest(error);
      } else if (!radio.setSpreadingFactor(sfValue)) {
        String message = String("Не удалось применить SF=") + String(sfValue) + " — значение не поддерживается устройством.";
        respondBadRequest(message);
      } else {
        resp = String("SF:OK");
      }
    } else {
      resp = String(radio.getSpreadingFactor());
    }
  } else if (cmd == "CR") {
    if (server.hasArg("v")) {
      int crValue = 0;
      String error;
      const String raw = server.arg("v");
      if (!parseCodingRateArgument(raw, crValue, error)) {
        respondBadRequest(error);
      } else if (!radio.setCodingRate(crValue)) {
        String message = String("Не удалось применить CR=") + String(crValue) + " — значение не поддерживается устройством.";
        respondBadRequest(message);
      } else {
        resp = String("CR:OK");
      }
    } else {
      resp = String(radio.getCodingRate());
    }
  } else if (cmd == "PW") {
    if (server.hasArg("v")) {
      uint8_t preset = 0;
      String error;
      const String raw = server.arg("v");
      if (!parsePowerPresetArgument(raw, preset, error)) {
        respondBadRequest(error);
      } else if (!radio.setPower(preset)) {
        String message = String("Не удалось применить предустановку мощности ") + String(preset) + ".";
        respondBadRequest(message);
      } else {
        resp = String("PW:OK");
      }
    } else {
      resp = String(radio.getPower());
    }
  } else if (cmd == "RXBG") {
    if (server.hasArg("v")) {
      bool enable = server.arg("v").toInt() != 0;           // интерпретируем любое ненулевое значение как "вкл"
      if (radio.setRxBoostedGainMode(enable)) {
        resp = String("RXBG:");
        resp += radio.isRxBoostedGainEnabled() ? "1" : "0"; // подтверждаем фактическое состояние
      } else {
        resp = String("RXBG:ERR");
      }
    } else {
      resp = radio.isRxBoostedGainEnabled() ? String("1") : String("0");
    }
  } else if (cmd == "BANK") {
    if (server.hasArg("v")) {
      String raw = server.arg("v");
      raw.trim();
      if (raw.length() == 0) {
        respondBadRequest("Параметр BANK не может быть пустым.");
      } else {
        const char letter = raw[0];
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        const bool known = (upper == 'E' || upper == 'W' || upper == 'T' || upper == 'A' || upper == 'H');
        if (!known) {
          respondBadRequest(String("Неизвестный банк каналов \"") + raw + "\". Допустимые значения: E, W, T, A, H.");
        } else if (!radio.setBank(parseBankChar(letter))) {
          respondBadRequest(String("Не удалось применить банк \"") + raw + "\".");
        }
      }
    }
    if (resp.length() == 0) {
      resp = bankToLetter(radio.getBank());
    }
  } else if (cmd == "CH") {
    if (server.hasArg("v")) {
      uint16_t parsedChannel = 0;
      String error;
      const String raw = server.arg("v");
      if (!parseChannelArgument(raw, radio.getBankSize(), parsedChannel, error)) {
        respondBadRequest(error);
      } else if (!radio.setChannel(static_cast<uint8_t>(parsedChannel))) {
        String message = String("Не удалось переключиться на канал ") + String(parsedChannel) + ".";
        respondBadRequest(message);
      }
    }
    if (resp.length() == 0) {
      resp = String(radio.getChannel());
    }
  } else if (cmd == "CHLIST") {
    ChannelBank b = radio.getBank();
    if (server.hasArg("bank")) {
      String rawBank = server.arg("bank");
      rawBank.trim();
      if (rawBank.length() == 0) {
        respondBadRequest("Параметр bank не может быть пустым.");
      } else {
        const char letter = rawBank[0];
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        const bool known = (upper == 'E' || upper == 'W' || upper == 'T' || upper == 'A' || upper == 'H');
        if (!known) {
          respondBadRequest(String("Неизвестный банк каналов \"") + rawBank + "\". Допустимые значения: E, W, T, A, H.");
        } else {
          b = parseBankChar(letter);
        }
      }
    }
    if (statusCode == 200) {
      resp = cmdChlist(b);
    }
  } else if (cmd == "INFO") {
    resp = cmdInfo();
  } else if (cmd == "VER") {
    resp = readVersionFile();
  } else if (cmd == "STS") {
    int cnt = server.hasArg("n") ? server.arg("n").toInt() : 10;
    resp = cmdSts(cnt);
  } else if (cmd == "RSTS") {
    int cnt = server.hasArg("n") ? server.arg("n").toInt() : 10;
    bool wantJson = false;
    if (server.hasArg("full")) {
      wantJson = server.arg("full").toInt() != 0;
    } else if (server.hasArg("json")) {
      wantJson = server.arg("json").toInt() != 0;
    } else if (server.hasArg("fmt")) {
      String fmt = server.arg("fmt");
      fmt.toLowerCase();
      wantJson = (fmt == "json");
    }
    if (wantJson) {
      resp = cmdRstsJson(cnt);
      contentType = "application/json";
    } else {
      resp = cmdRsts(cnt);
    }
  } else if (cmd == "ACK") {
    if (server.hasArg("toggle")) {
      ackEnabled = !ackEnabled;
    } else if (server.hasArg("v")) {
      ackEnabled = server.arg("v").toInt() != 0;
    }
    tx.setAckEnabled(ackEnabled);
    resp = ackStateText();
  } else if (cmd == "LIGHT") {
    if (server.hasArg("toggle")) {
      lightPackMode = !lightPackMode;
    } else if (server.hasArg("v")) {
      lightPackMode = server.arg("v").toInt() != 0;
    }
    resp = lightPackStateText();
  } else if (cmd == "ACKR") {
    if (server.hasArg("v")) {
      int raw = server.arg("v").toInt();
      if (raw < 0) raw = 0;
      if (raw > 10) raw = 10;
      ackRetryLimit = static_cast<uint8_t>(raw);
      tx.setAckRetryLimit(ackRetryLimit);
    }
    resp = String(ackRetryLimit);
  } else if (cmd == "PAUSE") {
    if (server.hasArg("v")) {
      long raw = server.arg("v").toInt();
      if (raw < 0) raw = 0;
      if (raw > 60000) raw = 60000;
      tx.setSendPause(static_cast<uint32_t>(raw));
    }
    resp = String(tx.getSendPause());
  } else if (cmd == "ACKT") {
    if (server.hasArg("v")) {
      long raw = server.arg("v").toInt();
      if (raw < 0) raw = 0;
      if (raw > 60000) raw = 60000;
      tx.setAckTimeout(static_cast<uint32_t>(raw));
    }
    uint32_t effective = tx.getAckTimeout();
    if (effective == 0) {
      resp = String("0 (ожидание подтверждений выключено, очередь работает без тайм-аута)");  // фиксируем особое значение 0
    } else {
      resp = String(effective);
    }
  } else if (cmd == "ACKD") {
    if (server.hasArg("v")) {
      long raw = server.arg("v").toInt();
      if (raw < static_cast<long>(kAckDelayMinMs)) raw = static_cast<long>(kAckDelayMinMs);
      if (raw > static_cast<long>(kAckDelayMaxMs)) raw = static_cast<long>(kAckDelayMaxMs);
      ackResponseDelayMs = static_cast<uint32_t>(raw);
      tx.setAckResponseDelay(ackResponseDelayMs);
    }
    resp = String(ackResponseDelayMs);
  } else if (cmd == "ENC") {
    bool hasModifier = server.hasArg("toggle") || server.hasArg("v") || cmdArg.length() > 0;
    if (keySafeModeActive && hasModifier) {
      resp = keySafeModeErrorJson();
      contentType = "application/json";
    } else {
      if (server.hasArg("toggle")) {
        encryptionEnabled = !encryptionEnabled;
      } else if (server.hasArg("v")) {
        encryptionEnabled = server.arg("v").toInt() != 0;
      }
      tx.setEncryptionEnabled(encryptionEnabled);
      rx.setEncryptionEnabled(encryptionEnabled);
      resp = encryptionEnabled ? String("ENC:1") : String("ENC:0");
    }
  } else if (cmd == "TESTMODE") {
    String value;
    if (server.hasArg("v")) {
      value = server.arg("v");
    } else {
      value = cmdArg;
    }
    resp = cmdTestMode(value);
  } else if (cmd == "BCN") {
    resp = cmdBcn();
  } else if (cmd == "TXL") {
    String arg = server.hasArg("size") ? server.arg("size") :
                 (server.hasArg("len") ? server.arg("len") : server.arg("v"));
    size_t sz = static_cast<size_t>(arg.toInt());
    resp = cmdTxl(sz);
  } else if (cmd == "TX") {
    String payload;
    if (server.hasArg("data")) payload = server.arg("data");
    else if (server.hasArg("msg")) payload = server.arg("msg");
    else if (server.hasArg("plain")) payload = server.arg("plain");
    else payload = server.arg("v");
    resp = cmdTx(payload);
  } else if (cmd == "ENCT") {
    resp = cmdEnct();
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "TESTRXM") {
    bool hasOverride = false;
    String overrideText;
    if (server.hasArg("msg")) {
      overrideText = server.arg("msg");
      hasOverride = true;
    } else if (server.hasArg("text")) {
      overrideText = server.arg("text");
      hasOverride = true;
    } else if (server.hasArg("payload")) {
      overrideText = server.arg("payload");
      hasOverride = true;
    } else if (server.hasArg("v")) {
      overrideText = server.arg("v");
      hasOverride = true;
    }
    if (hasOverride) {
      resp = cmdTestRxm(&overrideText);
      contentType = "application/json"; // Ответ в формате JSON
    } else {
      resp = cmdTestRxm();
      contentType = "application/json"; // Ответ в формате JSON
    }
  } else if (cmd == "KEYSTATE") {
    resp = cmdKeyState();
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "KEYSTORE") {
    String arg;
    if (server.hasArg("mode")) {
      arg = server.arg("mode");
    } else if (server.hasArg("v")) {
      arg = server.arg("v");
    } else {
      arg = cmdArg;
    }
    resp = cmdKeyStorage(arg);
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "KEYGEN") {
    if (cmdArg.equalsIgnoreCase("PEER")) {
      resp = cmdKeyGenPeer();
    } else {
      resp = cmdKeyGenSecure();
    }
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "KEYRESTORE") {
    resp = cmdKeyRestoreSecure();
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "KEYSEND") {
    resp = cmdKeyTransferSendLora();
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "KEYRECV") {
    String hex = server.hasArg("pub") ? server.arg("pub") : String();
    resp = cmdKeyReceiveSecure(hex);
    contentType = "application/json"; // Ответ в формате JSON
  } else if (cmd == "KEYTRANSFER") {
    if (cmdArg == "SEND") {
      resp = cmdKeyTransferSendLora();
      contentType = "application/json"; // Ответ в формате JSON
    } else if (cmdArg == "RECEIVE") {
      resp = cmdKeyTransferReceiveLora(KeyTransferCommandOrigin::Http);
      contentType = "application/json"; // Ответ в формате JSON
    } else {
      resp = String("{\"error\":\"mode\"}");
      contentType = "application/json"; // Ответ в формате JSON
    }
  } else {
    resp = "UNKNOWN";
  }
  server.send(statusCode, contentType, resp);
}

void handleVer() {
  server.send(200, "text/plain", readVersionFile());
}

// Отдаём последние N строк журнала для вкладки Debug
void handleLogHistory() {
  size_t limit = 120;                                      // значение по умолчанию
  if (server.hasArg("n")) {
    String arg = server.arg("n");
    arg.trim();
    long requested = arg.toInt();
    if (requested > 0) {
      if (requested > 200) requested = 200;                // ограничиваем размерами буфера
      limit = static_cast<size_t>(requested);
    }
  }
  auto logs = LogHook::getRecent(limit);
  String json = "{\"logs\":[";
  for (size_t i = 0; i < logs.size(); ++i) {
    if (i > 0) json += ',';
    const auto& entry = logs[i];
    json += "{\"id\":";
    json += String(static_cast<unsigned long>(entry.id));
    json += ",\"uptime\":";
    json += String(static_cast<unsigned long>(entry.uptime_ms));
    json += ",\"text\":\"";
    json += jsonEscape(String(entry.text));
    json += "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// Формирование SSID точки доступа с коротким уникальным идентификатором устройства
String makeAccessPointSsid() {
  String base = String(gConfig.wifi.ssid.c_str());
#if defined(ARDUINO)
  uint32_t suffix = 0;
#if defined(ESP32)
  uint64_t mac = ESP.getEfuseMac();
  suffix = static_cast<uint32_t>(mac & 0xFFFFFFULL);
#elif defined(ESP8266)
  suffix = ESP.getChipId() & 0xFFFFFFU;
#endif
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%06X", static_cast<unsigned>(suffix));
  base += "-";
  base += buf;
#else
  base += "-000000"; // фиксированный идентификатор для хостовых сборок
#endif
  return base;
}

// Настройка Wi-Fi точки доступа и запуск сервера
bool setupWifi() {
  String ssid = makeAccessPointSsid();                   // формируем SSID с суффиксом устройства
#if defined(ARDUINO)
  // Принудительно отключаем сохранение настроек и переводим модуль в режим точки доступа,
  // иначе после неудачных попыток подключения к STA интерфейс может так и не подняться.
  WiFi.persistent(false);
  WiFi.softAPdisconnect(true);
#if defined(WIFI_MODE_NULL)
  WiFi.mode(WIFI_MODE_NULL);                             // полный сброс подсистемы Wi-Fi
  delay(20);
#endif
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  // Запускаем точку доступа с несколькими повторными попытками на случай зависших настроек
  constexpr uint8_t kMaxApAttempts = 3;
  bool apStarted = false;
  for (uint8_t attempt = 0; attempt < kMaxApAttempts && !apStarted; ++attempt) {
    if (attempt > 0) {
      LOG_WARN("Wi-Fi: повторный запуск точки доступа (попытка %u)", static_cast<unsigned>(attempt + 1));
      WiFi.softAPdisconnect(true);
      delay(100);
#if defined(WIFI_MODE_NULL)
      WiFi.mode(WIFI_MODE_NULL);
      delay(20);
#endif
      WiFi.mode(WIFI_AP);
      WiFi.setSleep(false);
    }
    apStarted = WiFi.softAP(ssid.c_str(), gConfig.wifi.password.c_str());
  }
  if (!apStarted) {                                      // создаём AP
    LOG_ERROR("Wi-Fi: не удалось запустить точку доступа %s", ssid.c_str());
    return false;
  }

  // Задаём статический IP 192.168.4.1 для точки доступа
  IPAddress local_ip;
  IPAddress gateway;
  IPAddress subnet;
  bool ipOk = local_ip.fromString(gConfig.wifi.ip.c_str());
  bool gatewayOk = gateway.fromString(gConfig.wifi.gateway.c_str());
  bool subnetOk = subnet.fromString(gConfig.wifi.subnet.c_str());
  if (!ipOk || !gatewayOk || !subnetOk) {
    LOG_WARN("Wi-Fi: некорректные параметры IP в конфигурации, используется стандартный профиль");
    local_ip = IPAddress(192, 168, 4, 1);
    gateway = IPAddress(192, 168, 4, 1);
    subnet = IPAddress(255, 255, 255, 0);
  }
  if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
    LOG_WARN("Wi-Fi: не удалось применить статический IP, используется конфигурация по умолчанию");
  }
#else
  // В хостовой сборке точка доступа не запускается — ограничиваемся инициализацией веб-сервера.
#endif
  static const char* kImageHeaders[] = {
    "X-Image-Profile",
    "X-Image-Frame-Width",
    "X-Image-Frame-Height",
    "X-Image-Scaled-Width",
    "X-Image-Scaled-Height",
    "X-Image-Offset-X",
    "X-Image-Offset-Y",
    "X-Image-Original-Size",
    "X-Image-Grayscale"
  };
  server.collectHeaders(kImageHeaders, sizeof(kImageHeaders) / sizeof(kImageHeaders[0]));
  server.on("/", handleRoot);                                         // обработчик страницы
  server.on("/style.css", handleStyleCss);                           // CSS веб-интерфейса
  server.on("/script.js", handleScriptJs);                           // JS логика
  server.on("/libs/sha256.js", handleSha256Js);                      // библиотека SHA-256
  server.on("/libs/mgrs.js", handleMgrsJs);                          // преобразование MGRS
  server.on("/libs/freq-info.csv", handleFreqInfoCsv);               // справочник частот каналов
  server.on("/libs/geostat_tle.js", handleGeostatTleJs);             // статический список спутников
  server.on("/ver", handleVer);                                      // версия приложения
  server.on("/events", HTTP_GET, handleSseConnect);                  // SSE-канал push-уведомлений
  server.on("/api/logs", handleLogHistory);                          // выгрузка журнала Serial
  server.on("/api/tx", handleApiTx);                                 // отправка текста по радио
  server.on("/api/tx-image", handleApiTxImage);                      // отправка изображения по радио
  server.on("/cmd", handleCmdHttp);                                  // обработка команд
  server.on("/api/cmd", handleCmdHttp);                              // совместимый эндпоинт
  server.begin();                                                      // старт сервера
#if defined(ARDUINO)
  String ip = WiFi.softAPIP().toString();
  LOG_INFO("Wi-Fi: точка доступа %s запущена, IP %s", ssid.c_str(), ip.c_str());
#else
  LOG_INFO("Wi-Fi: веб-сервер запущен в тестовом режиме (SSID %s)", ssid.c_str());
#endif
  return true;
}

void setup() {
  Logger::init();                                        // сбрасываем очередь логов перед запуском задач
#if defined(ARDUINO)
  static TaskHandle_t loggerTaskHandle = nullptr;        // гарантируем одиночный запуск задачи
  if (loggerTaskHandle == nullptr) {
#if defined(ESP32)
    const uint32_t stackSize = 4096;
    xTaskCreatePinnedToCore(Logger::task, "logger", stackSize, nullptr, 2, &loggerTaskHandle, 1);
#else
    const uint32_t stackSize = 4096;
    xTaskCreate(Logger::task, "logger", stackSize, nullptr, 2, &loggerTaskHandle);
#endif
  }
#endif
  // Настраиваем контроллер safe mode, чтобы он управлял флагами и модулями шифрования.
  configureKeySafeModeController(
      [](bool enabled) {
        tx.setEncryptionEnabled(enabled);
        rx.setEncryptionEnabled(enabled);
      },
      &encryptionEnabled,
      &keySafeModeActive,
      &keyStorageReady);
  Serial.begin(115200);
  bool serialReady = waitForSerial(1500);                // ждём подключения Serial, но не блокируемся
  serialWasReady = serialReady;                          // фиксируем текущее состояние для отслеживания перехода
  // После инициализации UART привязываем KeyLoader к Serial с проверкой доступности порта.
  KeyLoader::setLogCallback([](const __FlashStringHelper* msg) -> bool {
    if (!Serial) {
      return false;                                      // повторим попытку, когда USB появится
    }
    Serial.println(msg);
    return true;
  });
#if SR_HAS_ESP_COREDUMP
  gCoreDumpClearPending = true;
  gCoreDumpClearAfterMs = millis() + 500;  // ждём старта фоновых задач
#endif
  bool storageReadyNow = ensureKeyStorageReady("startup");
  LogHook::setDispatcher([](const LogHook::Entry& entry) {
    enqueueLogEntry(entry); // складываем строку в очередь для неблокирующей доставки
  });
  if (!serialReady) {
    LOG_WARN("Serial: USB-подключение не обнаружено, продолжаем запуск без ожидания ПК");
  }
  if (storageReadyNow) {
    String backendName = KeyLoader::backendName(KeyLoader::getBackend());
    LOG_INFO("Хранилище ключей: %s", backendName.c_str());
    if (KeyLoader::flashEncryptionFallbackInUse()) {
      LOG_WARN("KeyLoader: ⚠️ Flash Encryption отключена, используется временный доступ к NVS по конфигурации");
      if (!KeyLoader::flashEncryptionFallbackWarningLogged()) {
        LOG_WARN("KeyLoader: повторное ensureStorage() выполнит ещё одно предупреждение после инициализации Serial");
      }
      LOG_WARN("KeyLoader: включите аппаратное шифрование флеша перед боевой эксплуатацией, иначе ключи уязвимы");
    }
  } else {
    LOG_WARN("Key: защищённый режим активен до повторного успешного ensureStorage()");
  }
  if (!setupWifi()) {                                 // запускаем точку доступа
    LOG_ERROR("Wi-Fi: веб-интерфейс останется недоступным до перезапуска");
  }
  // Пытаемся запустить радиомодуль с ограничением по числу попыток, чтобы не зависнуть в setup()
  constexpr uint8_t kRadioInitAttempts = 3;
  bool radioReady = false;
  for (uint8_t attempt = 1; attempt <= kRadioInitAttempts; ++attempt) {
    if (radio.begin()) {
      radioReady = true;
      break;
    }
    const int16_t code = radio.getLastErrorCode();
    char prefix[160];
    std::snprintf(prefix, sizeof(prefix),
                  "RadioSX1262: radio.begin() попытка %u завершилась ошибкой, код=",
                  static_cast<unsigned>(attempt));
    LOG_ERROR_VAL(prefix, code);
  }
  if (!radioReady) {
    LOG_ERROR("RadioSX1262: инициализация не завершена, продолжаем работу без готовности радио");
  }
  tx.setAckEnabled(ackEnabled);
  tx.setAckRetryLimit(ackRetryLimit);
  tx.setSendPause(gConfig.radio.sendPauseMs);
  tx.setAckTimeout(gConfig.radio.ackTimeoutMs);
  ackResponseDelayMs = gConfig.radio.ackResponseDelayMs; // фиксируем стартовую задержку ACK
  tx.setAckResponseDelay(ackResponseDelayMs);
  tx.setEncryptionEnabled(encryptionEnabled);
  rx.setEncryptionEnabled(encryptionEnabled);
  rx.setBuffer(&recvBuf);                                   // сохраняем принятые пакеты
  recvBuf.setNotificationCallback([](ReceivedBuffer::Kind kind, const ReceivedBuffer::Item& item) {
    // Передаём уведомление всем подписчикам SSE
    broadcastReceivedPush(kind, item);
  });
  // обработка входящих данных с учётом ACK
  rx.setAckCallback([&]() {
    LOG_INFO("ACK: получен");
    tx.onAckReceived();
  });
  rx.setCallback([&](const uint8_t* d, size_t l){
    if (protocol::ack::isAckPayload(d, l)) {              // ACK уже обработан отдельным колбэком
      return;
    }
#if defined(ARDUINO)
    if (rxSerialDumpEnabled && Serial) {                  // не блокируемся, если USB-хост не подключён
      (void)dumpRxToSerialWithPrefix(Serial, d, l);       // фиксируем предупреждение при усечении дампа
    }
#else
    (void)rxSerialDumpEnabled;
#endif
    LOG_INFO("RX: пакет на %u байт", static_cast<unsigned>(l));
    if (ackEnabled) {                                     // отправляем подтверждение
      const uint8_t ack_msg[1] = {protocol::ack::MARKER};
      tx.queue(ack_msg, sizeof(ack_msg));
      tx.loop();
    }
  });
  radio.setReceiveCallback([&](const uint8_t* d, size_t l){  // привязка приёма
    if (handleKeyTransferFrame(d, l)) return;                // перехватываем кадр обмена ключами
    rx.onReceive(d, l);
  });
  radio.setIrqLogCallback(onRadioIrqLog);                    // транслируем IRQ-логи в SSE даже при отключённом Serial
  LOG_INFO("Команды: BF <полоса>, SF <фактор>, CR <код>, BANK <e|w|t|a|h>, CH <номер>, PW <0-9>, RXBG <0|1>, TX <строка>, TXL <размер>, BCN, INFO, STS <n>, RSTS <n>, ACK [0|1], LIGHT [0|1], ACKR <повторы>, PAUSE <мс>, ACKT <мс>, ACKD <мс>, ENC [0|1], PI, SEAR, TESTRXM, KEYTRANSFER SEND, KEYTRANSFER RECEIVE, KEYSTORE [auto|nvs]");
}

void loop() {
#if SR_HAS_ESP_COREDUMP
    if (gCoreDumpClearPending && millis() >= gCoreDumpClearAfterMs) {
      if (clearCorruptedCoreDumpConfig()) {
        gCoreDumpClearPending = false;
      }
    }
#endif
    server.handleClient();                  // обработка HTTP-запросов
    maintainPushSessions();                 // поддержка активных push-клиентов
    flushPendingLogEntries();               // передаём накопленные логи без блокировок Serial
    flushPendingIrqStatus();                // публикуем последний статус IRQ при наличии подписчиков
    const bool serialNowReady = static_cast<bool>(Serial); // отслеживаем появление USB-подключения
    if (serialNowReady && !serialWasReady) {
      KeyLoader::flushBufferedLogs();       // Serial стал доступен — пробуем выгрузить буфер KeyLoader
    }
    serialWasReady = serialNowReady;
    if (!keyTransferRuntime.waiting) {
      radio.loop();                         // обработка входящих пакетов
      rx.tickCleanup();                     // фоновая очистка очередей RX даже без новых кадров
      tx.loop();                            // обработка очередей передачи
    }
    processTestRxm();                       // генерация тестовых входящих сообщений
    processKeyTransferReceiveState();       // неблокирующий контроль ожидания KEYTRANSFER
    const unsigned long now = millis();
    if (serialLineBuffer.length() > 0 && serialLastByteAtMs != 0 &&
        (now - serialLastByteAtMs) > kSerialLineTimeoutMs) {
      serialLineBuffer = "";              // тайм-аут: сбрасываем незавершённую команду
      serialLineOverflow = false;
      serialLastByteAtMs = 0;
    }

    while (Serial.available()) {
      const int incoming = Serial.read();
      if (incoming < 0) {
        break;                             // неожиданный код — выходим из цикла
      }
      serialLastByteAtMs = millis();
      if (incoming == '\r') {
        continue;                          // игнорируем CR для поддержки CRLF
      }
      if (incoming != '\n') {
        if (!serialLineOverflow) {
          if (serialLineBuffer.length() < kSerialLineMaxLength) {
            serialLineBuffer += static_cast<char>(incoming);
          } else {
            serialLineOverflow = true;     // достигнут предел длины команды
          }
        }
        continue;
      }

      String line = serialLineBuffer;
      const bool overflowed = serialLineOverflow;
      serialLineBuffer = "";
      serialLineOverflow = false;

      if (overflowed) {
        Serial.println("Ошибка: команда превышает допустимую длину и была сброшена");
        continue;
      }

      line.trim();
      if (line.length() == 0) {
        continue;                          // пустые строки игнорируем
      }

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
        if (b == 'e' || b == 'E') {
          radio.setBank(ChannelBank::EAST);
          Serial.println("Банк Восток");
        } else if (b == 'w' || b == 'W') {
          radio.setBank(ChannelBank::WEST);
          Serial.println("Банк Запад");
        } else if (b == 't' || b == 'T') {
          radio.setBank(ChannelBank::TEST);
          Serial.println("Банк Тест");
        } else if (b == 'a' || b == 'A') {
          radio.setBank(ChannelBank::ALL);
          Serial.println("Банк All");
        } else if (b == 'h' || b == 'H') {
          radio.setBank(ChannelBank::HOME);
          Serial.println("Банк Home");
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
      } else if (line.startsWith("RXBG")) {                   // управление режимом повышенного усиления приёмника
        if (line.length() <= 4) {
          Serial.print("RXBG: ");
          Serial.println(radio.isRxBoostedGainEnabled() ? "включён" : "выключен");
        } else {
          String arg = line.substring(4);
          arg.trim();
          bool desired = radio.isRxBoostedGainEnabled();
          bool parsed = false;
          if (arg.equalsIgnoreCase("1") || arg.equalsIgnoreCase("ON") || arg.equalsIgnoreCase("TRUE") ||
              arg.equalsIgnoreCase("ВКЛ")) {
            desired = true;
            parsed = true;
          } else if (arg.equalsIgnoreCase("0") || arg.equalsIgnoreCase("OFF") || arg.equalsIgnoreCase("FALSE") ||
                     arg.equalsIgnoreCase("ВЫКЛ")) {
            desired = false;
            parsed = true;
          } else if (arg.equalsIgnoreCase("TOGGLE") || arg.equalsIgnoreCase("SWAP")) {
            desired = !desired;
            parsed = true;
          }
          if (!parsed) {
            Serial.println("RXBG: используйте RXBG <0|1|toggle>");
          } else if (radio.setRxBoostedGainMode(desired)) {
            Serial.print("RXBG: ");
            Serial.println(radio.isRxBoostedGainEnabled() ? "включён" : "выключен");
          } else {
            Serial.println("RXBG: ошибка установки режима");
          }
        }
      } else if (line.equalsIgnoreCase("BCN")) {
        tx.prepareExternalSend();
        int16_t state = radio.sendBeacon();
        tx.completeExternalSend();
        if (state == RadioSX1262::ERR_TIMEOUT) {
          Serial.println("Маяк не отправлен: радио занято");
        } else if (state != IRadio::ERR_NONE) {
          Serial.print("Ошибка отправки маяка, код=");
          Serial.println(state);
        } else {
          Serial.println("Маяк отправлен");
        }
      } else if (line.equalsIgnoreCase("INFO")) {
        // выводим текущие настройки радиомодуля
        Serial.print("Банк: ");
        switch (radio.getBank()) {
          case ChannelBank::EAST: Serial.println("Восток"); break;
          case ChannelBank::WEST: Serial.println("Запад"); break;
          case ChannelBank::TEST: Serial.println("Тест"); break;
          case ChannelBank::ALL: Serial.println("All"); break;
          case ChannelBank::HOME: Serial.println("Home"); break;
        }
        Serial.print("Канал: "); Serial.println(radio.getChannel());
        Serial.print("RX: "); Serial.print(radio.getRxFrequency(), 3); Serial.println(" MHz");
        Serial.print("TX: "); Serial.print(radio.getTxFrequency(), 3); Serial.println(" MHz");
        Serial.print("BW: "); Serial.print(radio.getBandwidth(), 2); Serial.println(" kHz");
        Serial.print("SF: "); Serial.println(radio.getSpreadingFactor());
        Serial.print("CR: "); Serial.println(radio.getCodingRate());
        Serial.print("Power: "); Serial.print(radio.getPower()); Serial.println(" dBm");
        Serial.print("Pause: "); Serial.print(tx.getSendPause()); Serial.println(" ms");
        Serial.print("ACK timeout: "); Serial.print(tx.getAckTimeout()); Serial.println(" ms");
        Serial.print("ACK delay: "); Serial.print(ackResponseDelayMs); Serial.println(" ms");
        Serial.print("ACK: "); Serial.println(ackEnabled ? "включён" : "выключен");
      } else if (line.startsWith("STS")) {
        int cnt = line.length() > 3 ? line.substring(4).toInt() : 10;
        if (cnt <= 0) cnt = 10;                       // значение по умолчанию
        auto logs = SimpleLogger::getLast(cnt);
        for (const auto& s : logs) {
          Serial.println(s.c_str());
        }
      } else if (line.startsWith("RSTS")) {
        String args = line.substring(4);                              // выделяем аргументы команды
        args.trim();
        bool wantJson = false;                                        // флаг расширенного вывода
        int cnt = 10;                                                 // количество записей по умолчанию
        if (args.length() > 0) {
          String rest = args;
          while (rest.length() > 0) {                                 // разбираем аргументы по пробелам
            int space = rest.indexOf(' ');
            String token = space >= 0 ? rest.substring(0, space) : rest;
            rest = space >= 0 ? rest.substring(space + 1) : String();
            token.trim();
            rest.trim();
            if (token.length() == 0) continue;                        // пропускаем пустые сегменты
            if (token.equalsIgnoreCase("FULL") || token.equalsIgnoreCase("JSON")) {
              wantJson = true;                                        // включаем JSON-формат
              continue;
            }
            int value = token.toInt();                                // пытаемся интерпретировать число
            if (value > 0) cnt = value;                               // положительное значение задаёт лимит
          }
        }
        if (cnt <= 0) cnt = 10;                                       // защита от некорректных значений
        if (wantJson) {
          String json = cmdRstsJson(cnt);                             // сериализуем полный снимок
          Serial.println(json);                                       // отправляем JSON для проверки данных
        } else {
          auto names = recvBuf.list(cnt);                             // стандартный список имён
          for (const auto& n : names) {
            Serial.println(n.c_str());
          }
        }
      } else if (line.startsWith("TX ")) {
        String msg = line.substring(3);                     // исходный текст
        uint32_t testId = 0;                                // идентификатор тестового сообщения
        String testErr;                                     // ошибка тестовой эмуляции
        bool simulated = false;                             // отметка об успешной эмуляции
        if (testModeEnabled) {
          DEBUG_LOG("RC: тестовый режим TX");
          simulated = testModeProcessMessage(msg, testId, testErr);
        }

        uint32_t id = 0;                                    // идентификатор реальной очереди
        String err;                                         // ошибка постановки в очередь
        if (enqueueTextMessage(msg, id, err)) {
          DEBUG_LOG_VAL("RC: сообщение поставлено id=", static_cast<int>(id));
          Serial.print("Пакет отправлен, id=");
          Serial.println(id);
        } else {
          Serial.print("Ошибка постановки пакета: ");
          Serial.println(err);
        }

        if (testModeEnabled) {                              // выводим статус тестового режима
          if (simulated) {
            Serial.print("TESTMODE: сообщение сохранено id=");
            Serial.println(testId);
          } else {
            Serial.print("TESTMODE: ошибка — ");
            Serial.println(testErr);
          }
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
      } else if (line.startsWith("TESTMODE")) {
        String arg = line.length() > 8 ? line.substring(8) : String();
        arg.trim();
        String response = cmdTestMode(arg);
        Serial.println(response);
      } else if (line.equalsIgnoreCase("ENCT")) {
        // тест шифрования: создаём сообщение, шифруем и расшифровываем
        const uint8_t key[16]   = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        const uint8_t nonce[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
        const char* text = "Test ENCT";                    // исходное сообщение
        size_t len = strlen(text);
        std::vector<uint8_t> cipher, tag, plain;
        bool enc = crypto::chacha20poly1305::encrypt(key, sizeof(key), nonce, sizeof(nonce),
                                                     nullptr, 0,
                                                     reinterpret_cast<const uint8_t*>(text), len,
                                                     cipher, tag);
        bool dec = false;
        if (enc) {
          dec = crypto::chacha20poly1305::decrypt(key, sizeof(key), nonce, sizeof(nonce),
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
      } else if (line.startsWith("TESTRXM ")) {
        String overrideText = line.substring(8);
        Serial.println(cmdTestRxm(&overrideText));
      } else if (line.equalsIgnoreCase("TESTRXM")) {
        Serial.println(cmdTestRxm());
      } else if (line.equalsIgnoreCase("KEYTRANSFER SEND")) {
        Serial.println(cmdKeyTransferSendLora());
      } else if (line.equalsIgnoreCase("KEYTRANSFER RECEIVE")) {
        Serial.println(cmdKeyTransferReceiveLora(KeyTransferCommandOrigin::Serial));
      } else if (line.startsWith("KEYSTORE")) {
        String arg = line.length() > 8 ? line.substring(8) : String();
        arg.trim();
        Serial.println(cmdKeyStorage(arg));
      } else if (line.startsWith("ENC ")) {
        if (keySafeModeActive) {
          Serial.println("ENC: команда недоступна в защищённом режиме");
        } else {
          encryptionEnabled = line.substring(4).toInt() != 0;
          tx.setEncryptionEnabled(encryptionEnabled);
          rx.setEncryptionEnabled(encryptionEnabled);
          Serial.print("ENC: ");
          Serial.println(encryptionEnabled ? "включено" : "выключено");
        }
      } else if (line.equalsIgnoreCase("ENC")) {
        if (keySafeModeActive) {
          Serial.println("ENC: команда недоступна в защищённом режиме");
        } else {
          encryptionEnabled = !encryptionEnabled;
          tx.setEncryptionEnabled(encryptionEnabled);
          rx.setEncryptionEnabled(encryptionEnabled);
          Serial.print("ENC: ");
          Serial.println(encryptionEnabled ? "включено" : "выключено");
        }
      } else if (line.startsWith("ACKR")) {
        int value = ackRetryLimit;
        if (line.length() > 4) value = line.substring(5).toInt();
        if (value < 0) value = 0;
        if (value > 10) value = 10;
        ackRetryLimit = static_cast<uint8_t>(value);
        tx.setAckRetryLimit(ackRetryLimit);
        Serial.print("ACKR: ");
        Serial.println(ackRetryLimit);
      } else if (line.startsWith("PAUSE")) {
        long value = tx.getSendPause();
        if (line.length() > 5) value = line.substring(6).toInt();
        if (value < 0) value = 0;
        if (value > 60000) value = 60000;
        tx.setSendPause(static_cast<uint32_t>(value));
        Serial.print("PAUSE: ");
        Serial.print(value);
        Serial.println(" ms");
      } else if (line.startsWith("ACKT")) {
        long value = tx.getAckTimeout();
        if (line.length() > 4) value = line.substring(5).toInt();
        if (value < 0) value = 0;
        if (value > 60000) value = 60000;
        tx.setAckTimeout(static_cast<uint32_t>(value));
        uint32_t applied = tx.getAckTimeout();
        Serial.print("ACKT: ");
        Serial.print(applied);
        if (applied == 0) {
          Serial.println(" ms (ожидание подтверждений выключено, очередь работает без тайм-аута)");  // подчёркиваем особый режим ожидания
        } else {
          Serial.println(" ms");
        }
      } else if (line.startsWith("ACKD")) {
        long value = static_cast<long>(ackResponseDelayMs);
        if (line.length() > 4) value = line.substring(5).toInt();
        if (value < static_cast<long>(kAckDelayMinMs)) value = static_cast<long>(kAckDelayMinMs);
        if (value > static_cast<long>(kAckDelayMaxMs)) value = static_cast<long>(kAckDelayMaxMs);
        ackResponseDelayMs = static_cast<uint32_t>(value);
        tx.setAckResponseDelay(ackResponseDelayMs);
        Serial.print("ACKD: ");
        Serial.print(ackResponseDelayMs);
        Serial.println(" ms");
      } else if (line.startsWith("ACK")) {
        if (line.length() > 3) {                          // установка явного значения
          ackEnabled = line.substring(4).toInt() != 0;
        } else {
          ackEnabled = !ackEnabled;                       // переключение
        }
        tx.setAckEnabled(ackEnabled);
        Serial.print("ACK: ");
        Serial.println(ackEnabled ? "включён" : "выключен");
      } else if (line.startsWith("LIGHT")) {
        if (line.length() > 5) {                          // обработка явного значения
          lightPackMode = line.substring(6).toInt() != 0;
        } else {
          lightPackMode = !lightPackMode;                 // простое переключение
        }
        Serial.print("LIGHT: ");
        Serial.println(lightPackMode ? "включён" : "выключен");
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

