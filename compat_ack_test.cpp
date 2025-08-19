#ifndef ARDUINO
// Совместимый тест ACK для десктопной сборки
#include "Arduino.h"
#include "tx_pipeline.h"
#include "rx_pipeline.h"
#include "fragmenter.h"
#include "message_buffer.h"
#include "encryptor.h"
#include "frame.h"
#include "tdd_scheduler.h"
#include <vector>
#include <cstring>
#include <algorithm>

// Глобальные указатели для маршрутизации кадров
static TxPipeline* g_tx = nullptr;
static RxPipeline* g_rx_local = nullptr;   // Приём ACK
static RxPipeline* g_rx_remote = nullptr;  // Приём обычных пакетов
static std::vector<uint8_t> g_pending_ack;

// Заглушки радио
bool Radio_sendRaw(const uint8_t* data, size_t len) {
  if (!data || len < FRAME_HEADER_SIZE) return true;
  FrameHeader hdr;
  if (!FrameHeader::decode(hdr, data, len)) return true;
  if (hdr.flags & F_ACK) {
    g_pending_ack.assign(data, data + len); // доставим позже
  } else {
    if (g_rx_remote) g_rx_remote->onReceive(data, len);
  }
  return true;
}
bool Radio_setBandwidth(uint32_t) { return true; }
bool Radio_setSpreadingFactor(uint8_t) { return true; }
bool Radio_setCodingRate(uint8_t) { return true; }
bool Radio_setTxPower(int8_t) { return true; }
void Radio_forceRx() {}
bool Radio_getSNR(float& snr) { snr = 0.0f; return true; }
bool Radio_getEbN0(float& ebn0) { ebn0 = 0.0f; return true; }
bool Radio_isSynced() { return true; }
bool Radio_setFrequency(uint32_t) { return true; }
void Radio_onReceive(const uint8_t*, size_t) {}

static void processPendingAck() {
  if (!g_pending_ack.empty() && g_rx_local) {
    g_rx_local->onReceive(g_pending_ack.data(), g_pending_ack.size());
    g_pending_ack.clear();
  }
}

int main() {
  PipelineMetrics m_local{}, m_remote{};
  MessageBuffer buf_local;
  Fragmenter frag_local;
  NoEncryptor enc_local, enc_remote;
  TxPipeline tx(buf_local, frag_local, enc_local, m_local);
  RxPipeline rx_local(enc_local, m_local);    // принимает ACK
  RxPipeline rx_remote(enc_remote, m_remote); // принимает сообщение
  g_tx = &tx; g_rx_local = &rx_local; g_rx_remote = &rx_remote;

  // Минимальная конфигурация
  tdd::setParams(50, 50, 0);
  tx.setFecMode(TxPipeline::FecMode::FEC_OFF);
  rx_local.setFecMode(RxPipeline::FecMode::FEC_OFF);
  rx_remote.setFecMode(RxPipeline::FecMode::FEC_OFF);
  tx.setInterleaveDepth(1); rx_local.setInterleaveDepth(1); rx_remote.setInterleaveDepth(1);
  tx.setHeaderDup(false); rx_local.setHeaderDup(false); rx_remote.setHeaderDup(false);
  rx_remote.setAckAgg(0);
  constexpr size_t MSG_LEN = 5;
  tx.setPayloadLen(MSG_LEN);

  std::vector<uint8_t> received;
  bool ack_ok = false;

  rx_remote.setMessageCallback([&](uint32_t, const uint8_t* d, size_t n) {
    size_t copy = std::min(n, MSG_LEN);
    received.assign(d, d + copy);
  });
  rx_local.setAckCallback([&](uint32_t hi, uint32_t bm) {
    ack_ok = true;
    tx.notifyAck(hi, bm);
  });

  const uint8_t msg[MSG_LEN] = {1,2,3,4,5};
  buf_local.enqueue(msg, MSG_LEN, true);

  for (int i = 0; i < 20 && buf_local.hasPending(); ++i) {
    tx.loop();
    processPendingAck();
    delay(30);
  }

  bool size_ok = received.size() == MSG_LEN;
  bool data_ok = memcmp(received.data(), msg, std::min(received.size(), MSG_LEN)) == 0;
  bool ack_seen = m_local.ack_seen == 1;
  bool ok = ack_ok && ack_seen;

  Serial.printf("size_ok=%d data_ok=%d ack_ok=%d ack_seen=%d\n",
                size_ok, data_ok, ack_ok, ack_seen);
  Serial.println(ok ? "TEST OK" : "TEST FAIL");
  return ok ? 0 : 1;
}
#endif // !ARDUINO

