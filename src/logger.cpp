#include "logger.h"
// Реализация асинхронного логгера с кольцевым буфером и ограничением скорости

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>
#ifdef ARDUINO
#  include <Arduino.h>
#  include "freertos/FreeRTOS.h"
#  include "freertos/task.h"
#  include "freertos/portmacro.h"
#  include "esp_timer.h"
#endif

#include "libs/log_hook/log_hook.h"

namespace {

// Настройки очереди логов
constexpr size_t kQueueDepth = 128;                 // количество записей в кольцевом буфере
constexpr size_t kMaxPayload = 240;                 // максимальная длина сообщения
constexpr uint8_t kFrameMagic = 0x02;               // маркер начала фрейма

struct RecordStorage {
  Logger::RecordType type = Logger::RecordType::Text;  // тип записи
  DefaultSettings::LogLevel level = DefaultSettings::LogLevel::INFO; // уровень
  uint32_t timestamp_us = 0;                           // отметка времени
  uint16_t length = 0;                                 // фактическая длина строки
  bool truncated = false;                              // сообщение обрезано
  std::array<char, kMaxPayload + 1> payload{};         // текст + нуль-терминатор
};

Logger::Stats stats;                         // счётчики потерь

struct RingBuffer {
  std::array<RecordStorage, kQueueDepth> data{}; // хранилище записей
  size_t head = 0;                               // индекс первой записи
  size_t tail = 0;                               // индекс следующего свободного слота
  size_t size = 0;                               // количество элементов

  bool push(const RecordStorage& rec) {
    if (size == data.size()) {
      // При переполнении удаляем самую старую запись
      head = (head + 1) % data.size();
      --size;
      ++stats.dropped_records;
    }
    data[tail] = rec;
    tail = (tail + 1) % data.size();
    ++size;
    return true;
  }

  bool pop(RecordStorage& out) {
    if (size == 0) return false;
    out = data[head];
    head = (head + 1) % data.size();
    --size;
    return true;
  }
};

RingBuffer g_ring;                           // глобальный буфер

#ifdef ARDUINO
portMUX_TYPE g_spinlock = portMUX_INITIALIZER_UNLOCKED; // спинлок для доступа из ISR
struct Lock {
  explicit Lock(bool from_isr) : from_isr_(from_isr) {
    if (from_isr_) {
      portENTER_CRITICAL_ISR(&g_spinlock);
    } else {
      portENTER_CRITICAL(&g_spinlock);
    }
  }
  ~Lock() {
    if (from_isr_) {
      portEXIT_CRITICAL_ISR(&g_spinlock);
    } else {
      portEXIT_CRITICAL(&g_spinlock);
    }
  }
  bool from_isr_;
};
#else
std::mutex g_mutex;                         // мьютекс для потокобезопасности на хосте
struct Lock {
  explicit Lock(bool /*from_isr*/) { g_mutex.lock(); }
  ~Lock() { g_mutex.unlock(); }
};
#endif

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int j = 0; j < 8; ++j) {
      if (crc & 0x8000) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

RecordStorage buildRecord(DefaultSettings::LogLevel level,
                          Logger::RecordType type,
                          const std::string& line,
                          bool* truncated) {
  RecordStorage rec;
  rec.type = type;
  rec.level = level;
  rec.timestamp_us = Logger::nowMicros();
  rec.length = static_cast<uint16_t>(std::min(line.size(), kMaxPayload));
  if (line.size() > kMaxPayload) {
    rec.truncated = true;
    if (truncated) *truncated = true;
  }
  std::memcpy(rec.payload.data(), line.data(), rec.length);
  rec.payload[rec.length] = '\0';
  return rec;
}

std::string renderRecord(const RecordStorage& rec) {
  std::string text(rec.payload.data(), rec.length);
  if (rec.truncated) {
    text.append(" …");
  }
  if (rec.type == Logger::RecordType::Csv) {
    text.insert(0, "CSV,");
  }
  return text;
}

bool pushRecord(DefaultSettings::LogLevel level,
                Logger::RecordType type,
                const std::string& line,
                bool from_isr) {
  bool truncated = false;
  RecordStorage rec = buildRecord(level, type, line, &truncated);
  Lock lock(from_isr);
  if (truncated) {
    stats.truncated_records++;
    stats.truncated_bytes += static_cast<uint32_t>(line.size() > kMaxPayload
                                                    ? line.size() - kMaxPayload
                                                    : 0);
  }
  g_ring.push(rec);
#ifndef ARDUINO
  // На хостовых тестах сразу доставляем запись в LogHook, имитируя работу задачи
  LogHook::append(renderRecord(rec).c_str());
#endif
  return true;
}

} // namespace

