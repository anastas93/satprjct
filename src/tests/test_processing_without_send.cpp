#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include "tx_module.h"
#include "rx_module.h"
#include "libs/received_buffer/received_buffer.h"

// Радио, которое сохраняет кадры, но не передаёт их автоматически
class CollectRadio : public IRadio {
public:
  std::vector<std::vector<uint8_t>> sent; // накопленные кадры
  int16_t send(const uint8_t* data, size_t len) override {
    sent.emplace_back(data, data + len);  // сохраняем кадр
    return 0;
  }
  void setReceiveCallback(RxCallback) override {}
};

int main() {
  CollectRadio radio;
  TxModule tx(radio, std::array<size_t,4>{64,64,64,64}, PayloadMode::SMALL);
  tx.setSendPause(0); // отключаем паузу для последовательной отправки
  RxModule rx;
  std::vector<uint8_t> received;
  rx.setCallback([&](const uint8_t* d, size_t l) { received.assign(d, d + l); });

  // Формируем тестовое сообщение из 20 байт
  std::vector<uint8_t> data(20);
  for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i);

  tx.queue(data.data(), data.size()); // ставим сообщение в очередь
  // Вызываем loop() до тех пор, пока не отправятся все части сообщения
  while (true) {
    size_t before = radio.sent.size();
    tx.loop();
    if (radio.sent.size() == before) break; // новых кадров нет
  }

  // Передаём накопленные кадры в приёмник вручную
  for (auto& frame : radio.sent) {
    rx.onReceive(frame.data(), frame.size());
  }

  // Проверяем, что собранное сообщение совпадает с исходным
  assert(received == data);
  ReceivedBuffer rawBuffer;
  rx.setBuffer(&rawBuffer); // подключаем буфер, чтобы проверить сохранение сырых пакетов

  std::vector<uint8_t> shortData{0xAB, 0xCD, 0xEF};
  rx.onReceive(shortData.data(), shortData.size());
  assert(received == shortData); // короткое сообщение должно напрямую попасть в колбэк

  ReceivedBuffer::Item rawItem{};
  assert(rawBuffer.popRaw(rawItem));
  assert(rawItem.id == 0);          // для сырых пакетов используется специальный идентификатор
  assert(rawItem.part == 0);        // первый пакет получает нулевой порядковый номер
  assert(rawItem.data == shortData);

  std::cout << "OK" << std::endl;
  return 0;
}
