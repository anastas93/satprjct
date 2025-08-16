
#pragma once
#include <stddef.h>
#include <stdint.h>
bool Radio_sendRaw(const uint8_t* data, size_t len);
bool Radio_setBandwidth(uint32_t khz);
bool Radio_setSpreadingFactor(uint8_t sf);
bool Radio_setCodingRate(uint8_t cr4x);
bool Radio_setTxPower(int8_t dBm);
void Radio_forceRx();

bool Radio_setFrequency(uint32_t hz);
inline bool Radio_setFrequencyHz(uint32_t hz) { return Radio_setFrequency(hz); }
void Radio_onReceive(const uint8_t* data, size_t len);
