#include <Arduino.h>
#include <array>
#include <vector>
#include <deque>
#include <string>
#include <cstring> //  strlen
#include <algorithm> //  std::equal
#include <cstdio>    //  snprintf   JSON
#include <cstdlib>   //  strtol/strtof   HTTP-
#include <cerrno>    //    strtol/strtof
#include <cctype>    //  std::isspace    

// ---    ---
#include "radio_sx1262.h"
#include "tx_module.h"
#include "rx_module.h" //  
#include "default_settings.h"
#include "libs/config_loader/config_loader.h" //    

// ---   ---
#include "libs/text_converter/text_converter.h"   //  UTF-8 -> CP1251
#include "libs/simple_logger/simple_logger.h"     //  
#include "logger.h"                               //   
#include "libs/received_buffer/received_buffer.h" //   
#include "libs/packetizer/packet_gatherer.h"      //    
#include "libs/packetizer/packet_splitter.h"      //   
#include "libs/crypto/chacha20_poly1305.h"        // AEAD ChaCha20-Poly1305
#include "libs/key_loader/key_loader.h"           //    ECDH
#include "libs/key_transfer/key_transfer.h"       //     LoRa
#include "libs/key_transfer_waiter/key_transfer_waiter.h" //    KEYTRANSFER
#include "libs/protocol/ack_utils.h"              //  ACK-
#include "key_safe_mode.h"                        //     
#include "sse_buffered_writer.h"                  //   SSE-
#include "rx_serial_dump.h"                       //   RX-  Serial

// ---   - ---
#include <WiFi.h>        //   Wi-Fi
#include <WebServer.h>   //  HTTP-
#if defined(ARDUINO) && defined(ESP32)
#include <esp_system.h>  //     ESP32
#endif
#include "web/web_content.h"      //   -
#include "http_body_reader.h"     //   HTTP-
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
#include "esp_core_dump.h"      //  core dump  ESP32 (  )
#define SR_HAS_COREDUMP_IMAGE_CHECK 1
#else
#define SR_HAS_COREDUMP_IMAGE_CHECK 0
#endif
#include "esp_partition.h"      //     
#if SR_HAS_INCLUDE("spi_flash_mmap.h")
#include "spi_flash_mmap.h"     //      
#elif SR_HAS_INCLUDE("esp_spi_flash.h")
#include "esp_spi_flash.h"      //    
#endif
#ifndef SPI_FLASH_SEC_SIZE
#define SPI_FLASH_SEC_SIZE 4096  //     
#endif
#if SR_HAS_INCLUDE("esp_ipc.h")
#include "esp_ipc.h"            //      0
#define SR_HAS_ESP_IPC 0
#else
#define SR_HAS_ESP_IPC 0
#endif
#if SR_HAS_INCLUDE("esp_idf_version.h")
#include "esp_idf_version.h"    //    ESP-IDF
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
//   ,  /ver     
static const char kEmbeddedVersion[] PROGMEM =
#include "ver"
;
#else
//       
static const char kEmbeddedVersion[] =
#include "ver"
;
#endif

//   ,   Arduino
//       .
struct LegacyTestPacket;
struct TestRxmSpec;

//     Serial c   
RadioSX1262 radio;
//     160 ,      5000 
TxModule tx(radio, std::array<size_t,4>{
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY,
  DefaultSettings::TX_QUEUE_CAPACITY});
RxModule rx;                //  
ReceivedBuffer recvBuf;     //   
static const ConfigLoader::Config& gConfig = ConfigLoader::getConfig(); //  
bool ackEnabled = gConfig.radio.useAck; //    ACK
bool encryptionEnabled = gConfig.radio.useEncryption; //  
bool keyStorageReady = false;               //     
bool keySafeModeActive = false;             //        
uint8_t ackRetryLimit = gConfig.radio.ackRetryLimit; //     ACK
static constexpr uint32_t kAckDelayMinMs = 0;             //     ACK
static constexpr uint32_t kAckDelayMaxMs = 5000;          //     ACK
uint32_t ackResponseDelayMs = gConfig.radio.ackResponseDelayMs; //    ACK
bool lightPackMode = false;             //      
bool testModeEnabled = false;           //    SendMsg_BR/received_msg
uint8_t testModeLocalCounter = 0;       //      
bool rxSerialDumpEnabled = DefaultSettings::RX_SERIAL_DUMP_ENABLED; //   RX  Serial

WebServer server(80);       // HTTP-  -

template <typename T, size_t N>
constexpr size_t progmemLength(const T (&)[N]) {
  return N;
}

template <size_t N>
constexpr size_t progmemLength(const char (&array)[N]) {
  return N ? (N - 1) : 0;
}

//       JSON- API.
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

//   Serial    ,     Wi-Fi
bool waitForSerial(unsigned long timeout_ms) {
#if defined(ARDUINO)
  const unsigned long start = millis();
  while (!Serial) {
    if (timeout_ms > 0 && (millis() - start) >= timeout_ms) {
      return false;  //  -    USB
    }
    delay(10);  //    
  }
#else
  (void)timeout_ms;  //    Serial 
#endif
  return true;
}

//     SSE,  Arduino- 
//    .
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
String makeKeyStateJson();
void broadcastKeyState();
void broadcastKeyState(const String& payload);

//     
struct TestRxmState {
  bool active = false;          //   
  uint8_t produced = 0;         //   
  uint32_t nextAt = 0;          //     (millis)
} testRxmState;

//     TESTRXM
struct TestRxmSpec {
  uint8_t percent;  //     
  bool useGatherer; //     
};

//     (   PacketGatherer)
static constexpr size_t kTestRxmCount = 5;
static constexpr TestRxmSpec kTestRxmSpecs[kTestRxmCount] = {
  {100, false},
  {50,  false},
  {30,  false},
  {20,  false},
  {100, true},
};

