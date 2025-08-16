// Minimal WebServer stub for host compilation.
// This header defines a minimal WebServer class that emulates the API of
// Arduino's ESP32 WebServer library.  Only the subset of functionality
// required by the ESP32 LoRa Pipeline code is implemented.  Handlers are
// accepted but never invoked because no HTTP traffic exists in the stub.

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Arduino.h"
#include <functional>

class WebServer {
public:
  explicit WebServer(uint16_t port) { (void)port; }

  // Register a handler for the given URI.  The ESP32 WebServer supports
  // various overloads; we only implement the simplest form accepting a
  // function pointer.  Parameters beyond the URI and handler are ignored.
  void on(const char* /*uri*/, std::function<void(void)> fn) {
    // In the stub we simply store the handler pointer but never call it.
    (void)fn;
  }
  void on(const char* uri, void (*handler)(void)) {
    (void)uri;
    (void)handler;
  }
  // Begin listening for connections.  No-op in the stub.
  void begin() {}

  // Process any pending client requests.  No-op in the stub.
  void handleClient() {}

  // Query string helpers: in the stub we always return false/no value.
  bool hasArg(const String&) { return false; }
  String arg(const String&) { return String(""); }

  // Send a response with given status code, content type and payload.
  void send(int /*code*/, const char* /*type*/, const String& /*payload*/) {}
  void send(int /*code*/, const String& /*type*/, const String& /*payload*/) {}
};

#endif // WEBSERVER_H