namespace Logger {

void init() {
  Lock lock(false);
  g_ring = RingBuffer{};
  stats = Stats{};
}

static bool shouldAccept(DefaultSettings::LogLevel level) {
  if (!DefaultSettings::DEBUG) return false;
  return level <= DefaultSettings::LOG_LEVEL;
}

bool enqueue(DefaultSettings::LogLevel level, const std::string& line) {
  if (!shouldAccept(level)) return false;
  return pushRecord(level, RecordType::Text, line, false);
}

bool enqueueFromISR(DefaultSettings::LogLevel level, const std::string& line) {
  if (!shouldAccept(level)) return false;
  return pushRecord(level, RecordType::Text, line, true);
}

bool enqueueValue(DefaultSettings::LogLevel level, const char* prefix, long long value) {
  if (!shouldAccept(level)) return false;
  char buffer[kMaxPayload + 1];
  int written = std::snprintf(buffer, sizeof(buffer), "%s%lld", prefix, value);
  if (written < 0) {
    return false;
  }
  std::string line(buffer, std::strlen(buffer));
  return pushRecord(level, RecordType::Text, line, false);
}

namespace {
std::string formatVa(const char* fmt, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int required = std::vsnprintf(nullptr, 0, fmt, copy);
  va_end(copy);
  if (required <= 0) {
    return std::string();
  }
  std::vector<char> buffer(static_cast<size_t>(required) + 1);
  std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
  return std::string(buffer.data(), buffer.data() + std::strlen(buffer.data()));
}

bool enqueueVa(DefaultSettings::LogLevel level,
               const char* fmt,
               va_list args,
               bool from_isr) {
  if (!shouldAccept(level)) return false;
  std::string line = formatVa(fmt, args);
  return pushRecord(level, RecordType::Text, line, from_isr);
}
} // namespace

bool enqueuef(DefaultSettings::LogLevel level, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  bool result = enqueueVa(level, fmt, args, false);
  va_end(args);
  return result;
}

bool enqueuefFromISR(DefaultSettings::LogLevel level, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  bool result = enqueueVa(level, fmt, args, true);
  va_end(args);
  return result;
}

Stats consumeStats() {
  Lock lock(false);
  Stats snapshot = stats;
  stats = Stats{};
  return snapshot;
}

size_t queueCapacity() {
  return kQueueDepth;
}

size_t payloadLimit() {
  return kMaxPayload;
}

uint32_t nowMicros() {
#ifdef ARDUINO
  return static_cast<uint32_t>(esp_timer_get_time());
#else
  using clock = std::chrono::steady_clock;
  static const auto start = clock::now();
  auto delta = clock::now() - start;
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
  return static_cast<uint32_t>(us);
#endif
}

void enqueueWrapSample(const char* tag,
                       uint32_t start_us,
                       uint32_t end_us) {
  const uint32_t duration = (end_us >= start_us) ? (end_us - start_us)
                                                 : (0xFFFFFFFFu - start_us + end_us + 1);
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "%s,%u,%u,%u", tag, start_us, end_us, duration);
  pushRecord(DefaultSettings::LogLevel::DEBUG, RecordType::Csv, buffer, false);
}

