#include <cassert>
#include <iostream>
#include <vector>
#include <algorithm>
#include <array>
#include <thread>
#include <chrono>
#include <sstream>
#define private public
#define protected public
#include "tx_module.h"
#undef private
#undef protected
#include "rx_module.h"
#include "../libs/frame/frame_header.h"        // заголовок кадра
#include "../libs/scrambler/scrambler.h"       // дескремблирование кадра
#include "../libs/crypto/aes_ccm.h"            // расшифровка
#include "../libs/crypto/chacha20_poly1305.h"  // параметры нового AEAD
#include "../libs/key_loader/key_loader.h" // загрузка ключа
#include "../libs/protocol/ack_utils.h"   // маркер ACK
#include "../libs/simple_logger/simple_logger.h" // контроль статусов

// Простая заглушка радиоинтерфейса
class MockRadio : public IRadio {
public:
  std::vector<uint8_t> last;
  std::vector<std::vector<uint8_t>> history;
  int16_t send(const uint8_t* data, size_t len) override {
    last.assign(data, data + len);
    history.emplace_back(last);
    return 0;
  }
  void setReceiveCallback(RxCallback) override {}
};

namespace {

// Новые параметры пилотного маркера, применяемые без CRC-проверки
constexpr size_t PILOT_INTERVAL = 64;                                      // период вставки пилотов
constexpr std::array<uint8_t,7> PILOT_MARKER{{0x7E,'S','A','T','P',0xD6,0x9F}}; // сигнатура пилота
constexpr size_t PILOT_PREFIX_LEN = PILOT_MARKER.size() - 2;               // длина префикса без CRC

// Удаляем пилоты из полезной нагрузки, повторяя поведение RxModule
void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) return;                                          // защита от пустых буферов
  out.reserve(len);
  size_t count = 0;
  size_t i = 0;
  while (i < len) {
    if (count && count % PILOT_INTERVAL == 0) {
      size_t remaining = len - i;
      if (remaining >= PILOT_MARKER.size() &&
          std::equal(PILOT_MARKER.begin(), PILOT_MARKER.begin() + PILOT_PREFIX_LEN, data + i)) {
        uint16_t crc = static_cast<uint16_t>(data[i + PILOT_PREFIX_LEN]) |
                       (static_cast<uint16_t>(data[i + PILOT_PREFIX_LEN + 1]) << 8);
        if (crc == 0x9FD6 && FrameHeader::crc16(data + i, PILOT_PREFIX_LEN) == crc) {
          i += PILOT_MARKER.size();
          continue;                                                       // пропускаем всю сигнатуру
        }
      }
    }
    out.push_back(data[i]);
    ++count;
    ++i;
  }
}

// Декодируем кадр без CRC, учитывая возможное дублирование заголовка
bool decodeFrameNoCrc(const std::vector<uint8_t>& raw,
                      FrameHeader& hdr,
                      std::vector<uint8_t>& payload,
                      size_t& header_bytes) {
  if (raw.size() < FrameHeader::SIZE) return false;                       // слишком короткий буфер
  std::vector<uint8_t> descrambled(raw);
  scrambler::descramble(descrambled.data(), descrambled.size());
  FrameHeader primary;
  FrameHeader secondary;
  bool primary_ok = FrameHeader::decode(descrambled.data(), descrambled.size(), primary);
  bool secondary_ok = false;
  if (descrambled.size() >= FrameHeader::SIZE * 2) {
    secondary_ok = FrameHeader::decode(descrambled.data() + FrameHeader::SIZE,
                                       descrambled.size() - FrameHeader::SIZE,
                                       secondary);
  }
  if (!primary_ok && !secondary_ok) return false;                         // обе копии не прочитались
  hdr = primary_ok ? primary : secondary;                                 // по умолчанию берём первую
  header_bytes = FrameHeader::SIZE;
  if (!primary_ok && secondary_ok) {
    header_bytes = FrameHeader::SIZE * 2;                                 // первая копия недоступна
  }
  payload.clear();
  std::vector<uint8_t> cleaned;
  removePilots(descrambled.data() + header_bytes,
               descrambled.size() - header_bytes,
               cleaned);
  payload = std::move(cleaned);
  return true;
}

