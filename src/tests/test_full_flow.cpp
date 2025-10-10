#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <array>
#include "tx_module.h"
#include "rx_module.h"

// Петлевой радиоинтерфейс, сразу передающий данные в приёмник
class LoopbackRadio : public IRadio {
public:
  RxCallback cb;                                  // сохранённый колбэк
  std::vector<std::vector<uint8_t>> sent;         // список отправленных кадров
  int16_t send(const uint8_t* data, size_t len) override {
    sent.emplace_back(data, data + len);
    if (cb) cb(data, len);
    return 0;
  }
  void setReceiveCallback(RxCallback c) override { cb = c; }
};

// Проверка передачи сообщения указанного размера через Tx и Rx
static void run_message(size_t size) {
  LoopbackRadio radio;
  TxModule tx(radio, std::array<size_t,4>{64,64,64,64}, PayloadMode::SMALL);
  tx.setSendPause(0);                                   // убираем задержку, чтобы тест шёл без ожиданий
  RxModule rx;
  std::vector<uint8_t> received_total;

  rx.setCallback([&](const uint8_t* d, size_t l) {
    received_total.insert(received_total.end(), d, d + l); // аккумулируем все куски сообщения
  });
  radio.setReceiveCallback([&](const uint8_t* d, size_t l) { rx.onReceive(d, l); });

  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(i);
  tx.queue(data.data(), data.size());
  for (int i = 0; i < 100 && received_total.size() < data.size(); ++i) {
    tx.loop();
  }
  assert(received_total == data);                      // проверяем целостность
}

int main() {
  run_message(10);
  run_message(20);
  run_message(400); // проверяем сборку длинного сообщения (более одного блока)

  // Проверка паузы между отправками
  LoopbackRadio radio;
  TxModule tx(radio, std::array<size_t,4>{4,4,4,4}, PayloadMode::SMALL);
  tx.setSendPause(50);                           // пауза 50 мс
  std::vector<uint8_t> pkt(10, 1);
  tx.queue(pkt.data(), pkt.size());
  tx.queue(pkt.data(), pkt.size());
  radio.setReceiveCallback([](const uint8_t*, size_t) {});
  tx.loop();                                      // отправка первого пакета
  tx.loop();                                      // второй должен подождать
  assert(radio.sent.size() == 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  tx.loop();                                      // теперь второй уйдёт
  assert(radio.sent.size() == 2);
  std::cout << "OK" << std::endl;
  return 0;
}
