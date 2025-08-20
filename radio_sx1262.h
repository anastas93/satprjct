#pragma once
#include <RadioLib.h>
#include <vector>
#include "radio_interface.h"

// Реализация радиоинтерфейса на базе SX1262
class RadioSX1262 : public IRadio {
public:
  RadioSX1262();
  // Инициализация модуля с параметрами из примера
  bool begin();
  // Отправка данных
  void send(const uint8_t* data, size_t len) override;
  // Установка колбэка приёма
  void setReceiveCallback(RxCallback cb) override;

private:
  static void onDio1Static();            // статический обработчик прерывания
  void handleDio1();                     // обработка приёма

  SX1262 radio_;                         // экземпляр радиомодуля
  RxCallback rx_cb_;                     // пользовательский колбэк
  static RadioSX1262* instance_;         // указатель на текущий объект

  // Параметры радиоканала из исходного примера
  int radio_preset_ = 0;
  int pw_preset_ = 9;
  int bw_preset_ = 3;
  int sf_preset_ = 2;
  int cr_preset_ = 0;
  float tcxo_ = 0;

  static const float fRX_[10];
  static const float fTX_[10];
  static const int8_t Pwr_[10];
  static const float BW_[5];
  static const int8_t SF_[8];
  static const int8_t CR_[4];
};