// Заглушка, которая всегда проваливает шифрование для проверки обработки ошибок
bool encryptAlwaysFail(const uint8_t* key, size_t key_len,
                       const uint8_t* nonce, size_t nonce_len,
                       const uint8_t* aad, size_t aad_len,
                       const uint8_t* input, size_t input_len,
                       std::vector<uint8_t>& output,
                       std::vector<uint8_t>& tag) {
  (void)key;
  (void)key_len;
  (void)nonce;
  (void)nonce_len;
  (void)aad;
  (void)aad_len;
  (void)input;
  (void)input_len;
  output.clear();
  tag.clear();
  return false;
}

} // namespace

// Проверка формирования кадра и вызова send
int main() {
  // Проверяем, что ACK определяется только бинарным маркером
  const uint8_t ack_marker = protocol::ack::MARKER;
  assert(protocol::ack::isAckPayload(&ack_marker, 1));
  const uint8_t ack_text[] = {'A', 'C', 'K'};
  assert(!protocol::ack::isAckPayload(ack_text, sizeof(ack_text)));

  MockRadio radio;
  TxModule tx(radio, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  tx.setAckResponseDelay(0);
  auto drainTx = [](TxModule& module) {
    size_t iterations = 0;
    while (iterations < 256) {
      if (!module.loop()) break;
      ++iterations;
    }
    return iterations;
  };
  const char* msg = "HELLO";
  uint16_t id = tx.queue(reinterpret_cast<const uint8_t*>(msg), 5);
  assert(id==1);                          // ожидаемый ID
  size_t initialLoops = drainTx(tx);      // формируем кадр по шагам
  assert(initialLoops == 1);
  FrameHeader hdr;
  std::vector<uint8_t> payload_clean;
  size_t header_bytes = 0;
  assert(decodeFrameNoCrc(radio.last, hdr, payload_clean, header_bytes));
  assert(hdr.msg_id==1);
  assert(hdr.frag_cnt == 1);
  // 5 байт полезной нагрузки + 12-символьный префикс split -> 17 байт открытого текста
  const size_t plain_len = 5 + 12;
  const size_t cipher_len = plain_len + crypto::chacha20poly1305::TAG_SIZE; // +16 байт тега
  const size_t conv_input = cipher_len + 1;                                 // добавочный байт хвоста
  const size_t expected_payload_len = conv_input * 2;                       // свёртка увеличивает размер вдвое
  const size_t expected_pilot_count = expected_payload_len / PILOT_INTERVAL;
  const size_t expected_frame_len = FrameHeader::SIZE + expected_payload_len +
                                    expected_pilot_count * PILOT_MARKER.size();
  assert(hdr.getPayloadLen() == expected_payload_len);
  assert(radio.last.size() == expected_frame_len);
  const size_t actual_pilot_bytes = (radio.last.size() - header_bytes) - payload_clean.size();
  assert(actual_pilot_bytes == expected_pilot_count * PILOT_MARKER.size());
  RxModule rx;
  std::vector<uint8_t> decoded;
  rx.setCallback([&](const uint8_t* data, size_t len) {
    decoded.assign(data, data + len);
  });
  rx.onReceive(radio.last.data(), radio.last.size());
  assert(decoded.size() >= 5);
  std::string text(decoded.begin(), decoded.end());
  assert(text.find("HELLO") != std::string::npos);
  std::cout << "OK" << std::endl;

  // Проверяем, что при отсутствии ACK остальные части пакета переносятся в архив
  MockRadio radioAck;
  TxModule txAck(radioAck, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txAck.setAckResponseDelay(0);
  txAck.setAckEnabled(true);
  txAck.setAckRetryLimit(1);          // одна повторная отправка
  txAck.setAckTimeout(1);             // минимальный положительный тайм-аут для ускоренных повторов
  txAck.setSendPause(0);              // убираем ожидание между циклами
  std::vector<uint8_t> big(80, 'A');  // гарантированно несколько частей
  txAck.queue(big.data(), big.size());
  size_t firstAttempt = drainTx(txAck); // первая попытка
  assert(firstAttempt > 0);
  auto firstLogs = SimpleLogger::getLast(1);
  assert(!firstLogs.empty());
  assert(firstLogs.front().find(" PROG") != std::string::npos);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  size_t retryAttempt = drainTx(txAck); // повторная отправка после тайм-аута
  assert(retryAttempt > 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  size_t archiveStep = drainTx(txAck); // тайм-аут, пакет уходит в архив вместе с остатком
  assert(archiveStep == 0);
  size_t sent_before = radioAck.history.size();
  size_t noMoreFragments = drainTx(txAck); // новых фрагментов не должно появиться
  assert(noMoreFragments == 0);
  assert(radioAck.history.size() == sent_before);

  // Проверяем обработку отказа подготовки фрагментов и повторную попытку без пропуска индексов
  MockRadio radioFail;
  TxModule txFail(radioFail, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txFail.setAckEnabled(false);
  txFail.setSendPause(0);
  TxModule::setEncryptOverrideForTests(&encryptAlwaysFail);
  const char failing_msg[] = "ERR";
  uint16_t failId = txFail.queue(reinterpret_cast<const uint8_t*>(failing_msg), sizeof(failing_msg));
  assert(failId == 1);
  bool firstFailAttempt = txFail.loop();
  assert(!firstFailAttempt);
  assert(radioFail.history.empty());
  assert(txFail.delayed_);
  assert(txFail.delayed_->fragments.empty());
  assert(txFail.delayed_->next_fragment == 0);
  bool secondFailAttempt = txFail.loop();
  assert(!secondFailAttempt);
  assert(radioFail.history.empty());
  assert(txFail.delayed_);
  assert(txFail.delayed_->next_fragment == 0);
  TxModule::resetEncryptOverrideForTests();
  bool recoveryAttempt = txFail.loop();
  assert(recoveryAttempt);
  assert(radioFail.history.size() == 1);
  FrameHeader failHdr;
  std::vector<uint8_t> failPayload;
  size_t failHeaderBytes = 0;
  assert(decodeFrameNoCrc(radioFail.last, failHdr, failPayload, failHeaderBytes));
  assert(failHdr.getFragIdx() == 0);

  // Проверяем завершение пакета при обнулении лимита ретраев до прихода ACK
  MockRadio radioNoAck;
  TxModule txNoAck(radioNoAck, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txNoAck.setAckResponseDelay(0);
  txNoAck.setAckEnabled(true);
  txNoAck.setAckRetryLimit(2);
  txNoAck.setAckTimeout(1000);
  txNoAck.setSendPause(0);
  const char first_msg[] = "DATA1";
  txNoAck.queue(reinterpret_cast<const uint8_t*>(first_msg), sizeof(first_msg));
  size_t firstNoAck = drainTx(txNoAck); // первая отправка требует ACK
  assert(firstNoAck > 0);
  assert(radioNoAck.history.size() == 1);
  txNoAck.setAckRetryLimit(0);        // отменяем ожидание подтверждения
  const char second_msg[] = "DATA2";
  txNoAck.queue(reinterpret_cast<const uint8_t*>(second_msg), sizeof(second_msg));
  size_t secondNoAck = drainTx(txNoAck); // второе сообщение должно уйти без ожидания тайм-аута
  assert(secondNoAck > 0);
  assert(radioNoAck.history.size() == 2);

  // Проверяем, что при ack_timeout=0 пакет не попадает в архив после обнуления лимита
  MockRadio radioZero;
  TxModule txZero(radioZero, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txZero.setAckResponseDelay(0);
  txZero.setAckEnabled(true);
  txZero.setAckRetryLimit(2);
  txZero.setAckTimeout(0);
  txZero.setSendPause(0);
  const char zero_msg[] = "ZERO";
  txZero.queue(reinterpret_cast<const uint8_t*>(zero_msg), sizeof(zero_msg));
  size_t zeroFirst = drainTx(txZero);  // сообщение отправлено и ждёт ACK
  assert(zeroFirst > 0);
  assert(radioZero.history.size() == 1);
  txZero.setAckRetryLimit(0);         // сбрасываем лимит — сообщение считается доставленным
  size_t zeroSecond = drainTx(txZero); // не должно появиться повторных отправок
  assert(zeroSecond == 0);
  auto zeroLog = SimpleLogger::getLast(1);
  assert(!zeroLog.empty());
  assert(zeroLog.front().find(" GO") != std::string::npos);
  assert(radioZero.history.size() == 1);
  txZero.setAckEnabled(false);        // если бы сообщение ушло в архив, оно бы восстановилось и отправилось
  size_t zeroFinal = drainTx(txZero);
  assert(zeroFinal == 0);
  assert(radioZero.history.size() == 1);

  // Проверяем, что ACK-сообщения уходят раньше обычных пакетов
  MockRadio radioPriority;
  TxModule txPriority(radioPriority, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txPriority.setAckResponseDelay(0);
  txPriority.setAckEnabled(true);
  txPriority.setAckRetryLimit(1);
  txPriority.setAckTimeout(0);
  txPriority.setSendPause(0);
  const char priorityMsg[] = "DATA";
  txPriority.queue(reinterpret_cast<const uint8_t*>(priorityMsg), sizeof(priorityMsg));
  const uint8_t ackPayloadMarker = protocol::ack::MARKER;
  txPriority.queue(&ackPayloadMarker, 1);      // отправляем ACK поверх очереди
  txPriority.loop();                           // первый кадр обязан быть ACK
  assert(radioPriority.history.size() == 1);
  auto ackFrame = radioPriority.history.front();
  FrameHeader ackHdr;
  std::vector<uint8_t> ackPayloadVec;
  size_t ackHeaderBytes = 0;
  assert(decodeFrameNoCrc(ackFrame, ackHdr, ackPayloadVec, ackHeaderBytes));
  assert(ackHeaderBytes == FrameHeader::SIZE);
  assert((ackHdr.getFlags() & (FrameHeader::FLAG_ENCRYPTED | FrameHeader::FLAG_CONV_ENCODED)) == 0);
  assert(ackHdr.getPayloadLen() == 1);
  assert(ackFrame.size() == FrameHeader::SIZE + ackHdr.getPayloadLen());
  assert(ackPayloadVec.size() == 1);
  assert(ackPayloadVec.front() == protocol::ack::MARKER);
  RxModule rxAck;
  std::vector<uint8_t> ackPlain;
  bool ackNotified = false;
  rxAck.setAckCallback([&]() { ackNotified = true; });
  rxAck.setCallback([&](const uint8_t* data, size_t len) {
    ackPlain.assign(data, data + len);
  });
  rxAck.onReceive(radioPriority.history.front().data(), radioPriority.history.front().size());
  assert(ackNotified);
  assert(ackPlain.size() == 1 && ackPlain.back() == protocol::ack::MARKER);
  txPriority.loop();
  assert(radioPriority.history.size() == 2);

  // Проверяем, что поздний ACK после тайм-аута не приводит к дополнительному повтору
  MockRadio radioLateAck;
  TxModule txLateAck(radioLateAck, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txLateAck.setAckResponseDelay(0);
  txLateAck.setAckEnabled(true);
  txLateAck.setAckRetryLimit(1);
  txLateAck.setAckTimeout(5);
  txLateAck.setSendPause(0);
  const char late_msg[] = "LATE";
  txLateAck.queue(reinterpret_cast<const uint8_t*>(late_msg), sizeof(late_msg));
  txLateAck.loop();
  assert(radioLateAck.history.size() == 1);
  assert(txLateAck.inflight_.has_value());
  txLateAck.waiting_ack_ = false; // имитируем срабатывание тайм-аута, сбросив ожидание ACK
  txLateAck.onAckReceived();      // ACK пришёл до фактического повтора
  assert(!txLateAck.inflight_.has_value());
  bool lateAckLoop = txLateAck.loop();
  assert(!lateAckLoop);
  assert(radioLateAck.history.size() == 1);

  // Проверяем, что после перевода ack_timeout=0 очередь продолжает двигаться без ожидания ACK
  MockRadio radioFlow;
  TxModule txFlow(radioFlow, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txFlow.setAckResponseDelay(0);
  txFlow.setAckEnabled(true);
  txFlow.setAckRetryLimit(3);
  txFlow.setAckTimeout(10);
  txFlow.setSendPause(0);
  const char flowFirst[] = "FLOW1";
  const char flowSecond[] = "FLOW2";
  txFlow.queue(reinterpret_cast<const uint8_t*>(flowFirst), sizeof(flowFirst));
  txFlow.queue(reinterpret_cast<const uint8_t*>(flowSecond), sizeof(flowSecond));
  bool firstFlowSend = txFlow.loop();
  assert(firstFlowSend);
  assert(radioFlow.history.size() == 1);
  txFlow.setAckTimeout(0);            // отключаем ожидание подтверждения на лету
  bool secondFlowSend = txFlow.loop();
  assert(secondFlowSend);             // второй пакет не должен зависнуть из-за ожидания ACK
  assert(radioFlow.history.size() == 2);
  bool noMore = txFlow.loop();
  assert(!noMore);
  assert(radioFlow.history.size() == 2);

  // Проверяем, что увеличение ack_timeout во время ожидания не вызывает немедленный повтор
  MockRadio radioGrowTimeout;
  TxModule txGrowTimeout(radioGrowTimeout, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txGrowTimeout.setAckResponseDelay(0);
  txGrowTimeout.setAckEnabled(true);
  txGrowTimeout.setAckRetryLimit(1);
  txGrowTimeout.setAckTimeout(10);
  txGrowTimeout.setSendPause(0);
  const char growMsg[] = "GROW";
  txGrowTimeout.queue(reinterpret_cast<const uint8_t*>(growMsg), sizeof(growMsg));
  bool firstGrowSend = txGrowTimeout.loop();
  assert(firstGrowSend);
  assert(radioGrowTimeout.history.size() == 1);
  assert(txGrowTimeout.waiting_ack_);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  txGrowTimeout.setAckTimeout(40);
  bool noImmediateRepeat = txGrowTimeout.loop();
  assert(!noImmediateRepeat);
  assert(radioGrowTimeout.history.size() == 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(40));
  bool retryAfterGrow = txGrowTimeout.loop();
  assert(retryAfterGrow);
  assert(radioGrowTimeout.history.size() == 2);

  // Проверяем, что при ack_timeout < pause повтор и следующий пакет не ждут паузу
  MockRadio radioPauseAck;
  TxModule txPauseAck(radioPauseAck, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txPauseAck.setAckResponseDelay(0);
  txPauseAck.setAckEnabled(true);
  txPauseAck.setAckRetryLimit(1);
  txPauseAck.setAckTimeout(10);
  txPauseAck.setSendPause(50);
  const char retryMsg[] = "RETRY";
  const char nextMsg[] = "NEXT";
  txPauseAck.queue(reinterpret_cast<const uint8_t*>(retryMsg), sizeof(retryMsg));
  txPauseAck.queue(reinterpret_cast<const uint8_t*>(nextMsg), sizeof(nextMsg));
  bool firstPauseSend = txPauseAck.loop();
  assert(firstPauseSend);
  assert(radioPauseAck.history.size() == 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(15)); // ждём меньше паузы, но дольше тайм-аута ACK
  bool retryPauseSend = txPauseAck.loop();
  assert(retryPauseSend);
  assert(radioPauseAck.history.size() == 2);
  txPauseAck.onAckReceived();
  bool nextPauseSend = txPauseAck.loop();
  assert(nextPauseSend);
  assert(radioPauseAck.history.size() == 3);
  return 0;
}
