#include "rx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточный кодер/декодер
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/ccsds_link/scrambler.h" // скремблер
#include <vector>

static constexpr size_t RS_DATA_LEN = 223;     // длина блока данных RS
static constexpr size_t RS_ENC_LEN = 255;      // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга

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

  // Деинтерливинг и декодирование
  std::vector<uint8_t> result;
  if (payload.size() == RS_ENC_LEN * 2) {
    lfsr_descramble(payload.data(), payload.size(), (uint16_t)hdr.msg_id); // дескремблирование
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload.data(), payload.size()); // деинтерливинг бит
    std::vector<uint8_t> vit_dec;
    if (!conv_codec::viterbiDecode(payload.data(), payload.size(), vit_dec)) return;
    byte_interleaver::deinterleave(vit_dec.data(), vit_dec.size()); // байтовый деинтерливинг
    std::vector<uint8_t> decoded(RS_DATA_LEN);
    if (!rs255223::decode(vit_dec.data(), decoded.data())) return;
    result.swap(decoded);
  } else if (payload.size() == RS_ENC_LEN) {
    byte_interleaver::deinterleave(payload.data(), payload.size()); // байтовый деинтерливинг
    std::vector<uint8_t> decoded(RS_DATA_LEN);
    if (!rs255223::decode(payload.data(), decoded.data())) return;
    result.swap(decoded);
  } else {
    result.swap(payload); // кадр без кодирования
  }

  cb_(result.data(), result.size());
}

// Установка колбэка для обработки сообщений
void RxModule::setCallback(Callback cb) {
  cb_ = cb;
}
