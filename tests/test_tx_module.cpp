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
  void send(const uint8_t* data, size_t len) override { last.assign(data, data+len); }
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
  return 0;
}