//      
static const char kTestRxmLorem[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed eros arcu, "
    "ultricies et maximus vitae, porttitor condimentum erat. Nam commodo porttitor.";

//    TESTRXM    
static constexpr size_t kTestRxmMaxSourceLength = 2048; //    (~2 )
String testRxmSourceText;

//       LoRa
struct KeyTransferRuntime {
  bool waiting = false;                 //     
  bool completed = false;               //    
  String error;                         //      HTTP/Serial
  uint32_t last_msg_id = 0;             //   
  bool legacy_peer = false;             //      -
  bool ephemeral_active = false;        //     
  uint32_t waitStartedAt = 0;           //    (millis)
  uint32_t waitDeadlineAt = 0;          //    (millis, 0 =  -)
} keyTransferRuntime;

KeyTransferWaiter keyTransferWaiter;    //    KEYTRANSFER

//     KEYTRANSFER,     "  "
static void resetKeyTransferWaitTiming() {
  keyTransferRuntime.waitStartedAt = 0;
  keyTransferRuntime.waitDeadlineAt = 0;
}

// ---     Serial ---
static String serialLineBuffer;                         //   
static bool serialLineOverflow = false;                 //   
static unsigned long serialLastByteAtMs = 0;            //    
static constexpr size_t kSerialLineMaxLength = 1024;    //    
static constexpr unsigned long kSerialLineTimeoutMs = 2000; // -    (   )
static bool serialWasReady = false;                     //    Serial

//     hex- ( )
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

//  hex-   ;  / 
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

//     hex-
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

//   Arduino String -> std::string
static std::string toStdString(const String& value) {
  return std::string(value.c_str());
}

//      JSON
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
//      Idle-    IPC
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
static constexpr uint32_t kCoreDumpIpcTargetCore = 0; //  PRO    ESP-IDF
#else
#define SR_IDLE_TASK_HANDLE_FOR_CORE(core) xTaskGetIdleTaskHandleForCPU(core)
static constexpr uint32_t kCoreDumpIpcTargetCore = ESP_IPC_CPU_PRO; //  PRO   
#endif

// ,    core dump   FreeRTOS.
bool gCoreDumpClearPending = true;
uint32_t gCoreDumpClearAfterMs = 0;

//   IPC-  core dump
struct CoreDumpClearContext {
  const esp_partition_t* part = nullptr;      //  core dump
  uint8_t zeros[32] = {};                     //    
  esp_err_t eraseErr = ESP_OK;                //  
  esp_err_t writeErr = ESP_OK;                //  
};

//  / core dump   PRO  IPC
static void coreDumpClearIpc(void* arg) {
  CoreDumpClearContext* ctx = static_cast<CoreDumpClearContext*>(arg);
  if (!ctx || !ctx->part) {
    return;
  }
  //    ,      4 
  size_t eraseSize = std::min<size_t>(ctx->part->size, SPI_FLASH_SEC_SIZE);
  ctx->eraseErr = esp_partition_erase_range(ctx->part, 0, eraseSize);
  if (ctx->eraseErr != ESP_OK) {
    return;
  }
  ctx->writeErr = esp_partition_write(ctx->part, 0, ctx->zeros, sizeof(ctx->zeros));
}

//     core dump,      CRC
bool clearCorruptedCoreDumpConfig() {
#if SR_HAS_COREDUMP_IMAGE_CHECK
  //     core dump  ,   
  //     .
  esp_err_t checkErr = esp_core_dump_image_check();
  if (checkErr == ESP_OK) {
    Serial.println("Core dump:  ,   ");
    return true;
  }
  if (checkErr == ESP_ERR_NOT_FOUND) {
    Serial.println("Core dump:  ,   ");
    return true;
  }
  if (checkErr != ESP_ERR_INVALID_CRC && checkErr != ESP_ERR_INVALID_SIZE) {
    Serial.print("Core dump: image_check  =");
    Serial.println(static_cast<int>(checkErr));
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  Serial.print("Core dump: image_check    (=");
  Serial.print(static_cast<int>(checkErr));
  Serial.println(")   ");
#endif
  TaskHandle_t idle0 = SR_IDLE_TASK_HANDLE_FOR_CORE(0);
#if (portNUM_PROCESSORS > 1)
  TaskHandle_t idle1 = SR_IDLE_TASK_HANDLE_FOR_CORE(1);
#else
  TaskHandle_t idle1 = idle0;
#endif
  if (idle0 == nullptr || idle1 == nullptr) {
    Serial.println("Core dump: Idle-   ,  ");
    gCoreDumpClearAfterMs = millis() + 500;
    return false;
  }
  //    Arduino+ESP32  esp_core_dump_image_erase()
  //   assert  FreeRTOS ( Idle-   ).
  //    core dump   API  .
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  if (!part) {
    Serial.println("Core dump:   ");
    return true;
  }
  //      ,     
  static constexpr size_t kProbeSize = 32; //    esp_core_dump_flash_config_t
  uint8_t probe[kProbeSize];
  esp_err_t err = esp_partition_read(part, 0, probe, sizeof(probe));
  if (err != ESP_OK) {
    Serial.print("Core dump:  , =");
    Serial.println(static_cast<int>(err));
    //     core dump      .
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
    Serial.println("Core dump:   ");
    return true;
  }
  //     0xFF,  esp_core_dump    ,
  //         IPC   0.
  CoreDumpClearContext ctx;
  ctx.part = part;
  std::fill_n(ctx.zeros, sizeof(ctx.zeros), 0);
  esp_err_t ipcErr = ESP_OK;
#if SR_HAS_ESP_IPC
  ipcErr = esp_ipc_call_blocking(kCoreDumpIpcTargetCore, &coreDumpClearIpc, &ctx);
#else
  //    esp_ipc     ,     .
  coreDumpClearIpc(&ctx);
#endif
  if (ipcErr != ESP_OK) {
    Serial.print("Core dump: IPC-  , =");
    Serial.println(static_cast<int>(ipcErr));
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  if (ctx.eraseErr != ESP_OK) {
    Serial.print("Core dump:  , =");
    Serial.println(static_cast<int>(ctx.eraseErr));
    gCoreDumpClearAfterMs = millis() + 1000;
    return false;
  }
  if (ctx.writeErr == ESP_OK) {
    Serial.println("Core dump:  ");
    return true;
  }
  Serial.print("Core dump:  , =");
  Serial.println(static_cast<int>(ctx.writeErr));
  gCoreDumpClearAfterMs = millis() + 1000;
  return false;
}
#endif


//      ASCII
String vectorToString(const std::vector<uint8_t>& data) {
  String out;
  out.reserve(data.size());
  for (uint8_t b : data) {
    out += static_cast<char>(b);
  }
  return out;
}

// ,     JPEG   FF D8 FF
bool isLikelyJpeg(const uint8_t* data, size_t len) {
  if (!data || len < 3) {
    return false;
  }
  return data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

//  CP1251  UTF-8    -
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

//    
String receivedKindToString(ReceivedBuffer::Kind kind) {
  switch (kind) {
    case ReceivedBuffer::Kind::Raw:   return String("raw");
    case ReceivedBuffer::Kind::Split: return String("split");
    case ReceivedBuffer::Kind::Ready: return String("ready");
  }
  return String("unknown");
}

//  JSON-    
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

// ---  push-  SSE ---
static constexpr uint32_t kPushKeepAliveMs = 15000;   //  keep-alive  SSE 
static constexpr size_t kPushMaxClients = 4;          //   
static constexpr uint32_t kPushSendTimeoutMs = 1500;   //     
static constexpr size_t kPushSendChunkLimit = 512;     //      

struct PushClientSession {
  NetworkClient client;       //   SSE
  uint32_t lastActivityMs;    //    
  uint32_t lastKeepAliveMs;   //   keep-alive
  SseBufferedWriter<NetworkClient, String> writer; //      
};

static std::vector<PushClientSession> gPushSessions;  //   SSE
static uint32_t gPushNextEventId = 1;                 //   
static std::deque<LogHook::Entry> gPendingLogEntries; //      
static constexpr size_t kMaxPendingLogEntries = 64;   //    

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
  String message;          //      IRQ
  uint32_t uptimeMs = 0;   // uptime     IRQ
  uint32_t capturedAtMs = 0; //    (millis)
  bool hasValue = false;   //    - IRQ
  bool dirty = false;      //    SSE-
};

static LastIrqStatus gLastIrqStatus;                  //   IRQ  SSE

//      ,    Serial   SSE
void enqueueLogEntry(const LogHook::Entry& entry) {
  if (gPendingLogEntries.size() >= kMaxPendingLogEntries) {
    gPendingLogEntries.pop_front(); //      
  }
  gPendingLogEntries.push_back(entry);
}

void updateLastIrqStatus(const char* message, uint32_t uptimeMs) {
  if (!message) {
    return; //  ,  
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
    return; //      IRQ
  }
  if (!force && !gLastIrqStatus.dirty) {
    return; //     
  }
  if (!broadcastIrqStatus(gLastIrqStatus)) {
    return; //       
  }
  gLastIrqStatus.dirty = false;
}

static void onRadioIrqLog(const char* message, uint32_t uptimeMs) {
  updateLastIrqStatus(message, uptimeMs);
}

//  JSON-      
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

//  JSON-    SSE
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

//   SSE-;  false,    
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
    return false; //  -       
  }
  return true;
}

//   keep-alive    
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
      continue; // ,    
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

//     
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

//      SSE
void broadcastKeyState(const String& payload) {
  if (gPushSessions.empty()) return;
  maintainPushSessions();
  String eventName = F("keystate");
  uint32_t eventId = gPushNextEventId++;
  for (auto it = gPushSessions.begin(); it != gPushSessions.end();) {
    if (!sendSseFrame(*it, eventName, payload, eventId)) {
      it->client.stop();
      it = gPushSessions.erase(it);
    } else {
      ++it;
    }
  }
}

void broadcastKeyState() {
  broadcastKeyState(makeKeyStateJson());
}

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

//       SSE-
void flushPendingLogEntries() {
  if (gPendingLogEntries.empty() || gPushSessions.empty()) {
    return; //   ,    
  }
  while (!gPendingLogEntries.empty() && !gPushSessions.empty()) {
    LogHook::Entry entry = gPendingLogEntries.front();
    gPendingLogEntries.pop_front();
    if (!broadcastLogEntry(entry)) {
      //     ( )       
      gPendingLogEntries.push_front(entry);
      break;
    }
  }
}

//   SSE  /events
void handleSseConnect() {
  NetworkClient baseClient = server.client();
  if (!baseClient) {
    return;
  }
  baseClient.setTimeout(0);
  baseClient.setNoDelay(true);
  NetworkClient client = baseClient; //      
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
  String keyStateSnapshot = makeKeyStateJson();
  if (keyStateSnapshot.length()) {
    if (!sendSseFrame(stored, F("keystate"), keyStateSnapshot, gPushNextEventId++)) {
      stored.client.stop();
      gPushSessions.pop_back();
      return;
    }
  }
  flushPendingLogEntries(); //       
  flushPendingIrqStatus(true); //   IRQ   
  static uint32_t lastPushLogMs = 0;
  const int32_t sinceLastLog = static_cast<int32_t>(now - lastPushLogMs);
  if (lastPushLogMs == 0 || sinceLastLog < 0 || sinceLastLog >= 30000) {
    LOG_INFO("HTTP push:  ");
    lastPushLogMs = now;
  }
}

//      
uint32_t nextTestRxmId() {
  static uint32_t counter = 60000;
  counter = (counter + 1) % 100000;
  if (counter == 0) counter = 1;
  return counter;
}

//       SendMsg_BR/received_msg
uint32_t nextTestModeMessageId() {
  static uint32_t counter = 90000;
  counter = (counter + 1) % 100000;
  if (counter == 0) counter = 1;
  return counter;
}

//    0/1/on/off   TESTMODE
bool parseOnOffToken(const String& value, bool& out) {
  String token = value;
  token.trim();
  if (token.length() == 0) return false;
  String lower = token;
  lower.toLowerCase();
  if (lower == "1" || lower == "on" || lower == "true" || lower == "" || lower == "" || lower == "") {
    out = true;
    return true;
  }
  if (lower == "0" || lower == "off" || lower == "false" || lower == "" || lower == "" || lower == "") {
    out = false;
    return true;
  }
  if (lower == "toggle" || lower == "swap" || lower == "" || lower == "") {
    out = !testModeEnabled;
    return true;
  }
  return false;
}

//      SendMsg_BR
static constexpr size_t kLegacyPacketSize = 112;
static constexpr size_t kLegacyHeaderSize = 9;
static constexpr size_t kLegacyPayloadMax = 96;
static constexpr uint8_t kLegacySourceAddress = 1;
static constexpr uint8_t kLegacyBroadcastAddress = 0;

//       
struct LegacyTestPacket {
  std::array<uint8_t, kLegacyPacketSize> data{}; //   
  size_t length = kLegacyHeaderSize;             //  
  uint8_t source = kLegacySourceAddress;         //  
  uint8_t destination = kLegacyBroadcastAddress; //   ()
  bool encrypted = false;                        //  
  uint8_t keyIndex = 0;                          //   ( )
};

//     SendMsg_BR
LegacyTestPacket buildLegacyTestPacket(const std::vector<uint8_t>& payload) {
  LegacyTestPacket packet;
  packet.data.fill(0);
  static uint8_t seqA = 0x2A; //    
  static uint8_t seqB = 0x7C;
  seqA = static_cast<uint8_t>(seqA + 1);
  seqB = static_cast<uint8_t>(seqB + 3);
  packet.data[1] = seqA;
  packet.data[2] = seqB;
  packet.data[0] = packet.data[1] ^ packet.data[2];
  packet.data[3] = packet.source;
  packet.data[4] = packet.destination;
  packet.data[5] = 0; //    
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

//       received_msg
String formatLegacyDisplayString(const LegacyTestPacket& packet, const String& decoded) {
  String text = decoded;
  if (text.length() == 0) text = String("");
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

//    SendMsg_BR > received_msg    
bool testModeProcessMessage(const String& payload, uint32_t& outId, String& err) {
  String trimmed = payload;
  trimmed.trim();
  if (trimmed.length() == 0) {
    err = " ";
    return false;
  }
  std::vector<uint8_t> cp = utf8ToCp1251(trimmed.c_str());
  if (cp.empty()) {
    err = " ";
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

//      
void setTestRxmSourceText(const String& text) {
  String normalized = text;
  normalized.trim();
  if (normalized.length() == 0) {
    testRxmSourceText = "";
    DEBUG_LOG("TESTRXM:   ");
    return;
  }
  const size_t limit = kTestRxmMaxSourceLength;
  const size_t normalizedLen = static_cast<size_t>(normalized.length());
  if (normalizedLen > limit) {
    normalized.remove(static_cast<unsigned int>(limit));
  }
  testRxmSourceText = normalized;
  DEBUG_LOG_VAL("TESTRXM:  , =", static_cast<int>(testRxmSourceText.length()));
}
String makeTestRxmPayload(uint8_t index, const TestRxmSpec& spec) {
  const char* sourcePtr = nullptr;
  size_t sourceLen = 0;
  if (testRxmSourceText.length() > 0) {                           //   
    sourcePtr = testRxmSourceText.c_str();
    sourceLen = static_cast<size_t>(testRxmSourceText.length());
  } else {                                                         //    Lorem ipsum
    sourcePtr = kTestRxmLorem;
    sourceLen = strlen(kTestRxmLorem);
  }
  if (sourceLen == 0) {                                           //    
    sourcePtr = kTestRxmLorem;
    sourceLen = strlen(kTestRxmLorem);
  }
  size_t take = (sourceLen * spec.percent + 99) / 100;             //  
  if (take == 0) take = sourceLen;                                 //    
  if (take > sourceLen) take = sourceLen;                          //   
  String payload;
  const size_t prefixReserve = spec.useGatherer ? 96 : 64;         //     UTF-8
  payload.reserve(static_cast<unsigned int>(prefixReserve + take)); //    
  payload += "RXM Lorem ";
  payload += String(static_cast<int>(index + 1));
  payload += " (";
  payload += String(static_cast<int>(spec.percent));
  payload += "%";
  if (spec.useGatherer) payload += ", ";
  payload += "): ";
  for (size_t i = 0; i < take; ++i) {                              //    
    payload += sourcePtr[i];
  }
  return payload;
}

//         
void enqueueTestRxmMessage(uint32_t id, const TestRxmSpec& spec, const String& payload) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(payload.c_str());
  const size_t len = static_cast<size_t>(payload.length());
  if (len == 0) return;                                            //    

  if (!spec.useGatherer) {                                         //   
    recvBuf.pushReady(id, data, len);
    return;
  }

  PacketGatherer gatherer(PayloadMode::SMALL, DefaultSettings::GATHER_BLOCK_SIZE);
  gatherer.reset();                                                //   
  const size_t chunk = DefaultSettings::GATHER_BLOCK_SIZE;         //    ""
  size_t offset = 0;
  uint32_t part = 0;
  while (offset < len) {                                           //   
    size_t take = std::min(chunk, len - offset);
    recvBuf.pushRaw(id, part, data + offset, take);                //   R-xxxxxx
    gatherer.add(data + offset, take);                             //   
    offset += take;
    ++part;
  }
  const auto& combined = gatherer.get();                           //   SP-xxxxx
  if (!combined.empty()) {
    recvBuf.pushSplit(id, combined.data(), combined.size());
    recvBuf.pushReady(id, combined.data(), combined.size());       //  GO-xxxxx
  } else {
    recvBuf.pushReady(id, data, len);                              // fallback    
  }
}

//  JSON   
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

//        UI/CLI
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

//  JSON-     
String keySafeModeErrorJson() {
  String json = "{\"error\":\"key-safe-mode\",\"message\":\"  \",\"safeMode\":true";
  if (keySafeModeHasReason()) {
    json += ",\"context\":\"";
    json += escapeJson(keySafeModeReason());
    json += "\"";
  }
  json += "}";
  return json;
}

//      (true   )
bool keyOperationsAllowed() {
  return keyStorageReady && !keySafeModeActive;
}

//       
bool ensureKeyStorageReady(const char* reason) {
  bool ok = KeyLoader::ensureStorage();
  if (ok) {
    deactivateKeySafeMode(reason);
  } else {
    activateKeySafeMode(reason);
  }
  return ok;
}

//           
bool ensureKeyOperationsAvailable(const char* reason) {
  if (keyOperationsAllowed()) {
    return true;
  }
  return ensureKeyStorageReady(reason);
}

//       
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

static constexpr uint32_t kKeyTransferReceiveTimeoutMs = 8000; // -   KEYTRANSFER

//  JSON    
String makeKeyTransferErrorJson(const String& code) {
  String response = String(F("{\"error\":\"")) + code + F("\"");
  if (code.equalsIgnoreCase("verify")) {
    response += F(",\"message\":\"  \"");
  } else if (code.equalsIgnoreCase("timeout")) {
    response += F(",\"message\":\" -  \"");
  } else if (code.equalsIgnoreCase("busy")) {
    response += F(",\"message\":\"  KEYTRANSFER   \"");
  } else if (code.equalsIgnoreCase("ephemeral")) {
    response += F(",\"message\":\"    \"");
  }
  response += F("}");
  return response;
}

//  JSON   
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

//    KEYTRANSFER;  true,   
bool handleKeyTransferFrame(const uint8_t* data, size_t len) {
  uint32_t msg_id = 0;
  KeyTransfer::FramePayload payload;
  if (!KeyTransfer::parseFrame(data, len, payload, msg_id)) {
    return false;                                  //      
  }
  keyTransferRuntime.last_msg_id = msg_id;
  keyTransferRuntime.legacy_peer = (payload.version == KeyTransfer::VERSION_LEGACY);
  if (!keyTransferRuntime.waiting) {
    SimpleLogger::logStatus("KEYTRANSFER IGN");   //   
    return true;                                   //     RxModule
  }
  const bool certificate_supplied =
      ((payload.flags & KeyTransfer::FLAG_HAS_CERTIFICATE) != 0) ||
      payload.version == KeyTransfer::VERSION_CERTIFICATE;
  if (certificate_supplied) {
    if (KeyTransfer::hasTrustedRoot()) {
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
        Serial.print("KEYTRANSFER:   : ");
        Serial.println(cert_error.c_str());
        return true;
      }
    } else if (payload.certificate.valid && !payload.certificate.chain.empty()) {
      // Сертификат присутствует, но доверенный корень не задан — пропускаем проверку для совместимой прошивки.
      SimpleLogger::logStatus("KEYTRANSFER CERT SKIP");
      Serial.println("KEYTRANSFER:    ,      ");
    }
  } else if (KeyTransfer::hasTrustedRoot()) {
    Serial.println("KEYTRANSFER:        ");
  }

  KeyLoader::KeyRecord previous_snapshot;          //      
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
    Serial.println("KEYTRANSFER:    ");
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
        reloadCryptoModules();                        //    
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
      Serial.print("KEYTRANSFER:   (local=");
      Serial.print(toHex(local_id));
      Serial.print(", remote=");
      Serial.print(toHex(payload.key_id));
      Serial.println(")");
      Serial.println("KEYTRANSFER:    ");
      return true;
    }
  }
  reloadCryptoModules();                          //  Tx/Rx  
  keyTransferRuntime.completed = true;
  keyTransferRuntime.waiting = false;
  keyTransferRuntime.error = String();
  if (keyTransferRuntime.ephemeral_active) {
    KeyLoader::endEphemeralSession();
    keyTransferRuntime.ephemeral_active = false;
  }
  resetKeyTransferWaitTiming();
  String keyStateJson = makeKeyStateJson();
  keyTransferWaiter.finalizeSuccess(toStdString(keyStateJson));
  broadcastKeyState(keyStateJson);
  SimpleLogger::logStatus("KEYTRANSFER OK");
  Serial.println("KEYTRANSFER:     LoRa");
  return true;
}

String cmdKeyState() {
  DEBUG_LOG("Key:  ");
  String keyStateJson = makeKeyStateJson();
  broadcastKeyState(keyStateJson);
  return keyStateJson;
}

//     (NVS/auto)
String cmdKeyStorage(const String& mode) {
  String trimmed = mode;
  trimmed.trim();
  if (trimmed.length() == 0) {
    return makeKeyStorageStatusJson();
  }
  if (trimmed.equalsIgnoreCase("RETRY")) {
    LOG_INFO("Key:       RETRY");
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
  DEBUG_LOG("Key:   ");
  if (!ensureKeyOperationsAvailable("command KEYGEN")) {
    return keySafeModeErrorJson();
  }
  if (KeyLoader::generateLocalKey()) {
    const bool synced_with_peer = KeyLoader::regenerateFromPeer();
    //      ,   .
    if (synced_with_peer) {
      DEBUG_LOG("Key:       ");
    }
    reloadCryptoModules();
    String keyStateJson = makeKeyStateJson();
    broadcastKeyState(keyStateJson);
    return keyStateJson;
  }
  return String("{\"error\":\"keygen\"}");
}

String cmdKeyGenPeer() {
  DEBUG_LOG("Key:    ");
  if (!ensureKeyOperationsAvailable("command KEYGEN PEER")) {
    return keySafeModeErrorJson();
  }
  if (KeyLoader::regenerateFromPeer()) {
    reloadCryptoModules();
    String keyStateJson = makeKeyStateJson();
    broadcastKeyState(keyStateJson);
    return keyStateJson;
  }
  return String("{\"error\":\"peer\"}");
}

String cmdKeyRestoreSecure() {
  DEBUG_LOG("Key:     ");
  if (!ensureKeyOperationsAvailable("command KEYRESTORE")) {
    return keySafeModeErrorJson();
  }
  if (KeyLoader::restorePreviousKey()) {
    reloadCryptoModules();
    String keyStateJson = makeKeyStateJson();
    broadcastKeyState(keyStateJson);
    return keyStateJson;
  }
  return String("{\"error\":\"restore\"}");
}

String cmdKeySendSecure() {
  return cmdKeyTransferSendLora();                   //     
}

String cmdKeyReceiveSecure(const String& hex) {
  DEBUG_LOG("Key:   ");
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
  String keyStateJson = makeKeyStateJson();
  broadcastKeyState(keyStateJson);
  return keyStateJson;
}

//     LoRa   
String cmdKeyTransferSendLora() {
  DEBUG_LOG("Key:    LoRa");
  if (!ensureKeyOperationsAvailable("command KEYTRANSFER SEND")) {
    return keySafeModeErrorJson();
  }
  auto state = KeyLoader::getState();
  std::array<uint8_t,4> key_id{};
  std::array<uint8_t,32> ephemeral_public{};
  const std::array<uint8_t,32>* ephemeral_ptr = nullptr;
  bool use_legacy = keyTransferRuntime.legacy_peer;
  if (use_legacy) {
    //   -      
    KeyLoader::endEphemeralSession();
    bool has_peer = KeyLoader::hasPeerPublic();
    if (has_peer) {
      if (!KeyLoader::previewPeerKeyId(key_id)) {
        SimpleLogger::logStatus("KEYTRANSFER ERR PREVIEW");
        Serial.println("KEYTRANSFER:    ");
        return String("{\"error\":\"peer\"}");
      }
    } else {
      key_id.fill(0);
    }
  } else {
    //       X25519
    if (!KeyLoader::startEphemeralSession(ephemeral_public, true)) {
      SimpleLogger::logStatus("KEYTRANSFER ERR EPHEM");
      Serial.println("KEYTRANSFER:     ");
      return String("{\"error\":\"ephemeral\"}");
    }
    key_id.fill(0);  //       
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
    Serial.println("KEYTRANSFER:  ,  ");
    return String("{\"error\":\"radio_busy\"}");
  }
  if (sendState != IRadio::ERR_NONE) {
    SimpleLogger::logStatus("KEYTRANSFER ERR RADIO");
    Serial.print("KEYTRANSFER:  , =");
    Serial.println(sendState);
    return String("{\"error\":\"radio\"}");
  }
  keyTransferRuntime.last_msg_id = msg_id;
  SimpleLogger::logStatus("KEYTRANSFER SEND");
  Serial.println("KEYTRANSFER:    ");
  String keyStateJson = makeKeyStateJson();
  broadcastKeyState(keyStateJson);
  return keyStateJson;
}

//       LoRa
String cmdKeyTransferReceiveLora(KeyTransferCommandOrigin origin) {
  DEBUG_LOG("Key:    LoRa");
  if (!ensureKeyOperationsAvailable("command KEYTRANSFER RECEIVE")) {
    return keySafeModeErrorJson();
  }

  //          .
  if (origin == KeyTransferCommandOrigin::Http && keyTransferWaiter.httpPending()) {
    String ready = String(keyTransferWaiter.consumeHttp().c_str());
    return ready.length() ? ready : makeKeyTransferWaitingJson();
  }
  if (origin == KeyTransferCommandOrigin::Serial && keyTransferWaiter.serialPending()) {
    String ready = String(keyTransferWaiter.consumeSerial().c_str());
    return ready.length() ? ready : makeKeyTransferWaitingJson();
  }

  //    ,   .
  if (keyTransferRuntime.waiting) {
    return makeKeyTransferWaitingJson();
  }

  //   ,      .
  if (keyTransferWaiter.hasPendingResult()) {
    return makeKeyTransferErrorJson(String("busy"));
  }

  std::array<uint8_t,32> prepared_ephemeral{};
  if (!KeyLoader::startEphemeralSession(prepared_ephemeral, false)) {
    SimpleLogger::logStatus("KEYTRANSFER ERR EPHEM");
    Serial.println("KEYTRANSFER:       ");
    return makeKeyTransferErrorJson(String("ephemeral"));
  }

  //   :      .
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
    Serial.println("KEYTRANSFER:   ");
  }
  return makeKeyTransferWaitingJson();
}

//     KEYTRANSFER  loop()
void processKeyTransferReceiveState() {
  if (keyTransferRuntime.waiting) {
    //     , TX  push-,    .
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
        Serial.println("KEYTRANSFER:  -  ");
      }
    }
  }

  //     ,  JSON    .
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

