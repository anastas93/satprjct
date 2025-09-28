#include <cassert>
#include <iostream>
#include <vector>
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
#include "../libs/frame/frame_header.h" // заголовок кадра
#include "../libs/scrambler/scrambler.h" // дескремблирование кадра
#include "../libs/crypto/aes_ccm.h"      // расшифровка
#include "../libs/key_loader/key_loader.h" // загрузка ключа
#include "../libs/protocol/ack_utils.h"   // маркер ACK

// Простая заглушка радиоинтерфейса
class MockRadio : public IRadio {
public:
  std::vector<uint8_t> last;
  std::vector<std::vector<uint8_t>> history;
  void send(const uint8_t* data, size_t len) override {
    last.assign(data, data + len);
    history.emplace_back(last);
  }
  void setReceiveCallback(RxCallback) override {}
};

// Проверка формирования кадра и вызова send
int main() {
  MockRadio radio;
  TxModule tx(radio, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  tx.setAckResponseDelay(0);
  const char* msg = "HELLO";
  uint32_t id = tx.queue(reinterpret_cast<const uint8_t*>(msg), 5);
  assert(id==1);                          // ожидаемый ID
  tx.loop();                              // формируем кадр
  const size_t minFrame = FrameHeader::SIZE*2 + 22;
  assert(radio.last.size() >= minFrame);
  assert(radio.last.size() <= 245);
  auto frameCopy = radio.last;
  scrambler::descramble(frameCopy.data(), frameCopy.size());
  FrameHeader hdr;
  assert(FrameHeader::decode(frameCopy.data(), frameCopy.size(), hdr));
  assert(hdr.msg_id==1);
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
  txAck.loop();                       // первая попытка
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  txAck.loop();                       // срабатывает тайм-аут, попытка №2 станет доступна
  txAck.loop();                       // повторная отправка
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  txAck.loop();                       // тайм-аут, пакет уходит в архив вместе с остатком
  size_t sent_before = radioAck.history.size();
  txAck.loop();                       // новых фрагментов не должно появиться
  assert(radioAck.history.size() == sent_before);

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
  txNoAck.loop();                     // первая отправка требует ACK
  assert(radioNoAck.history.size() == 1);
  txNoAck.setAckRetryLimit(0);        // отменяем ожидание подтверждения
  const char second_msg[] = "DATA2";
  txNoAck.queue(reinterpret_cast<const uint8_t*>(second_msg), sizeof(second_msg));
  txNoAck.loop();                     // второе сообщение должно уйти без ожидания тайм-аута
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
  txZero.loop();                      // сообщение отправлено и ждёт ACK
  assert(radioZero.history.size() == 1);
  txZero.setAckRetryLimit(0);         // сбрасываем лимит — сообщение считается доставленным
  txZero.loop();                      // не должно появиться повторных отправок
  assert(radioZero.history.size() == 1);
  txZero.setAckEnabled(false);        // если бы сообщение ушло в архив, оно бы восстановилось и отправилось
  txZero.loop();
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
  const uint8_t ackPayload = protocol::ack::MARKER;
  txPriority.queue(&ackPayload, 1);            // отправляем ACK поверх очереди
  txPriority.loop();                           // первый кадр обязан быть ACK
  assert(radioPriority.history.size() == 1);
  auto ackFrame = radioPriority.history.front();
  auto ackDecoded = ackFrame;
  scrambler::descramble(ackDecoded.data(), ackDecoded.size());
  FrameHeader ackHdr;
  assert(FrameHeader::decode(ackDecoded.data(), ackDecoded.size(), ackHdr));
  assert((ackHdr.flags & (FrameHeader::FLAG_ENCRYPTED | FrameHeader::FLAG_CONV_ENCODED)) == 0);
  assert(ackHdr.payload_len == 1);
  assert(ackFrame.size() == FrameHeader::SIZE * 2 + ackHdr.payload_len);
  const uint8_t* ackPayloadPtr = ackDecoded.data() + FrameHeader::SIZE * 2;
  assert(*ackPayloadPtr == protocol::ack::MARKER);
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