std::string makeFrame(RecordType type,
                      DefaultSettings::LogLevel level,
                      uint32_t timestamp_us,
                      const std::string& payload) {
  const size_t len = std::min(payload.size(), static_cast<size_t>(0xFFFF));
  std::string frame;
  frame.reserve(len + 9);
  frame.push_back(static_cast<char>(kFrameMagic));
  frame.push_back(static_cast<char>(type));
  frame.push_back(static_cast<char>(level));
  frame.push_back(static_cast<char>((timestamp_us >> 24) & 0xFF));
  frame.push_back(static_cast<char>((timestamp_us >> 16) & 0xFF));
  frame.push_back(static_cast<char>((timestamp_us >> 8) & 0xFF));
  frame.push_back(static_cast<char>(timestamp_us & 0xFF));
  frame.push_back(static_cast<char>((len >> 8) & 0xFF));
  frame.push_back(static_cast<char>(len & 0xFF));
  frame.append(payload.data(), len);
  uint16_t crc = crc16(reinterpret_cast<const uint8_t*>(frame.data()) + 1, frame.size() - 1);
  frame.push_back(static_cast<char>((crc >> 8) & 0xFF));
  frame.push_back(static_cast<char>(crc & 0xFF));
  return frame;
}

bool parseFrame(const std::string& frame,
                RecordType& type,
                DefaultSettings::LogLevel& level,
                uint32_t& timestamp_us,
                std::string& payload) {
  if (frame.size() < 10) return false;
  if (static_cast<uint8_t>(frame[0]) != kFrameMagic) return false;
  const uint8_t type_val = static_cast<uint8_t>(frame[1]);
  const uint8_t level_val = static_cast<uint8_t>(frame[2]);
  const uint16_t length = static_cast<uint16_t>((static_cast<uint8_t>(frame[7]) << 8) |
                                                static_cast<uint8_t>(frame[8]));
  if (frame.size() != static_cast<size_t>(length) + 11) return false;
  uint16_t crc_calc = crc16(reinterpret_cast<const uint8_t*>(frame.data()) + 1, frame.size() - 3);
  uint16_t crc_frame = static_cast<uint16_t>((static_cast<uint8_t>(frame[frame.size() - 2]) << 8) |
                                             static_cast<uint8_t>(frame[frame.size() - 1]));
  if (crc_calc != crc_frame) return false;
  type = static_cast<RecordType>(type_val);
  level = static_cast<DefaultSettings::LogLevel>(level_val);
  timestamp_us = (static_cast<uint32_t>(static_cast<uint8_t>(frame[3])) << 24) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(frame[4])) << 16) |
                 (static_cast<uint32_t>(static_cast<uint8_t>(frame[5])) << 8) |
                 static_cast<uint32_t>(static_cast<uint8_t>(frame[6]));
  payload.assign(frame.data() + 9, length);
  return true;
}

void task(void*) {
#ifdef ARDUINO
  const TickType_t idle_delay = pdMS_TO_TICKS(5);
  uint32_t tokens = DefaultSettings::LOGGER_RATE_BURST;
  uint32_t last_refill_us = nowMicros();
  RecordStorage current;
  bool has_record = false;
  for (;;) {
    if (!has_record) {
      {
        Lock lock(false);
        if (!g_ring.pop(current)) {
          has_record = false;
        } else {
          has_record = true;
        }
      }
      if (!has_record) {
        vTaskDelay(idle_delay);
        continue;
      }
    }
    const uint32_t now_us = nowMicros();
    const uint32_t elapsed = now_us - last_refill_us;
    if (elapsed >= 1000000UL / DefaultSettings::LOGGER_RATE_LINES_PER_SEC) {
      const uint32_t refill = elapsed * DefaultSettings::LOGGER_RATE_LINES_PER_SEC / 1000000UL;
      tokens = std::min(tokens + refill, DefaultSettings::LOGGER_RATE_BURST);
      last_refill_us = now_us;
    }
    if (tokens == 0) {
      vTaskDelay(idle_delay);
      continue;
    }
    --tokens;
    std::string text = renderRecord(current);
    if (Serial) {
      Serial.println(text.c_str());
      if (DefaultSettings::SERIAL_FLUSH_AFTER_LOG) {
        Serial.flush();
      }
    }
    LogHook::append(text.c_str());
    has_record = false;
  }
#else
  (void)0; // На хосте задача не используется
#endif
}

} // namespace Logger

