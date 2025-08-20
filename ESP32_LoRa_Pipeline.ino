
#include <Arduino.h>
#include <vector>
#include <functional>
#include <Preferences.h>

#include "config.h"
#include "freq_map.h"
#include "frame.h"
#include "metrics.h"
#include "encryptor.h"
#include "encryptor_ccm.h"
#include "message_buffer.h"
#include "fragmenter.h"
#include "tx_pipeline.h"
#include "rx_pipeline.h"
#include "radio_adapter.h"
#include "frame_log.h"
#include "tdd_scheduler.h"
#include "selftest.h"
#include "satping.h"
#include "web_interface.h"
#include "web_style.h"
#include "web_script.h"
#include "crypto_spec.h"
#include "radio_state.h" // состояние радио

#include <string.h> // for memcmp
#include <ctype.h>   // for toupper when parsing hex strings
#include <math.h>    // для вычисления Eb/N0

// --- WiFi and Web server includes ---
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#ifdef ARDUINO_ARCH_ESP32
#include <mbedtls/ecdh.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/private_access.h>
#endif

#include <RadioLib.h>
#include "libs/ccsds_link/ccsds_link.cpp" // реализация функций encode/decode

static constexpr int PIN_NSS = 5;
static constexpr int PIN_DIO1 = 26;
static constexpr int PIN_NRST = 27;
static constexpr int PIN_BUSY = 25;
static constexpr int PIN_TCXO_DETECT = 15;

SX1262 radio = SX1262(new Module(PIN_NSS, PIN_DIO1, PIN_NRST, PIN_BUSY));

PipelineMetrics g_metrics;
CcmEncryptor g_ccm;
MessageBuffer g_buf;
Fragmenter g_frag;
TxPipeline g_tx(g_buf, g_frag, g_ccm, g_metrics);
RxPipeline g_rx(g_ccm, g_metrics);

Preferences prefs;
Bank g_bank = Bank::MAIN;
int g_preset = 0;
// Указатель на текущий список частот; позволяет быстро менять банк
// и не обращаться к функции GetFreqTable каждый раз.
const std::vector<FreqPreset>* g_freq_table = &FREQ_MAIN;
float g_freq_rx_mhz = (*g_freq_table)[0].rxMHz;
float g_freq_tx_mhz = (*g_freq_table)[0].txMHz;
float g_curr_freq_mhz = g_freq_rx_mhz; // текущая фактическая частота
float g_bw_khz = 125.0f;
uint8_t g_sf = 9;
uint8_t g_cr = 5;
int8_t g_txp = 14;
bool g_rx_boost = false;                // усиленный режим приёма

bool g_ack_on = cfg::ACK_ENABLED_DEFAULT;
bool g_enc_on = cfg::ENCRYPTION_ENABLED_DEFAULT;
uint8_t g_kid = 1;
uint8_t g_retryN = cfg::SEND_RETRY_COUNT_DEFAULT;
uint16_t g_retryMS = cfg::SEND_RETRY_MS_DEFAULT;
bool g_dup_on = cfg::HEADER_DUP_DEFAULT;
uint8_t g_window = cfg::SR_WINDOW_DEFAULT;
uint16_t g_ackAgg = cfg::T_ACK_AGG_DEFAULT;
uint8_t g_ackJitter = 0;                 // джиттер ожидания ACK в процентах
std::vector<float> g_backoff;            // коэффициенты увеличения паузы между повторами
uint8_t g_burst = cfg::SR_WINDOW_DEFAULT; // лимит фрагментов в серии
bool g_autoRate = false;                  // включена ли автонастройка профиля
float g_perHigh = 0.2f;                   // верхний порог PER для ухудшения
float g_perLow  = 0.05f;                  // нижний порог PER для улучшения
float g_ebn0High = 8.0f;                  // верхний порог Eb/N0 для улучшения
float g_ebn0Low  = 4.0f;                  // нижний порог Eb/N0 для ухудшения

// Таймстампы для простейшего rate-limit HTTP запросов
unsigned long g_last_send_ms = 0;   // последний /send
unsigned long g_last_ping_ms = 0;   // последний /ping

// ----------------------------------------------------------------------------
// Asynchronous ping state and radio busy flag.  When g_radio_busy is true the
// Tx pipeline will not transmit frames.  g_ping_active indicates an ongoing
// ping transaction; g_ping_stage tracks the state machine; g_ping_tx holds
// the current ping bytes; g_ping_start_us stores the microsecond timestamp
// when the ping was sent; g_ping_timeout_us defines the timeout in
// microseconds to wait for a response (for long‑distance links set ~1–2s).
volatile bool g_radio_busy = false;
volatile bool g_ping_active = false;
uint8_t g_ping_tx[5] = {0};
uint32_t g_ping_start_us = 0;
uint32_t g_ping_timeout_us = 1500000;  // default 1.5 seconds
int g_ping_stage = 0;

// Receive flag and buffer for asynchronous ping and RX callback.
// These globals are defined here so they are visible to handlePing() and
// handlePingAsync() below.  They are updated by Radio_onReceive() declared
// later in this file.
static volatile bool g_hasRx = false;
static std::vector<uint8_t> g_rxBuf;
volatile bool txDoneFlag = false;
volatile bool rxDoneFlag = false;

// ISR DIO1: выставляет флаги завершения TX и RX
// В новой версии RadioLib вызываем getIrqStatus(),
// который сразу очищает соответствующие флаги прерываний.
void IRAM_ATTR isr() {
  uint16_t irq = radio.getIrqStatus();
  if (irq & RADIOLIB_SX126X_IRQ_TX_DONE) {
    txDoneFlag = true;   // завершена передача
  }
  if (irq & RADIOLIB_SX126X_IRQ_RX_DONE) {
    rxDoneFlag = true;   // завершён приём
  }
  // Дополнительный вызов очистки не требуется
}

// -----------------------------------------------------------------------------
// Key status and request state
//
// g_key_status indicates whether the currently installed encryption key was
// generated locally (Local) or received from another device (Received).  The
// status is persisted in NVS (key_remote) so it survives reboots.  When
// g_key_status is Received the UI displays this and the key indicator uses
// the "remote" colour.
//
// g_key_request_active becomes true when a key request has been sent and
// remains true until a key response is received.  The UI uses this flag to
// blink the key status indicator to show a pending request.
enum class KeyStatus { Local = 0, Received = 1 };
static KeyStatus g_key_status = KeyStatus::Local;
static bool g_key_request_active = false;

// ---------------------------------------------------------------------------
// Key exchange using authenticated ECDH (KEYDH1/KEYDH2).
//
// To securely agree on a new session key without exposing it over the air, the
// code below implements a lightweight ECDH handshake authenticated with a
// pre‑shared root key.  The initiator sends a KEYDH1 message containing its
// public key and an HMAC over that key.  The responder verifies the HMAC,
// generates its own key pair, derives the shared secret, derives a new AES
// key, and responds with a KEYDH2 message that includes its public key and
// HMAC.  The initiator then derives the same secret and installs the new key.
// The g_key_request_active flag is reused to blink the UI indicator while a
// handshake is in progress.

static bool g_keydh_in_progress = false;
static bool g_keydh_initiator = false;
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
static mbedtls_ecdh_context g_keydh_ctx;
static mbedtls_ctr_drbg_context g_keydh_ctr_drbg;
static mbedtls_entropy_context g_keydh_entropy;
static unsigned char g_keydh_pub[65];
static size_t g_keydh_pub_len = 0;
#endif
// Maximum size of serialBuffer (for chat).  When exceeded the oldest
// characters will be discarded to prevent memory bloat.
static const size_t SERIAL_MAX = 8 * 1024;

// -----------------------------------------------------------------------------
// WiFi AP credentials.  When the device starts it will create a Wi‑Fi access
// point with this SSID and password.  Connect to the AP and browse to
// http://192.168.4.1/ to access the control panel.
const char* WIFI_SSID = "ESP32-LoRa";
const char* WIFI_PASSWORD = "12345678";

// WebServer instance listening on port 80.  Этот сервер отдаёт веб-интерфейс.
WebServer server(80);
// WebSocket сервер для мгновенной передачи логов
WebSocketsServer wsServer(81);

// Buffer used to accumulate messages for the web UI.  Whenever a message is
// sent or received over LoRa it is appended to this buffer.  The /serial
// endpoint returns the current buffer contents and then clears it.
String serialBuffer = "";

// Forward declarations of HTTP handlers.  See implementations below.
void handleRoot();
void handleSend();
void handleSerial();
void handleStyle();
void handleScript();
void handleSetBank();
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
void handleSetPreset();
void handleSetBw();
void handleSetSf();
void handleSetCr();
void handleSetTxp();
void handleSetRxBoost();
void handleSetFec();
void handleSetInter();
void handleSetPayload();
void handleSetPilot();
void handleSetDup();
void handleSetWin();
void handleSetAckAgg();
void handleSetBurst();
void handleSetAck();
void handleSetAckJitter();
void handleSetBackoff();
void handleSetAutorate();
void handleSetTxProfile();
void handleSetPerTh();
void handleSetEbn0Th();
void handleToggleEnc();
void handleMetrics();
void handlePing();
void handleChannelPing();
void handlePresetPing();
void handleMassPing();
void handleLinkDiag(); // выдача метрик канала
void handleSatPingAdv(); // расширенный satping
void handleSetRetryN();
void handleSetRetryMS();
void handleSetKid();
void handleSetKey();
void handleSave();
void handleLoad();
void handleReset();
void handleSetTdd();
void handleArchiveList();
void handleArchiveRestore();
void handleIdReset();
void handleReplayClr();
void handleFrames();
void runPing();
void handleKeyTest(); // new HTTP handler for key exchange test
bool performKeyExchange(); // local ECDH key exchange test implementation
// Handlers for additional commands exposed via the web UI.  These
// implement the SIMPLE, LARGE, ENCTEST, ENCTESTBAD and MSGID operations
// traditionally entered from the serial console.  Definitions are
// provided further down in this file.
void handleSimple();
void handleLarge();
void handleEncTest();
void handleEncTestBad();
void handleSelfTest();
void handleSetMsgId();
// Append a line to the chat buffer with automatic size limiting
void chatLine(const String& s);
// Perform asynchronous ping state progression; called from loop()
void handlePingAsync();
// Add handlers to control retry count and retry timeout if needed (optional)

// QoS handlers.  These endpoints expose the SENDQ, LARGEQ, QOS and
// QOSMODE commands over HTTP.  They allow sending messages with high,
// normal or low priority, enqueuing large test buffers for QoS,
// querying current queue usage and changing the scheduler mode.
void handleSendQ();
void handleLargeQ();
void handleQos();
void handleSetQosMode();

// Key request/response handlers.  These endpoints and helpers implement
// the exchange of encryption keys between devices.  A key request (KEYREQ)
// asks the remote device to send its current key, while a key response
// (KEYRES) contains the KID and 16‑byte key in hex.  The UI uses
// /keyreq to send a request, /keysend to send a response, and /keystatus
// to query the current key status and pending request flag.
void handleKeyReq();
void handleKeySend();
void handleKeyStatus();
// ECDH key exchange handlers and helpers
void handleKeyDh();
void startKeyDh();
bool processKeyDh(const String& m);
// Helpers to enqueue key request and response messages.  These functions
// append system messages to the chat, set g_key_request_active and
// persist the message ID.
void sendKeyRequest();
void sendKeyResponse();

// End of forward declarations

// Проверка простого rate-limit: запрещает обращения чаще указанного интервала
static bool rateLimit(unsigned long &last, unsigned long interval_ms) {
  unsigned long now = millis();
  if (now - last < interval_ms) return false;
  last = now;
  return true;
}

class SerialChatPrint : public Print {
  Print& serial;
  String line;
public:
  explicit SerialChatPrint(Print& s) : serial(s) {}
  size_t write(uint8_t c) override {
    serial.write(c);
    if (c == '\n') {
      chatLine(line);
      line = "";
    } else if (c != '\r') {
      line += (char)c;
    }
    return 1;
  }
};

static inline void persistMsgId() {
  prefs.begin("lora", false);
  prefs.putULong("msgid", g_buf.nextId());
  prefs.end();
}

// Вычисляет CRC‑16 (CRC‑CCITT) от текущего 16‑байтового AES‑ключа.
// Полученное 16‑битное значение служит основой для 4‑значного
// шестнадцатеричного идентификатора, отображаемого рядом с надписью
// "Local" в веб‑интерфейсе для проверки активного ключа.
static void applyPreset() {
  // Используем ранее выбранный список частот
  if (!g_freq_table) {
    g_freq_table = &FREQ_MAIN;
  }
  if (g_preset < 0 || g_preset >= (int)g_freq_table->size()) return;

  g_freq_rx_mhz = (*g_freq_table)[g_preset].rxMHz;
  g_freq_tx_mhz = (*g_freq_table)[g_preset].txMHz;
  Radio_setFrequency((uint32_t)(g_freq_rx_mhz * 1e6f));
  // Сразу возвращаем радио в приём на полный цикл TDD
  Radio_forceRx(msToTicks(tdd::cycleLen()));
}

static void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  HELP, VER|VERSION, RADIO, METRICS, DUMP <N>"));
  Serial.println(F("  SEND <text>, SIMPLE, LARGE [size]"));
  Serial.println(F("  SENDQ <H|N|L> <text>, LARGEQ <H|N|L> [size]"));
  Serial.println(F("  QOS, QOSMODE <STRICT|W421>"));
  Serial.println(F("  B <0..2>, P <0..9>"));
  Serial.println(F("  FREQRX <MHz>, FREQTX <MHz>, FREQ <MHz> (alias FREQRX)"));
  Serial.println(F("  BW <kHz>, SF <7..12>, CR <5..8>, TXP <dBm>"));
  Serial.println(F("  SAVE, LOAD, RESET"));
  Serial.println(F("  KA <0|1>, RETRYN <n>, RETRYMS <ms>"));
  Serial.println(F("  ENC <0|1>, KID <id>, KEY <hex32>"));
  Serial.println(F("  ENCTEST [size], ENCTESTBAD"));
  Serial.println(F("  MSGID [n]"));
  Serial.println(F("  KEYX"));
  Serial.println(F("  KEYDH"));
  Serial.println(F("  SATPING"));
}

static void printVersion() {
  Serial.print(F("PIPE v"));
  Serial.print(cfg::PIPE_VERSION);
  Serial.print(F(" | MTU="));
  Serial.print(cfg::LORA_MTU);
  Serial.print(F(" | payload/frame="));
  Serial.print(FRAME_PAYLOAD_MAX);
  Serial.println();
}

