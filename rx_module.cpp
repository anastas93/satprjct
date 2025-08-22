#include "rx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include <vector>

// Удаление пилотов из полезной нагрузки
static std::vector<uint8_t> removePilots(const uint8_t* data, size_t len) {
  std::vector<uint8_t> out;
  if (!data || len == 0) {               // проверка указателя и длины
    return out;
  }
  out.reserve(len);
  size_t count = 0;
  for (size_t i = 0; i < len; ++i) {
    if (count && count % 64 == 0 && i + 1 < len && data[i] == 0x55 && data[i + 1] == 0x2D) {
      i++; // пропускаем пилот
      continue;
    }
    out.push_back(data[i]);
    ++count;
  }
  return out;
}

// Передаём данные колбэку, если заголовок валиден
void RxModule::onReceive(const uint8_t* data, size_t len) {
  if (!cb_ || !data || len < FrameHeader::SIZE * 2) return; // проверка указателя

  FrameHeader hdr;
  bool ok = FrameHeader::decode(data, len, hdr);
  if (!ok) ok = FrameHeader::decode(data + FrameHeader::SIZE, len - FrameHeader::SIZE, hdr);
  if (!ok) return; // оба заголовка повреждены

  const uint8_t* payload_p = data + FrameHeader::SIZE * 2;
  size_t payload_len = len - FrameHeader::SIZE * 2;
  auto payload = removePilots(payload_p, payload_len);
  if (payload.size() != hdr.payload_len) return; // несоответствие длины
  if (!hdr.checkFrameCrc(payload.data(), payload.size())) return; // неверный CRC

  cb_(payload.data(), payload.size());
}

// Установка колбэка для обработки сообщений
void RxModule::setCallback(Callback cb) {
  cb_ = cb;
}
