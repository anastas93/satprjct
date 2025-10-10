#include <cassert>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include <iostream>
#include <utility>
#include <sstream>
#define private public
#define protected public
#include "tx_module.h"
#undef private
#undef protected
#include "rx_module.h"
#include "../libs/protocol/ack_utils.h"

// Дуплексный радиоканал, пересылающий кадры напрямую парному экземпляру
class DuplexRadio : public IRadio {
public:
  DuplexRadio* peer = nullptr;                                 // ссылка на парный радиообъект
  RxCallback rx_cb;                                            // обработчик входящих кадров
  std::vector<std::vector<uint8_t>> history;                   // накопленные отправленные кадры

  int16_t send(const uint8_t* data, size_t len) override {
    history.emplace_back(data, data + len);                    // сохраняем кадр для проверки
    if (peer && peer->rx_cb) {
      peer->rx_cb(data, len);                                  // передаём кадр в приёмник напарника
    }
    return 0;
  }

  void setReceiveCallback(RxCallback cb) override {
    rx_cb = std::move(cb);                                     // сохраняем обработчик
  }
};

int main() {
  DuplexRadio radio_a;
  DuplexRadio radio_b;
  radio_a.peer = &radio_b;                                     // связываем радио в кольцо
  radio_b.peer = &radio_a;

  TxModule tx_a(radio_a, std::array<size_t,4>{32,32,32,32}, PayloadMode::SMALL);
  TxModule tx_b(radio_b, std::array<size_t,4>{32,32,32,32}, PayloadMode::SMALL);
  tx_a.setSendPause(0);                                        // убираем глобальную паузу
  tx_b.setSendPause(0);
  tx_a.setAckEnabled(true);
  tx_a.setAckRetryLimit(2);                                    // допускаем две попытки
  tx_a.setAckTimeout(5);                                       // быстрый тайм-аут для теста
  tx_a.setAckResponseDelay(0);
  tx_b.setAckEnabled(true);
  tx_b.setAckRetryLimit(0);                                    // ACK-ответы не требуют подтверждений
  tx_b.setAckTimeout(0);
  tx_b.setAckResponseDelay(0);

  RxModule rx_a;
  RxModule rx_b;

  size_t ack_count = 0;                                        // число полученных подтверждений
  radio_a.setReceiveCallback([&](const uint8_t* data, size_t len) {
    rx_a.onReceive(data, len);                                 // передаём кадры в приёмник узла A
  });
  radio_b.setReceiveCallback([&](const uint8_t* data, size_t len) {
    rx_b.onReceive(data, len);                                 // передаём кадры в приёмник узла B
  });

  rx_a.setCallback([](const uint8_t*, size_t) {});             // полезная нагрузка узлу A не требуется
  rx_a.setAckCallback([&]() {
    ++ack_count;                                                // учитываем ACK
    tx_a.onAckReceived();                                       // уведомляем передатчик
  });

  bool drop_first_ack = true;                                  // флаг подавления первого подтверждения
  size_t received_payload_bytes = 0;                            // накопленный полезный трафик на узле B
  rx_b.setCallback([&](const uint8_t* data, size_t len) {
    (void)data;                                                // подавляем предупреждение о неиспользуемом указателе
    received_payload_bytes += len;                             // считаем объём принятой нагрузки
    if (drop_first_ack) {                                      // первый ответ нарочно без ACK
      drop_first_ack = false;
      return;
    }
    const uint8_t marker = protocol::ack::MARKER;
    tx_b.queue(&marker, 1);                                    // ставим подтверждение в очередь отправки
  });

  const char first_payload[] = "ACK-RETRY";                    // сообщение, требующее подтверждения
  tx_a.queue(reinterpret_cast<const uint8_t*>(first_payload), sizeof(first_payload));
  bool first_send = tx_a.loop();
  assert(first_send);                                          // первая отправка состоялась
  assert(radio_a.history.size() == 1);                         // фиксация первичной передачи
  assert(tx_a.waiting_ack_);                                   // передатчик ждёт подтверждение

  std::this_thread::sleep_for(std::chrono::milliseconds(10));  // ждём истечения тайм-аута
  bool retry_send = tx_a.loop();                               // инициируем повтор
  assert(retry_send);
  assert(radio_a.history.size() == 2);                         // подтверждаем наличие повтора
  assert(tx_a.waiting_ack_);                                   // по-прежнему ждём ACK

  bool ack_tx_attempt = false;
  for (int i = 0; i < 5; ++i) {                                // несколько шагов для отправки ACK
    if (tx_b.loop()) {
      ack_tx_attempt = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(ack_tx_attempt);                                      // ACK должен уйти в эфир
  std::this_thread::sleep_for(std::chrono::milliseconds(1));   // даём времени обработать подтверждение
  assert(ack_count == 1);                                      // подтверждение принято один раз
  assert(!tx_a.waiting_ack_);                                  // ожидание ACK завершено
  assert(!tx_a.inflight_.has_value());                         // активный пакет снят

  const char second_payload[] = "ACK-OK";                      // проверяем цикл без потерь ACK
  tx_a.queue(reinterpret_cast<const uint8_t*>(second_payload), sizeof(second_payload));
  bool second_send = tx_a.loop();
  assert(second_send);
  assert(radio_a.history.size() == 3);                         // третья передача (вторая сессия)
  bool ack_sent = tx_b.loop();
  if (!ack_sent) {
    // допускаем, что ack окажется в архиве при отсутствии готового окна; пробуем ещё раз
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    ack_sent = tx_b.loop();
  }
  assert(ack_sent);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(ack_count == 2);                                      // второе подтверждение доставлено
  assert(!tx_a.waiting_ack_);                                  // ожидание снова снято

  assert(received_payload_bytes > 0);                          // данные достигли получателя
  assert(!drop_first_ack);                                     // флаг сброшен после имитации потери

  std::cout << "OK" << std::endl;
  return 0;
}
