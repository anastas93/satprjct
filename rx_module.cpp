#include "rx_module.h"
#include "libs/frame/frame_header.h" // заголовок кадра
#include "libs/rs255223/rs255223.h"    // RS(255,223)
#include "libs/byte_interleaver/byte_interleaver.h" // байтовый интерливинг
#include "libs/conv_codec/conv_codec.h" // свёрточный кодер/декодер
#include "libs/bit_interleaver/bit_interleaver.h" // битовый интерливинг
#include "libs/scrambler/scrambler.h" // скремблер
#include "libs/key_loader/key_loader.h" // загрузка ключа
#include "libs/crypto/aes_ccm.h" // AES-CCM шифрование
#include "default_settings.h"         // параметры по умолчанию
#include <vector>
#include <algorithm>
#include <array>

static constexpr size_t RS_DATA_LEN = DefaultSettings::GATHER_BLOCK_SIZE; // длина блока данных RS
static constexpr size_t TAG_LEN = 8;              // длина тега аутентичности
static constexpr size_t RS_ENC_LEN = 255;      // длина закодированного блока
static constexpr bool USE_BIT_INTERLEAVER = true; // включение битового интерливинга
static constexpr bool USE_RS = DefaultSettings::USE_RS; // использовать RS(255,223)

// Удаление пилотов из полезной нагрузки
static void removePilots(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  out.clear();
  if (!data || len == 0) {               // проверка указателя и длины
    return;
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
}

// Конструктор модуля приёма
RxModule::RxModule()
    : gatherer_(PayloadMode::SMALL, DefaultSettings::GATHER_BLOCK_SIZE),
      key_(KeyLoader::loadKey()) {} // ключ для последующего дешифрования

// Передаём данные колбэку, если заголовок валиден
void RxModule::onReceive(const uint8_t* data, size_t len) {
  if (!cb_ || !data || len < FrameHeader::SIZE * 2) return; // проверка указателя

  frame_buf_.assign(data, data + len);                     // переиспользуемый буфер кадра
  scrambler::descramble(frame_buf_.data(), frame_buf_.size()); // дескремблируем весь кадр

  FrameHeader hdr;
  bool ok = FrameHeader::decode(frame_buf_.data(), frame_buf_.size(), hdr);
  if (!ok)
    ok = FrameHeader::decode(frame_buf_.data() + FrameHeader::SIZE,
                             frame_buf_.size() - FrameHeader::SIZE, hdr);
  if (!ok) return; // оба заголовка повреждены

  const uint8_t* payload_p = frame_buf_.data() + FrameHeader::SIZE * 2;
  size_t payload_len = frame_buf_.size() - FrameHeader::SIZE * 2;
  removePilots(payload_p, payload_len, payload_buf_);      // убираем пилоты без выделений
  if (payload_buf_.size() != hdr.payload_len) return;      // несоответствие длины
  if (!hdr.checkFrameCrc(payload_buf_.data(), payload_buf_.size())) return; // неверный CRC

  // Деинтерливинг и декодирование
  result_buf_.clear();
  work_buf_.clear();
  size_t result_len = 0;
  if (USE_RS && payload_buf_.size() == RS_ENC_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // деинтерливинг бит
    if (!conv_codec::viterbiDecode(payload_buf_.data(), payload_buf_.size(), work_buf_)) return;
    if (!work_buf_.empty())
      byte_interleaver::deinterleave(work_buf_.data(), work_buf_.size()); // байтовый деинтерливинг
    result_buf_.resize(RS_DATA_LEN);
    if (!rs255223::decode(work_buf_.data(), result_buf_.data())) return;
    result_len = RS_DATA_LEN;
  } else if (USE_RS && payload_buf_.size() == RS_ENC_LEN) {
    byte_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // байтовый деинтерливинг
    result_buf_.resize(RS_DATA_LEN);
    if (!rs255223::decode(payload_buf_.data(), result_buf_.data())) return;
    result_len = RS_DATA_LEN;
  } else if (!USE_RS && payload_buf_.size() == RS_DATA_LEN * 2) {
    if (USE_BIT_INTERLEAVER)
      bit_interleaver::deinterleave(payload_buf_.data(), payload_buf_.size()); // деинтерливинг бит
    if (!conv_codec::viterbiDecode(payload_buf_.data(), payload_buf_.size(), result_buf_)) return;
    result_len = result_buf_.size();
  } else {
    result_buf_.assign(payload_buf_.begin(), payload_buf_.end()); // кадр без кодирования
    result_len = result_buf_.size();
  }

  // Дешифрование и сборка сообщения
  if (result_len < TAG_LEN) return;                  // недостаточно данных
  const uint8_t* cipher = result_buf_.data();
  size_t cipher_len = result_len - TAG_LEN;
  const uint8_t* tag = result_buf_.data() + cipher_len;
  if (!decrypt_ccm(key_.data(), key_.size(), nonce_.data(), nonce_.size(),
                   nullptr, 0, cipher, cipher_len,
                   tag, TAG_LEN, plain_buf_)) {
    LOG_ERROR("RxModule: ошибка дешифрования");
    return;                                           // прекращаем обработку
  }

  if (!plain_buf_.empty() && plain_buf_[0] == '[') {  // удаляем префикс [ID|n]
    auto it = std::find(plain_buf_.begin(), plain_buf_.end(), ']');
    if (it != plain_buf_.end()) {
      plain_buf_.erase(plain_buf_.begin(), it + 1);
    }
  }
  gatherer_.add(plain_buf_.data(), plain_buf_.size());
  plain_buf_.clear();                                  // очищаем буфер, сохраняя вместимость
  if (hdr.frag_idx + 1 == hdr.frag_cnt) {             // последний фрагмент
    const auto& full = gatherer_.get();
    if (buf_) {                                       // при наличии внешнего буфера сохраняем данные
      buf_->pushReady(hdr.msg_id, full.data(), full.size());
    }
    cb_(full.data(), full.size());                    // передаём сообщение пользователю
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
