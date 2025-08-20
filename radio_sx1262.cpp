#include "radio_sx1262.h"

RadioSX1262* RadioSX1262::instance_ = nullptr;  // инициализация статического указателя

const float RadioSX1262::fRX_[10] = { 251.900, 252.000, 253.800, 253.700, 257.000, 257.100, 256.850, 258.650, 262.225, 262.325 };
const float RadioSX1262::fTX_[10] = { 292.900, 293.100, 296.000, 294.700, 297.675, 295.650, 297.850, 299.650, 295.825, 295.925 };
const int8_t RadioSX1262::Pwr_[10] = { -5, -2, 1, 4, 7, 10, 13, 16, 19, 22 };
const float RadioSX1262::BW_[5] = { 7.81, 10.42, 15.63, 20.83, 31.25 };
const int8_t RadioSX1262::SF_[8] = { 5, 6, 7, 8, 9, 10, 11, 12 };
const int8_t RadioSX1262::CR_[4] = { 5, 6, 7, 8 };

RadioSX1262::RadioSX1262() : radio_(new Module(5, 26, 27, 25)) {}

bool RadioSX1262::begin() {
  instance_ = this;
  int state = radio_.begin(fRX_[radio_preset_], BW_[bw_preset_], SF_[sf_preset_],
                           CR_[cr_preset_], 0x18, Pwr_[pw_preset_], 10, tcxo_, false);
  if (state != RADIOLIB_ERR_NONE) {
    return false;
  }
  radio_.setDio1Action(onDio1Static);  // колбэк приёма
  radio_.startReceive();               // начинаем слушать эфир
  return true;
}

void RadioSX1262::send(const uint8_t* data, size_t len) {
  radio_.transmit((uint8_t*)data, len);
  radio_.startReceive();  // возвращаемся в режим приёма
}

void RadioSX1262::setReceiveCallback(RxCallback cb) { rx_cb_ = cb; }

void RadioSX1262::onDio1Static() {
  if (instance_) {
    instance_->handleDio1();
  }
}

void RadioSX1262::handleDio1() {
  size_t len = radio_.getPacketLength();
  if (len == 0) {
    radio_.startReceive();
    return;
  }
  std::vector<uint8_t> buf(len);
  int state = radio_.readData(buf.data(), len);
  if (state == RADIOLIB_ERR_NONE && rx_cb_) {
    rx_cb_(buf.data(), len);
  }
  radio_.startReceive();
}

