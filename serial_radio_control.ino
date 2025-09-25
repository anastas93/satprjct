#include <Arduino.h>
#include <array>
#include <vector>
#include <string>
#include <cstring> // для strlen
#include <algorithm> // для std::equal
#include <cstdio>    // для snprintf при подготовке JSON

// --- Радио и модули ---
#include "radio_sx1262.h"
#include "tx_module.h"
#include "rx_module.h" // модуль приёма
#include "default_settings.h"

// --- Вспомогательные библиотеки ---
#include "libs/text_converter/text_converter.h"   // конвертер UTF-8 -> CP1251
#include "libs/simple_logger/simple_logger.h"     // журнал статусов
#include "libs/received_buffer/received_buffer.h" // буфер принятых сообщений
#include "libs/packetizer/packet_gatherer.h"      // собиратель пакетов для теста
#include "libs/packetizer/packet_splitter.h"      // параметры делителя пакетов
#include "libs/crypto/aes_ccm.h"                  // AES-CCM шифрование
#include "libs/key_loader/key_loader.h"           // управление ключами и ECDH
#include "libs/key_transfer/key_transfer.h"       // обмен корневым ключом по LoRa
#include "libs/fs_utils/spiffs_guard.h"           // единый контроль монтажа SPIFFS

// --- Сеть и веб-интерфейс ---
#include <WiFi.h>        // работа с Wi-Fi
#include <WebServer.h>   // встроенный HTTP-сервер
#if defined(ARDUINO) && defined(ESP32)
#include <esp_system.h>  // доступ к уникальному идентификатору ESP32
#endif
#include "web/web_content.h"      // встроенные файлы веб-интерфейса
#include "web/geostat_tle_js.h"   // статический набор TLE
#ifndef ARDUINO
#include <fstream>
#else
#include <FS.h>
#include <SPIFFS.h>
#endif

#if defined(ESP32) && __has_include("esp_core_dump.h")
#include "esp_core_dump.h" // управление core dump на ESP32
#include "esp_partition.h" // прямой доступ к разделам флеша
#define SR_HAS_ESP_COREDUMP 1
#else
#define SR_HAS_ESP_COREDUMP 0
#endif

#ifdef ARDUINO
// Резервная версия прошивки, чтобы /ver отвечал даже без SPIFFS
static const char kEmbeddedVersion[] PROGMEM =
#include "ver"
;
#else
// Аналогичный резерв для тестовых сборок на хосте
static const char kEmbeddedVersion[] =
#include "ver"
;
#endif

// Предварительное объявление структуры тестового пакета, чтобы прототипы Arduino
// не нарушали компиляцию при автоматическом добавлении заголовков.
struct LegacyTestPacket;

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
bool encryptionEnabled = DefaultSettings::USE_ENCRYPTION; // режим шифрования
uint8_t ackRetryLimit = DefaultSettings::ACK_RETRY_LIMIT; // число повторов при ожидании ACK
bool testModeEnabled = false;           // флаг тестового режима SendMsg_BR/received_msg
uint8_t testModeLocalCounter = 0;       // локальный счётчик пакетов для тестового режима

WebServer server(80);       // HTTP-сервер для веб-интерфейса

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
} keyTransferRuntime;

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
// Пытаемся очистить повреждённую конфигурацию core dump, чтобы убрать перезагрузки с ошибкой CRC
void clearCorruptedCoreDumpConfig() {
  // На некоторых сборках Arduino+ESP32 вызов esp_core_dump_image_erase()
  // приводит к assert внутри FreeRTOS (нет Idle-задачи на втором ядре).
  // Поэтому стираем раздел core dump напрямую через API разделов флеша.
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  if (!part) {
    Serial.println("Core dump: раздел не найден");
    return;
  }
  esp_err_t err = esp_partition_erase_range(part, 0, part->size);
  if (err == ESP_OK) {
    Serial.println("Core dump: раздел очищен");
  } else {
    Serial.print("Core dump: ошибка стирания, код=");
    Serial.println(static_cast<int>(err));
  }
}
#endif

#if SR_HAS_ESP_COREDUMP
// Флаги, управляющие отложенной очисткой core dump после старта FreeRTOS.
bool gCoreDumpClearPending = true;
uint32_t gCoreDumpClearAfterMs = 0;
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
  json += toHex(DefaultSettings::DEFAULT_KEY);
  json += "\"";
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
  json += "}";
  return json;
}

