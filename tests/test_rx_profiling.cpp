#include <cassert>
#include <vector>
#include <array>
#include <sstream>
#include <iostream>
#include <algorithm>
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
  rx.setEncryptionEnabled(false);                        // отключаем принудительное дешифрование для чистых кадров

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
  auto short_stats = rx.dropStats();
  assert(short_stats.total >= 1);
  auto it_short = short_stats.by_stage.find("короткий кадр без заголовка");
  assert(it_short != short_stats.by_stage.end());
  assert(it_short->second >= 1);

  // Проверяем отбрасывание кадра с несовпадающими копиями заголовка
  FrameHeader primary_hdr;
  primary_hdr.msg_id = 7;
  primary_hdr.setFragIdx(0);
  primary_hdr.frag_cnt = 1;
  primary_hdr.setFlags(0);
  std::array<uint8_t, FrameHeader::SIZE> hdr_buf1{};
  std::array<uint8_t, FrameHeader::SIZE> hdr_buf2{};
  std::array<uint8_t, 4> expected_plain{0x10, 0x20, 0x30, 0x40};
  std::array<uint8_t, 12> fake_payload{0x10, 0x20, 0x30, 0x40,
                                       0xA0, 0xA1, 0xA2, 0xA3,
                                       0xA4, 0xA5, 0xA6, 0xA7};
  primary_hdr.setPayloadLen(static_cast<uint16_t>(fake_payload.size()));
  bool encoded_primary = primary_hdr.encode(hdr_buf1.data(), hdr_buf1.size(),
                                            fake_payload.data(), fake_payload.size());
  assert(encoded_primary);
  FrameHeader secondary_hdr = primary_hdr;
  secondary_hdr.setFlags(FrameHeader::FLAG_ENCRYPTED);     // создаём расхождение ключевых полей
  bool encoded_secondary = secondary_hdr.encode(hdr_buf2.data(), hdr_buf2.size(),
                                                fake_payload.data(), fake_payload.size());
  assert(encoded_secondary);
  std::vector<uint8_t> single_header_frame;
  single_header_frame.insert(single_header_frame.end(), hdr_buf1.begin(), hdr_buf1.end());
  auto corrupted_hdr = hdr_buf1;
  corrupted_hdr[1] ^= 0xFF;                                // повреждаем идентификатор в копии
  single_header_frame.insert(single_header_frame.end(), corrupted_hdr.begin(), corrupted_hdr.end());
  single_header_frame.insert(single_header_frame.end(), fake_payload.begin(), fake_payload.end());
  scrambler::scramble(single_header_frame.data(), single_header_frame.size()); // имитируем передачу
  received.clear();
  std::ostringstream log_capture;                          // перехватываем журнал
  auto* prev_buf = std::cout.rdbuf(log_capture.rdbuf());   // перенаправляем поток
  rx.onReceive(single_header_frame.data(), single_header_frame.size());
  std::cout.rdbuf(prev_buf);                               // возвращаем стандартный вывод
  auto log_text = log_capture.str();
  assert(received.size() == expected_plain.size());        // проверяем длину полезной нагрузки
  assert(std::equal(received.begin(), received.end(), expected_plain.begin())); // убеждаемся, что байты совпадают
  assert(log_text.find("RxModule: обнаружено расхождение валидности копий заголовка") != std::string::npos);

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
  auto mismatch_stats = rx.dropStats();
  assert(mismatch_stats.total >= 2);
  auto it_mismatch = mismatch_stats.by_stage.find("заголовки расходятся");
  assert(it_mismatch != mismatch_stats.by_stage.end());
  assert(it_mismatch->second >= 1);

  rx.resetDropStats();
  auto cleared_stats = rx.dropStats();
  assert(cleared_stats.total == 0);
  assert(cleared_stats.by_stage.empty());

  return 0;
}

