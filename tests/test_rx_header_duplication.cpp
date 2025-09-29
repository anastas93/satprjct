#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
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

// RAII-класс для временного перенаправления std::cout в буфер
struct CoutCapture {
  std::ostringstream oss;
  std::streambuf* prev = nullptr;
  CoutCapture() {
    prev = std::cout.rdbuf(oss.rdbuf());
  }
  ~CoutCapture() {
    std::cout.rdbuf(prev);
  }
};

int main() {
  CaptureRadio radio;
  TxModule tx(radio, std::array<size_t,4>{256,256,256,256}, PayloadMode::SMALL);
  tx.setSendPause(0);

  std::vector<uint8_t> payload(FrameHeader::SIZE + 20);
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<uint8_t>((i * 3) & 0xFF);
  }
  payload[0] = 0x00; // гарантируем отличие от байта версии заголовка

  uint16_t id = tx.queue(payload.data(), payload.size());
  assert(id != 0);
  while (tx.loop()) {
    // крутим очередь до отправки кадра
  }
  assert(!radio.history.empty());

  // Проверяем, что полезная нагрузка действительно отличается от заголовка
  auto frame_copy = radio.history.front();
  scrambler::descramble(frame_copy.data(), frame_copy.size());
  assert(frame_copy.size() >= FrameHeader::SIZE * 2);
  bool identical = std::equal(frame_copy.begin(), frame_copy.begin() + FrameHeader::SIZE,
                              frame_copy.begin() + FrameHeader::SIZE);
  assert(!identical); // полезная нагрузка не должна копировать заголовок

  RxModule rx;
  std::vector<uint8_t> decoded;
  rx.setCallback([&](const uint8_t* data, size_t len) {
    decoded.assign(data, data + len);
  });

  for (const auto& frame : radio.history) {
    CoutCapture capture; // перехватываем все предупреждения на время обработки кадра
    rx.onReceive(frame.data(), frame.size());
    std::string logs = capture.oss.str();
    assert(logs.find("RxModule: обнаружено расхождение копий заголовка") == std::string::npos);
  }

  assert(decoded == payload);

  std::cout << "OK" << std::endl;
  return 0;
}
