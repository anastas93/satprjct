#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include "tx_module.h"
#include "rx_module.h"
#include "libs/packetizer/packet_gatherer.h"

// Петлевой радиоинтерфейс, сразу передающий данные в приёмник
class LoopbackRadio : public IRadio {
public:
  RxCallback cb;                                  // сохранённый колбэк
  std::vector<std::vector<uint8_t>> sent;         // список отправленных кадров
  void send(const uint8_t* data, size_t len) override {
    sent.emplace_back(data, data + len);
    if (cb) cb(data, len);
  }
  void setReceiveCallback(RxCallback c) override { cb = c; }
};

// Проверка передачи сообщения указанного размера через Tx и Rx
static void run_message(size_t size) {
  MessageBuffer buf(64);                          // буфер сообщений
  LoopbackRadio radio;
  TxModule tx(radio, buf, PayloadMode::SMALL);
  RxModule rx;
  PacketGatherer gatherer(PayloadMode::SMALL);
  std::vector<uint8_t> received;

  rx.setCallback([&](const uint8_t* d, size_t l) {
    gatherer.add(d, l);
    if (gatherer.isComplete()) received = gatherer.get();
  });
  radio.setReceiveCallback([&](const uint8_t* d, size_t l) { rx.onReceive(d, l); });

  std::vector<uint8_t> data(size);
  for (size_t i = 0; i < size; ++i) data[i] = static_cast<uint8_t>(i);
  gatherer.reset();
  tx.queue(data.data(), data.size());
  for (int i = 0; i < 1000 && !gatherer.isComplete(); ++i) tx.loop(); // ограничиваем цикл
  assert(gatherer.isComplete());
  assert(received == data);                      // проверяем целостность
}

int main() {
  run_message(10);
  run_message(100);
  run_message(500);

  // Проверка паузы между отправками
  MessageBuffer buf(4);
  LoopbackRadio radio;
  TxModule tx(radio, buf, PayloadMode::SMALL);
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
