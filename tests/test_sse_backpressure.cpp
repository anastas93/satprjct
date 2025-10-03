#include "stubs/Arduino.h"
#include "../src/sse_buffered_writer.h"

#include <cassert>
#include <string>

// Простейшая заглушка сетевого клиента с ограниченным окном записи
class FakeNetworkClient {
 public:
  void setWindow(size_t size) { window_ = size; }

  size_t availableForWrite() { return window_; }

  size_t write(const uint8_t* data, size_t len) {
    if (window_ == 0 || len == 0) {
      return 0;
    }
    size_t toWrite = (len < window_) ? len : window_;
    buffer_.append(reinterpret_cast<const char*>(data), toWrite);
    window_ -= toWrite;
    return toWrite;
  }

  void resetBuffer() { buffer_.clear(); }

  const std::string& written() const { return buffer_; }

 private:
  size_t window_ = 0;
  std::string buffer_;
};

int main() {
  using Writer = SseBufferedWriter<FakeNetworkClient, std::string>;
  Writer writer;
  FakeNetworkClient client;
  uint32_t lastActivity = 0;
  uint32_t lastKeepAlive = 0;
  const uint32_t timeoutMs = 30;   // в тесте ускоряем тайм-аут для быстрой проверки
  const size_t chunkLimit = 3;     // ограничиваем куски тремя байтами

  ArduinoStub::gMillis = 0;
  client.setWindow(3);
  bool ok = writer.enqueueFrame(client,
                                lastActivity,
                                lastKeepAlive,
                                std::string("abcdefgh"),
                                true,
                                true,
                                timeoutMs,
                                chunkLimit);
  assert(ok);
  assert(!writer.empty());
  assert(client.written() == "abc"); // первый кусок сразу помещается в выходной буфер

  // Окно записи закрыто, несколько вызовов flush() не должны блокироваться и не должны
  // завершаться аварийно до истечения тайм-аута.
  for (int i = 0; i < 2; ++i) {
    ArduinoStub::gMillis += 10;
    bool flushOk = writer.flush(client, lastActivity, lastKeepAlive, timeoutMs, chunkLimit);
    assert(flushOk);
    assert(!writer.empty());
  }

  // По истечении тайм-аута writer сообщает об ошибке — в рабочем коде клиент отключится.
  ArduinoStub::gMillis += 21; // теперь общее ожидание превысило тайм-аут
  bool flushFail = writer.flush(client, lastActivity, lastKeepAlive, timeoutMs, chunkLimit);
  assert(!flushFail);

  // Сбрасываем состояние и проверяем успешную дозированную отправку при постепенном освобождении окна.
  writer.clear();
  client.resetBuffer();
  lastActivity = 0;
  lastKeepAlive = 0;
  ArduinoStub::gMillis = 0;

  client.setWindow(3);
  ok = writer.enqueueFrame(client,
                           lastActivity,
                           lastKeepAlive,
                           std::string("abcdefgh"),
                           true,
                           true,
                           timeoutMs,
                           chunkLimit);
  assert(ok);

  while (!writer.empty()) {
    ArduinoStub::gMillis += 5;
    client.setWindow(3);
    bool flushOk = writer.flush(client, lastActivity, lastKeepAlive, timeoutMs, chunkLimit);
    assert(flushOk);
  }

  assert(client.written() == "abcdefgh");
  assert(lastActivity == ArduinoStub::gMillis);
  assert(lastKeepAlive == ArduinoStub::gMillis);
  return 0;
}
