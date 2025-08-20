#include "radio_transport.h"
#include "radio_adapter.h"

// Реализация адаптера транспорта через существующий Radio_sendRaw
bool RadioTransport::sendFrame(const uint8_t* data, size_t len, Qos qos) {
  return Radio_sendRaw(data, len, qos);
}
