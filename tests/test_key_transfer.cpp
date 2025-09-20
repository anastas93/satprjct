#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include "libs/key_transfer/key_transfer.h"

// Проверка формирования и разбора кадра KEYTRANSFER
int main() {
  std::array<uint8_t,32> pub{};
  for (size_t i = 0; i < pub.size(); ++i) pub[i] = static_cast<uint8_t>(i + 1);
  std::array<uint8_t,4> id{0xAA, 0xBB, 0xCC, 0xDD};
  std::vector<uint8_t> frame;
  uint32_t msg_id = 0x11223344;
  assert(KeyTransfer::buildFrame(msg_id, pub, id, frame));
  assert(!frame.empty());

  std::array<uint8_t,32> decoded_pub{};
  std::array<uint8_t,4> decoded_id{};
  uint32_t decoded_msg = 0;
  assert(KeyTransfer::parseFrame(frame.data(), frame.size(), decoded_pub, decoded_id, decoded_msg));
  assert(decoded_pub == pub);
  assert(decoded_id == id);
  assert(decoded_msg == msg_id);
  std::cout << "OK" << std::endl;
  return 0;
}
