
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "libs/qos.h"

// Перевод миллисекунд в тики таймера (1 тик = 15.625 мкс)
inline uint32_t msToTicks(uint32_t ms) { return ms * 64; }

bool Radio_sendRaw(const uint8_t* data, size_t len, Qos q = Qos::Normal);
bool Radio_setBandwidth(uint32_t khz);
bool Radio_setSpreadingFactor(uint8_t sf);
bool Radio_setCodingRate(uint8_t cr4x);
bool Radio_setTxPower(int8_t dBm);
// Включение усиленного режима приёма (setRxBoostedGainMode)
bool Radio_setRxBoost(bool on);
// Принудительный переход в RX на указанное число тиков
void Radio_forceRx(uint32_t rx_ticks);

// Получение качественных метрик канала
bool Radio_getSNR(float& snr);        // отношение сигнал/шум последнего пакета
bool Radio_getEbN0(float& ebn0);      // отношение энергии бита к спектральной плотности шума
bool Radio_getLinkQuality(bool& good); // булев флаг качества канала
bool Radio_getRSSI(float& rssi);       // уровень сигнала RSSI
// Упрощённые методы получения метрик
inline float Radio_readSNR()  { float v = 0.0f; Radio_getSNR(v);  return v; }
inline float Radio_readEbN0() { float v = 0.0f; Radio_getEbN0(v); return v; }
inline bool  Radio_readLinkQuality() { bool v = false; Radio_getLinkQuality(v); return v; }
inline float Radio_readRSSI() { float v = 0.0f; Radio_getRSSI(v); return v; }
bool Radio_isSynced();                // есть ли синхронизация по уникальному слову

bool Radio_setFrequency(uint32_t hz);
inline bool Radio_setFrequencyHz(uint32_t hz) { return Radio_setFrequency(hz); }
void Radio_onReceive(const uint8_t* data, size_t len);
