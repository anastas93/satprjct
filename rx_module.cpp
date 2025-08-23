#include "rx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточный кодер/декодер
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "default_settings.h"         // параметры по умолчанию
#include <vector>
#include <algorithm>

static constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина блока данных RS
static constexpr size_t RS_ENC_LEN = 255;      // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга
static constexpr bool USE_RS = DefaultSettings::USE_RS; // использовать RS(255,223)

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

// Конструктор модуля приёма
RxModule::RxModule() : gatherer_(PayloadMode::SMALL, DefaultSettings::GATHER_BLOCK_SIZE) {}

// Передаём данные колбэку, если заголовок валиден
void RxModule::onReceive(const uint8_t* data, size_t len) {
  if (!cb_ || !data || len < FrameHeader::SIZE * 2) return; // проверка указателя

  std::vector<uint8_t> frame(data, data + len);            // копия для дескремблирования
  scrambler::descramble(frame.data(), frame.size());       // дескремблируем весь кадр

  FrameHeader hdr;
  bool ok = FrameHeader::decode(frame.data(), frame.size(), hdr);
  if (!ok)
    ok = FrameHeader::decode(frame.data() + FrameHeader::SIZE,
                             frame.size() - FrameHeader::SIZE, hdr);
  if (!ok) return; // оба заголовка повреждены

  const uint8_t* payload_p = frame.data() + FrameHeader::SIZE * 2;
  size_t payload_len = frame.size() - FrameHeader::SIZE * 2;
  auto payload = removePilots(payload_p, payload_len);
  if (payload.size() != hdr.payload_len) return; // несоответствие длины
  if (!hdr.checkFrameCrc(payload.data(), payload.size())) return; // неверный CRC

  // Деинтерливинг и декодирование
  std::vector<uint8_t> result;
  if (USE_RS && payload.size() == RS_ENC_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload.data(), payload.size()); // деинтерливинг бит
    std::vector<uint8_t> vit_dec;
    if (!conv_codec::viterbiDecode(payload.data(), payload.size(), vit_dec)) return;
    byte_interleaver::deinterleave(vit_dec.data(), vit_dec.size()); // байтовый деинтерливинг
    std::vector<uint8_t> decoded(RS_DATA_LEN);
    if (!rs255223::decode(vit_dec.data(), decoded.data())) return;
    result.swap(decoded);
  } else if (USE_RS && payload.size() == RS_ENC_LEN) {
    byte_interleaver::deinterleave(payload.data(), payload.size()); // байтовый деинтерливинг
    std::vector<uint8_t> decoded(RS_DATA_LEN);
    if (!rs255223::decode(payload.data(), decoded.data())) return;
    result.swap(decoded);
  } else if (!USE_RS && payload.size() == RS_DATA_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload.data(), payload.size()); // деинтерливинг бит
    std::vector<uint8_t> vit_dec;
    if (!conv_codec::viterbiDecode(payload.data(), payload.size(), vit_dec)) return;
    result.swap(vit_dec);
  } else {
    result.swap(payload); // кадр без кодирования
  }

  // Сборка сообщения из фрагментов
  if (!result.empty() && result[0] == '[') {            // удаляем префикс [ID|n]
    auto it = std::find(result.begin(), result.end(), ']');
    if (it != result.end()) {
      result.erase(result.begin(), it + 1);
    }
  }
  gatherer_.add(result.data(), result.size());
  if (hdr.frag_idx + 1 == hdr.frag_cnt) { // последний фрагмент
    const auto& full = gatherer_.get();
    if (buf_) { // при наличии внешнего буфера сохраняем данные
      buf_->pushReady(hdr.msg_id, full.data(), full.size());
    }
    cb_(full.data(), full.size());          // передаём сообщение пользователю
    gatherer_.reset();
  }
}

// Установка колбэка для обработки сообщений
void RxModule::setCallback(Callback cb) {
  cb_ = cb;
}

// Указание внешнего буфера для сохранения готовых сообщений
void RxModule::setBuffer(ReceivedBuffer* buf) {
  buf_ = buf;
}
