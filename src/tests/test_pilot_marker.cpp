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
#include "../libs/protocol/ack_utils.h"  // маркер компактного ACK

// Заглушка радиоинтерфейса с накоплением последнего отправленного кадра
class CaptureRadio : public IRadio {
public:
  std::vector<uint8_t> last;
  std::vector<std::vector<uint8_t>> history;
  int16_t send(const uint8_t* data, size_t len) override {
    last.assign(data, data + len);
    history.emplace_back(last);
    return 0;
  }
  void setReceiveCallback(RxCallback) override {}
};

namespace {

// Параметры нового пилотного маркера без проверки CRC заголовка
constexpr size_t PILOT_INTERVAL = 64;                                      // период вставки пилотов
constexpr std::array<uint8_t,7> PILOT_MARKER{{0x7E,'S','A','T','P',0xD6,0x9F}}; // сигнатура пилота
constexpr size_t PILOT_PREFIX_LEN = PILOT_MARKER.size() - 2;               // префикс без CRC

// Удаление пилотов из полезной нагрузки
void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) return;                                          // защита от пустых буферов
  out.reserve(len);
  size_t count = 0;
  size_t i = 0;
  while (i < len) {
    if (count && count % PILOT_INTERVAL == 0) {
      size_t remaining = len - i;
      if (remaining >= PILOT_MARKER.size() &&
          std::equal(PILOT_MARKER.begin(), PILOT_MARKER.begin() + PILOT_PREFIX_LEN, data + i)) {
        uint16_t crc = static_cast<uint16_t>(data[i + PILOT_PREFIX_LEN]) |
                       (static_cast<uint16_t>(data[i + PILOT_PREFIX_LEN + 1]) << 8);
        if (crc == 0x9FD6 && FrameHeader::crc16(data + i, PILOT_PREFIX_LEN) == crc) {
          i += PILOT_MARKER.size();
          continue;                                                       // полностью пропускаем маркер
        }
      }
    }
    out.push_back(data[i]);
    ++count;
    ++i;
  }
}

// Декодирование кадра без CRC с учётом возможного дублированного заголовка
bool decodeFrameNoCrc(const std::vector<uint8_t>& frame,
                      FrameHeader& hdr,
                      std::vector<uint8_t>& payload,
                      size_t& header_bytes) {
  if (frame.size() < FrameHeader::SIZE) return false;                      // короткий кадр без заголовка
  std::vector<uint8_t> descrambled(frame);
  scrambler::descramble(descrambled.data(), descrambled.size());
  FrameHeader primary;
  FrameHeader secondary;
  bool primary_ok = FrameHeader::decode(descrambled.data(), descrambled.size(), primary);
  bool secondary_ok = false;
  if (descrambled.size() >= FrameHeader::SIZE * 2) {
    secondary_ok = FrameHeader::decode(descrambled.data() + FrameHeader::SIZE,
                                       descrambled.size() - FrameHeader::SIZE,
                                       secondary);
  }
  if (!primary_ok && !secondary_ok) return false;                          // нет ни одной валидной копии
  hdr = primary_ok ? primary : secondary;
  header_bytes = primary_ok ? FrameHeader::SIZE : FrameHeader::SIZE * 2;
  std::vector<uint8_t> cleaned;
  removePilots(descrambled.data() + header_bytes,
               descrambled.size() - header_bytes,
               cleaned);
  payload = std::move(cleaned);
  return true;
}

} // namespace

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

  uint16_t id = tx.queue(payload.data(), payload.size());
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
    const uint8_t* payload_ptr = frame_copy.data() + FrameHeader::SIZE;
    size_t payload_len = frame_copy.size() - FrameHeader::SIZE;
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

  // Дополнительно убеждаемся, что компактный ACK формируется без заголовка
  size_t before_ack = radio.history.size();
  const uint8_t ack_marker = protocol::ack::MARKER;
  uint16_t ack_id = tx.queue(&ack_marker, 1);
  assert(ack_id != 0);
  while (tx.loop()) {
    // дожидаемся отправки ACK
  }
  assert(radio.history.size() == before_ack + 1);
  const auto& ack_frame = radio.history.back();
  FrameHeader ack_hdr;
  std::vector<uint8_t> ack_payload;
  size_t ack_header_bytes = 0;
  assert(decodeFrameNoCrc(ack_frame, ack_hdr, ack_payload, ack_header_bytes));
  assert(ack_header_bytes == FrameHeader::SIZE);
  assert((ack_hdr.getFlags() & (FrameHeader::FLAG_ENCRYPTED | FrameHeader::FLAG_CONV_ENCODED)) == 0);
  assert(ack_hdr.getPayloadLen() == 1);
  assert(ack_frame.size() == FrameHeader::SIZE + ack_hdr.getPayloadLen());
  assert(ack_payload.size() == 1);
  assert(ack_payload.front() == protocol::ack::MARKER);

  std::cout << "OK" << std::endl;
  return 0;
}
