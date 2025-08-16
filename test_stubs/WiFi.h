// Minimal WiFi stub for host compilation.
// This stub provides only the subset of the Arduino WiFi API needed by the
// ESP32 LoRa Pipeline code when building on a desktop system.  It does not
// perform any networking; all methods are no‑ops or return dummy values.

#ifndef WIFI_H
#define WIFI_H

#include "Arduino.h"

// Emulate WiFi modes; only WIFI_AP is used by the application.
#define WIFI_AP 1

// IPAddress stub representing an IPv4 address.  The toString() method
// returns a fixed string because the stub always uses 192.168.4.1 for
// the softAP address.
class IPAddress {
public:
  IPAddress(uint8_t a = 192, uint8_t b = 168, uint8_t c = 4, uint8_t d = 1) {
    (void)a; (void)b; (void)c; (void)d;
  }
  String toString() const { return String("192.168.4.1"); }
};

// WiFiClass stub.  Provides minimal implementations of the methods used in
// the ESP32 LoRa Pipeline code.  All methods are no‑ops; the only method
// returning a value is softAPIP(), which returns a dummy IPAddress.
class WiFiClass {
public:
  void mode(int) {}
  void softAP(const char*, const char*) {}
  IPAddress softAPIP() { return IPAddress(); }
};

// Global WiFi object used by the application code.  Marked inline to
// permit multiple inclusions across translation units without violating
// the one-definition rule.
inline WiFiClass WiFi;

#endif // WIFI_H