static void printMetrics() {
  Serial.printf("TX: frames=%u bytes=%u msgs=%u retries=%u ack_fail=%u ack_seen=%u ack_avg=%ums enc_fail=%u\n",
                g_metrics.tx_frames, g_metrics.tx_bytes, g_metrics.tx_msgs,
                g_metrics.tx_retries, g_metrics.ack_fail, g_metrics.ack_seen, g_metrics.ack_time_ms_avg,
                g_metrics.enc_fail);
  Serial.printf("RX: frames_ok=%u bytes=%u msgs_ok=%u frags=%u dup=%u crc_fail=%u drop_len=%u drop_ttl=%u drop_over=%u dec_fail_tag=%u dec_fail_oth=%u\n",
                g_metrics.rx_frames_ok, g_metrics.rx_bytes, g_metrics.rx_msgs_ok, g_metrics.rx_frags,
                g_metrics.rx_dup_msgs, g_metrics.rx_crc_fail, g_metrics.rx_drop_len_mismatch,
                g_metrics.rx_assem_drop_ttl, g_metrics.rx_assem_drop_overflow, g_metrics.dec_fail_tag, g_metrics.dec_fail_other);
}

// --- Управление состоянием радио -------------------------------------------

// Состояния радиомодуля описаны в radio_state.h
// Текущее состояние; по умолчанию радио в простое
static RadioState g_radio_state = RadioState::Idle;

// Обновление состояния с выводом сообщения только при фактической смене
static void Radio_updateState(RadioState st) {
  if (g_radio_state == st) { return; }
  g_radio_state = st;
  // Используем фактическую частоту из последнего вызова Radio_setFrequency
  float mhz = g_curr_freq_mhz;
  if (st == RadioState::Tx) {
    chatMsg("SYS", String("TX started ") + String(mhz, 3) + " MHz");
  } else if (st == RadioState::Rx) {
    chatMsg("SYS", String("RX started ") + String(mhz, 3) + " MHz");
  }
}

// ---------------------------------------------------------------------------

// Отправка сырых данных по радио
bool Radio_sendRaw(const uint8_t* data, size_t len) {
  // Переключаемся на частоту передачи через обёртку, чтобы избежать ошибок масштаба
  Radio_setFrequency((uint32_t)(g_freq_tx_mhz * 1e6f));
  // Смена состояния на передачу
  Radio_updateState(RadioState::Tx);
  int16_t st = radio.transmit(const_cast<uint8_t*>(data), len);
  radio.finishTransmit();             // принудительно завершаем передачу
  // Возвращаемся на актуальную частоту приёма той же обёрткой
  Radio_setFrequency((uint32_t)(g_freq_rx_mhz * 1e6f));
  // После передачи слушаем эфир на окно ACK+guard
  Radio_forceRx(msToTicks(tdd::ackWindowMs + tdd::guardMs));
  return st == RADIOLIB_ERR_NONE;
}
bool Radio_setFrequency(uint32_t hz) {
  // Конвертируем Гц в МГц и запоминаем установленное значение
  g_curr_freq_mhz = hz / 1e6f;
  return radio.setFrequency(g_curr_freq_mhz) == RADIOLIB_ERR_NONE;
}
bool Radio_setBandwidth(uint32_t khz) {
  return radio.setBandwidth(khz) == RADIOLIB_ERR_NONE;
}
bool Radio_setSpreadingFactor(uint8_t sf) {
  return radio.setSpreadingFactor(sf) == RADIOLIB_ERR_NONE;
}
bool Radio_setCodingRate(uint8_t cr4x) {
  return radio.setCodingRate(cr4x) == RADIOLIB_ERR_NONE;
}
bool Radio_setTxPower(int8_t dBm) {
  return radio.setOutputPower(dBm) == RADIOLIB_ERR_NONE;
}
bool Radio_setRxBoost(bool on) {
  // Включение/выключение усиленного режима приёма
  return radio.setRxBoostedGainMode(on, true) == RADIOLIB_ERR_NONE;
}
void Radio_forceRx(uint32_t rx_ticks) {
  // Перед запуском приёма заново выставляем частоту RX,
  // иначе драйвер может вернуться к 251.850 МГц после передачи
  Radio_setFrequency((uint32_t)(g_freq_rx_mhz * 1e6f));
  // Запуск приёма с таймаутом в тиках
  radio.startReceive(rx_ticks);
  // Смена состояния на приём
  Radio_updateState(RadioState::Rx);
}

// Получаем SNR последнего пакета из драйвера
bool Radio_getSNR(float& snr) {
  snr = radio.getSNR();
  return true;
}

// Расчёт Eb/N0 на основе текущего SNR и параметров модуляции
bool Radio_getEbN0(float& ebn0) {
  float snr = radio.getSNR();
  float efficiency = ((float)g_sf * 4.0f / (float)g_cr) / (1 << g_sf); // бит/Гц
  ebn0 = snr - 10.0f * log10f(efficiency);
  return true;
}

// Возвращает уровень RSSI последнего принятого пакета
bool Radio_getRSSI(float& rssi) {
  rssi = radio.getRSSI();
  return true;
}

static void radioPoll() {
  int16_t st = radio.available();
  if (st == RADIOLIB_ERR_NONE) {
    uint8_t tmp[cfg::LORA_MTU];
    int16_t len = radio.readData(tmp, sizeof(tmp));
    if (len == 5) {
      // Зеркальный пинг: приёмник немедленно возвращает эти 5 байт
      // отправителю для проверки канала связи
      radio.setFrequency(g_freq_tx_mhz);
      radio.transmit(tmp, 5);
      radio.finishTransmit();          // принудительно завершаем TX
      radio.setFrequency(g_freq_rx_mhz);
      radio.startReceive();
      return; // Не обрабатываем пакет дальше
    }
    if (len > 0) { Radio_onReceive(tmp, (size_t)len); }
  }
}

// UART
HardwareSerial SerialRadxa(1); // Additional UART for Radxa Zero 3W
static String g_line;
static String g_line_radxa;
static void handleCommand(const String& line);

static void saveConfig() {
  prefs.begin("lora", false);
  prefs.putUChar("bank", (uint8_t)g_bank);
  prefs.putChar("preset", (char)g_preset);
  prefs.putFloat("bw", g_bw_khz);
  prefs.putUChar("sf", g_sf);
  prefs.putUChar("cr", g_cr);
  prefs.putChar("txp", g_txp);
  prefs.putBool("rxboost", g_rx_boost);
  prefs.putBool("ack", g_ack_on);
  prefs.putUChar("rtryN", g_retryN);
  prefs.putUShort("rtryMS", g_retryMS);
  prefs.putBool("enc", g_enc_on);
  prefs.putUChar("kid", g_kid);
  prefs.putULong("msgid", g_buf.nextId());
  // Persist whether the current key came from remote (Received) or is local.
  prefs.putBool("key_remote", g_key_status == KeyStatus::Received);
  prefs.end();
  Serial.println(F("Saved."));
}

static void loadKeyFromNVS() {
  char name[8];
  snprintf(name, sizeof(name), "k%03u", (unsigned)g_kid);
  uint8_t key[16];
  prefs.begin("lora", true);
  size_t got = prefs.getBytesLength(name);
  if (got == 16) {
    prefs.getBytes(name, key, 16);
    g_ccm.setKey(g_kid, key, 16);
  }
  prefs.end();
}

static void loadConfig() {
  prefs.begin("lora", true);
  g_bank = (Bank)prefs.getUChar("bank", (uint8_t)g_bank);
  g_preset = (int)prefs.getChar("preset", (char)g_preset);
  g_bw_khz = prefs.getFloat("bw", g_bw_khz);
  g_sf = prefs.getUChar("sf", g_sf);
  g_cr = prefs.getUChar("cr", g_cr);
  g_txp = prefs.getChar("txp", g_txp);
  g_rx_boost = prefs.getBool("rxboost", false);
  g_ack_on = prefs.getBool("ack", cfg::ACK_ENABLED_DEFAULT);
  g_retryN = prefs.getUChar("rtryN", cfg::SEND_RETRY_COUNT_DEFAULT);
  g_retryMS = prefs.getUShort("rtryMS", cfg::SEND_RETRY_MS_DEFAULT);
  g_enc_on = prefs.getBool("enc", cfg::ENCRYPTION_ENABLED_DEFAULT);
  g_kid = prefs.getUChar("kid", g_kid);
  // Determine whether the current key was marked as remote or local.  The
  // boolean key_remote is stored in NVS; false means the key was generated
  // locally and true means it was received from another device.  If absent
  // default to false (Local).
  bool remote = prefs.getBool("key_remote", false);
  g_key_status = remote ? KeyStatus::Received : KeyStatus::Local;
  uint32_t nid = prefs.getULong("msgid", 1);
  prefs.end();
  g_buf.setNextId(nid);

  // Выбираем список частот и применяем остальные параметры
  g_freq_table = &GetFreqTable(g_bank);
  Radio_setBandwidth((uint32_t)g_bw_khz);
  Radio_setSpreadingFactor(g_sf);
  Radio_setCodingRate(g_cr);
  Radio_setTxPower(g_txp);
  Radio_setRxBoost(g_rx_boost);
  g_tx.enableAck(g_ack_on);
  g_tx.setRetry(g_retryN, g_retryMS);
  loadKeyFromNVS();
  g_ccm.setEnabled(g_enc_on);
  g_ccm.setActiveKid(g_kid);
  g_tx.setEncEnabled(g_enc_on);
  // Применяем выбранный пресет и переходим на приём
  applyPreset();
  Serial.println(F("Loaded."));
}

static void resetConfig() {
  prefs.begin("lora", false);
  prefs.clear();
  prefs.end();
  Serial.println(F("NVS cleared."));
}

static void printRadio() {
  Serial.printf("FRX=%.3f MHz FTX=%.3f MHz BW=%.1f kHz SF=%u CR=4/%u TXP=%d dBm ENC=%s KID=%u\n",
                g_freq_rx_mhz, g_freq_tx_mhz, g_bw_khz, g_sf, g_cr, g_txp, g_enc_on ? "ON" : "OFF", g_kid);
}

// -----------------------------------------------------------------------------
// HTTP handler implementations.  These functions implement the web control
// interface.  See sat_radv0.01a.ino for inspiration: a dark-themed chat
// window with controls for all LoRa parameters.  The HTML returned by
// handleRoot() contains JavaScript that periodically polls /serial for new
// messages and sends changes back to the server when the user interacts
// with controls.

void handleRoot() {
  // Serve the web control panel defined in a separate header file.
  String page = FPSTR(WEB_INTERFACE_HTML);
  page.replace("%ACK%", g_ack_on ? "checked" : "");
  page.replace("%ENC%", g_enc_on ? "checked" : "");
  page.replace("%DUP%", g_dup_on ? "checked" : "");
  page.replace("%RXBOOST%", g_rx_boost ? "checked" : "");
  server.send(200, "text/html", page);
}

// Handler that enqueues a message into the transmit buffer.  The message is
// appended to the UI buffer and a new message ID persisted.
void handleSend() {
  // Проверяем rate-limit: не чаще одного запроса в 200 мс
  if (!rateLimit(g_last_send_ms, 200)) {
    server.send(429, "text/plain", "rate");
    return;
  }
  if (!server.hasArg("msg")) {
    server.send(400, "text/plain", "msg missing");
    return;
  }
  String message = server.arg("msg");
  message.trim();
  if (message.length() == 0) {
    server.send(400, "text/plain", "empty");
    return;
  }
  // Convert to byte vector
  size_t n = (size_t)message.length();
  std::vector<uint8_t> bytes(n);
  memcpy(bytes.data(), message.c_str(), n);
  g_buf.enqueue(bytes.data(), bytes.size(), false);
  persistMsgId();
  // Отправляем строку в чат и сохраняем её в буфер
  chatMsg("TX", message);
  server.send(200, "text/plain", "ok");
}

// Handler that returns accumulated serial/output messages since last read.
void handleSerial() {
  server.send(200, "text/plain", serialBuffer);
  serialBuffer = "";
}

// Отдаёт минифицированный CSS
void handleStyle() {
  server.send_P(200, "text/css", WEB_STYLE_CSS);
}

// Отдаёт минифицированный JS
void handleScript() {
  server.send_P(200, "application/javascript", WEB_SCRIPT_JS);
}

// Обработчик событий WebSocket
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  (void)payload; (void)len;
  if (type == WStype_CONNECTED) {
    // При подключении клиента отправляем накопленный буфер,
    // чтобы сразу показать текущее состояние RX/TX
    if (serialBuffer.length()) {
      wsServer.sendTXT(num, serialBuffer.c_str());
    }
  }
}

void handleSetBank() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int b = server.arg("val").toInt();
  if (b < 0 || b > 2) {
    server.send(400, "text/plain", "range 0..2");
    return;
  }
  g_bank = static_cast<Bank>(b);
  g_freq_table = &GetFreqTable(g_bank);
  applyPreset();
  server.send(200, "text/plain", "bank changed");
  // Логируем изменение банка в чат
  chatMsg("SYS", String("Bank set to ") + String(b));
}

void handleSetPreset() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int p = server.arg("val").toInt();
  if (p < 0 || p > 9) {
    server.send(400, "text/plain", "range 0..9");
    return;
  }
  g_preset = p;
  applyPreset();
  server.send(200, "text/plain", "preset changed");
  // Логируем изменение пресета с указанием частот RX/TX
  chatMsg("SYS", String("Preset set to ") + String(p) +
                 String(" (RX ") + String(g_freq_rx_mhz,3) +
                 String(" MHz TX ") + String(g_freq_tx_mhz,3) + " MHz)");
}

void handleSetBw() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  float khz = server.arg("val").toFloat();
  if (khz < 7.8f || khz > 500.0f) {
    server.send(400, "text/plain", "range 7.8..500");
    return;
  }
  g_bw_khz = khz;
  Radio_setBandwidth((uint32_t)g_bw_khz);
  server.send(200, "text/plain", "bw changed");
  // Сообщаем новый BW в чат
  chatMsg("SYS", String("BW set to ") + String(khz) + " kHz");
}

void handleSetSf() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int sf = server.arg("val").toInt();
  if (sf < 7 || sf > 12) {
    server.send(400, "text/plain", "range 7..12");
    return;
  }
  g_sf = (uint8_t)sf;
  Radio_setSpreadingFactor(g_sf);
  server.send(200, "text/plain", "sf changed");
  // Сообщаем новый SF в чат
  chatMsg("SYS", String("SF set to ") + String(sf));
}