//   index.html
void handleRoot() {
  sendProgmemAsset("text/html; charset=utf-8", INDEX_HTML, progmemLength(INDEX_HTML), false);
}

//   style.css
void handleStyleCss() {
  sendProgmemAsset("text/css; charset=utf-8", STYLE_CSS, progmemLength(STYLE_CSS));
}

//   script.js
void handleScriptJs() {
  sendProgmemAsset("application/javascript; charset=utf-8", SCRIPT_JS, progmemLength(SCRIPT_JS));
}

//   SHA-256
void handleSha256Js() {
  sendProgmemAsset("application/javascript; charset=utf-8", SHA256_JS, progmemLength(SHA256_JS));
}

//    MGRS > /
void handleMgrsJs() {
  sendProgmemAsset("application/javascript; charset=utf-8", MGRS_JS, progmemLength(MGRS_JS));
}

//  CSV-  
void handleFreqInfoCsv() {
  sendProgmemAsset("text/csv; charset=utf-8", FREQ_INFO_CSV, progmemLength(FREQ_INFO_CSV));
}

//    TLE   Pointing
void handleGeostatTleJs() {
  sendProgmemAsset("application/javascript; charset=utf-8", GEOSTAT_TLE_JS, progmemLength(GEOSTAT_TLE_JS));
}

