#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include "tx_module.h"
#include "../libs/frame/frame_header.h" // заголовок кадра
#include "../libs/scrambler/scrambler.h" // дескремблирование кадра
#include "../libs/crypto/aes_ccm.h"      // расшифровка
#include "../libs/key_loader/key_loader.h" // загрузка ключа

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
  assert(radio.last.size() == FrameHeader::SIZE*2 + 22); // 9 байт префикс + 5 данных + тег
  // дескремблируем кадр перед проверкой
  scrambler::descramble(radio.last.data(), radio.last.size());
  FrameHeader hdr;
  assert(FrameHeader::decode(radio.last.data(), radio.last.size(), hdr));
  assert(hdr.msg_id==1 && hdr.payload_len==22);
  // дешифруем полезную нагрузку и проверяем наличие "HELLO"
  std::vector<uint8_t> payload(radio.last.begin()+FrameHeader::SIZE*2, radio.last.end());
  std::vector<uint8_t> tag(payload.end()-8, payload.end());
  std::vector<uint8_t> cipher(payload.begin(), payload.end()-8);
  std::array<uint8_t,16> key = KeyLoader::loadKey();
  auto nonce = KeyLoader::makeNonce(1, 0);
  std::vector<uint8_t> plain;
  bool ok = decrypt_ccm(key.data(), key.size(), nonce.data(), nonce.size(),
                        nullptr, 0, cipher.data(), cipher.size(),
                        tag.data(), tag.size(), plain);
  assert(ok);
  std::string text(plain.begin(), plain.end());
  assert(text.size() >= 5 && text.substr(text.size()-5) == "HELLO");
  std::cout << "OK" << std::endl;

  // Проверяем, что при отсутствии ACK остальные части пакета переносятся в архив
  MockRadio radioAck;
  TxModule txAck(radioAck, std::array<size_t,4>{10,10,10,10}, PayloadMode::SMALL);
  txAck.setAckEnabled(true);
  txAck.setAckRetryLimit(1);          // одна повторная отправка
  txAck.setAckTimeout(0);             // мгновенный тайм-аут для теста
  txAck.setSendPause(0);              // убираем ожидание между циклами
  std::vector<uint8_t> big(80, 'A');  // гарантированно несколько частей
  txAck.queue(big.data(), big.size());
  txAck.loop();                       // первая попытка
  txAck.loop();                       // срабатывает тайм-аут, попытка №2 станет доступна
  txAck.loop();                       // повторная отправка
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
  return 0;
}
