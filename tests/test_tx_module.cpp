#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include "tx_module.h"
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
  RxModule rxAck;
  std::vector<uint8_t> ackPlain;
  rxAck.setCallback([&](const uint8_t* data, size_t len) {
    ackPlain.assign(data, data + len);
  });
  rxAck.onReceive(radioPriority.history.front().data(), radioPriority.history.front().size());
  assert(!ackPlain.empty() && ackPlain.back() == protocol::ack::MARKER);
  txPriority.loop();
  assert(radioPriority.history.size() == 2);

  // Проверяем, что при ack_timeout=0 модуль ожидает подтверждение до его получения
  MockRadio radioWait;
  TxModule txWait(radioWait, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txWait.setAckEnabled(true);
  txWait.setAckRetryLimit(3);
  txWait.setAckTimeout(0);            // бесконечное ожидание ACK
  txWait.setSendPause(0);
  const char wait_msg[] = "WAIT";
  txWait.queue(reinterpret_cast<const uint8_t*>(wait_msg), sizeof(wait_msg));
  bool firstSend = txWait.loop();
  assert(firstSend);
  assert(radioWait.history.size() == 1);
  bool secondSend = txWait.loop();
  assert(!secondSend);                // повтор не должен стартовать без подтверждения
  assert(radioWait.history.size() == 1);
  txWait.onAckReceived();             // имитируем приход подтверждения
  bool afterAck = txWait.loop();
  assert(!afterAck);
  assert(radioWait.history.size() == 1);
  return 0;
}