//       
void handleApiTx() {
  if (server.method() != HTTP_POST) {                       //   POST
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  // ,      Content-Type
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
  String body = server.arg("plain");                       //   
  uint32_t testId = 0;                                      //   
  String testErr;                                           //   
  bool simulated = false;                                   //   
  if (testModeEnabled) {
    simulated = testModeProcessMessage(body, testId, testErr);
    if (!simulated && testErr.length() > 0) {               //    
      std::string log = "TESTMODE:    ";
      log += testErr.c_str();
      SimpleLogger::logStatus(log);
    }
  }
  uint32_t id = 0;                                          //   
  String err;                                               //   
  if (enqueueTextMessage(body, id, err)) {                  //    Serial-
    String resp = "ok:";
    resp += String(id);
    server.send(200, "text/plain", resp);
  } else {
    if (testModeEnabled) {                                  //     
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
    server.send(400, "text/plain", err);                   //   
  }
}

//  JPEG-      
void handleApiTxImage() {
  if (server.method() != HTTP_POST) {                       //   POST
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  //  MIME-     
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
  }();                                                     //    
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
  auto client = server.client();                            //   HTTP-
  std::vector<uint8_t> payload;
  bool lengthExceeded = false;
  bool incomplete = false;
  auto delayFn = []() { delay(1); };                        //   
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
  //      ( )
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
    SimpleLogger::logStatus(log);                            //    
  } else {
    server.send(400, "text/plain", err);
  }
}

//       
ChannelBank parseBankChar(char b) {
  switch (b) {
    case 'e': case 'E': return ChannelBank::EAST;
    case 'w': case 'W': return ChannelBank::WEST;
    case 't': case 'T': return ChannelBank::TEST;
    case 'a': case 'A': return ChannelBank::ALL;
    case 'h': case 'H': return ChannelBank::HOME;
    case 'n': case 'N': return ChannelBank::NEW;
    default: return radio.getBank();
  }
}

//      
void processTestRxm() {
  if (!testRxmState.active) return;                        //   
  uint32_t now = millis();
  int32_t delta = static_cast<int32_t>(now - testRxmState.nextAt);
  if (delta < 0) return;                                   //  

  const uint8_t index = testRxmState.produced;
  if (index >= kTestRxmCount) {                            //     
    testRxmState.active = false;
    testRxmState.nextAt = 0;
    return;
  }

  const TestRxmSpec& spec = kTestRxmSpecs[index];          //  
  String payload = makeTestRxmPayload(index, spec);        //   
  const uint32_t id = nextTestRxmId();                     //  
  enqueueTestRxmMessage(id, spec, payload);                //   

  DEBUG_LOG("TESTRXM:   ");
  DEBUG_LOG_VAL("TESTRXM: id=", id);
  DEBUG_LOG_VAL("TESTRXM: bytes=", static_cast<int>(payload.length()));
  if (spec.useGatherer) {
    DEBUG_LOG("TESTRXM:    ");
  }

  testRxmState.produced++;
  if (testRxmState.produced >= kTestRxmCount) {
    testRxmState.active = false;
    testRxmState.nextAt = 0;
    DEBUG_LOG("TESTRXM:  ");
  } else {
    testRxmState.nextAt = now + 500;                       //    0.5 
  }
}

//  TESTRXM       
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
  DEBUG_LOG("TESTRXM:    ");
  String json = "{\"status\":\"scheduled\",\"count\":";
  json += String(static_cast<int>(kTestRxmCount));
  json += ",\"intervalMs\":500}";
  return json;
}

