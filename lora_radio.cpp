#include "radio_interface.h"
#include "radio_adapter.h"

// Низкоуровневая функция передачи, реализуемая в прошивке
extern bool radioTransmit(const uint8_t* data, size_t len);

// Адаптер для текущего драйвера LoRa
class LoraRadio : public IRadioTransport {
public:
  bool setFrequency(uint32_t hz) override { return Radio_setFrequency(hz); }
  bool transmit(const uint8_t* data, size_t len) override { return radioTransmit(data, len); }
  void openRx(uint32_t rx_ticks) override { Radio_forceRx(rx_ticks); }

  bool setBandwidth(uint32_t khz) override { return Radio_setBandwidth(khz); }
  bool setSpreadingFactor(uint8_t sf) override { return Radio_setSpreadingFactor(sf); }
  bool setCodingRate(uint8_t cr4x) override { return Radio_setCodingRate(cr4x); }
  bool setTxPower(int8_t dBm) override { return Radio_setTxPower(dBm); }
  bool setRxBoost(bool on) override { return Radio_setRxBoost(on); }

  bool getSNR(float& snr) override { return Radio_getSNR(snr); }
  bool getEbN0(float& ebn0) override { return Radio_getEbN0(ebn0); }
  bool getLinkQuality(bool& good) override { return Radio_getLinkQuality(good); }
  bool getRSSI(float& rssi) override { return Radio_getRSSI(rssi); }
  bool isSynced() override { return Radio_isSynced(); }
};

// Глобальный экземпляр транспорта
static LoraRadio g_lora_radio;
IRadioTransport* g_radio = &g_lora_radio;