void handleSetCr() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int cr = server.arg("val").toInt();
  if (cr < 5 || cr > 8) {
    server.send(400, "text/plain", "range 5..8");
    return;
  }
  g_cr = (uint8_t)cr;
  Radio_setCodingRate(g_cr);
  server.send(200, "text/plain", "cr changed");
  // Сообщаем новый CR в чат
  chatMsg("SYS", String("CR set to 4/") + String(cr));
}

void handleSetTxp() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int8_t txp = (int8_t)server.arg("val").toInt();
  if (txp < -9 || txp > 22) {
    server.send(400, "text/plain", "range -9..22");
    return;
  }
  g_txp = txp;
  Radio_setTxPower(g_txp);
  server.send(200, "text/plain", "txp changed");
  // Сообщаем новый уровень мощности в чат
  chatMsg("SYS", String("TXP set to ") + String(txp) + " dBm");
}

// Включение/выключение усиленного приёма
void handleSetRxBoost() {
  bool on = server.arg("val") == "1";
  g_rx_boost = on;
  Radio_setRxBoost(g_rx_boost);
  server.send(200, "text/plain", g_rx_boost ? "rxboost on" : "rxboost off");
  // Выводим статус усиленного приёма в чат
  chatMsg("SYS", String("RXBOOST=") + (g_rx_boost ? "ON" : "OFF"));
}

// Установка режима FEC: off/rs_vit/ldpc
void handleSetFec() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  String v = server.arg("val");
  TxPipeline::FecMode m;
  if (v == "off") m = TxPipeline::FEC_OFF;
  else if (v == "rs_vit") m = TxPipeline::FEC_RS_VIT;
  else if (v == "ldpc") m = TxPipeline::FEC_LDPC;
  else {
    server.send(400, "text/plain", "off|rs_vit|ldpc");
    return;
  }
  g_tx.setFecMode(m);
  g_rx.setFecMode((RxPipeline::FecMode)m);
  server.send(200, "text/plain", "fec changed");
  // Логируем выбранный режим FEC
  chatMsg("SYS", String("FEC=") + v);
}

// Установка глубины интерливера: 1/4/8/16 байт
void handleSetInter() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int d = server.arg("val").toInt();
  if (!(d == 1 || d == 4 || d == 8 || d == 16)) {
    server.send(400, "text/plain", "1|4|8|16");
    return;
  }
  g_tx.setInterleaveDepth((uint8_t)d);
  g_rx.setInterleaveDepth((uint8_t)d);
  server.send(200, "text/plain", "inter changed");
  // Логируем глубину интерливера
  chatMsg("SYS", String("INTER=") + String(d));
}

// Установка размера полезной нагрузки
void handleSetPayload() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int p = server.arg("val").toInt();
  int maxp = cfg::LORA_MTU - FRAME_HEADER_SIZE;
  if (p < 1 || p > maxp) {
    server.send(400, "text/plain", "range 1.." + String(maxp));
    return;
  }
  g_tx.setPayloadLen((uint16_t)p);
  server.send(200, "text/plain", "payload changed");
  // Сообщаем размер полезной нагрузки в чат
  chatMsg("SYS", String("PAYLOAD=") + String(p));
}

// Установка интервала пилотных вставок (0=выкл)
void handleSetPilot() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int iv = server.arg("val").toInt();
  if (iv < 0 || iv > 512) {
    server.send(400, "text/plain", "range 0..512");
    return;
  }
  g_tx.setPilotInterval((uint16_t)iv);
  g_rx.setPilotInterval((uint16_t)iv);
  server.send(200, "text/plain", "pilot changed");
  // Сообщаем интервал пилотов в чат
  chatMsg("SYS", String("PILOT=") + String(iv));
}

void handleSetDup() {
  bool on = server.arg("val") == "1";
  g_dup_on = on;
  g_tx.setHeaderDup(on);
  g_rx.setHeaderDup(on);
  server.send(200, "text/plain", on ? "dup on" : "dup off");
  // Логируем переключение дублирования заголовка
  chatMsg("SYS", String("HDRDUP=") + (on ? "ON" : "OFF"));
}

void handleSetWin() {
  int w = server.arg("val").toInt();
  if (w < 1 || w > 32) { server.send(400, "text/plain", "range 1..32"); return; }
  g_window = (uint8_t)w;
  g_tx.setWindowSize(g_window);
  g_rx.setWindowSize(g_window);
  server.send(200, "text/plain", "win set");
  // Сообщаем размер окна ARQ
  chatMsg("SYS", String("WIN=") + String(w));
}

void handleSetAckAgg() {
  int ms = server.arg("val").toInt();
  if (ms < 0 || ms > 1000) { server.send(400, "text/plain", "range 0..1000"); return; }
  g_ackAgg = (uint16_t)ms;
  g_rx.setAckAgg(g_ackAgg);
  server.send(200, "text/plain", "ackagg set");
  // Логируем интервал агрегации ACK
  chatMsg("SYS", String("ACKAGG=") + String(ms));
}

// Установка предела фрагментов в серии перед ожиданием ACK
void handleSetBurst() {
  int n = server.arg("val").toInt();
  if (n < 1 || n > 32) { server.send(400, "text/plain", "range 1..32"); return; }
  g_burst = (uint8_t)n;
  g_tx.setBurstFrags(g_burst);
  server.send(200, "text/plain", "burst set");
  // Логируем предел серии фрагментов
  chatMsg("SYS", String("BURST=") + String(n));
}

// Обработчик явного включения/выключения ACK
void handleSetAck() {
  bool on = server.arg("val") == "1";
  g_ack_on = on;
  g_tx.enableAck(g_ack_on);
  server.send(200, "text/plain", g_ack_on ? "ack on" : "ack off");
  // Логируем состояние ACK
  chatMsg("SYS", String("ACK=") + (g_ack_on ? "ON" : "OFF"));
}

// Установка процента джиттера ожидания ACK
void handleSetAckJitter() {
  if (!server.hasArg("val")) { server.send(400, "text/plain", "val missing"); return; }
  int pct = server.arg("val").toInt();
  if (pct < 0 || pct > 100) { server.send(400, "text/plain", "range 0..100"); return; }
  g_ackJitter = (uint8_t)pct;
  server.send(200, "text/plain", "ackjitter set");
  // Логируем величину джиттера ожидания ACK
  chatMsg("SYS", String("ACKJITTER=") + String(pct) + "%");
}

// Задание списка коэффициентов backoff через запятую
void handleSetBackoff() {
  if (!server.hasArg("val")) { server.send(400, "text/plain", "val missing"); return; }
  String v = server.arg("val");
  g_backoff.clear();
  int start = 0;
  while (start < v.length()) {
    int comma = v.indexOf(',', start);
    String token = (comma == -1) ? v.substring(start) : v.substring(start, comma);
    float c = token.toFloat();
    if (c > 0) g_backoff.push_back(c);
    if (comma == -1) break;
    start = comma + 1;
  }
  server.send(200, "text/plain", "backoff set");
  // Логируем коэффициенты бэкоффа
  chatMsg("SYS", String("BACKOFF=") + v);
}

// Переключение режима автонастройки профиля
void handleSetAutorate() {
  bool on = server.arg("val") == "1";
  g_autoRate = on;
  server.send(200, "text/plain", on ? "autorate on" : "autorate off");
  // Логируем режим автонастройки
  chatMsg("SYS", String("AUTORATE=") + (on ? "ON" : "OFF"));
}

// Ручной выбор профиля передачи P0..P3
void handleSetTxProfile() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int p = server.arg("val").toInt();
  if (p < 0 || p > 3) {
    server.send(400, "text/plain", "range 0..3");
    return;
  }
  g_tx.setProfile((uint8_t)p);
  server.send(200, "text/plain", "txprofile set");
  // Сообщаем выбранный профиль в чат
  chatMsg("SYS", String("TX profile P") + String(p));
}

// Установка порогов PER (hi/lo)
void handleSetPerTh() {
  if (!server.hasArg("hi") || !server.hasArg("lo")) { server.send(400, "text/plain", "hi/lo missing"); return; }
  g_perHigh = server.arg("hi").toFloat();
  g_perLow  = server.arg("lo").toFloat();
  server.send(200, "text/plain", "perth set");
  // Логируем пороги PER
  chatMsg("SYS", String("PERTH=") + String(g_perHigh,2) + "," + String(g_perLow,2));
}

// Установка порогов Eb/N0 (hi/lo)
void handleSetEbn0Th() {
  if (!server.hasArg("hi") || !server.hasArg("lo")) { server.send(400, "text/plain", "hi/lo missing"); return; }
  g_ebn0High = server.arg("hi").toFloat();
  g_ebn0Low  = server.arg("lo").toFloat();
  server.send(200, "text/plain", "ebn0th set");
  // Логируем пороги Eb/N0
  chatMsg("SYS", String("EBN0TH=") + String(g_ebn0High,1) + "," + String(g_ebn0Low,1));
}

void handleToggleEnc() {
  g_enc_on = !g_enc_on;
  g_ccm.setEnabled(g_enc_on);
  g_tx.setEncEnabled(g_enc_on);
  server.send(200, "text/plain", g_enc_on ? "enc on" : "enc off");
  // Логируем изменение состояния шифрования
  chatMsg("SYS", String("ENC=") + (g_enc_on ? "ON" : "OFF"));
}

void handleMetrics() {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "TX frames=%u bytes=%u msgs=%u retries=%u ack_fail=%u ack_seen=%u ack_avg=%ums enc_fail=%u\n"
           "RX frames_ok=%u bytes=%u msgs_ok=%u frags=%u dup=%u crc_fail=%u drop_len=%u drop_ttl=%u drop_over=%u dec_fail_tag=%u dec_fail_oth=%u",
           g_metrics.tx_frames, g_metrics.tx_bytes, g_metrics.tx_msgs,
           g_metrics.tx_retries, g_metrics.ack_fail, g_metrics.ack_seen, g_metrics.ack_time_ms_avg,
           g_metrics.enc_fail,
           g_metrics.rx_frames_ok, g_metrics.rx_bytes, g_metrics.rx_msgs_ok, g_metrics.rx_frags,
           g_metrics.rx_dup_msgs, g_metrics.rx_crc_fail, g_metrics.rx_drop_len_mismatch,
           g_metrics.rx_assem_drop_ttl, g_metrics.rx_assem_drop_overflow, g_metrics.dec_fail_tag, g_metrics.dec_fail_other);
  server.send(200, "text/plain", buf);
  // Логируем запрос метрик
  chatMsg("SYS", "Metrics requested");
}

// Обработчик /linkdiag: отдаёт короткие метрики канала в JSON
void handleLinkDiag() {
  String json = "{";
  json += "\"per\":" + String(g_metrics.per_window.avg(), 3);
  json += ",\"rtt_p50\":" + String(g_metrics.rtt_window_ms.avg(), 2);
  json += ",\"rtt_p95\":" + String(g_metrics.rtt_window_ms.avg(), 2);
  json += ",\"goodput\":" + String(g_metrics.goodput_window.avg(), 2);
  json += ",\"profile\":\"" + String((int)g_bank) + ":" + String(g_preset) + "\"";
  json += ",\"bitmap\":\"\"";
  json += ",\"tx_frames\":" + String(g_metrics.tx_frames);
  json += ",\"rx_frames\":" + String(g_metrics.rx_frames_ok);
  json += ",\"queue\":" + String(g_buf.size());
  json += "}"; 
  server.send(200, "application/json", json);
}

// Обработчик расширенного пинга SatPingRun
void handleSatPingAdv() {
  PingOptions opts; // значения по умолчанию уже заданы
  if (server.hasArg("count")) opts.count = server.arg("count").toInt();
  if (server.hasArg("interval")) opts.interval_ms = server.arg("interval").toInt();
  if (server.hasArg("timeout")) opts.timeout_ms = server.arg("timeout").toInt();
  if (server.hasArg("size")) opts.frag_size = server.arg("size").toInt();
  if (server.hasArg("duration")) opts.duration_min = server.arg("duration").toInt();
  if (server.hasArg("fec")) {
    String f = server.arg("fec");
    if (f == "rs_vit") opts.fec_mode = PingFecMode::FEC_RS_VIT;
    else if (f == "ldpc") opts.fec_mode = PingFecMode::FEC_LDPC;
    else if (f == "rep2") opts.fec_mode = PingFecMode::FEC_REPEAT2;
  }
  if (server.hasArg("retries")) opts.retries = server.arg("retries").toInt();
  PingStats st; // сюда соберём статистику
  SatPingRun(opts, st);
  String json = String("{\"sent\":") + st.sent +
                ",\"received\":" + st.received +
                ",\"crc_fail\":" + st.crc_fail +
                ",\"fec_fail\":" + st.fec_fail +
                ",\"no_ack\":" + st.no_ack +
                ",\"timeout\":" + st.timeout +
                ",\"retrans\":" + st.retransmits +
                ",\"fec_mode\":" + (int)st.fec_mode +
                ",\"frag_size\":" + st.frag_size + "}";
  server.send(200, "application/json", json);
}

void handlePing() {
  // Start an asynchronous ping. Проверяем rate-limit: не чаще одного запроса в секунду
  if (!rateLimit(g_last_ping_ms, 1000)) {
    server.send(429, "text/plain", "rate");
    return;
  }
  // Если пинг или другая операция уже выполняются, сообщаем о занятости.
  if (g_ping_active || g_radio_busy) {
    server.send(429, "text/plain", "busy");
    return;
  }
  // Set radio busy flag to prevent other transmissions during ping
  g_radio_busy = true;
  g_ping_active = true;
  g_ping_stage = 0;
  // Clear any pending receive flag so we start fresh
  g_hasRx = false;
  // Announce start to chat and respond to HTTP client
  chatMsg("SYS", "Ping started");
  server.send(200, "text/plain", "ping started");
}

// Синхронная проверка канала на текущем пресете
void handleChannelPing() {
  if (g_ping_active || g_radio_busy) {
    server.send(429, "text/plain", "busy");
    return;
  }
  g_radio_busy = true;
  bool ok = ChannelPing();
  g_radio_busy = false;
  chatMsg("SYS", ok ? "ChannelPing OK" : "ChannelPing FAIL");
  server.send(200, "text/plain", ok ? "ok" : "fail");
}

