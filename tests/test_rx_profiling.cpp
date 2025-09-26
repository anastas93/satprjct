#include <cassert>
#include <vector>
#include <array>
#include "tx_module.h"
#include "rx_module.h"

// Петлевой радиоинтерфейс для теста профилирования
class LoopbackRadio : public IRadio {
public:
  RxCallback cb;                                         // сохранённый обработчик
  void send(const uint8_t* data, size_t len) override {  // немедленная передача в RX
    if (cb) cb(data, len);
  }
  void setReceiveCallback(RxCallback c) override { cb = c; }
};

int main() {
  LoopbackRadio radio;
  TxModule tx(radio, std::array<size_t,4>{64,64,64,64}, PayloadMode::SMALL);
  tx.setSendPause(0);

  RxModule rx;
  rx.enableProfiling(true);                              // включаем диагностику пути

  std::vector<uint8_t> received;
  rx.setCallback([&](const uint8_t* data, size_t len) {
    received.assign(data, data + len);
  });
  radio.setReceiveCallback([&](const uint8_t* data, size_t len) { rx.onReceive(data, len); });

  std::vector<uint8_t> payload{1,2,3,4,5,6,7,8};
  tx.queue(payload.data(), payload.size());
  for (int i = 0; i < 100 && received.size() < payload.size(); ++i) {
    tx.loop();
  }
  assert(received == payload);                           // сообщение доставлено полностью

  auto ok_profile = rx.lastProfiling();
  assert(ok_profile.valid);
  assert(!ok_profile.dropped);
  assert(ok_profile.raw_len > 0);
  assert(ok_profile.decoded_len >= payload.size());
  assert(ok_profile.total.count() > 0);

  std::array<uint8_t, 2> broken_raw{0x01, 0x02};
  rx.onReceive(broken_raw.data(), broken_raw.size());     // намеренно короткий кадр
  auto drop_profile = rx.lastProfiling();
  assert(drop_profile.valid);
  assert(drop_profile.dropped);
  assert(drop_profile.raw_len == broken_raw.size());
  assert(!drop_profile.drop_stage.empty());

  return 0;
}