void reloadCryptoModules() {
  tx.reloadKey();
  rx.reloadKey();
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
  static bool spiffsMounted = false;
  if (!spiffsMounted) {
    auto mountResult = fs_utils::ensureSpiffsMounted();
    spiffsMounted = mountResult.ok();
    if (!spiffsMounted) {
      DEBUG_LOG("SPIFFS: %s", fs_utils::describeStatus(mountResult.status));
    }
  }
  if (spiffsMounted) {
    File f = SPIFFS.open("/ver", "r");
    if (f) {
      String text = f.readStringUntil('\n');
      f.close();
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
  }
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

// Обработка специального кадра KEYTRANSFER; возвращает true, если пакет потреблён
bool handleKeyTransferFrame(const uint8_t* data, size_t len) {
  std::array<uint8_t,32> remote_pub{};
  std::array<uint8_t,4> remote_id{};
  uint32_t msg_id = 0;
  if (!KeyTransfer::parseFrame(data, len, remote_pub, remote_id, msg_id)) {
    return false;                                  // кадр не относится к обмену ключами
  }
  keyTransferRuntime.last_msg_id = msg_id;
  if (!keyTransferRuntime.waiting) {
    SimpleLogger::logStatus("KEYTRANSFER IGN");   // пришёл неожиданный ключ
    return true;                                   // не передаём дальше в RxModule
  }
  if (!KeyLoader::applyRemotePublic(remote_pub)) {
    keyTransferRuntime.error = String("apply");
    keyTransferRuntime.waiting = false;
    keyTransferRuntime.completed = false;
    SimpleLogger::logStatus("KEYTRANSFER ERR");
    Serial.println("KEYTRANSFER: ошибка применения удалённого ключа");
    return true;
  }
  reloadCryptoModules();                          // обновляем Tx/Rx новым ключом
  keyTransferRuntime.completed = true;
  keyTransferRuntime.waiting = false;
  keyTransferRuntime.error = String();
  SimpleLogger::logStatus("KEYTRANSFER OK");
  Serial.println("KEYTRANSFER: получен корневой ключ по LoRa");
  return true;
}

String cmdKeyState() {
  DEBUG_LOG("Key: запрос состояния");
  return makeKeyStateJson();
}

// Управление бэкендом хранилища ключей (SPIFFS/NVS/auto)
String cmdKeyStorage(const String& mode) {
  String trimmed = mode;
  trimmed.trim();
  if (trimmed.length() == 0) {
    return makeKeyStorageStatusJson();
  }
  KeyLoader::StorageBackend backend = KeyLoader::StorageBackend::UNKNOWN;
  if (trimmed.equalsIgnoreCase("AUTO")) {
    backend = KeyLoader::StorageBackend::UNKNOWN;
  } else if (trimmed.equalsIgnoreCase("SPIFFS")) {
    backend = KeyLoader::StorageBackend::SPIFFS;
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
  fs_utils::SpiffsMountResult mountStatus{};
  if (KeyLoader::generateLocalKey(nullptr, &mountStatus)) {
    reloadCryptoModules();
    return makeKeyStateJson();
  }
  if (!mountStatus.ok()) {
    String json = "{\"error\":\"spiffs\",\"reason\":\"";
    json += fs_utils::describeStatus(mountStatus.status);
    json += "\"";
    if (mountStatus.error_code != 0) {
      json += ",\"code\":";
      json += String(mountStatus.error_code);
    }
    json += "}";
    return json;
  }
  return String("{\"error\":\"keygen\"}");
}

String cmdKeyRestoreSecure() {
  DEBUG_LOG("Key: восстановление ключа из резервной копии");
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
  auto state = KeyLoader::getState();
  auto key_id = KeyLoader::keyId(state.session_key);
  std::vector<uint8_t> frame;
  uint32_t msg_id = generateKeyTransferMsgId();
  if (!KeyTransfer::buildFrame(msg_id, state.root_public, key_id, frame)) {
    return String("{\"error\":\"build\"}");
  }
  tx.prepareExternalSend();
  radio.send(frame.data(), frame.size());
  tx.completeExternalSend();
  keyTransferRuntime.last_msg_id = msg_id;
  SimpleLogger::logStatus("KEYTRANSFER SEND");
  Serial.println("KEYTRANSFER: отправлен публичный корневой ключ");
  return makeKeyStateJson();
}

// Ожидание и приём корневого ключа через LoRa
String cmdKeyTransferReceiveLora() {
  DEBUG_LOG("Key: ожидание ключа по LoRa");
  keyTransferRuntime.waiting = true;
  keyTransferRuntime.completed = false;
  keyTransferRuntime.error = String();
  keyTransferRuntime.last_msg_id = 0;
  SimpleLogger::logStatus("KEYTRANSFER WAIT");
  unsigned long start = millis();
  const unsigned long timeout_ms = 8000;              // тайм-аут ожидания 8 секунд
  while (millis() - start < timeout_ms) {
    if (keyTransferRuntime.completed) {
      keyTransferRuntime.completed = false;           // сбрасываем флаг на будущее
      return makeKeyStateJson();
    }
    if (keyTransferRuntime.error.length()) {
      String err = keyTransferRuntime.error;
      keyTransferRuntime.error = String();
      keyTransferRuntime.waiting = false;
      return String("{\"error\":\"") + err + "\"}";
    }
    radio.loop();                                     // даём шанс обработать входящие пакеты
    tx.loop();                                        // обрабатываем очередь передачи
    delay(5);                                         // небольшая пауза, чтобы не блокировать CPU
  }
  keyTransferRuntime.waiting = false;
  keyTransferRuntime.completed = false;
  if (keyTransferRuntime.error.length()) {
    String err = keyTransferRuntime.error;
    keyTransferRuntime.error = String();
    return String("{\"error\":\"") + err + "\"}";
  }
  return String("{\"error\":\"timeout\"}");
}

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

// Отдаём библиотеку преобразования MGRS → широта/долгота
void handleMgrsJs() {
  server.send_P(200, "application/javascript", MGRS_JS);
}

// Отдаём CSV-справочник частот каналов
void handleFreqInfoCsv() {
  server.send_P(200, "text/csv", FREQ_INFO_CSV);
}

// Отдаём постоянный набор TLE для вкладки Pointing
void handleGeostatTleJs() {
  server.send_P(200, "application/javascript", GEOSTAT_TLE_JS);
}

// Принимаем текст и отправляем его по радио
void handleApiTx() {
  if (server.method() != HTTP_POST) {                       // разрешаем только POST
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
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
  String body = server.arg("plain");
  if (body.length() == 0) {                                 // защита от пустого тела
    server.send(400, "text/plain", "empty");
    return;
  }
  const size_t len = static_cast<size_t>(body.length());
  static const size_t kImageMaxPayload = []() {
    PacketSplitter splitter(PayloadMode::LARGE);
    return DefaultSettings::TX_QUEUE_CAPACITY * splitter.payloadSize();
  }();                                                     // реальный потолок для очереди
  if (len > kImageMaxPayload) {
    String reason = "image exceeds queue capacity (max ";
    reason += String(static_cast<unsigned long>(kImageMaxPayload));
    reason += " bytes)";
    server.send(413, "text/plain", reason);
    std::string log = "IMG TX reject bytes=" + std::to_string(len) +
                      " limit=" + std::to_string(kImageMaxPayload);
    SimpleLogger::logStatus(log);
    return;
  }
  std::vector<uint8_t> payload(len);
  std::memcpy(payload.data(), body.c_str(), len);           // копируем данные в вектор
  if (!isLikelyJpeg(payload.data(), payload.size())) {
    server.send(415, "text/plain", "not jpeg");
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
  bool ok = radio.ping(ping.data(), ping.size(),
                       resp.data(), resp.size(),
                       respLen, DefaultSettings::PING_WAIT_MS * 1000UL,
                       elapsed);
  tx.completeExternalSend();
  if (ok && respLen == ping.size() &&
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
    std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> resp{};
    size_t respLen = 0;
    uint32_t elapsed = 0;
    tx.prepareExternalSend();
    bool ok_ch = radio.ping(pkt.data(), pkt.size(),
                            resp.data(), resp.size(),
                            respLen, DefaultSettings::PING_WAIT_MS * 1000UL,
                            elapsed);
    tx.completeExternalSend();
    (void)respLen; // длина не нужна при проверке ок
    (void)elapsed; // время в поиске не используется
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
  uint32_t id = tx.queue(data.data(), data.size());
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
  radio.sendBeacon();
  tx.completeExternalSend();
  return String("BCN:OK");
}

// Команда ENCT повторяет тест шифрования и возвращает подробности
String cmdEnct() {
  const uint8_t key[16]   = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  auto nonce = KeyLoader::makeNonce(0, 0);
  const char* text = "Test ENCT";
  size_t len = strlen(text);
  std::vector<uint8_t> cipher, tag, plain;
  bool enc = encrypt_ccm(key, sizeof(key), nonce.data(), nonce.size(),
                         nullptr, 0,
                         reinterpret_cast<const uint8_t*>(text), len,
                         cipher, tag, 8);
  bool dec = false;
  if (enc) {
    dec = decrypt_ccm(key, sizeof(key), nonce.data(), nonce.size(),
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
  s += "\nACK: "; s += ackEnabled ? "включён" : "выключен";
  s += "\nRX boosted gain: ";
  s += radio.isRxBoostedGainEnabled() ? "включён" : "выключен";
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
  if (cmd == "PI") {
    resp = cmdPing();
  } else if (cmd == "SEAR") {
    resp = cmdSear();
  } else if (cmd == "BF" || cmd == "BW") {
    if (server.hasArg("v")) {
      float bw = server.arg("v").toFloat();
      resp = radio.setBandwidth(bw) ? String("BF:OK") : String("BF:ERR");
    } else {
      resp = String(radio.getBandwidth(), 2);
    }
  } else if (cmd == "SF") {
    if (server.hasArg("v")) {
      int sf = server.arg("v").toInt();
      resp = radio.setSpreadingFactor(sf) ? String("SF:OK") : String("SF:ERR");
    } else {
      resp = String(radio.getSpreadingFactor());
    }
  } else if (cmd == "CR") {
    if (server.hasArg("v")) {
      int cr = server.arg("v").toInt();
      resp = radio.setCodingRate(cr) ? String("CR:OK") : String("CR:ERR");
    } else {
      resp = String(radio.getCodingRate());
    }
  } else if (cmd == "PW") {
    if (server.hasArg("v")) {
      int pw = server.arg("v").toInt();
      resp = radio.setPower(pw) ? String("PW:OK") : String("PW:ERR");
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
      char b = server.arg("v")[0];
      if (!radio.setBank(parseBankChar(b))) {
        resp = String("BANK:ERR");
      }
    }
    if (resp.length() == 0) {
      resp = bankToLetter(radio.getBank());
    }
  } else if (cmd == "CH") {
    int ch = server.arg("v").toInt();
    if (server.hasArg("v")) {
      if (!radio.setChannel(ch)) {
        resp = String("CH:ERR current=");
        resp += String(radio.getChannel());
      }
    }
    if (resp.length() == 0) {
      resp = String(radio.getChannel());
    }
  } else if (cmd == "CHLIST") {
    ChannelBank b = radio.getBank();
    if (server.hasArg("bank")) {
      b = parseBankChar(server.arg("bank")[0]);
    }
    resp = cmdChlist(b);
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
    resp = String(tx.getAckTimeout());
  } else if (cmd == "ENC") {
    if (server.hasArg("toggle")) {
      encryptionEnabled = !encryptionEnabled;
    } else if (server.hasArg("v")) {
      encryptionEnabled = server.arg("v").toInt() != 0;
    }
    tx.setEncryptionEnabled(encryptionEnabled);
    rx.setEncryptionEnabled(encryptionEnabled);
    resp = encryptionEnabled ? String("ENC:1") : String("ENC:0");
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
    } else {
      resp = cmdTestRxm();
    }
  } else if (cmd == "KEYSTATE") {
    resp = cmdKeyState();
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
  } else if (cmd == "KEYGEN") {
    resp = cmdKeyGenSecure();
  } else if (cmd == "KEYRESTORE") {
    resp = cmdKeyRestoreSecure();
  } else if (cmd == "KEYSEND") {
    resp = cmdKeyTransferSendLora();
  } else if (cmd == "KEYRECV") {
    String hex = server.hasArg("pub") ? server.arg("pub") : String();
    resp = cmdKeyReceiveSecure(hex);
  } else if (cmd == "KEYTRANSFER") {
    if (cmdArg == "SEND") {
      resp = cmdKeyTransferSendLora();
    } else if (cmdArg == "RECEIVE") {
      resp = cmdKeyTransferReceiveLora();
    } else {
      resp = String("{\"error\":\"mode\"}");
    }
  } else {
    resp = "UNKNOWN";
  }
  server.send(200, contentType, resp);
}

void handleVer() {
  server.send(200, "text/plain", readVersionFile());
}

// Формирование SSID точки доступа с коротким уникальным идентификатором устройства
String makeAccessPointSsid() {
  String base = String(DefaultSettings::WIFI_SSID);
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
void setupWifi() {
  // Задаём статический IP 192.168.4.1 для точки доступа
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  String ssid = makeAccessPointSsid();                   // формируем SSID с суффиксом устройства
  WiFi.softAP(ssid.c_str(), DefaultSettings::WIFI_PASS); // создаём AP
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
  server.on("/api/tx", handleApiTx);                                 // отправка текста по радио
  server.on("/api/tx-image", handleApiTxImage);                      // отправка изображения по радио
  server.on("/cmd", handleCmdHttp);                                  // обработка команд
  server.on("/api/cmd", handleCmdHttp);                              // совместимый эндпоинт
  server.begin();                                                      // старт сервера
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());                                     // выводим адрес
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
#if SR_HAS_ESP_COREDUMP
  gCoreDumpClearPending = true;
  gCoreDumpClearAfterMs = millis() + 500;  // ждём старта фоновых задач
#endif
  KeyLoader::ensureStorage();
  Serial.print("Хранилище ключей: ");
  Serial.println(KeyLoader::backendName(KeyLoader::getBackend()));
  setupWifi();                                       // запускаем точку доступа
  radio.begin();
  tx.setAckEnabled(ackEnabled);
  tx.setAckRetryLimit(ackRetryLimit);
  tx.setSendPause(DefaultSettings::SEND_PAUSE_MS);
  tx.setAckTimeout(DefaultSettings::ACK_TIMEOUT_MS);
  tx.setEncryptionEnabled(encryptionEnabled);
  rx.setEncryptionEnabled(encryptionEnabled);
  rx.setBuffer(&recvBuf);                                   // сохраняем принятые пакеты
  // обработка входящих данных с учётом ACK
  rx.setCallback([&](const uint8_t* d, size_t l){
    if (l == 3 && d[0]=='A' && d[1]=='C' && d[2]=='K') { // пришёл ACK
      Serial.println("ACK: получен");
      tx.onAckReceived();
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
    if (handleKeyTransferFrame(d, l)) return;                // перехватываем кадр обмена ключами
    rx.onReceive(d, l);
  });
  Serial.println("Команды: BF <полоса>, SF <фактор>, CR <код>, BANK <e|w|t|a|h>, CH <номер>, PW <0-9>, RXBG <0|1>, TX <строка>, TXL <размер>, BCN, INFO, STS <n>, RSTS <n>, ACK [0|1], ACKR <повторы>, PAUSE <мс>, ACKT <мс>, ENC [0|1], PI, SEAR, TESTRXM, KEYTRANSFER SEND, KEYTRANSFER RECEIVE, KEYSTORE [auto|spiffs|nvs]");
}

void loop() {
#if SR_HAS_ESP_COREDUMP
    if (gCoreDumpClearPending && millis() >= gCoreDumpClearAfterMs) {
      clearCorruptedCoreDumpConfig();
      gCoreDumpClearPending = false;
    }
#endif
    server.handleClient();                  // обработка HTTP-запросов
    radio.loop();                           // обработка входящих пакетов
    tx.loop();                              // обработка очередей передачи
    processTestRxm();                       // генерация тестовых входящих сообщений
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
      // управление режимом повышенного усиления приёмника
    } else if (line.startsWith("RXBG")) {
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
      radio.sendBeacon();
      tx.completeExternalSend();
      Serial.println("Маяк отправлен");
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
      } else if (line.startsWith("TESTRXM ")) {
        String overrideText = line.substring(8);
        Serial.println(cmdTestRxm(&overrideText));
      } else if (line.equalsIgnoreCase("TESTRXM")) {
        Serial.println(cmdTestRxm());
      } else if (line.equalsIgnoreCase("KEYTRANSFER SEND")) {
        Serial.println(cmdKeyTransferSendLora());
      } else if (line.equalsIgnoreCase("KEYTRANSFER RECEIVE")) {
        Serial.println(cmdKeyTransferReceiveLora());
      } else if (line.startsWith("KEYSTORE")) {
        String arg = line.length() > 8 ? line.substring(8) : String();
        arg.trim();
        Serial.println(cmdKeyStorage(arg));
      } else if (line.startsWith("ENC ")) {
        encryptionEnabled = line.substring(4).toInt() != 0;
        tx.setEncryptionEnabled(encryptionEnabled);
        rx.setEncryptionEnabled(encryptionEnabled);
        Serial.print("ENC: ");
        Serial.println(encryptionEnabled ? "включено" : "выключено");
      } else if (line.equalsIgnoreCase("ENC")) {
        encryptionEnabled = !encryptionEnabled;
        tx.setEncryptionEnabled(encryptionEnabled);
        rx.setEncryptionEnabled(encryptionEnabled);
        Serial.print("ENC: ");
        Serial.println(encryptionEnabled ? "включено" : "выключено");
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
        Serial.print("ACKT: ");
        Serial.print(value);
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
