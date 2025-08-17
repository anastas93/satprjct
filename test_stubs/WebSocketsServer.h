// Заглушка WebSocketsServer для хостовых тестов
#pragma once
#include "Arduino.h"

enum WStype_t { WStype_ERROR, WStype_DISCONNECTED, WStype_CONNECTED, WStype_TEXT };
typedef void (*WSEventHandler)(uint8_t, WStype_t, uint8_t*, size_t);
class WebSocketsServer {
 public:
  explicit WebSocketsServer(uint16_t port) { (void)port; }
  void begin() {}
  void onEvent(WSEventHandler) {}
  void loop() {}
  void broadcastTXT(const char*) {}
};
