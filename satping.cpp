
#include <Arduino.h>
#include <string.h>
#include "RadioLib.h"
#include "freq_map.h"

extern SX1262 radio;
extern float g_freq_rx_mhz;
extern float g_freq_tx_mhz;
extern int   g_preset;

void SatPing() {
  // Build 5-byte ping pattern
  uint8_t ping[5];
  uint8_t rx[5];
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();
  ping[0] = ping[1] ^ ping[2];
  ping[3] = 0x01;     // address placeholder
  ping[4] = 0x00;

  Serial.print(F("~&Server: RX:"));
  Serial.print(g_freq_rx_mhz, 3);
  Serial.print(F("/TX:"));
  Serial.print(g_freq_tx_mhz, 3);
  Serial.println(F("~"));

  // TX on TX frequency
  radio.setFrequency(g_freq_tx_mhz);
  radio.transmit(ping, 5);

  // measure RTT
  uint32_t t1 = micros();
  // switch to RX and wait for 5 bytes
  radio.setFrequency(g_freq_rx_mhz);
  int state = radio.receive(rx, 5);
  uint32_t t2 = micros();

  bool same = (state == RADIOLIB_ERR_NONE) && (memcmp(ping, rx, 5) == 0);
  if (same) {
    Serial.print(F("Ping>OK  RSSI:"));
    Serial.print(radio.getRSSI());
    Serial.print(F(" dBm/SNR:"));
    Serial.print(radio.getSNR());
    Serial.print(F(" dB RTT: "));
    Serial.print((unsigned long)(t2 - t1));
    Serial.println(F(" us"));
  } else {
    Serial.print(F("Ping>Bad, code "));
    Serial.println(state);
  }

  delay(150);
  // Back to RX idle
  radio.setFrequency(g_freq_rx_mhz);
  radio.startReceive();
}