// Пинг выбранного пресета из указанного банка
void handlePresetPing() {
  if (!server.hasArg("bank") || !server.hasArg("preset")) {
    server.send(400, "text/plain", "args missing");
    return;
  }
  int b = server.arg("bank").toInt();
  int p = server.arg("preset").toInt();
  if (p < 0 || p > 9) {
    server.send(400, "text/plain", "preset 0-9");
    return;
  }
  Bank bank = Bank::MAIN;
  if (b == 1) bank = Bank::RESERVE;
  else if (b == 2) bank = Bank::TEST;
  if (g_ping_active || g_radio_busy) {
    server.send(429, "text/plain", "busy");
    return;
  }
  g_radio_busy = true;
  bool ok = PresetPing(bank, p);
  g_radio_busy = false;
  chatMsg("SYS", ok ? "PresetPing OK" : "PresetPing FAIL");
  server.send(200, "text/plain", ok ? "ok" : "fail");
}

// Обход всех пресетов выбранного банка
void handleMassPing() {
  if (!server.hasArg("bank")) {
    server.send(400, "text/plain", "bank missing");
    return;
  }
  int b = server.arg("bank").toInt();
  Bank bank = Bank::MAIN;
  if (b == 1) bank = Bank::RESERVE;
  else if (b == 2) bank = Bank::TEST;
  if (g_ping_active || g_radio_busy) {
    server.send(429, "text/plain", "busy");
    return;
  }
  g_radio_busy = true;
  MassPing(bank);
  g_radio_busy = false;
  chatMsg("SYS", "MassPing done");
  server.send(200, "text/plain", "done");
}

// Set retry count for automatic retransmissions.  Range: 0..10
void handleSetRetryN() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int n = server.arg("val").toInt();
  if (n < 0 || n > 10) {
    server.send(400, "text/plain", "range 0..10");
    return;
  }
  g_retryN = (uint8_t)n;
  g_tx.setRetry(g_retryN, g_retryMS);
  server.send(200, "text/plain", "retryn changed");
  // Логируем число повторов
  chatMsg("SYS", String("RETRYN set to ") + String(n));
}

// Set retry timeout (ms) for automatic retransmissions.  Range: 100..10000
void handleSetRetryMS() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int ms = server.arg("val").toInt();
  if (ms < 100 || ms > 10000) {
    server.send(400, "text/plain", "range 100..10000");
    return;
  }
  g_retryMS = (uint16_t)ms;
  g_tx.setRetry(g_retryN, g_retryMS);
  server.send(200, "text/plain", "retryms changed");
  // Логируем таймаут повторов
  chatMsg("SYS", String("RETRYMS set to ") + String(ms) + " ms");
}

// Set encryption key identifier (KID)
void handleSetKid() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  int kid = server.arg("val").toInt();
  if (kid < 0 || kid > 255) {
    server.send(400, "text/plain", "range 0..255");
    return;
  }
  g_kid = (uint8_t)kid;
  g_ccm.setActiveKid(g_kid);
  loadKeyFromNVS();
  server.send(200, "text/plain", "kid changed");
  // Логируем выбранный KID
  chatMsg("SYS", String("KID set to ") + String(kid));
}

// Set encryption key (32 hex characters).  Associates the key with the current KID.
void handleSetKey() {
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "val missing");
    return;
  }
  String hex = server.arg("val");
  hex.trim();
  if (hex.length() != 32) {
    server.send(400, "text/plain", "key expects 32 hex chars");
    return;
  }
  uint8_t key[16];
  for (int i = 0; i < 16; i++) {
    char c1 = hex[2 * i];
    char c2 = hex[2 * i + 1];
    auto toN = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      c = (char)std::toupper((unsigned char)c);
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    int n1 = toN(c1), n2 = toN(c2);
    if (n1 < 0 || n2 < 0) {
      server.send(400, "text/plain", "bad hex");
      return;
    }
    key[i] = (uint8_t)((n1 << 4) | n2);
  }
  char name[8];
  snprintf(name, sizeof(name), "k%03u", (unsigned)g_kid);
  prefs.begin("lora", false);
  prefs.putBytes(name, key, 16);
  prefs.end();
  bool ok = g_ccm.setKey(g_kid, key, 16);
  server.send(200, "text/plain", ok ? "key loaded" : "key failed");
  // Сообщаем результат загрузки ключа
  if (ok) chatMsg("SYS", String("Key loaded for KID ") + String(g_kid));
  else    chatMsg("SYS", "Failed to load key");
}

// Save configuration to NVS
void handleSave() {
  saveConfig();
  server.send(200, "text/plain", "config saved");
  // Отражаем сохранение конфигурации
  chatMsg("SYS", "Config saved");
}

// Load configuration from NVS
void handleLoad() {
  loadConfig();
  server.send(200, "text/plain", "config loaded");
  // Отражаем загрузку конфигурации
  chatMsg("SYS", "Config loaded");
}

// Reset (clear) configuration in NVS
void handleReset() {
  resetConfig();
  server.send(200, "text/plain", "config reset");
  // Отражаем сброс конфигурации
  chatMsg("SYS", "Config reset");
}

// Установка параметров TDD-планировщика
void handleSetTdd() {
  if (!server.hasArg("tx") || !server.hasArg("ack") || !server.hasArg("guard")) {
    server.send(400, "text/plain", "tx|ack|guard missing");
    return;
  }
  unsigned long tx = server.arg("tx").toInt();
  unsigned long ack = server.arg("ack").toInt();
  unsigned long g = server.arg("guard").toInt();
  tdd::setParams(tx, ack, g);
  server.send(200, "text/plain", "tdd set");
  // Логируем параметры TDD
  chatMsg("SYS", String("TDD tx=") + String(tx) + ", ack=" + String(ack) + ", g=" + String(g));
}

// Список сообщений в архиве
void handleArchiveList() {
  std::vector<uint32_t> ids;
  g_buf.listArchived(ids);
  String out;
  for (uint32_t id : ids) {
    out += String(id) + "\n";
  }
  server.send(200, "text/plain", out);
}

// Восстановить одно сообщение из архива
void handleArchiveRestore() {
  bool ok = g_buf.restoreArchived();
  server.send(200, "text/plain", ok ? "restored" : "empty");
  // Логируем результат восстановления архива
  chatMsg("SYS", ok ? String("archive restored") : String("archive empty"));
}

// Сброс счётчика сообщений
void handleIdReset() {
  g_buf.setNextId(1);
  persistMsgId();
  server.send(200, "text/plain", "msgid reset");
  // Логируем сброс счётчика сообщений
  chatMsg("SYS", "msg_id set to 1");
}

// Очистка окна anti-replay (заглушка)
void handleReplayClr() {
  server.send(200, "text/plain", "replay cleared");
  // Логируем очистку окна повторов
  chatMsg("SYS", "replay window cleared");
}

// Возврат последних кадров в JSON
void handleFrames() {
  int drop = -1;
  if (server.hasArg("drop")) drop = server.arg("drop").toInt();
  String s = FrameLog::json(20, drop);
  server.send(200, "application/json", s);
}

// -----------------------------------------------------------------------------
// Web handlers for command endpoints.  These endpoints expose the
// SIMPLE, LARGE, ENCTEST, ENCTESTBAD and MSGID commands to the web UI.
// Each handler responds immediately and adds a system message to the chat.

void handleSimple() {
  // Enqueue a simple test message ("ping").
  const char* msg = "ping";
  g_buf.enqueue(reinterpret_cast<const uint8_t*>(msg), strlen(msg), false);
  persistMsgId();
  server.send(200, "text/plain", "simple queued");
  // Сообщаем о постановке тестового сообщения
  chatMsg("SYS", "SIMPLE queued");
}

void handleLarge() {
  // Enqueue a large test message of the requested size.  Default to 1200 bytes.
  int n = 1200;
  if (server.hasArg("size")) {
    int val = server.arg("size").toInt();
    if (val > 0 && val <= 65535) n = val;
  }
  std::vector<uint8_t> big(n);
  for (int i = 0; i < n; i++) {
    big[i] = (uint8_t)(i & 0xFF);
  }
  g_buf.enqueue(big.data(), big.size(), false);
  persistMsgId();
  server.send(200, "text/plain", "large queued");
  // Логируем постановку большого сообщения
  chatMsg("SYS", String("LARGE ") + String(n) + String(" bytes queued"));
}

void handleEncTest() {
  // Run encryption self‑test.  If a size parameter is provided, run that
  // many bytes; otherwise run a comprehensive battery of tests.  Results
  // are printed to the serial console; the chat indicates the test has
  // started.
  size_t n = 0;
  if (server.hasArg("size")) {
    int val = server.arg("size").toInt();
    if (val > 0) {
      n = (size_t)val;
    }
  }
  // Используем вывод и в UART, и в окно чата
  SerialChatPrint out(Serial);
  if (n > 0) {
    EncSelfTest_run(g_ccm, n, out);
  } else {
    EncSelfTest_battery(g_ccm, out);
  }
  server.send(200, "text/plain", "enctest started");
  // Логируем запуск теста шифрования
  chatMsg("SYS", "ENCTEST started");
}

void handleEncTestBad() {
  // Run an encryption test with an invalid KID to verify failure handling.
  // Сообщаем результат теста также в чат
  SerialChatPrint out(Serial);
  EncSelfTest_badKid(g_ccm, out);
  server.send(200, "text/plain", "enctestbad started");
  // Логируем запуск негативного теста шифрования
  chatMsg("SYS", "ENCTESTBAD started");
}

// Выполняем полный самотест и передаём статус в чат
void handleSelfTest() {
  // Логируем запуск самотеста
  chatMsg("SYS", "SELFTEST started");
  SerialChatPrint out(Serial);
  SelfTest_runAll(g_ccm, out);
  chatMsg("SYS", "SELFTEST finished");
  server.send(200, "text/plain", "selftest done");
}

void handleSetMsgId() {
  // Set or query the next message ID.  If val is provided it becomes the
  // next ID (minimum 1).  The current next ID is returned in the body.
  if (server.hasArg("val")) {
    uint32_t id = (uint32_t)strtoul(server.arg("val").c_str(), nullptr, 10);
    if (id == 0) {
      id = 1;
    }
    g_buf.setNextId(id);
    persistMsgId();
    // Логируем установку идентификатора
    chatMsg("SYS", String("MSGID set to ") + String(id));
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)g_buf.nextId());
  server.send(200, "text/plain", buf);
}

// -----------------------------------------------------------------------------
// Key exchange helpers and HTTP handlers
//
// sendKeyRequest() enqueues a "KEYREQ" message and sets the request flag.  It
// also emits a system message to the chat and persists the message ID.
void sendKeyRequest() {
  const char* req = "KEYREQ";
  g_buf.enqueue(reinterpret_cast<const uint8_t*>(req), strlen(req), true);
  persistMsgId();
  g_key_request_active = true;
  chatLine(String("*SYS:* KEYREQ queued"));
}

// sendKeyResponse() constructs a "KEYRES <kid> <hexkey>" message using the
// current key and enqueues it.  After queuing the response the pending
// request flag is cleared.  A system message is appended to the chat.
void sendKeyResponse() {
  // Build key response string.  Format: KEYRES <kid> <32‑hex>.
  char buf[64];
  // Берём AES-ключ из общего хранилища
  const uint8_t* k = crypto_spec::CURRENT_KEY;
  char hexstr[33];
  for (int i = 0; i < 16; i++) {
    static const char* hexdig = "0123456789ABCDEF";
    hexstr[2*i]   = hexdig[(k[i] >> 4) & 0xF];
    hexstr[2*i+1] = hexdig[k[i] & 0xF];
  }
  hexstr[32] = '\0';
  snprintf(buf, sizeof(buf), "KEYRES %u %s", (unsigned)g_kid, hexstr);
  g_buf.enqueue(reinterpret_cast<const uint8_t*>(buf), strlen(buf), true);
  persistMsgId();
  g_key_request_active = false;
  chatLine(String("*SYS:* KEYRES queued"));
}

// Handle /keyreq: send a key request to the peer.
void handleKeyReq() {
  sendKeyRequest();
  server.send(200, "text/plain", "keyreq queued");
}

// Handle /keysend: send our key in response to a request.  Useful to
// manually push a key to another device.
void handleKeySend() {
  sendKeyResponse();
  server.send(200, "text/plain", "keysend queued");
}

// Обработчик /keystatus: возвращает JSON со статусом ключа и флагом запроса.
// Дополнительно включает 4‑значный хеш активного AES‑ключа в формате HEX.
void handleKeyStatus() {
  String st = (g_key_status == KeyStatus::Received) ? "remote" : "local";
  // Хеш берём из заранее вычисленного CRC-16; если ключ не задан, выводим прочерки
  char hbuf[5];
  if (crypto_spec::CURRENT_KEY_CRC == 0) {
    strcpy(hbuf, "----");
  } else {
    snprintf(hbuf, sizeof(hbuf), "%04X", (unsigned)crypto_spec::CURRENT_KEY_CRC & 0xFFFF);
  }
  String json = String("{\"status\":\"") + st + String("\",\"request\":") + (g_key_request_active ? "1" : "0") + String("\",\"hash\":\"") + hbuf + String("\"}");
  server.send(200, "application/json", json);
}

// Обработчик /keyhash: отдаёт 4‑значный хеш активного AES‑ключа или сообщает об его отсутствии.
void handleKeyHash() {
  try {
    if (crypto_spec::CURRENT_KEY_CRC == 0) {
      // Ключ не задан — возвращаем 404 и текстовое пояснение
      server.send(404, "text/plain", "no-key");
      return;
    }
    char hbuf[5];
    snprintf(hbuf, sizeof(hbuf), "%04X", (unsigned)crypto_spec::CURRENT_KEY_CRC & 0xFFFF);
    server.send(200, "text/plain", hbuf);
  } catch (...) {
    // Любая неожиданная ошибка — лаконичный ответ 500
    server.send(500, "text/plain", "error");
  }
}

// ---------------------------------------------------------------------------
// ECDH key exchange helpers and HTTP handler.