String cmdTestRxm() {
  return cmdTestRxm(nullptr);
}

//      
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

//     
String cmdSear() {
  uint8_t prevCh = radio.getChannel();
  String out;
  for (int ch = 0; ch < radio.getBankSize(); ++ch) {
    if (!radio.setChannel(ch)) {
      out += "CH "; out += String(ch); out += ": \n";
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
    (void)respLen; //      
    (void)elapsed; //     
    if (pingState == IRadio::ERR_NONE) {
      out += "CH "; out += String(ch); out += ": RSSI ";
      out += String(radio.getLastRssi()); out += " SNR ";
      out += String(radio.getLastSnr()); out += "\n";
    } else if (pingState == RadioSX1262::ERR_TIMEOUT) {
      out += "CH "; out += String(ch); out += ":  \n";
    } else if (pingState == RadioSX1262::ERR_PING_TIMEOUT) {
      out += "CH "; out += String(ch); out += ": -\n";
    } else {
      out += "CH "; out += String(ch); out += ": err=";
      out += String(pingState); out += "\n";
    }
  }
  radio.setChannel(prevCh);
  return out;
}

//        CSV
String cmdChlist(ChannelBank bank) {
  //  CSV   ch,tx,rx,rssi,snr,st,scan
  String out = "ch,tx,rx,rssi,snr,st,scan\n";
  uint16_t n = RadioSX1262::bankSize(bank);
  for (uint16_t i = 0; i < n; ++i) {
    out += String(i);                               //  
    out += ',';
    out += String(RadioSX1262::bankTx(bank, i), 3); //  
    out += ',';
    out += String(RadioSX1262::bankRx(bank, i), 3); //  
    out += ",0,0,,\n";                             //   rssi/snr/st/scan
  }
  return out;
}

//        HTTP
String bankToLetter(ChannelBank bank) {
  switch (bank) {
    case ChannelBank::EAST: return "e";
    case ChannelBank::WEST: return "w";
    case ChannelBank::TEST: return "t";
    case ChannelBank::ALL:  return "a";
    case ChannelBank::HOME: return "h";
    case ChannelBank::NEW:  return "n";
    default:                return "";
  }
}

//     ACK
String ackStateText() {
  return ackEnabled ? String("ACK:1") : String("ACK:0");
}

//    Light pack
String lightPackStateText() {
  return lightPackMode ? String("LIGHT:1") : String("LIGHT:0");
}

//       
bool enqueueTextMessage(const String& payload, uint32_t& outId, String& err) {
  String trimmed = payload;
  trimmed.trim();
  if (trimmed.length() == 0) {
    err = " ";
    return false;
  }
  std::vector<uint8_t> data = utf8ToCp1251(trimmed.c_str());
  if (data.empty()) {
    err = " ";
    return false;
  }
  uint32_t id = 0;
  if (lightPackMode) {                                   // Light pack     
    id = tx.queuePlain(data.data(), data.size());
    if (id == 0) {                                       //    (    )
      id = tx.queue(data.data(), data.size(), 0, true);  //      
    }
  } else {
    id = tx.queue(data.data(), data.size(), 0, true);
  }
  if (id == 0) {
    err = " ";
    return false;
  }
  tx.loop();
  outId = id;
  return true;
}

//    (, JPEG)   
bool enqueueBinaryMessage(const uint8_t* data, size_t len, uint32_t& outId, String& err) {
  if (!data || len == 0) {
    err = " ";
    return false;
  }
  tx.setPayloadMode(PayloadMode::LARGE);                    //     
  uint32_t id = tx.queue(data, len);
  tx.setPayloadMode(PayloadMode::SMALL);                    //   
  if (id == 0) {
    err = " ";
    return false;
  }
  tx.loop();
  outId = id;
  return true;
}

//  TX     HTTP-
String cmdTx(const String& payload) {
  uint32_t testId = 0;                               //    
  String testErr;                                    //    
  bool simulated = false;                            //    
  if (testModeEnabled) {
    simulated = testModeProcessMessage(payload, testId, testErr);
    if (!simulated && testErr.length() > 0) {        //   
      std::string log = "TESTMODE:    ";
      log += testErr.c_str();
      SimpleLogger::logStatus(log);
    }
  }

  uint32_t id = 0;                                   //   
  String err;                                        //   
  if (enqueueTextMessage(payload, id, err)) {
    String ok = "TX:OK id=";                         //    
    ok += String(id);
    return ok;
  }

  String out = "TX:ERR ";
  out += err;                                        //     
  if (testModeEnabled) {
    if (simulated) {                                 //    
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

//  TXL     
String cmdTxl(size_t sz) {
  if (sz == 0 || sz > 8192) {
    return String("TXL:  ");
  }
  std::vector<uint8_t> data(sz);
  for (size_t i = 0; i < sz; ++i) {
    data[i] = static_cast<uint8_t>(i & 0xFF);
  }
  tx.setPayloadMode(PayloadMode::LARGE);
  uint32_t id = tx.queue(data.data(), data.size());
  tx.setPayloadMode(PayloadMode::SMALL);
  if (id == 0) {
    return String("TXL:  ");
  }
  tx.loop();
  String out = "TXL:OK id=";
  out += String(id);
  out += " size=";
  out += String(sz);
  return out;
}

//     SendMsg_BR/received_msg
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
    testModeLocalCounter = 0;                                  //   
    SimpleLogger::logStatus(testModeEnabled ? "TESTMODE ON" : "TESTMODE OFF");
    Serial.print("TESTMODE: ");
    Serial.println(testModeEnabled ? "" : "");
  }
  String resp = "TESTMODE:";
  resp += testModeEnabled ? "1" : "0";
  return resp;
}

//  BCN   
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

//  ENCT      
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
  DEBUG_LOG("ENCT:   ");
  return String("{\"status\":\"error\"}");
}

//    
String cmdInfo() {
  String s;
  s += "Bank: ";
  switch (radio.getBank()) {
    case ChannelBank::EAST: s += ""; break;
    case ChannelBank::WEST: s += ""; break;
    case ChannelBank::TEST: s += ""; break;
    case ChannelBank::ALL: s += "All"; break;
    case ChannelBank::HOME: s += "Home"; break;
    case ChannelBank::NEW: s += "New"; break;
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
  s += "\nACK: "; s += ackEnabled ? "" : "";
  s += "\nRX boosted gain: ";
  s += radio.isRxBoostedGainEnabled() ? "" : "";
  s += "\nLight pack: "; s += lightPackMode ? "" : "";
  return s;
}

//   
String cmdSts(int cnt) {
  if (cnt <= 0) cnt = 10;
  auto logs = SimpleLogger::getLast(cnt);
  String s;
  for (const auto& l : logs) {
    s += l.c_str(); s += '\n';
  }
  return s;
}

//     
String cmdRsts(int cnt) {
  if (cnt <= 0) cnt = 10;
  auto names = recvBuf.list(cnt);
  String s;
  for (const auto& n : names) {
    s += n.c_str(); s += '\n';
  }
  return s;
}

// JSON-   
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

//        HTTP-   
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
    errorMessage = String(" ") + valueName + "    .";
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
    errorMessage = String("  ") + valueName + "     .";
    return false;
  }
  while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  if (*end != '\0') {
    errorMessage = String("    ") + valueName + " (\"" + rawInput + "\").";
    return false;
  }
  if (parsed < minValue || parsed > maxValue) {
    String range = String(minValue, static_cast<unsigned int>(precision));
    range += "";
    range += String(maxValue, static_cast<unsigned int>(precision));
    if (units && units[0] != '\0') {
      range += " ";
      range += units;
    }
    errorMessage = String("  ") + valueName + " \"" + trimmed + "\"    " + range + ".";
    return false;
  }
  outValue = parsed;
  return true;
}

