#include "radio_interface.h"
#include "radio_adapter.h"
#include "libs/qos_moderator.h"
// Низкоуровневая функция передачи, реализуется в прошивке
extern bool radioTransmit(const uint8_t* data, size_t len);

// Адаптер для текущего модуля LoRa
class LoraRadio : public IRadioTransport {
public:
  bool setFrequency(uint32_t hz) override {
    return Radio_setFrequency(hz);
  }
  bool transmit(const uint8_t* data, size_t len, Qos qos) override {
    // Проверяем приоритет через модератор QoS
    if (!Qos_allow(qos)) return false;
    return radioTransmit(data, len);
  }
  void openRx(uint32_t rx_ticks) override {
    Radio_forceRx(rx_ticks);
  }
};

// Глобальный экземпляр адаптера
static LoraRadio g_lora_radio;
IRadioTransport& g_radio = g_lora_radio;