// Initiate an authenticated ECDH handshake.  This function generates a
// Curve25519 key pair, computes an HMAC over the public key using a
// pre‑shared root key and enqueues a KEYDH1 message with ACK.  It sets
// g_keydh_in_progress, g_keydh_initiator and g_key_request_active so the
// UI indicator blinks while waiting for the response.
void startKeyDh() {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  if (g_keydh_in_progress) {
    chatLine(String("*SYS:* KEYDH already in progress"));
    return;
  }
  mbedtls_ctr_drbg_init(&g_keydh_ctr_drbg);
  mbedtls_entropy_init(&g_keydh_entropy);
  const char* pers = "keydh";
  int ret = mbedtls_ctr_drbg_seed(&g_keydh_ctr_drbg, mbedtls_entropy_func,
                                  &g_keydh_entropy,
                                  (const unsigned char*)pers, strlen(pers));
  if (ret != 0) {
    chatLine(String("*SYS:* KEYDH seed error"));
    mbedtls_ctr_drbg_free(&g_keydh_ctr_drbg);
    mbedtls_entropy_free(&g_keydh_entropy);
    return;
  }
  mbedtls_ecdh_init(&g_keydh_ctx);
  ret = mbedtls_ecdh_setup(&g_keydh_ctx, MBEDTLS_ECP_DP_CURVE25519);
  if (ret != 0) {
    chatLine(String("*SYS:* KEYDH setup error"));
    mbedtls_ecdh_free(&g_keydh_ctx);
    mbedtls_ctr_drbg_free(&g_keydh_ctr_drbg);
    mbedtls_entropy_free(&g_keydh_entropy);
    return;
  }
  g_keydh_pub_len = 0;
  ret = mbedtls_ecdh_make_public(&g_keydh_ctx, &g_keydh_pub_len, g_keydh_pub,
                                 sizeof(g_keydh_pub),
                                 mbedtls_ctr_drbg_random, &g_keydh_ctr_drbg);
  if (ret != 0 || g_keydh_pub_len == 0) {
    chatLine(String("*SYS:* KEYDH public key error"));
    mbedtls_ecdh_free(&g_keydh_ctx);
    mbedtls_ctr_drbg_free(&g_keydh_ctr_drbg);
    mbedtls_entropy_free(&g_keydh_entropy);
    return;
  }
  unsigned char hmac[32];
  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  ret = mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  if (ret == 0) {
    ret = mbedtls_md_hmac_starts(&md, crypto_spec::KEYDH_ROOT_KEY, sizeof(crypto_spec::KEYDH_ROOT_KEY));
    if (ret == 0) ret = mbedtls_md_hmac_update(&md, g_keydh_pub, g_keydh_pub_len);
    if (ret == 0) ret = mbedtls_md_hmac_finish(&md, hmac);
  }
  mbedtls_md_free(&md);
  if (ret != 0) {
    chatLine(String("*SYS:* KEYDH HMAC error"));
    mbedtls_ecdh_free(&g_keydh_ctx);
    mbedtls_ctr_drbg_free(&g_keydh_ctr_drbg);
    mbedtls_entropy_free(&g_keydh_entropy);
    return;
  }
  String msg = "KEYDH1 ";
  static const char* hexdig = "0123456789ABCDEF";
  for (size_t i = 0; i < g_keydh_pub_len; i++) {
    msg += hexdig[(g_keydh_pub[i] >> 4) & 0xF];
    msg += hexdig[g_keydh_pub[i] & 0xF];
  }
  msg += " ";
  for (int i = 0; i < 8; i++) {
    msg += hexdig[(hmac[i] >> 4) & 0xF];
    msg += hexdig[hmac[i] & 0xF];
  }
  g_buf.enqueue(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length(), true);
  persistMsgId();
  g_keydh_in_progress = true;
  g_keydh_initiator = true;
  g_key_request_active = true;
  chatLine(String("*SYS:* KEYDH1 queued"));
#else
  chatLine(String("*SYS:* KEYDH not supported on host build"));
#endif
}

// Process a KEYDH1 or KEYDH2 message.  Returns true if handled.
bool processKeyDh(const String& m) {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  if (m.startsWith("KEYDH1")) {
    int p1 = m.indexOf(' ');
    int p2 = (p1 > 0) ? m.indexOf(' ', p1 + 1) : -1;
    if (p1 < 0 || p2 < 0) {
      chatLine(String("*SYS:* KEYDH1 malformed"));
      return true;
    }
    String pubStr = m.substring(p1 + 1, p2);
    String macStr = m.substring(p2 + 1);
    pubStr.trim(); macStr.trim();
    if ((pubStr.length() % 2) != 0 || macStr.length() < 16) {
      chatLine(String("*SYS:* KEYDH1 invalid lengths"));
      return true;
    }
    size_t remotePubLen = pubStr.length() / 2;
    if (remotePubLen > 65) remotePubLen = 65;
    unsigned char remotePub[65];
    auto toN = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      c = toupper(c);
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    bool okParse = true;
    for (size_t i = 0; i < remotePubLen; i++) {
      int n1 = toN(pubStr.charAt(2 * i));
      int n2 = toN(pubStr.charAt(2 * i + 1));
      if (n1 < 0 || n2 < 0) { okParse = false; break; }
      remotePub[i] = (unsigned char)((n1 << 4) | n2);
    }
    unsigned char remoteMac[8];
    size_t macBytes = macStr.length() / 2;
    if (macBytes > 8) macBytes = 8;
    for (size_t i = 0; i < macBytes; i++) {
      int n1 = toN(macStr.charAt(2 * i));
      int n2 = toN(macStr.charAt(2 * i + 1));
      if (n1 < 0 || n2 < 0) { okParse = false; break; }
      remoteMac[i] = (unsigned char)((n1 << 4) | n2);
    }
    if (!okParse) {
      chatLine(String("*SYS:* KEYDH1 hex parse error"));
      return true;
    }
    // Verify HMAC
    unsigned char calcHmac[32];
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    int ret = mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    if (ret == 0) {
      ret = mbedtls_md_hmac_starts(&md, crypto_spec::KEYDH_ROOT_KEY, sizeof(crypto_spec::KEYDH_ROOT_KEY));
      if (ret == 0) ret = mbedtls_md_hmac_update(&md, remotePub, remotePubLen);
      if (ret == 0) ret = mbedtls_md_hmac_finish(&md, calcHmac);
    }
    mbedtls_md_free(&md);
    bool macOk = (ret == 0);
    if (macOk) {
      for (size_t i = 0; i < macBytes; i++) {
        if (remoteMac[i] != calcHmac[i]) { macOk = false; break; }
      }
    }
    if (!macOk) {
      chatLine(String("*SYS:* KEYDH1 HMAC mismatch"));
      return true;
    }
    // Responder side: derive secret and respond
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    const char* persR = "keydh_resp";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char*)persR, strlen(persR));
    bool ok = (ret == 0);
    ret = mbedtls_ecdh_setup(&ctx, MBEDTLS_ECP_DP_CURVE25519);
    ok = ok && (ret == 0);
    size_t pubLen = 0;
    unsigned char pub[65];
    if (ok) {
      ret = mbedtls_ecdh_make_public(&ctx, &pubLen, pub, sizeof(pub),
                                     mbedtls_ctr_drbg_random, &ctr_drbg);
      ok = ok && (ret == 0 && pubLen > 0);
    }
    if (ok) {
      ret = mbedtls_ecdh_read_public(&ctx, remotePub, remotePubLen);
      ok = ok && (ret == 0);
    }
    unsigned char secret[32];
    size_t secretLen = 0;
    if (ok) {
      ret = mbedtls_ecdh_calc_secret(&ctx, &secretLen, secret, sizeof(secret),
                                     mbedtls_ctr_drbg_random, &ctr_drbg);
      ok = ok && (ret == 0 && secretLen > 0);
    }
    if (ok) {
      unsigned char hash[32];
      mbedtls_sha256(secret, secretLen, hash, 0);
      uint8_t newKey[16];
      memcpy(newKey, hash, 16);
      uint8_t newKid = (uint8_t)((g_kid + 1) & 0xFF);
      if (newKid == 0) newKid = 1;
      g_kid = newKid;
      bool okSet = g_ccm.setKey(g_kid, newKey, 16);
      g_ccm.setActiveKid(g_kid);
      g_ccm.setEnabled(true);
      g_tx.setEncEnabled(true);
      g_key_status = KeyStatus::Received;
      g_key_request_active = false;
      prefs.begin("lora", false);
      char namebuf[8];
      snprintf(namebuf, sizeof(namebuf), "k%03u", (unsigned)g_kid);
      prefs.putBytes(namebuf, newKey, 16);
      prefs.putUChar("kid", g_kid);
      prefs.putBool("enc", true);
      prefs.putBool("key_remote", true);
      prefs.end();
      // Compute HMAC over our public key
      unsigned char respHmac[32];
      mbedtls_md_context_t md2;
      mbedtls_md_init(&md2);
      int ret2 = mbedtls_md_setup(&md2, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
      if (ret2 == 0) {
        ret2 = mbedtls_md_hmac_starts(&md2, crypto_spec::KEYDH_ROOT_KEY, sizeof(crypto_spec::KEYDH_ROOT_KEY));
        if (ret2 == 0) ret2 = mbedtls_md_hmac_update(&md2, pub, pubLen);
        if (ret2 == 0) ret2 = mbedtls_md_hmac_finish(&md2, respHmac);
      }
      mbedtls_md_free(&md2);
      String resp = "KEYDH2 ";
      static const char* hex = "0123456789ABCDEF";
      for (size_t i = 0; i < pubLen; i++) {
        resp += hex[(pub[i] >> 4) & 0xF];
        resp += hex[pub[i] & 0xF];
      }
      resp += " ";
      for (int i = 0; i < 8; i++) {
        resp += hex[(respHmac[i] >> 4) & 0xF];
        resp += hex[respHmac[i] & 0xF];
      }
      g_buf.enqueue(reinterpret_cast<const uint8_t*>(resp.c_str()), resp.length(), true);
      persistMsgId();
      chatLine(String("*SYS:* KEYDH2 queued; KID=") + String((unsigned)g_kid));
    } else {
      chatLine(String("*SYS:* KEYDH1 processing failed"));
    }
    mbedtls_ecdh_free(&ctx);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return true;
  }
  if (m.startsWith("KEYDH2")) {
    if (!g_keydh_in_progress || !g_keydh_initiator) {
      return false;
    }
    int p1 = m.indexOf(' ');
    int p2 = (p1 > 0) ? m.indexOf(' ', p1 + 1) : -1;
    if (p1 < 0 || p2 < 0) {
      chatLine(String("*SYS:* KEYDH2 malformed"));
      return true;
    }
    String pubStr = m.substring(p1 + 1, p2);
    String macStr = m.substring(p2 + 1);
    pubStr.trim(); macStr.trim();
    if ((pubStr.length() % 2) != 0 || macStr.length() < 16) {
      chatLine(String("*SYS:* KEYDH2 invalid lengths"));
      return true;
    }
    size_t remotePubLen = pubStr.length() / 2;
    if (remotePubLen > 65) remotePubLen = 65;
    unsigned char remotePub[65];
    auto toN2 = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      c = toupper(c);
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    bool okParse = true;
    for (size_t i = 0; i < remotePubLen; i++) {
      int n1 = toN2(pubStr.charAt(2 * i));
      int n2 = toN2(pubStr.charAt(2 * i + 1));
      if (n1 < 0 || n2 < 0) { okParse = false; break; }
      remotePub[i] = (unsigned char)((n1 << 4) | n2);
    }
    unsigned char remoteMac[8];
    size_t macBytes = macStr.length() / 2;
    if (macBytes > 8) macBytes = 8;
    for (size_t i = 0; i < macBytes; i++) {
      int n1 = toN2(macStr.charAt(2 * i));
      int n2 = toN2(macStr.charAt(2 * i + 1));
      if (n1 < 0 || n2 < 0) { okParse = false; break; }
      remoteMac[i] = (unsigned char)((n1 << 4) | n2);
    }
    if (!okParse) {
      chatLine(String("*SYS:* KEYDH2 hex parse error"));
      return true;
    }
    // Verify HMAC
    unsigned char calcHmac[32];
    mbedtls_md_context_t md3;
    mbedtls_md_init(&md3);
    int ret = mbedtls_md_setup(&md3, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    if (ret == 0) {
      ret = mbedtls_md_hmac_starts(&md3, crypto_spec::KEYDH_ROOT_KEY, sizeof(crypto_spec::KEYDH_ROOT_KEY));
      if (ret == 0) ret = mbedtls_md_hmac_update(&md3, remotePub, remotePubLen);
      if (ret == 0) ret = mbedtls_md_hmac_finish(&md3, calcHmac);
    }
    mbedtls_md_free(&md3);
    bool macOk = (ret == 0);
    if (macOk) {
      for (size_t i = 0; i < macBytes; i++) {
        if (remoteMac[i] != calcHmac[i]) { macOk = false; break; }
      }
    }
    if (!macOk) {
      chatLine(String("*SYS:* KEYDH2 HMAC mismatch"));
      mbedtls_ecdh_free(&g_keydh_ctx);
      mbedtls_ctr_drbg_free(&g_keydh_ctr_drbg);
      mbedtls_entropy_free(&g_keydh_entropy);
      g_keydh_in_progress = false;
      g_keydh_initiator = false;
      g_key_request_active = false;
      return true;
    }
    size_t secretLen = 0;
    unsigned char secret[32];
    int ret2 = mbedtls_ecdh_read_public(&g_keydh_ctx, remotePub, remotePubLen);
    if (ret2 == 0) {
      ret2 = mbedtls_ecdh_calc_secret(&g_keydh_ctx, &secretLen, secret, sizeof(secret),
                                      mbedtls_ctr_drbg_random, &g_keydh_ctr_drbg);
    }
    if (ret2 == 0 && secretLen > 0) {
      unsigned char hash[32];
      mbedtls_sha256(secret, secretLen, hash, 0);
      uint8_t newKey[16];
      memcpy(newKey, hash, 16);
      uint8_t newKid = (uint8_t)((g_kid + 1) & 0xFF);
      if (newKid == 0) newKid = 1;
      g_kid = newKid;
      bool okSet = g_ccm.setKey(g_kid, newKey, 16);
      g_ccm.setActiveKid(g_kid);
      g_ccm.setEnabled(true);
      g_tx.setEncEnabled(true);
      g_key_status = KeyStatus::Local;
      g_key_request_active = false;
      prefs.begin("lora", false);
      char namebuf[8];
      snprintf(namebuf, sizeof(namebuf), "k%03u", (unsigned)g_kid);
      prefs.putBytes(namebuf, newKey, 16);
      prefs.putUChar("kid", g_kid);
      prefs.putBool("enc", true);
      prefs.putBool("key_remote", false);
      prefs.end();
      chatLine(String("*SYS:* KEYDH completed; KID=") + String((unsigned)g_kid));
    } else {
      chatLine(String("*SYS:* KEYDH2 secret error"));
    }
    mbedtls_ecdh_free(&g_keydh_ctx);
    mbedtls_ctr_drbg_free(&g_keydh_ctr_drbg);
    mbedtls_entropy_free(&g_keydh_entropy);
    g_keydh_in_progress = false;
    g_keydh_initiator = false;
    return true;
  }
  return false;
#else
  (void)m;
  return false;
#endif
}