//      HTTP-     
static bool parseIntArgument(const String& rawInput,
                             long minValue,
                             long maxValue,
                             long& outValue,
                             String& errorMessage,
                             const char* valueName) {
  String trimmed = rawInput;
  trimmed.trim();
  if (trimmed.length() == 0) {
    errorMessage = String(" ") + valueName + "    .";
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
    errorMessage = String("   ") + valueName + "      long.";
    return false;
  }
  while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  if (*end != '\0') {
    errorMessage = String("    ") + valueName + " (\"" + rawInput + "\").";
    return false;
  }
  if (parsed < minValue || parsed > maxValue) {
    errorMessage = String("  ") + valueName + " \"" + trimmed + "\"    "
                 + String(minValue) + "" + String(maxValue) + ".";
    return false;
  }
  outValue = parsed;
  return true;
}

//      (BW/BF)
static bool parseBandwidthArgument(const String& rawInput, float& outValue, String& errorMessage) {
  static constexpr float kBandwidthMinKHz = 7.81f;
  static constexpr float kBandwidthMaxKHz = 31.25f;
  return parseFloatArgument(rawInput, kBandwidthMinKHz, kBandwidthMaxKHz, outValue, errorMessage, "BW", "");
}

//   spreading factor
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

//   coding rate
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

//       
static bool parseChannelArgument(const String& rawInput,
                                 uint16_t bankSize,
                                 uint16_t& outValue,
                                 String& errorMessage) {
  if (bankSize == 0) {
    errorMessage = "    .";
    return false;
  }
  long parsed = 0;
  if (!parseIntArgument(rawInput, 0, static_cast<long>(bankSize) - 1, parsed, errorMessage, "")) {
    return false;
  }
  outValue = static_cast<uint16_t>(parsed);
  return true;
}

//    
static bool parsePowerPresetArgument(const String& rawInput, uint8_t& outValue, String& errorMessage) {
  static constexpr long kPowerPresetMin = 0;
  static constexpr long kPowerPresetMax = 9;
  long parsed = 0;
  if (!parseIntArgument(rawInput, kPowerPresetMin, kPowerPresetMax, parsed, errorMessage, " ")) {
    return false;
  }
  outValue = static_cast<uint8_t>(parsed);
  return true;
}

