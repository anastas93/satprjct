#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include "tx_module.h"
#include "rx_module.h"
#include "libs/received_buffer/received_buffer.h"

// Петлевой радиоинтерфейс для имитации передачи/приёма
class LoopbackRadio : public IRadio {
public:
  RxCallback cb;                                  // сохранённый колбэк приёма
  void send(const uint8_t* data, size_t len) override {
    if (cb) cb(data, len);                        // сразу передаём пакет в RxModule
  }
  void setReceiveCallback(RxCallback callback) override { cb = callback; }
};

int main() {
  LoopbackRadio radio;
  TxModule tx(radio, std::array<size_t,4>{8,8,8,8}, PayloadMode::SMALL);
  RxModule rx;
  ReceivedBuffer buffer;                          // внешний буфер для RxModule
  std::vector<uint8_t> received;                  // место для данных из колбэка

  rx.setCallback([&](const uint8_t* data, size_t len) {
    received.assign(data, data + len);            // запоминаем принятую полезную нагрузку
  });
  rx.setBuffer(&buffer);                          // подключаем буфер принятых сообщений
  radio.setReceiveCallback([&](const uint8_t* data, size_t len) {
    rx.onReceive(data, len);                      // пересылаем кадр в RxModule
  });

  const std::vector<uint8_t> payload = {'T','E','S','T'}; // тестовое сообщение
  uint16_t id = tx.queue(payload.data(), payload.size());
  assert(id == 1);                                 // ожидаем первый идентификатор
  bool sent = tx.loop();
  assert(sent);                                    // кадр должен быть отправлен
  assert(received == payload);                     // колбэк получает исходное сообщение

  auto names = buffer.list(5);                     // совместимость со списком имён
  assert(names.size() == 1);
  assert(names[0] == "GO-00001");                 // ожидаем имя готового сообщения

  auto snapshot = buffer.snapshot(5);              // проверяем наличие данных в снимке
  assert(snapshot.size() == 1);
  assert(snapshot[0].kind == ReceivedBuffer::Kind::Ready);
  assert(snapshot[0].item.id == id);
  assert(snapshot[0].item.name == "GO-00001");
  assert(snapshot[0].item.data == payload);        // полезная нагрузка сохранена полностью

  ReceivedBuffer::Item item;
  assert(buffer.popReady(item));                   // извлекаем готовые данные
  assert(item.id == id);
  assert(item.name == "GO-00001");                // имя сохраняется внутри элемента
  assert(item.data == payload);                    // содержимое совпадает с исходным
  std::cout << "OK" << std::endl;
  return 0;
}
