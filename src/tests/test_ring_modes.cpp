#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include "tx_module.h"
#include "rx_module.h"
#include "libs/packetizer/packet_splitter.h"

// Радиоинтерфейс с прямой петлёй на приёмник
class LoopbackRadio : public IRadio {
public:
  RxCallback cb;                                        // сохранённый обработчик приёма
  std::vector<std::vector<uint8_t>> frames;             // накопленные кадры для статистики

  int16_t send(const uint8_t* data, size_t len) override {
    frames.emplace_back(data, data + len);              // фиксируем отправленный кадр
    if (cb) cb(data, len);                              // немедленно перекидываем пакет в приёмник
    return 0;
  }

  void setReceiveCallback(RxCallback c) override { cb = std::move(c); }
};

struct ModeCase {
  PayloadMode mode;                                     // проверяемый режим размера полезной нагрузки
  const char* name;                                     // строковое обозначение для сообщений об ошибках
};

// Формирование тестового сообщения с псевдослучайным наполнением
static std::vector<uint8_t> makePayload(size_t size, uint8_t seed) {
  std::vector<uint8_t> payload(size);
  for (size_t i = 0; i < size; ++i) {
    payload[i] = static_cast<uint8_t>((i * 31 + seed) & 0xFF); // простая линейная формула даёт повторяемый узор
  }
  return payload;
}

// Проверка полного цикла передачи-приёма для указанного режима
static void runMode(const ModeCase& mode_case) {
  LoopbackRadio radio;
  TxModule tx(radio, std::array<size_t,4>{256,256,256,256}, mode_case.mode);
  tx.setSendPause(0);                                   // убираем искусственную задержку между кадрами
  RxModule rx;
  std::vector<std::vector<uint8_t>> delivered;          // список собранных сообщений

  rx.setCallback([&](const uint8_t* data, size_t len) {
    delivered.emplace_back(data, data + len);           // сохраняем полученный блок для проверки
  });
  radio.setReceiveCallback([&](const uint8_t* data, size_t len) {
    rx.onReceive(data, len);                            // имитируем прохождение по радиоэфиру
  });

  PacketSplitter splitter(mode_case.mode);
  const size_t chunk = splitter.payloadSize();
  std::vector<size_t> lengths = {
      1,
      chunk / 2 + 3,
      chunk,
      chunk + 7,
      chunk * 2,
      chunk * 2 + 19,
  };

  uint8_t seed = 0;
  for (size_t len : lengths) {
    auto payload = makePayload(len, static_cast<uint8_t>(seed++));
    const size_t delivered_before = delivered.size();
    const uint16_t id = tx.queue(payload.data(), payload.size());
    assert(id != 0);                                    // очередь должна принять сообщение

    bool done = false;
    for (int iter = 0; iter < 2000; ++iter) {
      tx.loop();
      if (delivered.size() > delivered_before) {
        done = true;
        break;
      }
    }
    assert(done && "Сообщение не дошло до приёмника в разумный срок");
    const auto& received = delivered.back();
    assert(received == payload && "Полученные данные не совпадают с отправленными");
  }

  std::cout << "Режим " << mode_case.name << " успешно прошёл кольцевой тест" << std::endl;
}

int main() {
  const std::array<ModeCase,3> modes{{
      {PayloadMode::SMALL, "SMALL"},
      {PayloadMode::MEDIUM, "MEDIUM"},
      {PayloadMode::LARGE, "LARGE"},
  }};

  for (const auto& mode_case : modes) {
    runMode(mode_case);
  }

  std::cout << "Кольцевой тест всех режимов завершён успешно" << std::endl;
  return 0;
}