// HTTP handler for /keydh
void handleKeyDh() {
  startKeyDh();
  server.send(200, "text/plain", "keydh started");
}

// -----------------------------------------------------------------------------
// QoS HTTP handlers.  These expose the SENDQ and LARGEQ commands, plus
// statistics and mode control.  They mirror the UART commands SENDQ,
// LARGEQ, QOS and QOSMODE described in the API.

// Helper to convert priority character to Qos enum
static Qos parseQos(const String& prio) {
  if (prio.equalsIgnoreCase("H")) return Qos::High;
  if (prio.equalsIgnoreCase("L")) return Qos::Low;
  return Qos::Normal;
}

void handleSendQ() {
  // Expect ?prio=<H|N|L>&msg=<text>; missing args result in 400
  if (!server.hasArg("prio") || !server.hasArg("msg")) {
    server.send(400, "text/plain", "missing prio or msg");
    return;
  }
  String pr = server.arg("prio");
  String txt = server.arg("msg");
  txt.trim();
  if (txt.length() == 0) {
    server.send(400, "text/plain", "empty msg");
    return;
  }
  Qos q = parseQos(pr);
  std::vector<uint8_t> bytes(txt.length());
  memcpy(bytes.data(), txt.c_str(), txt.length());
  if (g_buf.enqueueQos(bytes.data(), bytes.size(), false, q) == 0) {
    server.send(503, "text/plain", "queue full");
    return;
  }
  persistMsgId();
  server.send(200, "text/plain", "sendq queued");
  // Логируем постановку сообщения в очередь
  chatMsg("SYS", String("SENDQ(") + pr + String(") queued"));
}

void handleLargeQ() {
  // Expect ?prio=<H|N|L>&size=<n>; default size=1200
  if (!server.hasArg("prio")) {
    server.send(400, "text/plain", "missing prio");
    return;
  }
  String pr = server.arg("prio");
  int sz = 1200;
  if (server.hasArg("size")) {
    int val = server.arg("size").toInt();
    if (val > 0 && val <= 65535) sz = val;
  }
  Qos q = parseQos(pr);
  std::vector<uint8_t> big(sz);
  for (int i = 0; i < sz; i++) big[i] = (uint8_t)(i & 0xFF);
  if (g_buf.enqueueQos(big.data(), big.size(), false, q) == 0) {
    server.send(503, "text/plain", "queue full");
    return;
  }
  persistMsgId();
  server.send(200, "text/plain", "largeq queued");
  // Логируем постановку большого сообщения в очередь
  chatMsg("SYS", String("LARGEQ ") + String(sz) + String(" bytes (prio ") + pr + String(") queued"));
}

void handleQos() {
  // Build a multiline string with counts and bytes per priority and current mode
  char buf[256];
  const char* mode = (g_buf.schedulerMode() == QosMode::Strict) ? "STRICT" : "W421";
  snprintf(buf, sizeof(buf),
           "Mode=%s\nH: q=%u bytes=%u cap=%u\nN: q=%u bytes=%u cap=%u\nL: q=%u bytes=%u cap=%u",
           mode,
           (unsigned)g_buf.lenH(), (unsigned)g_buf.bytesH(), (unsigned)cfg::TX_BUF_QOS_CAP_HIGH,
           (unsigned)g_buf.lenN(), (unsigned)g_buf.bytesN(), (unsigned)cfg::TX_BUF_QOS_CAP_NORMAL,
           (unsigned)g_buf.lenL(), (unsigned)g_buf.bytesL(), (unsigned)cfg::TX_BUF_QOS_CAP_LOW);
  server.send(200, "text/plain", buf);
  // Логируем запрос статистики QoS
  chatMsg("SYS", "QOS stats requested");
}

void handleSetQosMode() {
  // Expect ?val=STRICT or W421 (case insensitive)
  if (!server.hasArg("val")) {
    server.send(400, "text/plain", "missing val");
    return;
  }
  String v = server.arg("val");
  v.trim(); v.toUpperCase();
  if (v == "STRICT" || v == "S") {
    g_buf.setSchedulerMode(QosMode::Strict);
    server.send(200, "text/plain", "qosmode strict");
    // Логируем смену режима QoS
    chatMsg("SYS", "QOSMODE=STRICT");
  } else if (v == "W421" || v == "WEIGHTED421") {
    g_buf.setSchedulerMode(QosMode::Weighted421);
    server.send(200, "text/plain", "qosmode w421");
    // Логируем смену режима QoS
    chatMsg("SYS", "QOSMODE=W421");
  } else {
    server.send(400, "text/plain", "use STRICT or W421");
  }
}

// -----------------------------------------------------------------------------
// Append a line to the chat buffer.  Keeps the buffer size under SERIAL_MAX
// by dropping the oldest characters when the limit is exceeded.
void chatLine(const String& s) {
  serialBuffer += s;
  serialBuffer += '\n';
  if (serialBuffer.length() > SERIAL_MAX) {
    // Remove the oldest data to keep within limit
    serialBuffer.remove(0, serialBuffer.length() - SERIAL_MAX);
  }
  wsServer.broadcastTXT(s.c_str());
}

// Формирует строку с тегом и единым форматом "*TAG:* текст" и выводит в чат
void chatMsg(const String& tag, const String& text) {
  chatLine(String("*") + tag + String(":* ") + text);
}

// -----------------------------------------------------------------------------
// Perform a local ECDH key exchange on this device.  This function simulates
// both sides of an ECDH handshake to derive a shared secret, then uses
// SHA‑256 to derive a 128‑bit AES key from the secret.  It installs the key
// into the CcmEncryptor, increments the KID, enables encryption and saves
// the key into NVS.  Returns true on success.  On host builds (without
// mbedTLS) the function generates a random 128‑bit key instead.
bool performKeyExchange() {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  int ret = 0;
  bool ok = false;
  // Predeclare buffers to avoid jumping over their initialization.  See
  // https://github.com/ARMmbed/mbedtls/blob/development/library/ecdh.c for
  // context structure details.  These buffers are used later for the
  // shared secret and hash, and must exist throughout the function.
  unsigned char secretA[32];
  unsigned char secretB[32];
  unsigned char hash[32];
  uint8_t newKey[16];
  uint8_t newKid = 0;

  // Buffers for high‑level ECDH API.  Declared here so that early exits
  // via goto do not cross their initialization.  We use these for
  // exporting and importing the public keys and for storing secret lengths.
  unsigned char pubA[65];
  unsigned char pubB[65];
  size_t pubA_len = 0;
  size_t pubB_len = 0;
  size_t secretLenA = 0;
  size_t secretLenB = 0;

  // Buffer for persisting key name.  Declared here so that jumps via
  // goto do not cross its initialization.  We fill this when the key is
  // successfully derived.
  char nameBuf[8];

  mbedtls_ecdh_context ctxA;
  mbedtls_ecdh_context ctxB;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
  const char* pers = "ecdh";
  mbedtls_ecdh_init(&ctxA);
  mbedtls_ecdh_init(&ctxB);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_entropy_init(&entropy);
  // Seed the DRBG
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char*)pers, strlen(pers));
  if (ret != 0) {
    goto finish;
  }
  // Use Curve25519 for ECDH
  ret = mbedtls_ecdh_setup(&ctxA, MBEDTLS_ECP_DP_CURVE25519);
  if (ret != 0) {
    goto finish;
  }
  ret = mbedtls_ecdh_setup(&ctxB, MBEDTLS_ECP_DP_CURVE25519);
  if (ret != 0) {
    goto finish;
  }
  // Perform high-level ECDH handshake using mbedTLS API.  We generate
  // public keys, exchange them locally, then compute the shared secret.
  // Generate public key for context A
  ret = mbedtls_ecdh_make_public(&ctxA, &pubA_len, pubA, sizeof(pubA),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    goto finish;
  }
  // Generate public key for context B
  ret = mbedtls_ecdh_make_public(&ctxB, &pubB_len, pubB, sizeof(pubB),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    goto finish;
  }
  // Each side imports the other's public key
  ret = mbedtls_ecdh_read_public(&ctxA, pubB, pubB_len);
  if (ret != 0) {
    goto finish;
  }
  ret = mbedtls_ecdh_read_public(&ctxB, pubA, pubA_len);
  if (ret != 0) {
    goto finish;
  }
  ret = mbedtls_ecdh_calc_secret(&ctxA, &secretLenA, secretA, sizeof(secretA),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    goto finish;
  }
  ret = mbedtls_ecdh_calc_secret(&ctxB, &secretLenB, secretB, sizeof(secretB),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    goto finish;
  }
  if (secretLenA != secretLenB || secretLenA == 0 ||
      memcmp(secretA, secretB, secretLenA) != 0) {
    ret = -1;
    goto finish;
  }
  // Derive 128-bit AES key by hashing the shared secret with SHA‑256
  mbedtls_sha256(secretA, 32, hash, 0 /* use full SHA‑256 */);
  memcpy(newKey, hash, 16);
  // Choose a new KID (increment, wrap 1..255).  Avoid 0.
  newKid = (uint8_t)((g_kid + 1) & 0xFF);
  if (newKid == 0) newKid = 1;
  g_kid = newKid;
  ok = g_ccm.setKey(g_kid, newKey, 16);
  g_ccm.setActiveKid(g_kid);
  g_ccm.setEnabled(true);
  g_tx.setEncEnabled(true);
  // Persist key into NVS for next boot
  snprintf(nameBuf, sizeof(nameBuf), "k%03u", (unsigned)g_kid);
  prefs.begin("lora", false);
  prefs.putBytes(nameBuf, newKey, 16);
  prefs.putUChar("kid", g_kid);
  prefs.putBool("enc", true);
  // Mark this key as local and persist the flag.  A value of false means
  // this key was generated locally rather than received from remote.
  prefs.putBool("key_remote", false);
  prefs.end();
  // Update key status after successful exchange
  g_key_status = KeyStatus::Local;
  ret = ok ? 0 : -1;
finish:
  mbedtls_ecdh_free(&ctxA);
  mbedtls_ecdh_free(&ctxB);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  return ret == 0;
#else
  // Host stub: generate a pseudo‑random 128‑bit key and install it.
  static bool seeded = false;
  if (!seeded) {
    srand((unsigned)micros());
    seeded = true;
  }
  uint8_t newKey[16];
  for (int i = 0; i < 16; i++) {
    newKey[i] = (uint8_t)(rand() & 0xFF);
  }
  uint8_t newKid = (uint8_t)((g_kid + 1) & 0xFF);
  if (newKid == 0) newKid = 1;
  g_kid = newKid;
  ok = g_ccm.setKey(g_kid, newKey, 16);
  g_ccm.setActiveKid(g_kid);
  g_ccm.setEnabled(true);
  g_tx.setEncEnabled(true);
  // Mark key as locally generated in stub environment
  g_key_status = KeyStatus::Local;
  return ok;
#endif
}

// HTTP handler for key exchange test.  Calls performKeyExchange(), appends
// a system line to the chat and returns a status message.  If a key was
// successfully derived it reports the new KID; otherwise it reports failure.
void handleKeyTest() {
  bool ok = performKeyExchange();
  if (ok) {
    String msg = String("*SYS:* Key exchange successful; new KID=") + String((unsigned)g_kid);
    chatLine(msg);
    server.send(200, "text/plain", "keytest ok");
  } else {
    chatLine(String("*SYS:* Key exchange failed"));
    server.send(500, "text/plain", "keytest failed");
  }
}

// -----------------------------------------------------------------------------
// Asynchronous ping state machine.  This function is called from loop() and
// progresses the ping operation one step at a time without blocking.  It
// initiates the ping, waits for a response with a timeout, computes
// metrics (RSSI/SNR/distance/time) and writes results to the chat.  The
// radio is returned to receive mode once complete.
void handlePingAsync() {
  if (!g_ping_active) return;
  if (g_ping_stage == 0) {
    // Build 5‑byte ping pattern
    g_ping_tx[1] = radio.randomByte();
    g_ping_tx[2] = radio.randomByte();
    g_ping_tx[0] = g_ping_tx[1] ^ g_ping_tx[2];
    g_ping_tx[3] = 0x01;
    g_ping_tx[4] = 0x00;
    // Report current RX/TX frequencies
    String info = String("RX:") + String(g_freq_rx_mhz, 3) + String("/TX:") + String(g_freq_tx_mhz, 3);
    chatLine(String("*SYS:* ") + info);
    Serial.print("~&Server: ");
    Serial.print(info);
    Serial.println("~");
    // Transmit ping on TX frequency
    radio.setFrequency(g_freq_tx_mhz);
    delay(0);
    radio.transmit(g_ping_tx, 5);
    radio.finishTransmit();             // завершаем передачу без ожидания
    // Record start time and switch to RX
    g_ping_start_us = micros();
    radio.setFrequency(g_freq_rx_mhz);
    // Ждём ответ не дольше g_ping_timeout_us
    Radio_forceRx(msToTicks(g_ping_timeout_us / 1000));
    g_ping_stage = 1;
    return;
  }
  if (g_ping_stage == 1) {
    // Check for incoming data or timeout
    uint32_t now = micros();
    if (g_hasRx) {
      // Copy received bytes and clear flag
      std::vector<uint8_t> rx = g_rxBuf;
      g_hasRx = false;
      // Compute elapsed time
      uint32_t dt = now - g_ping_start_us;
      // Determine if packet matches our ping
      bool ok = (rx.size() == 5);
      if (ok) {
        for (int i = 0; i < 5; i++) {
          if (rx[i] != g_ping_tx[i]) { ok = false; break; }
        }
      }
      if (ok) {
        float rssi = radio.getRSSI();
        float snr  = radio.getSNR();
        double dist_km = ((double)dt * 1e-6 * 299792458.0 / 2.0) / 1000.0;
        double ms_time = (double)dt * 0.001;
        String msg = String("Ping>OK RSSI:") + String(rssi, 1) + " dBm/SNR:" + String(snr, 1) + " dB distance:~" + String(dist_km, 3) + " km time:" + String(ms_time, 2) + " ms";
        chatLine(String("*SYS:* ") + msg);
        Serial.print("~&Server: ");
        Serial.print(msg);
        Serial.println("~");
      } else {
        // Non‑matching packet: treat as CRC error or bad packet
        chatLine(String("*SYS:* CRC error!"));
        Serial.print("~&Server: ");
        Serial.print("CRC error!");
        Serial.println("~");
      }
      // End ping transaction
      g_ping_active = false;
      g_ping_stage = 0;
      g_radio_busy = false;
      radio.setFrequency(g_freq_rx_mhz);
      // Возобновляем обычный приём
      Radio_forceRx(msToTicks(tdd::cycleLen()));
      return;
    }
    // Timeout: if elapsed time exceeds the timeout threshold
    if ((uint32_t)(now - g_ping_start_us) >= g_ping_timeout_us) {
      chatLine(String("*SYS:* Ping timeout"));
      Serial.print("~&Server: ");
      Serial.print("Ping timeout");
      Serial.println("~");
      // End ping transaction
      g_ping_active = false;
      g_ping_stage = 0;
      g_radio_busy = false;
      radio.setFrequency(g_freq_rx_mhz);
      // Возобновляем обычный приём
      Radio_forceRx(msToTicks(tdd::cycleLen()));
      return;
    }
    // Otherwise, still waiting; yield control
    delay(0);
    return;
  }
}