//  HTTP-  /cmd?c=PI
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
        String message = String("   BW=") + String(bwValue, 2) + "      .";
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
        String message = String("   SF=") + String(sfValue) + "     .";
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
        String message = String("   CR=") + String(crValue) + "     .";
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
        String message = String("     ") + String(preset) + ".";
        respondBadRequest(message);
      } else {
        resp = String("PW:OK");
      }
    } else {
      resp = String(radio.getPower());
    }
  } else if (cmd == "RXBG") {
    if (server.hasArg("v")) {
      bool enable = server.arg("v").toInt() != 0;           //      ""
      if (radio.setRxBoostedGainMode(enable)) {
        resp = String("RXBG:");
        resp += radio.isRxBoostedGainEnabled() ? "1" : "0"; //   
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
        respondBadRequest(" BANK    .");
      } else {
        const char letter = raw[0];
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        const bool known = (upper == 'E' || upper == 'W' || upper == 'T' || upper == 'A' || upper == 'H');
        if (!known) {
          respondBadRequest(String("   \"") + raw + "\".  : E, W, T, A, H.");
        } else if (!radio.setBank(parseBankChar(letter))) {
          respondBadRequest(String("    \"") + raw + "\".");
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
        String message = String("     ") + String(parsedChannel) + ".";
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
        respondBadRequest(" bank    .");
      } else {
        const char letter = rawBank[0];
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        const bool known = (upper == 'E' || upper == 'W' || upper == 'T' || upper == 'A' || upper == 'H');
        if (!known) {
          respondBadRequest(String("   \"") + rawBank + "\".  : E, W, T, A, H.");
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
      resp = String("0 (  ,    -)");  //    0
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
    contentType = "application/json"; //    JSON
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
      contentType = "application/json"; //    JSON
    } else {
      resp = cmdTestRxm();
      contentType = "application/json"; //    JSON
    }
  } else if (cmd == "KEYSTATE") {
    resp = cmdKeyState();
    contentType = "application/json"; //    JSON
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
    contentType = "application/json"; //    JSON
  } else if (cmd == "KEYGEN") {
    if (cmdArg.equalsIgnoreCase("PEER")) {
      resp = cmdKeyGenPeer();
    } else {
      resp = cmdKeyGenSecure();
    }
    contentType = "application/json"; //    JSON
  } else if (cmd == "KEYRESTORE") {
    resp = cmdKeyRestoreSecure();
    contentType = "application/json"; //    JSON
  } else if (cmd == "KEYSEND") {
    resp = cmdKeyTransferSendLora();
    contentType = "application/json"; //    JSON
  } else if (cmd == "KEYRECV") {
    String peerHex;
    if (server.hasArg("peer")) {
      peerHex = server.arg("peer");
    } else if (server.hasArg("pub")) {
      peerHex = server.arg("pub"); // устаревший псевдоним для совместимости
    }
    resp = cmdKeyReceiveSecure(peerHex);
    contentType = "application/json"; //    JSON
  } else if (cmd == "KEYTRANSFER") {
    if (cmdArg == "SEND") {
      resp = cmdKeyTransferSendLora();
      contentType = "application/json"; //    JSON
    } else if (cmdArg == "RECEIVE") {
      resp = cmdKeyTransferReceiveLora(KeyTransferCommandOrigin::Http);
      contentType = "application/json"; //    JSON
    } else {
      resp = String("{\"error\":\"mode\"}");
      contentType = "application/json"; //    JSON
    }
  } else {
    resp = "UNKNOWN";
  }
  server.send(statusCode, contentType, resp);
}

void handleVer() {
  server.send(200, "text/plain", readVersionFile());
}

//   N     Debug
void handleLogHistory() {
  size_t limit = 120;                                      //   
  if (server.hasArg("n")) {
    String arg = server.arg("n");
    arg.trim();
    long requested = arg.toInt();
    if (requested > 0) {
      if (requested > 200) requested = 200;                //   
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

//  SSID       
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
  base += "-000000"; //     
#endif
  return base;
}

//  Wi-Fi     
bool setupWifi() {
  String ssid = makeAccessPointSsid();                   //  SSID   
#if defined(ARDUINO)
  //           ,
  //       STA      .
  WiFi.persistent(false);
  WiFi.softAPdisconnect(true);
#if defined(WIFI_MODE_NULL)
  WiFi.mode(WIFI_MODE_NULL);                             //    Wi-Fi
  delay(20);
#endif
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  //           
  constexpr uint8_t kMaxApAttempts = 3;
  bool apStarted = false;
  for (uint8_t attempt = 0; attempt < kMaxApAttempts && !apStarted; ++attempt) {
    if (attempt > 0) {
      LOG_WARN("Wi-Fi:     ( %u)", static_cast<unsigned>(attempt + 1));
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
  if (!apStarted) {                                      //  AP
    LOG_ERROR("Wi-Fi:      %s", ssid.c_str());
    return false;
  }

  //   IP 192.168.4.1   
  IPAddress local_ip;
  IPAddress gateway;
  IPAddress subnet;
  bool ipOk = local_ip.fromString(gConfig.wifi.ip.c_str());
  bool gatewayOk = gateway.fromString(gConfig.wifi.gateway.c_str());
  bool subnetOk = subnet.fromString(gConfig.wifi.subnet.c_str());
  if (!ipOk || !gatewayOk || !subnetOk) {
    LOG_WARN("Wi-Fi:   IP  ,   ");
    local_ip = IPAddress(192, 168, 4, 1);
    gateway = IPAddress(192, 168, 4, 1);
    subnet = IPAddress(255, 255, 255, 0);
  }
  if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
    LOG_WARN("Wi-Fi:     IP,    ");
  }
#else
  //           -.
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
  server.on("/", handleRoot);                                         //  
  server.on("/style.css", handleStyleCss);                           // CSS -
  server.on("/script.js", handleScriptJs);                           // JS 
  server.on("/libs/sha256.js", handleSha256Js);                      //  SHA-256
  server.on("/libs/mgrs.js", handleMgrsJs);                          //  MGRS
  server.on("/libs/freq-info.csv", handleFreqInfoCsv);               //   
  server.on("/libs/geostat_tle.js", handleGeostatTleJs);             //   
  server.on("/ver", handleVer);                                      //  
  server.on("/events", HTTP_GET, handleSseConnect);                  // SSE- push-
  server.on("/api/logs", handleLogHistory);                          //   Serial
  server.on("/api/tx", handleApiTx);                                 //    
  server.on("/api/tx-image", handleApiTxImage);                      //    
  server.on("/cmd", handleCmdHttp);                                  //  
  server.on("/api/cmd", handleCmdHttp);                              //  
  server.begin();                                                      //  
#if defined(ARDUINO)
  String ip = WiFi.softAPIP().toString();
  LOG_INFO("Wi-Fi:   %s , IP %s", ssid.c_str(), ip.c_str());
#else
  LOG_INFO("Wi-Fi: -     (SSID %s)", ssid.c_str());
#endif
  return true;
}

void setup() {
  Logger::init();                                        //      
#if defined(ARDUINO)
  static TaskHandle_t loggerTaskHandle = nullptr;        //    
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
  //   safe mode,       .
  configureKeySafeModeController(
      [](bool enabled) {
        tx.setEncryptionEnabled(enabled);
        rx.setEncryptionEnabled(enabled);
      },
      &encryptionEnabled,
      &keySafeModeActive,
      &keyStorageReady);
  Serial.begin(115200);
  bool serialReady = waitForSerial(1500);                //   Serial,   
  serialWasReady = serialReady;                          //      
  //   UART  KeyLoader  Serial    .
  KeyLoader::setLogCallback([](const __FlashStringHelper* msg) -> bool {
    if (!Serial) {
      return false;                                      //  ,  USB 
    }
    Serial.println(msg);
    return true;
  });
#if SR_HAS_ESP_COREDUMP
  gCoreDumpClearPending = true;
  gCoreDumpClearAfterMs = millis() + 500;  //    
#endif
  bool storageReadyNow = ensureKeyStorageReady("startup");
  LogHook::setDispatcher([](const LogHook::Entry& entry) {
    enqueueLogEntry(entry); //       
  });
  if (!serialReady) {
    LOG_WARN("Serial: USB-  ,     ");
  }
  if (storageReadyNow) {
    String backendName = KeyLoader::backendName(KeyLoader::getBackend());
    LOG_INFO(" : %s", backendName.c_str());
  } else {
    LOG_WARN("Key:       ensureStorage()");
  }
  if (!setupWifi()) {                                 //   
    LOG_ERROR("Wi-Fi: -    ");
  }
  //        ,     setup()
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
                  "RadioSX1262: radio.begin()  %u  , =",
                  static_cast<unsigned>(attempt));
    LOG_ERROR_VAL(prefix, code);
  }
  if (!radioReady) {
    LOG_ERROR("RadioSX1262:   ,     ");
  }
  tx.setAckEnabled(ackEnabled);
  tx.setAckRetryLimit(ackRetryLimit);
  tx.setSendPause(gConfig.radio.sendPauseMs);
  tx.setAckTimeout(gConfig.radio.ackTimeoutMs);
  ackResponseDelayMs = gConfig.radio.ackResponseDelayMs; //    ACK
  tx.setAckResponseDelay(ackResponseDelayMs);
  tx.setEncryptionEnabled(encryptionEnabled);
  rx.setEncryptionEnabled(encryptionEnabled);
  rx.setBuffer(&recvBuf);                                   //   
  recvBuf.setNotificationCallback([](ReceivedBuffer::Kind kind, const ReceivedBuffer::Item& item) {
    //     SSE
    broadcastReceivedPush(kind, item);
  });
  //      ACK
  rx.setAckCallback([&]() {
    LOG_INFO("ACK: ");
    tx.onAckReceived();
  });
  rx.setCallback([&](const uint8_t* d, size_t l){
    if (protocol::ack::isAckPayload(d, l)) {              // ACK    
      return;
    }
#if defined(ARDUINO)
    if (rxSerialDumpEnabled && Serial) {                  //  ,  USB-  
      (void)dumpRxToSerialWithPrefix(Serial, d, l);       //     
    }
#else
    (void)rxSerialDumpEnabled;
#endif
    LOG_INFO("RX:   %u ", static_cast<unsigned>(l));
    if (ackEnabled) {                                     //  
      const uint8_t ack_msg[1] = {protocol::ack::MARKER};
      tx.queue(ack_msg, sizeof(ack_msg));
      tx.loop();
    }
  });
  radio.setReceiveCallback([&](const uint8_t* d, size_t l){  //  
    if (handleKeyTransferFrame(d, l)) return;                //    
    rx.onReceive(d, l);
  });
  radio.setIrqLogCallback(onRadioIrqLog);                    //  IRQ-  SSE    Serial
  LOG_INFO(": BF <>, SF <>, CR <>, BANK <e|w|t|a|h|n>, CH <>, PW <0-9>, RXBG <0|1>, TX <>, TXL <>, BCN, INFO, STS <n>, RSTS <n>, ACK [0|1], LIGHT [0|1], ACKR <>, PAUSE <>, ACKT <>, ACKD <>, ENC [0|1], PI, SEAR, TESTRXM, KEYTRANSFER SEND, KEYTRANSFER RECEIVE, KEYSTORE [auto|nvs]");
}

void loop() {
#if SR_HAS_ESP_COREDUMP
    if (gCoreDumpClearPending && millis() >= gCoreDumpClearAfterMs) {
      if (clearCorruptedCoreDumpConfig()) {
        gCoreDumpClearPending = false;
      }
    }
#endif
    server.handleClient();                  //  HTTP-
    maintainPushSessions();                 //   push-
    flushPendingLogEntries();               //      Serial
    flushPendingIrqStatus();                //    IRQ   
    const bool serialNowReady = static_cast<bool>(Serial); //   USB-
    if (serialNowReady && !serialWasReady) {
      KeyLoader::flushBufferedLogs();       // Serial       KeyLoader
    }
    serialWasReady = serialNowReady;
    if (!keyTransferRuntime.waiting) {
      radio.loop();                         //   
      rx.tickCleanup();                     //    RX    
      tx.loop();                            //   
    }
    processTestRxm();                       //    
    processKeyTransferReceiveState();       //    KEYTRANSFER
    const unsigned long now = millis();
    if (serialLineBuffer.length() > 0 && serialLastByteAtMs != 0 &&
        (now - serialLastByteAtMs) > kSerialLineTimeoutMs) {
      serialLineBuffer = "";              // -:   
      serialLineOverflow = false;
      serialLastByteAtMs = 0;
    }

    while (Serial.available()) {
      const int incoming = Serial.read();
      if (incoming < 0) {
        break;                             //      
      }
      serialLastByteAtMs = millis();
      if (incoming == '\r') {
        continue;                          //  CR   CRLF
      }
      if (incoming != '\n') {
        if (!serialLineOverflow) {
          if (serialLineBuffer.length() < kSerialLineMaxLength) {
            serialLineBuffer += static_cast<char>(incoming);
          } else {
            serialLineOverflow = true;     //    
          }
        }
        continue;
      }

      String line = serialLineBuffer;
      const bool overflowed = serialLineOverflow;
      serialLineBuffer = "";
      serialLineOverflow = false;

      if (overflowed) {
        Serial.println(":       ");
        continue;
      }

      line.trim();
      if (line.length() == 0) {
        continue;                          //   
      }

      if (line.startsWith("BF ") || line.startsWith("BW ")) {
        float bw = line.substring(3).toFloat();
        if (radio.setBandwidth(bw)) {
          Serial.println(" ");
        } else {
          Serial.println("  BW");
        }
      } else if (line.startsWith("SF ")) {
        int sf = line.substring(3).toInt();
        if (radio.setSpreadingFactor(sf)) {
          Serial.println("SF ");
        } else {
          Serial.println("  SF");
        }
      } else if (line.startsWith("CR ")) {
        int cr = line.substring(3).toInt();
        if (radio.setCodingRate(cr)) {
          Serial.println("CR ");
        } else {
          Serial.println("  CR");
        }
      } else if (line.startsWith("BANK ")) {
        char b = line.charAt(5);
        if (b == 'e' || b == 'E') {
          radio.setBank(ChannelBank::EAST);
          Serial.println(" ");
        } else if (b == 'w' || b == 'W') {
          radio.setBank(ChannelBank::WEST);
          Serial.println(" ");
        } else if (b == 't' || b == 'T') {
          radio.setBank(ChannelBank::TEST);
          Serial.println(" ");
        } else if (b == 'a' || b == 'A') {
          radio.setBank(ChannelBank::ALL);
          Serial.println(" All");
        } else if (b == 'h' || b == 'H') {
          radio.setBank(ChannelBank::HOME);
          Serial.println(" Home");
        } else if (b == 'n' || b == 'N') {
          radio.setBank(ChannelBank::NEW);
          Serial.println(" New");
        }
      } else if (line.startsWith("CH ")) {
        int ch = line.substring(3).toInt();
        if (radio.setChannel(ch)) {
          Serial.print(" ");
          Serial.println(ch);
        } else {
          Serial.println("  ");
        }
      } else if (line.startsWith("PW ")) {
        int pw = line.substring(3).toInt();
        if (radio.setPower(pw)) {
          Serial.println(" ");
        } else {
          Serial.println("  ");
        }
      } else if (line.startsWith("RXBG")) {                   //     
        if (line.length() <= 4) {
          Serial.print("RXBG: ");
          Serial.println(radio.isRxBoostedGainEnabled() ? "" : "");
        } else {
          String arg = line.substring(4);
          arg.trim();
          bool desired = radio.isRxBoostedGainEnabled();
          bool parsed = false;
          if (arg.equalsIgnoreCase("1") || arg.equalsIgnoreCase("ON") || arg.equalsIgnoreCase("TRUE") ||
              arg.equalsIgnoreCase("")) {
            desired = true;
            parsed = true;
          } else if (arg.equalsIgnoreCase("0") || arg.equalsIgnoreCase("OFF") || arg.equalsIgnoreCase("FALSE") ||
                     arg.equalsIgnoreCase("")) {
            desired = false;
            parsed = true;
          } else if (arg.equalsIgnoreCase("TOGGLE") || arg.equalsIgnoreCase("SWAP")) {
            desired = !desired;
            parsed = true;
          }
          if (!parsed) {
            Serial.println("RXBG:  RXBG <0|1|toggle>");
          } else if (radio.setRxBoostedGainMode(desired)) {
            Serial.print("RXBG: ");
            Serial.println(radio.isRxBoostedGainEnabled() ? "" : "");
          } else {
            Serial.println("RXBG:   ");
          }
        }
      } else if (line.equalsIgnoreCase("BCN")) {
        tx.prepareExternalSend();
        int16_t state = radio.sendBeacon();
        tx.completeExternalSend();
        if (state == RadioSX1262::ERR_TIMEOUT) {
          Serial.println("  :  ");
        } else if (state != IRadio::ERR_NONE) {
          Serial.print("  , =");
          Serial.println(state);
        } else {
          Serial.println(" ");
        }
      } else if (line.equalsIgnoreCase("INFO")) {
        //    
        Serial.print(": ");
        switch (radio.getBank()) {
          case ChannelBank::EAST: Serial.println(""); break;
          case ChannelBank::WEST: Serial.println(""); break;
          case ChannelBank::TEST: Serial.println(""); break;
          case ChannelBank::ALL: Serial.println("All"); break;
          case ChannelBank::HOME: Serial.println("Home"); break;
          case ChannelBank::NEW: Serial.println("New"); break;
        }
        Serial.print(": "); Serial.println(radio.getChannel());
        Serial.print("RX: "); Serial.print(radio.getRxFrequency(), 3); Serial.println(" MHz");
        Serial.print("TX: "); Serial.print(radio.getTxFrequency(), 3); Serial.println(" MHz");
        Serial.print("BW: "); Serial.print(radio.getBandwidth(), 2); Serial.println(" kHz");
        Serial.print("SF: "); Serial.println(radio.getSpreadingFactor());
        Serial.print("CR: "); Serial.println(radio.getCodingRate());
        Serial.print("Power: "); Serial.print(radio.getPower()); Serial.println(" dBm");
        Serial.print("Pause: "); Serial.print(tx.getSendPause()); Serial.println(" ms");
        Serial.print("ACK timeout: "); Serial.print(tx.getAckTimeout()); Serial.println(" ms");
        Serial.print("ACK delay: "); Serial.print(ackResponseDelayMs); Serial.println(" ms");
        Serial.print("ACK: "); Serial.println(ackEnabled ? "" : "");
      } else if (line.startsWith("STS")) {
        int cnt = line.length() > 3 ? line.substring(4).toInt() : 10;
        if (cnt <= 0) cnt = 10;                       //   
        auto logs = SimpleLogger::getLast(cnt);
        for (const auto& s : logs) {
          Serial.println(s.c_str());
        }
      } else if (line.startsWith("RSTS")) {
        String args = line.substring(4);                              //   
        args.trim();
        bool wantJson = false;                                        //   
        int cnt = 10;                                                 //    
        if (args.length() > 0) {
          String rest = args;
          while (rest.length() > 0) {                                 //    
            int space = rest.indexOf(' ');
            String token = space >= 0 ? rest.substring(0, space) : rest;
            rest = space >= 0 ? rest.substring(space + 1) : String();
            token.trim();
            rest.trim();
            if (token.length() == 0) continue;                        //   
            if (token.equalsIgnoreCase("FULL") || token.equalsIgnoreCase("JSON")) {
              wantJson = true;                                        //  JSON-
              continue;
            }
            int value = token.toInt();                                //   
            if (value > 0) cnt = value;                               //    
          }
        }
        if (cnt <= 0) cnt = 10;                                       //    
        if (wantJson) {
          String json = cmdRstsJson(cnt);                             //   
          Serial.println(json);                                       //  JSON   
        } else {
          auto names = recvBuf.list(cnt);                             //   
          for (const auto& n : names) {
            Serial.println(n.c_str());
          }
        }
      } else if (line.startsWith("TX ")) {
        String msg = line.substring(3);                     //  
        uint32_t testId = 0;                                //   
        String testErr;                                     //   
        bool simulated = false;                             //    
        if (testModeEnabled) {
          DEBUG_LOG("RC:   TX");
          simulated = testModeProcessMessage(msg, testId, testErr);
        }

        uint32_t id = 0;                                    //   
        String err;                                         //    
        if (enqueueTextMessage(msg, id, err)) {
          DEBUG_LOG_VAL("RC:   id=", static_cast<int>(id));
          Serial.print(" , id=");
          Serial.println(id);
        } else {
          Serial.print("  : ");
          Serial.println(err);
        }

        if (testModeEnabled) {                              //    
          if (simulated) {
            Serial.print("TESTMODE:   id=");
            Serial.println(testId);
          } else {
            Serial.print("TESTMODE:   ");
            Serial.println(testErr);
          }
        }
      } else if (line.startsWith("TXL ")) {
        int sz = line.substring(4).toInt();                //   
        if (sz > 0) {
          //        
          std::vector<uint8_t> data(sz);
          for (int i = 0; i < sz; ++i) {
            data[i] = static_cast<uint8_t>(i);            //  
          }
          tx.setPayloadMode(PayloadMode::LARGE);          //    
          uint32_t id = tx.queue(data.data(), data.size());
          tx.setPayloadMode(PayloadMode::SMALL);          //    
          if (id != 0) {
            tx.loop();                                   //   
            Serial.println("  ");
          } else {
            Serial.println("   ");
          }
        } else {
          Serial.println(" ");
        }
      } else if (line.startsWith("TESTMODE")) {
        String arg = line.length() > 8 ? line.substring(8) : String();
        arg.trim();
        String response = cmdTestMode(arg);
        Serial.println(response);
      } else if (line.equalsIgnoreCase("ENCT")) {
        //  :  ,   
        const uint8_t key[16]   = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        const uint8_t nonce[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
        const char* text = "Test ENCT";                    //  
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
          Serial.println("ENCT: ");
        } else {
          Serial.println("ENCT: ");
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
          Serial.println("ENC:     ");
        } else {
          encryptionEnabled = line.substring(4).toInt() != 0;
          tx.setEncryptionEnabled(encryptionEnabled);
          rx.setEncryptionEnabled(encryptionEnabled);
          Serial.print("ENC: ");
          Serial.println(encryptionEnabled ? "" : "");
        }
      } else if (line.equalsIgnoreCase("ENC")) {
        if (keySafeModeActive) {
          Serial.println("ENC:     ");
        } else {
          encryptionEnabled = !encryptionEnabled;
          tx.setEncryptionEnabled(encryptionEnabled);
          rx.setEncryptionEnabled(encryptionEnabled);
          Serial.print("ENC: ");
          Serial.println(encryptionEnabled ? "" : "");
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
          Serial.println(" ms (  ,    -)");  //    
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
        if (line.length() > 3) {                          //   
          ackEnabled = line.substring(4).toInt() != 0;
        } else {
          ackEnabled = !ackEnabled;                       // 
        }
        tx.setAckEnabled(ackEnabled);
        Serial.print("ACK: ");
        Serial.println(ackEnabled ? "" : "");
      } else if (line.startsWith("LIGHT")) {
        if (line.length() > 5) {                          //   
          lightPackMode = line.substring(6).toInt() != 0;
        } else {
          lightPackMode = !lightPackMode;                 //  
        }
        Serial.print("LIGHT: ");
        Serial.println(lightPackMode ? "" : "");
      } else if (line.equalsIgnoreCase("PI")) {
        //     
        ReceivedBuffer::Item dump;
        while (recvBuf.popReady(dump)) {}

        //  5-     
        std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> ping{};
        ping[1] = radio.randomByte();
        ping[2] = radio.randomByte();       // ID 
        ping[0] = ping[1] ^ ping[2];        //  = XOR
        ping[3] = 0;                        //  (  )
        ping[4] = 0;

        tx.queue(ping.data(), ping.size()); //    
        while (!tx.loop()) {                //   
          delay(1);                         //  
        }

        uint32_t t_start = micros();       //  
        bool ok = false;                   //  
        ReceivedBuffer::Item resp;

        //  ,    
        while (micros() - t_start < DefaultSettings::PING_WAIT_MS * 1000UL) {
          radio.loop();
          if (recvBuf.popReady(resp)) {
            if (resp.data.size() == ping.size() &&
                memcmp(resp.data.data(), ping.data(), ping.size()) == 0) {
              ok = true;                   //   
            }
            break;
          }
          delay(1);
        }

        if (ok) {
          uint32_t t_end = micros() - t_start;               //  
          float dist_km =
              ((t_end * 0.000001f) * 299792458.0f / 2.0f) / 1000.0f; //  
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
          Serial.println(": -");
        }
      } else if (line.equalsIgnoreCase("SEAR")) {
        //        
        uint8_t prevCh = radio.getChannel();         //   
        for (int ch = 0; ch < radio.getBankSize(); ++ch) {
          if (!radio.setChannel(ch)) {               // 
            Serial.print("CH ");
            Serial.print(ch);
            Serial.println(": ");
            continue;
          }
          //     
          ReceivedBuffer::Item d;
          while (recvBuf.popReady(d)) {}
          //    
          std::array<uint8_t, DefaultSettings::PING_PACKET_SIZE> pkt2{};
          pkt2[1] = radio.randomByte();
          pkt2[2] = radio.randomByte();         // ID 
          pkt2[0] = pkt2[1] ^ pkt2[2];          // 
          pkt2[3] = 0;
          pkt2[4] = 0;
          tx.queue(pkt2.data(), pkt2.size());
          while (!tx.loop()) {                  //  
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
            Serial.println(": -");
          }
        }
        radio.setChannel(prevCh);                    //   
      }
    }
}






