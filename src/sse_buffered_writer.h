#pragma once
// Буферизированная отправка SSE-кадров с учётом ограничений окна записи
// Все комментарии на русском согласно пользовательским требованиям

#include <Arduino.h>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <utility>

// Шаблон кадра SSE с параметризованным типом строки, чтобы переиспользовать код
// как на контроллере (Arduino String), так и в хостовых тестах (std::string).
template <typename StringType>
struct GenericSseFrame {
  StringType data;          // Полный текст кадра SSE
  size_t offset = 0;        // Сколько байт уже отправлено клиенту
  uint32_t waitStartMs = 0; // Время начала ожидания освобождения окна
  bool refreshActivity = false;   // Нужно ли обновить lastActivityMs после доставки
  bool refreshKeepAlive = false;  // Нужно ли обновить lastKeepAliveMs после доставки
};

// Буферизованный писатель, который умеет дозированно отправлять кадры SSE и
// отслеживать тайм-аут ожидания при отсутствии свободного окна у клиента.
template <typename Client, typename StringType>
class SseBufferedWriter {
 public:
  using Frame = GenericSseFrame<StringType>;

  bool empty() const { return frames_.empty(); }

  void clear() { frames_.clear(); }

  // Возвращает false, если за время timeoutMs не удалось продвинуться в отправке.
  bool flush(Client& client,
             uint32_t& lastActivityMs,
             uint32_t& lastKeepAliveMs,
             uint32_t timeoutMs,
             size_t maxChunkSize) {
    while (!frames_.empty()) {
      Frame& frame = frames_.front();
      const size_t total = static_cast<size_t>(frame.data.length());
      while (frame.offset < total) {
        const uint32_t now = millis();
        size_t available = static_cast<size_t>(client.availableForWrite());
        if (available == 0) {
          if (frame.waitStartMs == 0) {
            frame.waitStartMs = now;
          } else if (timeoutMs > 0 && static_cast<uint32_t>(now - frame.waitStartMs) >= timeoutMs) {
            return false; // окно так и не освободилось — считаем клиента зависшим
          }
          return true; // ждём следующего захода loop()
        }
        size_t chunk = total - frame.offset;
        if (maxChunkSize > 0 && chunk > maxChunkSize) {
          chunk = maxChunkSize;
        }
        if (chunk > available) {
          chunk = available;
        }
        if (chunk == 0) {
          if (frame.waitStartMs == 0) {
            frame.waitStartMs = now;
          } else if (timeoutMs > 0 && static_cast<uint32_t>(now - frame.waitStartMs) >= timeoutMs) {
            return false;
          }
          return true;
        }
        size_t written = client.write(
            reinterpret_cast<const uint8_t*>(frame.data.c_str() + frame.offset), chunk);
        if (written == 0) {
          if (frame.waitStartMs == 0) {
            frame.waitStartMs = now;
          } else if (timeoutMs > 0 && static_cast<uint32_t>(now - frame.waitStartMs) >= timeoutMs) {
            return false;
          }
          return true;
        }
        frame.offset += written;
        frame.waitStartMs = now;
        if (written < chunk) {
          return true; // клиент принял часть данных — ждём следующего цикла
        }
      }
      const uint32_t deliveredAt = millis();
      if (frame.refreshActivity) {
        lastActivityMs = deliveredAt;
      }
      if (frame.refreshKeepAlive) {
        lastKeepAliveMs = deliveredAt;
      }
      frames_.pop_front();
    }
    return true;
  }

  bool enqueueFrame(Client& client,
                    uint32_t& lastActivityMs,
                    uint32_t& lastKeepAliveMs,
                    StringType&& payload,
                    bool refreshActivity,
                    bool refreshKeepAlive,
                    uint32_t timeoutMs,
                    size_t maxChunkSize) {
    if (!flush(client, lastActivityMs, lastKeepAliveMs, timeoutMs, maxChunkSize)) {
      return false; // предыдущий кадр завис — прекращаем работу с клиентом
    }
    Frame frame;
    frame.data = std::move(payload);
    frame.offset = 0;
    frame.waitStartMs = millis();
    frame.refreshActivity = refreshActivity;
    frame.refreshKeepAlive = refreshKeepAlive;
    frames_.push_back(std::move(frame));
    return flush(client, lastActivityMs, lastKeepAliveMs, timeoutMs, maxChunkSize);
  }

 private:
  std::deque<Frame> frames_;
};