// -----------------------------------------------------------------------------
// runPing() performs a two‑way ping over LoRa.  It uses the same 5‑byte ping
// pattern as the original SatPing() but returns the results to the web UI.
// The function sends a ping on the TX frequency, immediately switches to
// the RX frequency to await a reply, measures the round‑trip time and
// computes an approximate distance based on the speed of light.  RSSI and
// SNR are also reported.  All output is sent to both the serial console
// (prefixed with "~&Server:") and to the serialBuffer as "*SYS:*" lines
// for display in the chat window.
void runPing() {
  // Build ping packet: bytes 0..2 contain an identifier and random bytes,
  // byte 3 is an address placeholder (0x01), byte 4 unused (0x00).
  uint8_t ping[5];
  uint8_t rx[5];
  // Generate two random bytes and compute checksum/identifier
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();
  ping[0] = ping[1] ^ ping[2];
  ping[3] = 0x01;
  ping[4] = 0x00;

  // Inform user of current RX/TX frequencies
  // Format frequencies with 3 decimal places
  String info = String("RX:") + String(g_freq_rx_mhz, 3) + String("/TX:") + String(g_freq_tx_mhz, 3);
  Serial.print("~&Server: ");
  Serial.print(info);
  Serial.println("~");
  // Логируем текущие частоты
  chatMsg("SYS", info);

  // Transmit ping on TX frequency
  radio.setFrequency(g_freq_tx_mhz);
  radio.transmit(ping, 5);
  radio.finishTransmit();               // немедленно завершаем передачу
  // Record transmit timestamp
  uint32_t time1 = micros();
  // Switch to RX frequency and receive up to 5 bytes
  radio.setFrequency(g_freq_rx_mhz);
  int state = radio.receive(rx, 5);
  // Record receive timestamp
  uint32_t time2 = micros();
  // Check if response matches our ping
  bool err_ping = memcmp(ping, rx, 5);
  if (state == RADIOLIB_ERR_NONE || err_ping == 0) {
    // Successful reception: compute RTT and derived metrics
    uint32_t time3 = time2 - time1;
    float rssi = radio.getRSSI();
    float snr = radio.getSNR();
    // Distance in kilometres: (t [us] -> s) * c / 2 / 1000
    double dist_km = ((double)time3 * 1e-6 * 299792458.0 / 2.0) / 1000.0;
    double ms_time = (double)time3 * 0.001;
    // Compose message with limited decimal places for clarity
    String msg = String("Ping>OK RSSI:") + String(rssi, 1) + " dBm/SNR:" + String(snr, 1) + " dB distance:~" + String(dist_km, 3) + " km time:" + String(ms_time, 2) + " ms";
    Serial.print("~&Server: ");
    Serial.print(msg);
    Serial.println("~");
    // Логируем результат пинга
    chatMsg("SYS", msg);
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // Received a packet but CRC failed
    String msg = String("CRC error!");
    Serial.print("~&Server: ");
    Serial.print(msg);
    Serial.println("~");
    // Логируем ошибку CRC
    chatMsg("SYS", msg);
  } else {
    // Some other error
    String msg = String("failed, code ") + String(state);
    Serial.print("~&Server: ");
    Serial.print(msg);
    Serial.println("~");
    // Логируем код ошибки
    chatMsg("SYS", msg);
  }
  // Короткая пауза для перехода в режим приёма
  delay(2);
  radio.setFrequency(g_freq_rx_mhz);
  // Возвращаем радио к приёму
  Radio_forceRx(msToTicks(tdd::cycleLen()));
}

static void handleCommand(const String& line) {
  if (line.length() == 0) return;
  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String arg = (sp < 0) ? "" : line.substring(sp + 1);
  String CM = cmd;
  CM.trim();
  CM.toUpperCase();

  if (CM == "HELP") {
    printHelp();
    return;
  }
  if (CM == "VER" || CM == "VERSION") {
    printVersion();
    return;
  }
  if (CM == "METRICS") {
    printMetrics();
    return;
  }
  if (CM == "RADIO") {
    printRadio();
    return;
  }
  if (CM == "DUMP") {
    unsigned n = arg.length() ? (unsigned)arg.toInt() : 10;
    FrameLog::dump(Serial, n);
    return;
  }

  if (CM == "SIMPLE") {
    const char* msg = "ping";
    g_buf.enqueue(reinterpret_cast<const uint8_t*>(msg), strlen(msg), false);
    persistMsgId();
    Serial.println(F("Queued SIMPLE."));
    return;
  }
  if (CM == "LARGE") {
    int sz = arg.length() ? arg.toInt() : 1200;
    if (sz < 1) sz = 1200;
    std::vector<uint8_t> big(sz);
    for (int i = 0; i < sz; i++) big[i] = (uint8_t)(i & 0xFF);
    g_buf.enqueue(big.data(), big.size(), false);
    persistMsgId();
    Serial.printf("Queued LARGE %d bytes.\n", sz);
    return;
  }
  if (CM == "SEND") {
    if (arg.length() == 0) {
      Serial.println(F("Usage: SEND <text>"));
      return;
    }
    size_t n = (size_t)arg.length();
    std::vector<uint8_t> bytes(n);
    memcpy(bytes.data(), arg.c_str(), n);
    g_buf.enqueue(bytes.data(), bytes.size(), false);
    persistMsgId();
    Serial.println(F("Queued SEND."));
    return;
  }

  if (CM == "B") {
    int b = arg.toInt();
    if (b < 0 || b > 2) {
      Serial.println(F("B range 0..2"));
      return;
    }
    g_bank = static_cast<Bank>(b);
    applyPreset();
    Serial.printf("Bank=%d Preset=%d FRX=%.3f FTX=%.3f\n", b, g_preset, g_freq_rx_mhz, g_freq_tx_mhz);
    return;
  }
  if (CM == "P") {
    int p = arg.toInt();
    if (p < 0 || p > 9) {
      Serial.println(F("P range 0..9"));
      return;
    }
    g_preset = p;
    applyPreset();
    Serial.printf("Preset=%d FRX=%.3f FTX=%.3f\n", g_preset, g_freq_rx_mhz, g_freq_tx_mhz);
    return;
  }
  if (CM == "FREQ" || CM == "FREQRX") {
    float mhz = arg.toFloat();
    if (mhz < 150.0f || mhz > 960.0f) {
      Serial.println(F("FREQRX 150..960 MHz"));
      return;
    }
    g_freq_rx_mhz = mhz;
    Radio_setFrequency((uint32_t)(g_freq_rx_mhz * 1e6f));
    // Обновляем приём после смены частоты
    Radio_forceRx(msToTicks(tdd::cycleLen()));
    Serial.printf("FRX=%.3f\n", g_freq_rx_mhz);
    // Сообщаем новую RX-частоту в чат
    chatMsg("SYS", String("FRX set to ") + String(g_freq_rx_mhz, 3) + " MHz");
    return;
  }
  if (CM == "FREQTX") {
    float mhz = arg.toFloat();
    if (mhz < 150.0f || mhz > 960.0f) {
      Serial.println(F("FREQTX 150..960 MHz"));
      return;
    }
    g_freq_tx_mhz = mhz;
    Serial.printf("FTX=%.3f\n", g_freq_tx_mhz);
    // Сообщаем новую TX-частоту в чат
    chatMsg("SYS", String("FTX set to ") + String(g_freq_tx_mhz, 3) + " MHz");
    return;
  }
  if (CM == "BW") {
    float khz = arg.toFloat();
    if (khz < 7.8f || khz > 500.0f) {
      Serial.println(F("BW 7.8..500 kHz"));
      return;
    }
    g_bw_khz = khz;
    Radio_setBandwidth((uint32_t)g_bw_khz);
    Serial.printf("BW=%.1f kHz\n", g_bw_khz);
    return;
  }
  if (CM == "SF") {
    int sf = arg.toInt();
    if (sf < 7 || sf > 12) {
      Serial.println(F("SF 7..12"));
      return;
    }
    g_sf = sf;
    Radio_setSpreadingFactor(g_sf);
    Serial.printf("SF=%d\n", g_sf);
    return;
  }
  if (CM == "CR") {
    int cr = arg.toInt();
    if (cr < 5 || cr > 8) {
      Serial.println(F("CR 5..8 (4/x)"));
      return;
    }
    g_cr = cr;
    Radio_setCodingRate(g_cr);
    Serial.printf("CR=4/%d\n", g_cr);
    return;
  }
  if (CM == "TXP") {
    int pwr = arg.toInt();
    if (pwr < -9 || pwr > 22) {
      Serial.println(F("TXP -9..22 dBm"));
      return;
    }
    g_txp = (int8_t)pwr;
    Radio_setTxPower(g_txp);
    Serial.printf("TXP=%d dBm\n", g_txp);
    return;
  }

  if (CM == "SAVE") {
    saveConfig();
    return;
  }
  if (CM == "LOAD") {
    loadConfig();
    return;
  }
  if (CM == "RESET") {
    resetConfig();
    return;
  }

  if (CM == "KA") {
    int v = arg.toInt();
    g_ack_on = (v != 0);
    g_tx.enableAck(g_ack_on);
    Serial.printf("ACK=%s\n", g_ack_on ? "ON" : "OFF");
    return;
  }
  if (CM == "RETRYN") {
    int n = arg.toInt();
    if (n < 0 || n > 10) {
      Serial.println(F("RETRYN 0..10"));
      return;
    }
    g_retryN = (uint8_t)n;
    g_tx.setRetry(g_retryN, g_retryMS);
    Serial.printf("RETRYN=%d\n", (int)g_retryN);
    return;
  }
  if (CM == "RETRYMS") {
    int ms = arg.toInt();
    if (ms < 100 || ms > 10000) {
      Serial.println(F("RETRYMS 100..10000"));
      return;
    }
    g_retryMS = (uint16_t)ms;
    g_tx.setRetry(g_retryN, g_retryMS);
    Serial.printf("RETRYMS=%d\n", (int)g_retryMS);
    return;
  }

  if (CM == "ENC") {
    int v = arg.toInt();
    g_enc_on = (v != 0);
    g_ccm.setEnabled(g_enc_on);
    g_tx.setEncEnabled(g_enc_on);
    Serial.printf("ENC=%s\n", g_enc_on ? "ON" : "OFF");
    return;
  }
  if (CM == "KID") {
    int kid = arg.toInt();
    if (kid < 0 || kid > 255) {
      Serial.println(F("KID 0..255"));
      return;
    }
    g_kid = (uint8_t)kid;
    g_ccm.setActiveKid(g_kid);
    loadKeyFromNVS();
    Serial.printf("KID=%u\n", (unsigned)g_kid);
    return;
  }
  if (CM == "KEY") {
    String hex = arg;
    hex.trim();
    if (hex.length() != 32) {
      Serial.println(F("KEY expects 32 hex chars (128-bit)"));
      return;
    }
    uint8_t key[16];
    for (int i = 0; i < 16; i++) {
      char c1 = hex[2 * i], c2 = hex[2 * i + 1];
      auto toN = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        c = toupper(c);
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int n1 = toN(c1), n2 = toN(c2);
      if (n1 < 0 || n2 < 0) {
        Serial.println(F("Bad hex"));
        return;
      }
      key[i] = (uint8_t)((n1 << 4) | n2);
    }
    char name[8];
    snprintf(name, sizeof(name), "k%03u", (unsigned)g_kid);
    prefs.begin("lora", false);
    prefs.putBytes(name, key, 16);
    prefs.end();
    bool ok = g_ccm.setKey(g_kid, key, 16);
    Serial.printf("KEY %s for KID=%u\n", ok ? "loaded" : "failed", (unsigned)g_kid);
    return;
  }

  if (CM == "MSGID") {
    if (arg.length()) {
      uint32_t n = (uint32_t)strtoul(arg.c_str(), nullptr, 10);
      if (n == 0) n = 1;
      g_buf.setNextId(n);
      persistMsgId();
    }
    Serial.printf("MSGID_NEXT=%lu\n", (unsigned long)g_buf.nextId());
    return;
  }

  if (CM == "ENCTEST") {
    if (arg.length()) {
      size_t n = (size_t)arg.toInt();
      EncSelfTest_run(g_ccm, n, Serial);
    } else {
      EncSelfTest_battery(g_ccm, Serial);
    }
    return;
  }
  if (CM == "ENCTESTBAD") {
    EncSelfTest_badKid(g_ccm, Serial);
    return;
  }
  // KEYREQ: send a key request to the remote device
  if (CM == "KEYREQ") {
    sendKeyRequest();
    Serial.println(F("KEYREQ queued"));
    return;
  }
  // KEYSEND: send a key response containing our current key
  if (CM == "KEYSEND") {
    sendKeyResponse();
    Serial.println(F("KEYSEND queued"));
    return;
  }
  // KEYSTATUS: report whether the current key was received from remote or
  // generated locally.  LOCAL means the key was generated locally, REMOTE
  // indicates it was received via KEYRES.
  if (CM == "KEYSTATUS") {
    const char* st = (g_key_status == KeyStatus::Received) ? "REMOTE" : "LOCAL";
    Serial.print(F("KEYSTATUS=")); Serial.println(st);
    return;
  }
  // KEYX/KEYTEST: perform a local ECDH key exchange test and install the new key.
  if (CM == "KEYX" || CM == "KEYTEST") {
    bool ok = performKeyExchange();
    Serial.println(ok ? F("Key exchange successful") : F("Key exchange failed"));
    return;
  }
  // KEYDH: initiate authenticated ECDH handshake and send KEYDH1
  if (CM == "KEYDH") {
    startKeyDh();
    Serial.println(F("KEYDH queued"));
    return;
  }

  if (CM == "SATPING") {
    SatPing();
    return;
  }

// SENDQ
if (CM == "SENDQ") {
  int sp2 = arg.indexOf(' ');
  if (sp2 < 0) { Serial.println(F("Usage: SENDQ <H|N|L> <text>")); return; }
  String q = arg.substring(0, sp2); String txt = arg.substring(sp2+1);
  q.trim(); txt.trim(); if (txt.length()==0) { Serial.println(F("Empty text")); return; }

  Qos qq = Qos::Normal;
  if (q.equalsIgnoreCase("H")) qq = Qos::High;
  else if (q.equalsIgnoreCase("L")) qq = Qos::Low;

  std::vector<uint8_t> bytes(txt.length()); memcpy(bytes.data(), txt.c_str(), txt.length());
  if (g_buf.enqueueQos(bytes.data(), bytes.size(), false, qq) == 0) { Serial.println(F("Queue full (cap or total)")); return; }
  persistMsgId();
  Serial.println(F("Queued SENDQ.")); return;
}
 // LARGEQ
if (CM == "LARGEQ") {
  int sp2 = arg.indexOf(' ');
  if (sp2 < 0) { Serial.println(F("Usage: LARGEQ <H|N|L> [size]")); return; }
  String q = arg.substring(0, sp2); String rest = arg.substring(sp2+1);
  q.trim(); rest.trim();
  int sz = rest.length()? rest.toInt() : 1200; if (sz < 1) sz = 1200;

  Qos qq = Qos::Normal;
  if (q.equalsIgnoreCase("H")) qq = Qos::High;
  else if (q.equalsIgnoreCase("L")) qq = Qos::Low;

  std::vector<uint8_t> big(sz); for (int i=0;i<sz;i++) big[i] = (uint8_t)(i & 0xFF);
  if (g_buf.enqueueQos(big.data(), big.size(), false, qq) == 0) { Serial.println(F("Queue full (cap or total)")); return; }
  persistMsgId();
  Serial.printf("Queued LARGEQ %d bytes.\n", sz); return;
}
if (CM == "QOS") {
  Serial.printf("QOS H: q=%u bytes=%u cap=%u\n", (unsigned)g_buf.lenH(), (unsigned)g_buf.bytesH(), (unsigned)cfg::TX_BUF_QOS_CAP_HIGH);
  Serial.printf("QOS N: q=%u bytes=%u cap=%u\n", (unsigned)g_buf.lenN(), (unsigned)g_buf.bytesN(), (unsigned)cfg::TX_BUF_QOS_CAP_NORMAL);
  Serial.printf("QOS L: q=%u bytes=%u cap=%u\n", (unsigned)g_buf.lenL(), (unsigned)g_buf.bytesL(), (unsigned)cfg::TX_BUF_QOS_CAP_LOW);
  return;
}

if (CM == "QOSMODE") {
  String v = arg; v.trim(); v.toUpperCase();
  if (v == "STRICT" || v == "S") { g_buf.setSchedulerMode(QosMode::Strict); Serial.println(F("QOSMODE=STRICT")); }
  else if (v == "W421") { g_buf.setSchedulerMode(QosMode::Weighted421); Serial.println(F("QOSMODE=W421")); }
  else { Serial.println(F("Usage: QOSMODE <STRICT|W421>")); }
  return;
}



  Serial.println(F("Unknown. Type HELP."));
}

