#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include "libs/key_transfer/key_transfer.h"

// Проверка формирования и разбора кадра KEYTRANSFER
int main() {
  std::array<uint8_t,32> pub{};
  std::array<uint8_t,32> eph{};
  for (size_t i = 0; i < pub.size(); ++i) {
    pub[i] = static_cast<uint8_t>(i + 1);
    eph[i] = static_cast<uint8_t>(0x80 + i);
  }
  std::array<uint8_t,4> id{0xAA, 0xBB, 0xCC, 0xDD};
  std::vector<uint8_t> frame;
  uint32_t msg_id = 0x11223344;
  assert(KeyTransfer::buildFrame(msg_id, pub, id, frame, &eph));
  assert(!frame.empty());

  KeyTransfer::FramePayload decoded_payload;
  uint32_t decoded_msg = 0;
  assert(KeyTransfer::parseFrame(frame.data(), frame.size(), decoded_payload, decoded_msg));
  assert(decoded_payload.public_key == pub);
  assert(decoded_payload.key_id == id);
  assert(decoded_payload.has_ephemeral);
  assert(decoded_payload.ephemeral_public == eph);
  assert(decoded_payload.version == KeyTransfer::VERSION_EPHEMERAL);
  assert(decoded_msg == msg_id);

  // Проверяем совместимость с обёрткой без доступа к эпемерному ключу
  std::array<uint8_t,32> decoded_pub{};
  std::array<uint8_t,4> decoded_id{};
  decoded_msg = 0;
  assert(KeyTransfer::parseFrame(frame.data(), frame.size(), decoded_pub, decoded_id, decoded_msg));
  assert(decoded_pub == pub);
  assert(decoded_id == id);
  assert(decoded_msg == msg_id);

  // Формируем и разбираем кадр в легаси-формате
  std::vector<uint8_t> legacy_frame;
  assert(KeyTransfer::buildFrame(msg_id, pub, id, legacy_frame, nullptr));
  KeyTransfer::FramePayload legacy_payload;
  assert(KeyTransfer::parseFrame(legacy_frame.data(), legacy_frame.size(), legacy_payload, decoded_msg));
  assert(!legacy_payload.has_ephemeral);
  assert(legacy_payload.version == KeyTransfer::VERSION_LEGACY);
  std::cout << "OK" << std::endl;
  return 0;
}
