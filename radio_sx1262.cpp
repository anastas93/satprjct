#include "radio_sx1262.h"
#include "default_settings.h"
#include <cmath>
#include <array>

RadioSX1262* RadioSX1262::instance_ = nullptr; // инициализация статического указателя

// Таблицы частот приёма и передачи для трёх банков
const float RadioSX1262::fRX_bank_[3][10] = {
    {251.900, 252.000, 253.800, 253.700, 257.000, 257.100, 256.850, 258.650, 262.225, 262.325}, // Восток
    {250.900, 252.050, 253.850, 262.175, 267.050, 255.550, 251.950, 257.825, 260.125, 252.400}, // Запад
    {250.000, 260.000, 270.000, 280.000, 290.000, 300.000, 310.000, 433.000, 434.000, 446.000}  // Тест
};
const float RadioSX1262::fTX_bank_[3][10] = {
    {292.900, 293.100, 296.000, 294.700, 297.675, 295.650, 297.850, 299.650, 295.825, 295.925}, // Восток
    {308.300, 293.050, 294.850, 295.775, 308.050, 296.550, 292.950, 297.075, 310.125, 309.700}, // Запад
    {250.000, 260.000, 270.000, 280.000, 290.000, 300.000, 310.000, 433.000, 434.000, 446.000}  // Тест
};

const int8_t RadioSX1262::Pwr_[10] = {-5, -2, 1, 4, 7, 10, 13, 16, 19, 22};
const float RadioSX1262::BW_[5] = {7.81, 10.42, 15.63, 20.83, 31.25};
const int8_t RadioSX1262::SF_[8] = {5, 6, 7, 8, 9, 10, 11, 12};
const int8_t RadioSX1262::CR_[4] = {5, 6, 7, 8};

RadioSX1262::RadioSX1262() : radio_(new Module(5, 26, 27, 25)) {}

bool RadioSX1262::begin() {
  instance_ = this;               // сохраняем указатель на объект
  return resetToDefaults();       // применяем настройки по умолчанию
}

void RadioSX1262::send(const uint8_t* data, size_t len) {
  float freq_tx = fTX_bank_[static_cast<int>(bank_)][channel_];
  float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];
  radio_.setFrequency(freq_tx);        // переключаемся на TX частоту
  radio_.transmit((uint8_t*)data, len);
  radio_.setFrequency(freq_rx);        // возвращаем частоту приёма
  radio_.startReceive();               // и продолжаем слушать эфир
}

void RadioSX1262::setReceiveCallback(RxCallback cb) { rx_cb_ = cb; }

bool RadioSX1262::setBank(ChannelBank bank) {
  bank_ = bank;
  channel_ = 0;
  return setFrequency(fRX_bank_[static_cast<int>(bank_)][channel_]);
}

bool RadioSX1262::setChannel(uint8_t ch) {
  if (ch >= 10) return false;
  channel_ = ch;
  return setFrequency(fRX_bank_[static_cast<int>(bank_)][channel_]);
}

bool RadioSX1262::setFrequency(float freq) {
  int state = radio_.setFrequency(freq);      // задаём частоту
  return state == RADIOLIB_ERR_NONE;          // возвращаем успех
}

bool RadioSX1262::setBandwidth(float bw) {
  int idx = -1;
  for (int i = 0; i < 5; ++i) {
    if (std::fabs(BW_[i] - bw) < 0.01f) { idx = i; break; }
  }
  if (idx < 0) return false;                           // значение не из таблицы
  bw_preset_ = idx;
  int state = radio_.setBandwidth(bw);                 // задаём полосу пропускания
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setSpreadingFactor(int sf) {
  int idx = -1;
  for (int i = 0; i < 8; ++i) {
    if (SF_[i] == sf) { idx = i; break; }
  }
  if (idx < 0) return false;                           // недопустимое значение
  sf_preset_ = idx;
  int state = radio_.setSpreadingFactor(sf);           // задаём фактор расширения
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setCodingRate(int cr) {
  int idx = -1;
  for (int i = 0; i < 4; ++i) {
    if (CR_[i] == cr) { idx = i; break; }
  }
  if (idx < 0) return false;                           // недопустимое значение
  cr_preset_ = idx;
  int state = radio_.setCodingRate(cr);                // задаём коэффициент кодирования
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setPower(uint8_t preset) {
  if (preset >= 10) return false;                      // индекс вне диапазона
  pw_preset_ = preset;
  int state = radio_.setOutputPower(Pwr_[pw_preset_]); // установка мощности
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::resetToDefaults() {
  // возвращаем все параметры к значениям из файла default_settings.h
  bank_ = DefaultSettings::BANK;       // банк каналов
  channel_ = DefaultSettings::CHANNEL; // канал
  pw_preset_ = DefaultSettings::POWER_PRESET; // мощность
  bw_preset_ = DefaultSettings::BW_PRESET;    // полоса
  sf_preset_ = DefaultSettings::SF_PRESET;    // фактор расширения
  cr_preset_ = DefaultSettings::CR_PRESET;    // коэффициент кодирования

  int state = radio_.begin(
      fRX_bank_[static_cast<int>(bank_)][channel_],
      BW_[bw_preset_], SF_[sf_preset_], CR_[cr_preset_],
      0x18, Pwr_[pw_preset_], 10, tcxo_, false);
  if (state != RADIOLIB_ERR_NONE) {
    return false;                                         // ошибка инициализации
  }
  radio_.setDio1Action(onDio1Static);                     // колбэк приёма
  radio_.startReceive();                                  // начинаем приём
  return true;
}

void RadioSX1262::onDio1Static() {
  if (instance_) {
    instance_->handleDio1();
  }
}

void RadioSX1262::handleDio1() {
  size_t len = radio_.getPacketLength();
  // При завершении передачи длина пакета может быть мусорной и вызвать
  // выделение огромного буфера, что приводит к перезагрузке.
  if (len == 0 || len > 256) {
    radio_.startReceive();
    return;
  }
  std::array<uint8_t, 256> buf;            // статический буфер на стеке
  int state = radio_.readData(buf.data(), len);
  if (state == RADIOLIB_ERR_NONE && rx_cb_) {
    rx_cb_(buf.data(), len);
  }
  radio_.startReceive();
}

