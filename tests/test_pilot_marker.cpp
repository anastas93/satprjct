#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>
#include "tx_module.h"
#include "rx_module.h"
#include "../libs/frame/frame_header.h" // структура заголовка кадра
#include "../libs/scrambler/scrambler.h" // операции скремблирования

// Заглушка радиоинтерфейса с накоплением последнего отправленного кадра
class CaptureRadio : public IRadio {
public:
  std::vector<uint8_t> last;
  std::vector<std::vector<uint8_t>> history;
  void send(const uint8_t* data, size_t len) override {
    last.assign(data, data + len);
    history.emplace_back(last);
  }
  void setReceiveCallback(RxCallback) override {}
};

int main() {
  CaptureRadio radio;
  TxModule tx(radio, std::array<size_t,4>{256,256,256,256}, PayloadMode::SMALL);
  tx.setSendPause(0);

  // Подготавливаем полезную нагрузку, которая содержит «старый» двухбайтовый маркер 0x55 0x2D
  // на позиции, совпадающей с границей вставки пилота.
  std::vector<uint8_t> payload(80);
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<uint8_t>(i & 0xFF);
  }
  payload[64] = 0x55;
  payload[65] = 0x2D;

  uint32_t id = tx.queue(payload.data(), payload.size());
  assert(id != 0);
  while (tx.loop()) {
    // крутим до опустошения очереди
  }
  assert(!radio.history.empty());

  // Проверяем, что в кадре присутствует новый сигнатурный пилотный маркер.
  const std::array<uint8_t,7> marker{{0x7E, 'S', 'A', 'T', 'P', 0xD6, 0x9F}};
  bool marker_found = false;
  for (const auto& frame : radio.history) {
    auto frame_copy = frame;
    scrambler::descramble(frame_copy.data(), frame_copy.size());
    const uint8_t* payload_ptr = frame_copy.data() + FrameHeader::SIZE * 2;
    size_t payload_len = frame_copy.size() - FrameHeader::SIZE * 2;
    for (size_t i = 0; i + marker.size() <= payload_len; ++i) {
      if (std::equal(marker.begin(), marker.end(), payload_ptr + i)) {
        marker_found = true;
        break;
      }
    }
    if (marker_found) break;
  }
  assert(marker_found);

  // Убеждаемся, что RxModule не теряет данные, даже если в них остались старые сигнатуры.
  RxModule rx;
  std::vector<uint8_t> decoded;
  rx.setCallback([&](const uint8_t* data, size_t len) {
    decoded.assign(data, data + len);
  });
  for (const auto& frame : radio.history) {
    rx.onReceive(frame.data(), frame.size());
  }
  assert(decoded == payload);

  std::cout << "OK" << std::endl;
  return 0;
}
