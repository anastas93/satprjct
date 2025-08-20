// Stub implementations of the radio adapter functions used by the
// ESP32 LoRa pipeline.  When building on a desktop system these stubs
// simply return success or perform no operation.  In a full integration
// test environment you can extend these to simulate sending/receiving
// frames.

#include <stdint.h>
#include <stddef.h>

// Send a raw LoRa frame.  Always returns true in the stub.
bool Radio_sendRaw(const uint8_t* data, size_t len) {
  (void)data;
  (void)len;
  return true;
}

// Stub frequency setter.  Always returns true.
bool Radio_setFrequencyHz(uint32_t hz) {
  (void)hz;
  return true;
}

// The Radio_setFrequency() alias is defined inline in the header and
// forwards to Radio_setFrequencyHz(), so no additional definition is needed.

bool Radio_setBandwidth(uint32_t khz) {
  (void)khz;
  return true;
}

bool Radio_setSpreadingFactor(uint8_t sf) {
  (void)sf;
  return true;
}

bool Radio_setCodingRate(uint8_t cr4x) {
  (void)cr4x;
  return true;
}

bool Radio_setTxPower(int8_t dBm) {
  (void)dBm;
  return true;
}

bool Radio_setRxBoost(bool on) {
  (void)on;
  return true;
}

void Radio_forceRx(uint32_t rx_ticks) {
  (void)rx_ticks;
  // Заглушка ничего не делает.
}

// Заглушки измерений качества канала
bool Radio_getSNR(float& snr) { snr = 0.0f; return true; }
bool Radio_getEbN0(float& ebn0) { ebn0 = 0.0f; return true; }
bool Radio_getLinkQuality(bool& good) { good = true; return true; }
bool Radio_getRSSI(float& rssi) { rssi = 0.0f; return true; }

void Radio_onReceive(const uint8_t* data, size_t len) {
  (void)data;
  (void)len;
  // In the real implementation this would deliver a frame to the RxPipeline.
}