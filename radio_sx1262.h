#pragma once
#include <RadioLib.h>
#include <vector>
#include <cstdint>
#include "radio_interface.h"
#include "channel_bank.h"

// Реализация радиоинтерфейса на базе SX1262
class RadioSX1262 : public IRadio {
public:
  RadioSX1262();
  // Инициализация модуля и установка параметров по умолчанию
  bool begin();
  // Отправка данных
  void send(const uint8_t* data, size_t len) override;
  // Выполнение пинга с ожиданием эха
  bool ping(const uint8_t* data, size_t len,
            uint8_t* response, size_t responseCapacity,
            size_t& receivedLen, uint32_t timeoutUs,
            uint32_t& elapsedUs);
  // Отправка служебного маяка
  void sendBeacon();
  // Обработка готовности пакета в основном цикле
  void loop();
  // Установка колбэка приёма
  void setReceiveCallback(RxCallback cb) override;
  // Возвращение в режим приёма
  void ensureReceiveMode() override;
  // Получение параметров последнего принятого пакета
  float getLastSnr() const;  // последний SNR
  float getLastRssi() const; // последний RSSI
  // Получить случайный байт
  uint8_t randomByte();
    // Выбор банка каналов (EAST, WEST, TEST, ALL)
    bool setBank(ChannelBank bank);
    // Выбор канала 0-166 в зависимости от банка
    bool setChannel(uint8_t ch);
  // Установка ширины полосы пропускания
  bool setBandwidth(float bw);
  // Установка фактора расширения
  bool setSpreadingFactor(int sf);
  // Установка коэффициента кодирования
  bool setCodingRate(int cr);
  // Установка уровня мощности по индексу пресета
  bool setPower(uint8_t preset);
  // Включение режима повышенного усиления приёмника
  bool setRxBoostedGainMode(bool enabled);
  // Получение текущих параметров
    ChannelBank getBank() const { return bank_; }
    uint8_t getChannel() const { return channel_; }
    // Получить количество каналов в текущем банке
    uint16_t getBankSize() const { return BANK_CHANNELS_[static_cast<int>(bank_)]; }
  // Получить количество каналов в произвольном банке
  static uint16_t bankSize(ChannelBank bank);
  // Получить частоту приёма для канала в указанном банке
  static float bankRx(ChannelBank bank, uint16_t ch);
  // Получить частоту передачи для канала в указанном банке
  static float bankTx(ChannelBank bank, uint16_t ch);
  float getBandwidth() const { return BW_[bw_preset_]; }
  int getSpreadingFactor() const { return SF_[sf_preset_]; }
  int getCodingRate() const { return CR_[cr_preset_]; }
  int getPower() const { return Pwr_[pw_preset_]; }
  bool isRxBoostedGainEnabled() const { return rxBoostedGainEnabled_; }
    float getRxFrequency() const { return fRX_bank_[static_cast<int>(bank_)][channel_]; }
    float getTxFrequency() const { return fTX_bank_[static_cast<int>(bank_)][channel_]; }
  // Сброс параметров к значениям по умолчанию с перезапуском приёма
  bool resetToDefaults();

private:
  static void onDio1Static();            // статический обработчик прерывания
  void handleDio1();                     // обработка приёма

  // Непосредственная установка частоты
  bool setFrequency(float freq);

  SX1262 radio_;                         // экземпляр радиомодуля
  RxCallback rx_cb_;                     // пользовательский колбэк
  static RadioSX1262* instance_;         // указатель на текущий объект
  volatile bool packetReady_ = false;    // флаг готовности пакета

  ChannelBank bank_ = ChannelBank::EAST; // текущий банк
  uint8_t channel_ = 0;                  // текущий канал
  int pw_preset_ = 9;
  int bw_preset_ = 3;
  int sf_preset_ = 2;
  int cr_preset_ = 0;
  float tcxo_ = 0;
  bool rxBoostedGainEnabled_ = false;

  float lastSnr_ = 0.0f;     // SNR последнего пакета
  float lastRssi_ = 0.0f;    // RSSI последнего пакета

    // Таблицы частот для всех банков
    static const float* fRX_bank_[5];
    static const float* fTX_bank_[5];
    static const uint16_t BANK_CHANNELS_[5];
  static const int8_t Pwr_[10];
  static const float BW_[5];
  static const int8_t SF_[8];
  static const int8_t CR_[4];
};