void Radio_onReceive(const uint8_t* data, size_t len) {
  g_rxBuf.assign(data, data + len);
  g_hasRx = true;
}

// Poll commands from a serial stream and dispatch complete lines
static void pollUart(Stream& s, String& line) {
  while (s.available()) {
    char c = (char)s.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(line);
      line = "";
    } else {
      line += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println(F("\nESP32 LoRa Pipeline (Track D: ENC+ACK+MSGID) start"));
  SerialRadxa.begin(cfg::RADXA_UART_BAUD);

  pinMode(PIN_TCXO_DETECT, INPUT_PULLUP);
  float tcxo = (digitalRead(PIN_TCXO_DETECT) == LOW) ? 2.4f : 0.0f;

  int16_t st = radio.begin(g_freq_rx_mhz, g_bw_khz, g_sf, g_cr, 0x18, g_txp, 10, tcxo, false);
  radio.setDio2AsRfSwitch(true); // включаем управление RF switch через DIO2
  radio.setDio1Action(isr); // регистрируем ISR на DIO1
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("Radio begin failed: %d\n", st);
  } else {
    Serial.println(F("Radio OK"));
  }
    g_tx.setRetry(g_retryN, g_retryMS);
    g_tx.enableAck(g_ack_on);
    g_tx.setHeaderDup(g_dup_on);
    g_tx.setWindowSize(g_window);
    g_tx.setBurstFrags(g_burst);
    g_rx.setHeaderDup(g_dup_on);
    g_rx.setWindowSize(g_window);
    g_rx.setAckAgg(g_ackAgg);
    g_ccm.setEnabled(g_enc_on);
    g_ccm.setActiveKid(g_kid);
    loadKeyFromNVS();
    g_tx.setEncEnabled(g_enc_on);
  // Первичный запуск приёма
  Radio_forceRx(msToTicks(tdd::cycleLen()));

  // Start Wi‑Fi access point and HTTP server.  The device will create an
  // access point with SSID WIFI_SSID and password WIFI_PASSWORD.  Connect to
  // http://192.168.4.1/ to access the web interface.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.print(F("AP IP: "));
  String ipStr = ip.toString();
  Serial.println(ipStr.c_str());
  // Register HTTP handlers.
  server.on("/", handleRoot);
  server.on("/style.css", handleStyle);
  server.on("/app.js", handleScript);
  server.on("/send", handleSend);
  server.on("/serial", handleSerial);
  server.on("/setbank", handleSetBank);
  server.on("/setpreset", handleSetPreset);
  server.on("/setbw", handleSetBw);
  server.on("/setsf", handleSetSf);
  server.on("/setcr", handleSetCr);
  server.on("/settxp", handleSetTxp);
  server.on("/setrxboost", handleSetRxBoost);
  server.on("/setfec", handleSetFec);       // установка режима FEC
  server.on("/setinter", handleSetInter);   // глубина интерливера
    server.on("/setpayload", handleSetPayload); // размер полезной нагрузки
    server.on("/setpilot", handleSetPilot);     // интервал пилотов
    server.on("/setdup", handleSetDup);         // дублирование заголовка
    server.on("/setwin", handleSetWin);         // размер окна SR-ARQ
    server.on("/setackagg", handleSetAckAgg);   // интервал агрегации ACK
    server.on("/setburst", handleSetBurst);     // предел фрагментов в серии
    server.on("/setack", handleSetAck);
    server.on("/setackjitter", handleSetAckJitter); // джиттер ожидания ACK
    server.on("/setbackoff", handleSetBackoff);     // коэффициенты backoff
    server.on("/setautorate", handleSetAutorate);   // включение автонастройки
    server.on("/settxprofile", handleSetTxProfile); // ручной профиль P0..P3
    server.on("/setperth", handleSetPerTh);         // пороги PER
    server.on("/setebn0th", handleSetEbn0Th);       // пороги Eb/N0
    server.on("/toggleenc", handleToggleEnc);
  server.on("/metrics", handleMetrics);
  server.on("/linkdiag", handleLinkDiag);
  server.on("/ping", handlePing);
  server.on("/channelping", handleChannelPing);
  server.on("/presetping", handlePresetPing);
  server.on("/massping", handleMassPing);
  server.on("/satping", handleSatPingAdv);
  server.on("/setretryn", handleSetRetryN);
  server.on("/setretryms", handleSetRetryMS);
  server.on("/setkid", handleSetKid);
  server.on("/setkey", handleSetKey);
  server.on("/save", handleSave);
  server.on("/load", handleLoad);
  server.on("/reset", handleReset);
  server.on("/tdd", handleSetTdd);
  server.on("/archivelist", handleArchiveList);
  server.on("/archiverestore", handleArchiveRestore);
  server.on("/idreset", handleIdReset);
  server.on("/replayclr", handleReplayClr);
  server.on("/frames", handleFrames);
  // Additional command endpoints
  server.on("/simple", handleSimple);
  server.on("/large", handleLarge);
  server.on("/enctest", handleEncTest);
  server.on("/enctestbad", handleEncTestBad);
  server.on("/selftest", handleSelfTest);
  server.on("/msgid", handleSetMsgId);
  // QoS endpoints
  server.on("/sendq", handleSendQ);
  server.on("/largeq", handleLargeQ);
  server.on("/qos", handleQos);
  server.on("/qosmode", handleSetQosMode);
  // Key exchange test endpoint
  server.on("/keytest", handleKeyTest);
  // Key request/response/status endpoints
  server.on("/keyreq", handleKeyReq);
  server.on("/keysend", handleKeySend);
  server.on("/keystatus", handleKeyStatus);
  server.on("/keyhash", handleKeyHash);
  // Authenticated ECDH key exchange endpoint
  server.on("/keydh", handleKeyDh);
  server.begin();
  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  // Run self-test at startup, outputting results to the serial console.
  SelfTest_runAll(g_ccm, Serial);

  g_rx.setMessageCallback([](uint32_t id, const uint8_t* d, size_t n) {
    Serial.printf("[RX msg %u] %u bytes\n", id, (unsigned)n);
    SerialRadxa.printf("[RX msg %u] %u bytes\n", id, (unsigned)n);
    // Convert raw bytes to a printable ASCII string.  Non‑printable
    // characters are replaced with '.'.
    String m = "";
    for (size_t i = 0; i < n; i++) {
      char c = (char)d[i];
      if (c < 32 || c > 126) c = '.';
      m += c;
    }
    SerialRadxa.println(m);
    // Handle key exchange messages (KEYDH1/KEYDH2).  If processKeyDh
    // returns true then the message was consumed.
    if (m.startsWith("KEYDH1") || m.startsWith("KEYDH2")) {
      if (processKeyDh(m)) {
        return;
      }
    }
    // Handle special key management messages
    if (m.startsWith("KEYREQ")) {
      // Received a key request from peer: send our key
      chatLine(String("*SYS:* Received KEYREQ"));
      sendKeyResponse();
      return;
    }
    if (m.startsWith("KEYRES")) {
      // Received a key response.  Expected format: KEYRES <kid> <hexkey>
      int s1 = m.indexOf(' ');
      int s2 = m.indexOf(' ', s1 + 1);
      if (s1 > 0 && s2 > s1) {
        String kidStr = m.substring(s1 + 1, s2);
        String keyStr = m.substring(s2 + 1);
        kidStr.trim(); keyStr.trim();
        bool okParse = true;
        uint8_t newKid = (uint8_t)strtoul(kidStr.c_str(), nullptr, 10);
        if (newKid == 0) okParse = false;
        if (keyStr.length() != 32) okParse = false;
        uint8_t keyBytes[16];
        if (okParse) {
          for (int i = 0; i < 16; i++) {
            char h1 = keyStr.charAt(i * 2);
            char h2 = keyStr.charAt(i * 2 + 1);
            auto toN = [](char c) -> int {
              if (c >= '0' && c <= '9') return c - '0';
              c = toupper(c);
              if (c >= 'A' && c <= 'F') return c - 'A' + 10;
              return -1;
            };
            int n1 = toN(h1);
            int n2 = toN(h2);
            if (n1 < 0 || n2 < 0) { okParse = false; break; }
            keyBytes[i] = (uint8_t)((n1 << 4) | n2);
          }
        }
        if (okParse) {
          g_kid = newKid;
          g_enc_on = true;
          g_ccm.setKey(g_kid, keyBytes, 16);
          g_ccm.setActiveKid(g_kid);
          g_ccm.setEnabled(true);
          g_tx.setEncEnabled(true);
          g_key_status = KeyStatus::Received;
          g_key_request_active = false;
          // Persist the new key and status into NVS
          prefs.begin("lora", false);
          char namebuf[8];
          snprintf(namebuf, sizeof(namebuf), "k%03u", (unsigned)g_kid);
          prefs.putBytes(namebuf, keyBytes, 16);
          prefs.putUChar("kid", g_kid);
          prefs.putBool("enc", true);
          prefs.putBool("key_remote", true);
          prefs.end();
          chatLine(String("*SYS:* Key received; KID=") + String((unsigned)g_kid));
        } else {
          chatLine(String("*SYS:* KEYRES parse error"));
        }
      } else {
        chatLine(String("*SYS:* KEYRES malformed"));
      }
      return;
    }
    // Обычное сообщение — выводим его в чат
    chatLine(String("*RX:* ") + m);
  });
  g_rx.setAckCallback([&](uint32_t hi, uint32_t bm) {
    g_tx.notifyAck(hi, bm);
  });

  loadConfig();
}

void loop() {
  pollUart(Serial, g_line);
  pollUart(SerialRadxa, g_line_radxa);
  // Handle incoming HTTP clients.
  server.handleClient();
  wsServer.loop();
  // Обработка завершения передачи
  if (txDoneFlag) {
    txDoneFlag = false;
    radio.startReceive();
    delay(2); // пауза для переключения в приём
  }
  // Обработка завершения приёма
  if (rxDoneFlag) {
    rxDoneFlag = false;
    uint8_t tmp[cfg::LORA_MTU];
    int16_t len = radio.readData(tmp, sizeof(tmp));
    if (len == 5) {
      radio.setFrequency(g_freq_tx_mhz);
      radio.transmit(tmp, 5);
      radio.finishTransmit();            // принудительный выход из TX
      radio.setFrequency(g_freq_rx_mhz);
      radio.startReceive();
    } else {
      if (len > 0) {
        Radio_onReceive(tmp, (size_t)len);
      }
      radio.startReceive();
    }
  }
  // Progress asynchronous ping state if active
  handlePingAsync();
  if (g_hasRx) {
    g_rx.onReceive(g_rxBuf.data(), g_rxBuf.size());
    g_hasRx = false;
  }
  // Only run the transmit pipeline when the radio is not busy (e.g. during ping)
  if (!g_radio_busy) {
    g_tx.loop();
  }
}
