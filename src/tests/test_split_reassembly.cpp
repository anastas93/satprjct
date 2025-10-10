#include <cassert>
#include <vector>
#include <array>
#include "tx_module.h"
#include "rx_module.h"

class CollectRadio : public IRadio {
public:
  std::vector<std::vector<uint8_t>> sent;
  int16_t send(const uint8_t* data, size_t len) override {
    sent.emplace_back(data, data + len);
    return 0;
  }
  void setReceiveCallback(RxCallback) override {}
};

int main() {
  CollectRadio radio;
  TxModule tx(radio, std::array<size_t,4>{64,64,64,64}, PayloadMode::LARGE);
  tx.setSendPause(0);
  RxModule rx;
  std::vector<uint8_t> received;
  rx.setCallback([&](const uint8_t* d, size_t l) {
    received.assign(d, d + l);
  });

  const size_t data_len = 512;
  std::vector<uint8_t> data(data_len);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = static_cast<uint8_t>((i * 7) & 0xFF);
  }

  uint16_t id = tx.queue(data.data(), data.size());
  assert(id != 0);
  while (tx.loop()) {}
  for (auto& frame : radio.sent) {
    rx.onReceive(frame.data(), frame.size());
  }
  assert(received == data);
  return 0;
}
