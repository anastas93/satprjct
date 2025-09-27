#include <cassert>
#include <vector>
#include <array>
#include "tx_module.h"
#include "rx_module.h"
#include "libs/frame/frame_header.h"
#include "libs/scrambler/scrambler.h"

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

  // Проверяем отбрасывание кадра с несовпадающими копиями заголовка
  FrameHeader primary_hdr;
  primary_hdr.msg_id = 7;
  primary_hdr.frag_idx = 0;
  primary_hdr.frag_cnt = 1;
  primary_hdr.payload_len = 4;
  std::array<uint8_t, FrameHeader::SIZE> hdr_buf1{};
  std::array<uint8_t, FrameHeader::SIZE> hdr_buf2{};
  std::array<uint8_t, 4> fake_payload{0x10, 0x20, 0x30, 0x40};
  bool encoded_primary = primary_hdr.encode(hdr_buf1.data(), hdr_buf1.size(),
                                            fake_payload.data(), fake_payload.size());
  assert(encoded_primary);
  FrameHeader secondary_hdr = primary_hdr;
  secondary_hdr.flags = FrameHeader::FLAG_ENCRYPTED;       // создаём расхождение ключевых полей
  bool encoded_secondary = secondary_hdr.encode(hdr_buf2.data(), hdr_buf2.size(),
                                                fake_payload.data(), fake_payload.size());
  assert(encoded_secondary);
  std::vector<uint8_t> mismatched_frame;
  mismatched_frame.insert(mismatched_frame.end(), hdr_buf1.begin(), hdr_buf1.end());
  mismatched_frame.insert(mismatched_frame.end(), hdr_buf2.begin(), hdr_buf2.end());
  mismatched_frame.insert(mismatched_frame.end(), fake_payload.begin(), fake_payload.end());
  scrambler::scramble(mismatched_frame.data(), mismatched_frame.size()); // имитируем передачу
  received.clear();
  rx.onReceive(mismatched_frame.data(), mismatched_frame.size());
  assert(received == mismatched_frame);                    // получаем сырой кадр без обработки
  auto mismatch_profile = rx.lastProfiling();
  assert(mismatch_profile.valid);
  assert(mismatch_profile.dropped);
  assert(mismatch_profile.drop_stage == "заголовки расходятся");

  return 0;
}

