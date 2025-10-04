#include "radio_sx1262.h"
#include "default_settings.h"
#include "libs/config_loader/config_loader.h" // доступ к загруженной конфигурации
#include "libs/radio/frequency_tables.h"      // таблицы частот и описания
#include <Arduino.h>
#include <cmath>
#include <array>
#include <cstring>
#include <type_traits>
#include <utility>
#include <cstdio>
#include <algorithm>

#ifndef RADIOLIB_SX126X_IRQ_NONE
#define RADIOLIB_SX126X_IRQ_NONE 0U
#endif

#ifndef RADIOLIB_SX126X_IRQ_ALL
#define RADIOLIB_SX126X_IRQ_ALL 0xFFFFU
#endif

RadioSX1262* RadioSX1262::instance_ = nullptr; // инициализация статического указателя
bool RadioSX1262::irqLoggerStarted_ = false;   // отметка о выводе стартового сообщения

// Максимальный размер пакета для SX1262
static constexpr size_t MAX_PACKET_SIZE = 245;

// RAII-обёртка для автоматического освобождения мьютекса радиомодуля
class ScopedRadioLock {
public:
  explicit ScopedRadioLock(RadioSX1262& radio) : radio_(radio) {}
  int16_t acquire(TickType_t timeout) {
    const int16_t state = radio_.lockRadio(timeout);
    locked_ = (state == RADIOLIB_ERR_NONE);
    return state;
  }
  ~ScopedRadioLock() {
    if (locked_) {
      radio_.unlockRadio();
    }
  }
  bool locked() const { return locked_; }

private:
  RadioSX1262& radio_;
  bool locked_ = false;
};

const float* RadioSX1262::fRX_bank_[6] = {
    frequency_tables::RX_EAST,
    frequency_tables::RX_WEST,
    frequency_tables::RX_TEST,
    frequency_tables::RX_ALL,
    frequency_tables::RX_HOME,
    frequency_tables::RX_NEW};
const float* RadioSX1262::fTX_bank_[6] = {
    frequency_tables::TX_EAST,
    frequency_tables::TX_WEST,
    frequency_tables::TX_TEST,
    frequency_tables::TX_ALL,
    frequency_tables::TX_HOME,
    frequency_tables::TX_NEW};
const uint16_t RadioSX1262::BANK_CHANNELS_[6] = {
    static_cast<uint16_t>(frequency_tables::EAST_BANK_SIZE),
    static_cast<uint16_t>(frequency_tables::WEST_BANK_SIZE),
    static_cast<uint16_t>(frequency_tables::TEST_BANK_SIZE),
    static_cast<uint16_t>(frequency_tables::ALL_BANK_SIZE),
    static_cast<uint16_t>(frequency_tables::HOME_BANK_SIZE),
    static_cast<uint16_t>(frequency_tables::NEW_BANK_SIZE)};

uint16_t RadioSX1262::bankSize(ChannelBank bank) {
  // Возвращаем количество каналов для указанного банка
  return BANK_CHANNELS_[static_cast<int>(bank)];
}

float RadioSX1262::bankRx(ChannelBank bank, uint16_t ch) {
  // Возвращаем частоту приёма для канала в выбранном банке
  return fRX_bank_[static_cast<int>(bank)][ch];
}

float RadioSX1262::bankTx(ChannelBank bank, uint16_t ch) {
  // Возвращаем частоту передачи для канала в выбранном банке
  return fTX_bank_[static_cast<int>(bank)][ch];
}

const char* RadioSX1262::bankDescription(ChannelBank bank) {
  // Возвращаем текстовое описание банка частот
  return frequency_tables::getBank(bank).description;
}

const int8_t RadioSX1262::Pwr_[10] = {-5, -2, 1, 4, 7, 10, 13, 16, 19, 22};
const float RadioSX1262::BW_[5] = {7.81, 10.42, 15.63, 20.83, 31.25};
const int8_t RadioSX1262::SF_[8] = {5, 6, 7, 8, 9, 10, 11, 12};
const int8_t RadioSX1262::CR_[4] = {5, 6, 7, 8};

RadioSX1262::RadioSX1262() : radio_(new Module(5, 26, 27, 25)) {}

bool RadioSX1262::begin() {
  instance_ = this;               // сохраняем указатель на объект
  if (!irqLoggerStarted_) {       // выводим стартовый лог только один раз
    DEBUG_LOG("IRQ logger started");
    irqLoggerStarted_ = true;
  }
  return resetToDefaults() == RADIOLIB_ERR_NONE; // применяем настройки по умолчанию
}

int16_t RadioSX1262::send(const uint8_t* data, size_t len) {
  if (!data || len == 0) {                   // проверка указателя и длины
    DEBUG_LOG("RadioSX1262: пустая передача");
    lastError_ = ERR_INVALID_ARGUMENT;
    return lastError_;
  }
  if (len > MAX_PACKET_SIZE) {               // проверка аппаратного лимита
    LOG_ERROR_VAL("RadioSX1262: превышен лимит длины=", len);
    lastError_ = ERR_INVALID_ARGUMENT;
    return lastError_;
  }

  ScopedRadioLock guard(*this);
  const int16_t lockState = guard.acquire(toTicks(LOCK_TIMEOUT_MS));
  if (lockState != RADIOLIB_ERR_NONE) {
    LOG_WARN("RadioSX1262: отправка отклонена — радио занято");
    lastError_ = lockState;
    return lastError_;
  }

  float freq_tx = fTX_bank_[static_cast<int>(bank_)][channel_];
  float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];
  DEBUG_LOG_VAL("RadioSX1262: отправка длины=", len);
  if (!setFrequency(freq_tx)) {              // переключаемся на TX частоту
    LOG_ERROR("RadioSX1262: не удалось установить TX-частоту перед передачей");
    return lastError_;
  }
  int state = radio_.transmit((uint8_t*)data, len); // отправляем пакет
  if (state != RADIOLIB_ERR_NONE) {          // проверка кода ошибки
    lastError_ = state;                      // сохраняем код сбоя
    LOG_ERROR_VAL("RadioSX1262: ошибка передачи, код=", state);
    setFrequency(freq_rx);                   // возвращаемся на частоту приёма после сбоя
    startReceiveWithRetry("send: возврат к приёму после ошибки передачи");
    return lastError_;
  }
  lastError_ = RADIOLIB_ERR_NONE;            // предыдущая операция прошла успешно
  setFrequency(freq_rx);                     // возвращаем частоту приёма
  startReceiveWithRetry("send: переход к приёму после передачи");
  DEBUG_LOG("RadioSX1262: запуск приёма после завершения передачи");
  DEBUG_LOG("RadioSX1262: передача завершена");
  return lastError_;
}

int16_t RadioSX1262::ping(const uint8_t* data, size_t len,
                          uint8_t* response, size_t responseCapacity,
                          size_t& receivedLen, uint32_t timeoutUs,
                          uint32_t& elapsedUs) {
  receivedLen = 0;                                        // сбрасываем длину ответа
  elapsedUs = 0;                                          // сбрасываем время
  if (!data || len == 0 || !response || responseCapacity == 0) {
    lastError_ = ERR_INVALID_ARGUMENT;                    // некорректные аргументы
    return lastError_;
  }

  ScopedRadioLock guard(*this);
  const int16_t lockState = guard.acquire(toTicks(LOCK_TIMEOUT_MS));
  if (lockState != RADIOLIB_ERR_NONE) {
    LOG_WARN("RadioSX1262: пинг отклонён — радио занято");
    lastError_ = lockState;
    return lastError_;
  }

  float freq_tx = fTX_bank_[static_cast<int>(bank_)][channel_];
  float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];

  if (!setFrequency(freq_tx)) {                           // не удалось установить TX
    LOG_ERROR("RadioSX1262: не удалось установить TX-частоту перед пингом");
    return lastError_;
  }
  int state = radio_.transmit((uint8_t*)data, len);       // отправляем пакет
  if (state != RADIOLIB_ERR_NONE) {                       // ошибка передачи
    lastError_ = state;                                   // фиксируем код ошибки
    setFrequency(freq_rx);
    startReceiveWithRetry("ping: возврат в RX после ошибки передачи");
    return lastError_;
  }
  if (!setFrequency(freq_rx)) {                           // возвращаем RX частоту
    startReceiveWithRetry("ping: попытка приёма после ошибки установки частоты");
    return lastError_;
  }

  packetReady_ = false;                                   // очищаем флаг готовности
  startReceiveWithRetry("ping: ожидание ответа");        // слушаем эфир
  flushPendingIrqLog();                                   // разбираем отложенные IRQ до входа в цикл ожидания
  DEBUG_LOG("RadioSX1262: запуск ожидания ответа после пинга");
  DEBUG_LOG_VAL("RadioSX1262: таймаут ожидания, мкс=", timeoutUs);
  uint32_t start = micros();                              // стартовое время ожидания
  std::array<uint8_t, 256> buf{};                         // временный буфер
  bool waitingLogged = false;                             // отметка первого ожидания

  while ((micros() - start) < timeoutUs) {                // ждём ответ с таймаутом
    flushPendingIrqLog();                                 // обрабатываем накопленные IRQ перед проверкой флага готовности
    if (!packetReady_) {                                  // пакета пока нет
      if (!waitingLogged) {                               // фиксируем начало ожидания
        DEBUG_LOG("RadioSX1262: ожидание готовности пакета");
        waitingLogged = true;
      }
      delay(1);
      continue;
    }
    size_t lenRead = radio_.getPacketLength();            // узнаём длину принятого пакета
    if (lenRead == 0 || lenRead > buf.size()) {           // защита от некорректного размера
      LOG_WARN_VAL("RadioSX1262: некорректная длина принятого пинг-ответа=", lenRead);
      startReceiveWithRetry("ping: повторный запуск приёма после некорректной длины");
      lastError_ = ERR_INVALID_ARGUMENT;
      return lastError_;
    }
    int rxState = radio_.readData(buf.data(), lenRead);   // читаем пакет
    if (rxState != RADIOLIB_ERR_NONE) {                   // ошибка чтения
      lastError_ = rxState;                               // сохраняем код ошибки чтения
      LOG_ERROR_VAL("RadioSX1262: ошибка чтения пинг-ответа, код=", rxState);
      startReceiveWithRetry("ping: перезапуск после чтения ответа");
      return lastError_;
    }
    receivedLen = lenRead;                                // сохраняем длину ответа
    std::memcpy(response, buf.data(),                     // копируем в пользовательский буфер
                std::min(receivedLen, responseCapacity));
    elapsedUs = micros() - start;                         // вычисляем затраченное время
    lastSnr_ = radio_.getSNR();
    lastRssi_ = radio_.getRSSI();
    DEBUG_LOG("RadioSX1262: пинг-ответ получен, длина=%u, время=%lu мкс",
              static_cast<unsigned>(receivedLen),
              static_cast<unsigned long>(elapsedUs));
    packetReady_ = false;
    startReceiveWithRetry("ping: ожидание ответа");
    lastError_ = RADIOLIB_ERR_NONE;
    return lastError_;
  }

  startReceiveWithRetry("ping: ожидание ответа");
  LOG_WARN("RadioSX1262: пинг-ответ не получен — истёк таймаут %lu мкс",
           static_cast<unsigned long>(timeoutUs));
  lastError_ = ERR_PING_TIMEOUT;
  return lastError_;
}



void RadioSX1262::setReceiveCallback(RxCallback cb) { rx_cb_ = cb; }

int16_t RadioSX1262::ensureReceiveMode() {
  ScopedRadioLock guard(*this);
  const int16_t lockState = guard.acquire(toTicks(LOCK_TIMEOUT_MS));
  if (lockState != RADIOLIB_ERR_NONE) {
    LOG_WARN("RadioSX1262: ensureReceiveMode отклонён — радио занято");
    lastError_ = lockState;
    return lastError_;
  }
  const float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];
  if (!setFrequency(freq_rx)) {
    LOG_ERROR("RadioSX1262: не удалось восстановить RX-частоту в ensureReceiveMode");
    return lastError_;
  }
  if (!startReceiveWithRetry("ensureReceiveMode: переход к приёму")) {
    return lastError_;
  }
  DEBUG_LOG("RadioSX1262: принудительный переход в режим приёма на частоте %.3f МГц",
            static_cast<double>(freq_rx));
  lastError_ = RADIOLIB_ERR_NONE;
  return lastError_;
}

// Получить SNR последнего принятого пакета
float RadioSX1262::getLastSnr() const { return lastSnr_; }

// Получить RSSI последнего принятого пакета
float RadioSX1262::getLastRssi() const { return lastRssi_; }

// Получить случайный байт из встроенного генератора
uint8_t RadioSX1262::randomByte() { return radio_.randomByte(); }

bool RadioSX1262::setBank(ChannelBank bank) {
  bank_ = bank;
  channel_ = 0;
  return setFrequency(fRX_bank_[static_cast<int>(bank_)][channel_]);
}

bool RadioSX1262::setChannel(uint8_t ch) {
  // проверяем, что канал входит в диапазон текущего банка
  if (ch >= BANK_CHANNELS_[static_cast<int>(bank_)]) return false;
  channel_ = ch;
  return setFrequency(fRX_bank_[static_cast<int>(bank_)][channel_]);
}

bool RadioSX1262::setFrequency(float freq) {
  int state = radio_.setFrequency(freq);      // задаём частоту
  lastError_ = state;                         // запоминаем результат операции
  return state == RADIOLIB_ERR_NONE;          // возвращаем успех
}

bool RadioSX1262::startReceiveWithRetry(const char* context) {
  constexpr uint8_t kMaxAttempts = 3;                     // ограничиваем число повторов
  const char* ctx = (context && context[0] != '\0') ? context : "без контекста";
  int16_t lastState = RADIOLIB_ERR_NONE;
  for (uint8_t attempt = 1; attempt <= kMaxAttempts; ++attempt) {
    const int16_t state = radio_.startReceive();          // пробуем запустить приём
    if (state == RADIOLIB_ERR_NONE) {
      lastError_ = RADIOLIB_ERR_NONE;                     // фиксируем успешное состояние
      if (attempt > 1) {
        DEBUG_LOG("RadioSX1262: startReceive() успешен с %u-й попытки (%s)",
                  static_cast<unsigned>(attempt), ctx);
      }
      return true;
    }
    lastState = state;
    char prefix[192];
    std::snprintf(prefix, sizeof(prefix),
                  "RadioSX1262: startReceive() попытка %u (%s) завершилась ошибкой, код=",
                  static_cast<unsigned>(attempt), ctx);
    LOG_ERROR_VAL(prefix, state);                          // фиксируем код ошибки
    lastError_ = state;                                    // сохраняем код для внешнего запроса
#if defined(RADIOLIB_ERR_CHANNEL_BUSY)
    const bool channelBusy = (state == RADIOLIB_ERR_CHANNEL_BUSY);
#elif defined(RADIOLIB_ERR_CHANNEL_BUSY_LBT)
    const bool channelBusy = (state == RADIOLIB_ERR_CHANNEL_BUSY_LBT);
#else
    const bool channelBusy = false;
#endif
    if (channelBusy) {                                      // канал занят — выполняем программный сброс
      const int16_t resetState = radio_.reset();
      if (resetState != RADIOLIB_ERR_NONE) {
        LOG_WARN_VAL("RadioSX1262: программный сброс после занятого канала вернул код=", resetState);
      } else {
        DEBUG_LOG("RadioSX1262: выполнен программный сброс SX1262 после занятого канала (%s)", ctx);
      }
    }
  }
  lastError_ = lastState;                                   // сохраняем код последней ошибки
  char message[192];
  std::snprintf(message, sizeof(message),
                "RadioSX1262: не удалось запустить приём после %u попыток (%s)",
                static_cast<unsigned>(kMaxAttempts), ctx);
  LOG_ERROR("%s", message);
  return false;
}

bool RadioSX1262::setBandwidth(float bw) {
  int idx = -1;
  for (int i = 0; i < 5; ++i) {
    if (std::fabs(BW_[i] - bw) < 0.01f) { idx = i; break; }
  }
  if (idx < 0) return false;                           // значение не из таблицы
  bw_preset_ = idx;
  int state = radio_.setBandwidth(bw);                 // задаём полосу пропускания
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setSpreadingFactor(int sf) {
  int idx = -1;
  for (int i = 0; i < 8; ++i) {
    if (SF_[i] == sf) { idx = i; break; }
  }
  if (idx < 0) return false;                           // недопустимое значение
  sf_preset_ = idx;
  int state = radio_.setSpreadingFactor(sf);           // задаём фактор расширения
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setCodingRate(int cr) {
  int idx = -1;
  for (int i = 0; i < 4; ++i) {
    if (CR_[i] == cr) { idx = i; break; }
  }
  if (idx < 0) return false;                           // недопустимое значение
  cr_preset_ = idx;
  int state = radio_.setCodingRate(cr);                // задаём коэффициент кодирования
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setPower(uint8_t preset) {
  if (preset >= 10) return false;                      // индекс вне диапазона
  pw_preset_ = preset;
  int state = radio_.setOutputPower(Pwr_[pw_preset_]); // установка мощности
  return state == RADIOLIB_ERR_NONE;                   // возвращаем успех
}

bool RadioSX1262::setRxBoostedGainMode(bool enabled) {
  int state = radio_.setRxBoostedGainMode(enabled, true); // устанавливаем режим LNA
  if (state == RADIOLIB_ERR_NONE) {
    rxBoostedGainEnabled_ = enabled;                     // сохраняем текущее состояние
    return true;
  }
  return false;
}

int16_t RadioSX1262::resetToDefaults() {
  ScopedRadioLock guard(*this);
  const int16_t lockState = guard.acquire(toTicks(LOCK_TIMEOUT_MS));
  if (lockState != RADIOLIB_ERR_NONE) {
    LOG_WARN("RadioSX1262: resetToDefaults отклонён — радио занято");
    lastError_ = lockState;
    return lastError_;
  }

  lastError_ = RADIOLIB_ERR_NONE;                // сбрасываем сохранённый код ошибки
  const auto& cfg = ConfigLoader::getConfig();   // читаем загруженную конфигурацию
  bank_ = cfg.radio.bank;                        // банк каналов
  uint16_t bankSize = BANK_CHANNELS_[static_cast<int>(bank_)];
  if (bankSize == 0) {
    bankSize = 1; // защита от деления на ноль, хотя такого не ожидается
  }
  if (cfg.radio.channel >= bankSize) {
    channel_ = static_cast<uint8_t>(bankSize - 1);
    LOG_WARN("RadioSX1262: канал %u вне диапазона банка, выбран %u",
             static_cast<unsigned>(cfg.radio.channel),
             static_cast<unsigned>(channel_));
  } else {
    channel_ = cfg.radio.channel; // канал
  }
  if (cfg.radio.powerPreset >= std::size(Pwr_)) {
    pw_preset_ = static_cast<uint8_t>(std::size(Pwr_) - 1);
    LOG_WARN("RadioSX1262: powerPreset %u превышает лимит, используется %u",
             static_cast<unsigned>(cfg.radio.powerPreset),
             static_cast<unsigned>(pw_preset_));
  } else {
    pw_preset_ = cfg.radio.powerPreset; // мощность
  }
  if (cfg.radio.bwPreset >= std::size(BW_)) {
    bw_preset_ = static_cast<uint8_t>(std::size(BW_) - 1);
    LOG_WARN("RadioSX1262: bwPreset %u превышает лимит, используется %u",
             static_cast<unsigned>(cfg.radio.bwPreset),
             static_cast<unsigned>(bw_preset_));
  } else {
    bw_preset_ = cfg.radio.bwPreset;    // полоса
  }
  if (cfg.radio.sfPreset >= std::size(SF_)) {
    sf_preset_ = static_cast<uint8_t>(std::size(SF_) - 1);
    LOG_WARN("RadioSX1262: sfPreset %u превышает лимит, используется %u",
             static_cast<unsigned>(cfg.radio.sfPreset),
             static_cast<unsigned>(sf_preset_));
  } else {
    sf_preset_ = cfg.radio.sfPreset;    // фактор расширения
  }
  if (cfg.radio.crPreset >= std::size(CR_)) {
    cr_preset_ = static_cast<uint8_t>(std::size(CR_) - 1);
    LOG_WARN("RadioSX1262: crPreset %u превышает лимит, используется %u",
             static_cast<unsigned>(cfg.radio.crPreset),
             static_cast<unsigned>(cr_preset_));
  } else {
    cr_preset_ = cfg.radio.crPreset;    // коэффициент кодирования
  }

  int state = radio_.begin(
      fRX_bank_[static_cast<int>(bank_)][channel_],
      BW_[bw_preset_], SF_[sf_preset_], CR_[cr_preset_],
      0x18, Pwr_[pw_preset_], DefaultSettings::PREAMBLE_LENGTH, tcxo_, false);
  if (state != RADIOLIB_ERR_NONE) {
    lastError_ = state;                             // сохраняем код ошибки инициализации
    return lastError_;                              // ошибка инициализации
  }
  radio_.setDio1Action(onDio1Static);                     // колбэк приёма
  if (!setRxBoostedGainMode(cfg.radio.rxBoostedGain)) {   // применение усиления по умолчанию
    rxBoostedGainEnabled_ = false;                        // фиксируем фактическое состояние
    LOG_WARN("RadioSX1262: не удалось установить RX boosted gain");
  }
  if (!startReceiveWithRetry("resetToDefaults: запуск приёма")) {
    return lastError_;                                    // не удалось войти в режим RX
  }
  DEBUG_LOG("RadioSX1262: запуск приёма после применения настроек по умолчанию на частоте %.3f МГц",
            static_cast<double>(fRX_bank_[static_cast<int>(bank_)][channel_]));
  lastError_ = RADIOLIB_ERR_NONE;
  return lastError_;
}


void RadioSX1262::onDio1Static() {
  if (instance_) {
    instance_->handleDio1();
  }
}

// Формирование человекочитаемой строки с активными IRQ-флагами без динамических аллокаций
size_t formatIrqLogMessage(uint32_t flags, char* buffer, size_t capacity) {
  if (!buffer || capacity == 0) {
    return 0;
  }
  buffer[0] = '\0';

  static constexpr struct {
    uint32_t mask;
    const char* name;
    const char* description;
  } kIrqMap[] = {
      {RADIOLIB_SX126X_IRQ_TX_DONE, "IRQ_TX_DONE", "передача пакета завершена"},
      {RADIOLIB_SX126X_IRQ_RX_DONE, "IRQ_RX_DONE", "пакет принят"},
      {RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED, "IRQ_PREAMBLE_DETECTED", "найдена преамбула"},
      {RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID, "IRQ_SYNCWORD_VALID", "синхрослово совпало"},
      {RADIOLIB_SX126X_IRQ_HEADER_VALID, "IRQ_HEADER_VALID", "корректный LoRa-заголовок принят"},
      {RADIOLIB_SX126X_IRQ_HEADER_ERR, "IRQ_HEADER_ERR", "ошибка заголовка (битые биты/несовпадение CRC заголовка)"},
      {RADIOLIB_SX126X_IRQ_CRC_ERR, "IRQ_CRC_ERR", "ошибка CRC полезной нагрузки"},
#if defined(RADIOLIB_IRQ_RX_TIMEOUT) || defined(RADIOLIB_IRQ_TX_TIMEOUT)
#ifdef RADIOLIB_IRQ_RX_TIMEOUT
      {RADIOLIB_IRQ_RX_TIMEOUT, "IRQ_RX_TIMEOUT", "истёк таймаут ожидания приёма"},
#endif
#ifdef RADIOLIB_IRQ_TX_TIMEOUT
      {RADIOLIB_IRQ_TX_TIMEOUT, "IRQ_TX_TIMEOUT", "истёк таймаут попытки передачи"},
#endif
#else
      {RADIOLIB_SX126X_IRQ_TIMEOUT, "IRQ_RX_TX_TIMEOUT", "сработал таймаут приёма/передачи"},
#endif
      {RADIOLIB_SX126X_IRQ_CAD_DONE, "IRQ_CAD_DONE", "завершено сканирование канала (Channel Activity Detection)"},
      {RADIOLIB_SX126X_IRQ_CAD_DETECTED, "IRQ_CAD_DETECTED", "в канале обнаружена активность LoRa"},
  };

  const uint32_t effectiveMask = flags & 0xFFFFU;
  if (effectiveMask == RADIOLIB_SX126X_IRQ_NONE) {
    std::snprintf(buffer, capacity, "RadioSX1262: IRQ-флаги отсутствуют (маска=0x0000)");
    return std::strlen(buffer);
  }

  char knownFlags[512];
  knownFlags[0] = '\0';
  size_t offset = 0U;
  uint32_t knownMask = 0U;
  for (const auto& entry : kIrqMap) {
    if ((effectiveMask & entry.mask) == 0U) {
      continue;
    }
    const char* format = (offset == 0U) ? "%s – %s" : " | %s – %s";
    const int written = std::snprintf(knownFlags + offset,
                                      sizeof(knownFlags) - offset,
                                      format, entry.name, entry.description);
    if (written < 0) {
      knownFlags[offset] = '\0';
      break;
    }
    const size_t writtenSize = static_cast<size_t>(written);
    if (writtenSize >= sizeof(knownFlags) - offset) {
      offset = sizeof(knownFlags) - 1U;
    } else {
      offset += writtenSize;
    }
    knownMask |= entry.mask;
  }

  const uint32_t unknownMask = effectiveMask & ~knownMask;
  if (knownFlags[0] != '\0') {
    if (unknownMask != 0U) {
      std::snprintf(buffer, capacity,
                    "RadioSX1262: IRQ=0x%04lX, расшифровка: [%s], неизвестные биты=0x%04lX",
                    static_cast<unsigned long>(effectiveMask),
                    knownFlags,
                    static_cast<unsigned long>(unknownMask));
    } else {
      std::snprintf(buffer, capacity,
                    "RadioSX1262: IRQ=0x%04lX, расшифровка: [%s]",
                    static_cast<unsigned long>(effectiveMask),
                    knownFlags);
    }
  } else {
    std::snprintf(buffer, capacity,
                  "RadioSX1262: IRQ=0x%04lX, известные флаги отсутствуют",
                  static_cast<unsigned long>(effectiveMask));
  }
  return std::strlen(buffer);
}

void RadioSX1262::logIrqFlags(uint32_t flags) {
  char message[224];
  formatIrqLogMessage(flags, message, sizeof(message));
  DEBUG_LOG("%s", message);
}

void RadioSX1262::flushPendingIrqLog() {
  // Обеспечиваем обходной ручной вызов переноса IRQ-логов из ISR в основной поток
  processPendingIrqLog();
}

void RadioSX1262::handleDio1() {
  irqNeedsRead_ = true;   // отмечаем необходимость чтения IRQ-регистров в основном потоке
  irqLogPending_ = true;  // помечаем, что требуется вывод в loop()
}

void RadioSX1262::processPendingIrqLog() {
  bool hasPending = false;
  bool needRead = false;
  bool shouldMarkPacketReady = false;   // итоговая оценка необходимости пометить пакет как готовый
  bool needRxRecovery = false;          // требуется ли перезапуск приёма после ошибок

#if defined(ARDUINO)
  noInterrupts();
#endif
  if (irqNeedsRead_) {
    irqNeedsRead_ = false;
    needRead = true;  // фиксируем запрос на чтение IRQ-регистров
  }
  if (irqLogPending_) {
    hasPending = true;
    irqLogPending_ = false; // сбрасываем флаг только при чтении актуальных данных
  }
#if defined(ARDUINO)
  interrupts();
#endif

  if (!hasPending && !needRead) {
    return; // нет отложенных событий для вывода
  }

  if (needRead) {
    const uint32_t flags = radio_.getIrqFlags();  // считываем флаги IRQ вне ISR
    const int16_t clearState =
        radio_.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL); // очищаем IRQ вне ISR
    pendingIrqFlags_ = flags;                // сохраняем флаги для последующего логирования
    pendingIrqClearState_ = clearState;      // сохраняем код очистки IRQ
    hasPending = true;                       // гарантируем обработку свежих данных

    const bool hasRxDone = (flags & RADIOLIB_SX126X_IRQ_RX_DONE) != 0U; // полное завершение приёма
    const bool hasHeaderValid = (flags & RADIOLIB_SX126X_IRQ_HEADER_VALID) != 0U; // заголовок распознан
    const bool hasSyncValid = (flags & RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID) != 0U; // синхрослово подтверждено
    const bool hasCrcError = (flags & RADIOLIB_SX126X_IRQ_CRC_ERR) != 0U;
    const bool hasHeaderError = (flags & RADIOLIB_SX126X_IRQ_HEADER_ERR) != 0U;

    const bool hasRxErrors = hasCrcError || hasHeaderError; // есть ошибки декодирования

    const bool rxDataAvailable = hasRxDone && (radio_.available() > 0); // в буфере есть полезные данные
    shouldMarkPacketReady = rxDataAvailable && !hasRxErrors;             // пакет подтверждён и готов к чтению

    const bool hasEarlyIndicator = !hasRxDone && (hasHeaderValid || hasSyncValid); // есть ранние признаки приёма
    needRxRecovery = (hasRxDone || hasEarlyIndicator) && hasRxErrors;              // требуется перезапуск RX

    if (hasEarlyIndicator && !hasRxErrors) {
      DEBUG_LOG("RadioSX1262: зафиксирован ранний индикатор приёма (header/sync), ждём RX_DONE");
    }
  }

  const uint32_t flags = pendingIrqFlags_;
  const int16_t clearState = pendingIrqClearState_;

  const uint32_t eventUptime = millis();
  if (flags == RADIOLIB_SX126X_IRQ_NONE) {
    static const char kNoFlagsMsg[] = "RadioSX1262: событие DIO1 без активных флагов IRQ";
    DEBUG_LOG("%s", kNoFlagsMsg);
    if (irqCallback_) {
      irqCallback_(kNoFlagsMsg, eventUptime);
    }
  } else {
    char message[224];
    formatIrqLogMessage(flags, message, sizeof(message));
    DEBUG_LOG("%s", message);
    if (irqCallback_) {
      irqCallback_(message, eventUptime);
    }
  }

  if (clearState != RADIOLIB_ERR_NONE) {
    LOG_WARN_VAL("RadioSX1262: не удалось очистить статус IRQ, код=", clearState);
  }

  if (shouldMarkPacketReady) {
#if defined(ARDUINO)
    noInterrupts();
#endif
    packetReady_ = true;  // подтверждаем наличие настоящего принятого пакета
#if defined(ARDUINO)
    interrupts();
#endif
    DEBUG_LOG("RadioSX1262: RX_DONE подтверждён, пакет готов к чтению");
  } else {
    DEBUG_LOG("RadioSX1262: событие DIO1 проигнорировано фильтром RX (пакет ещё не готов)");
  }

  if (needRxRecovery) {
    startReceiveWithRetry("processPendingIrqLog: восстановление после ошибок приёма");
  }
}

void RadioSX1262::setIrqLogCallback(IrqLogCallback cb) {
  irqCallback_ = cb;
}

// Проверка флага готовности и чтение данных
void RadioSX1262::loop() {
  flushPendingIrqLog();                    // отложенный вывод статусов IRQ
  if (!packetReady_) {                      // пакет пока не готов
    return;
  }
  size_t len = radio_.getPacketLength();    // длина доступного пакета после фильтра IRQ
  // При завершении передачи длина пакета может быть мусорной и вызвать
  // выделение огромного буфера, что приводит к перезагрузке
  if (len == 0 || len > 256) {
    packetReady_ = false;                   // сбрасываем флаг
    startReceiveWithRetry("loop: восстановление после некорректной длины");
    DEBUG_LOG("RadioSX1262: перезапуск приёма после мусорной длины пакета в loop");
    return;
  }
  std::array<uint8_t, 256> buf;            // статический буфер на стеке
  int state = radio_.readData(buf.data(), len);
  if (state == RADIOLIB_ERR_NONE) {
    lastSnr_ = radio_.getSNR();             // сохраняем SNR
    lastRssi_ = radio_.getRSSI();           // сохраняем RSSI
    if (rx_cb_) {
      rx_cb_(buf.data(), len);              // передаём данные пользователю
    }
    lastError_ = RADIOLIB_ERR_NONE;         // читаем без ошибок
  } else {
    lastError_ = state;                     // фиксируем код сбоя чтения
  }
  packetReady_ = false;                     // пакет обработан
  startReceiveWithRetry("loop: перезапуск приёма после обработки пакета");
  DEBUG_LOG("RadioSX1262: перезапуск приёма после обработки пакета в loop");
}

int16_t RadioSX1262::sendBeacon() {
  uint8_t beacon[15]{0};                  // буфер маяка
  beacon[1] = randomByte();               // случайный байт 1
  beacon[2] = randomByte();               // случайный байт 2
  beacon[0] = beacon[1] ^ beacon[2];      // идентификатор = XOR
  const char text[6] = {'B','E','A','C','O','N'}; // надпись "BEACON"
  memcpy(&beacon[9], text, 6);            // вставка текста
  return send(beacon, sizeof(beacon));    // отправляем как обычный пакет
}

TickType_t RadioSX1262::toTicks(uint32_t timeoutMs) const {
#if defined(ARDUINO)
  return pdMS_TO_TICKS(timeoutMs);
#else
  return static_cast<TickType_t>(timeoutMs);
#endif
}

int16_t RadioSX1262::lockRadio(TickType_t timeout) {
#if defined(ARDUINO)
  if (radioMutex_ == nullptr) {
    radioMutex_ = xSemaphoreCreateMutex();             // создаём мьютекс при первом обращении
    if (radioMutex_ == nullptr) {
      LOG_ERROR("RadioSX1262: не удалось создать мьютекс доступа к SX1262");
      return ERR_TIMEOUT;
    }
  }
  const BaseType_t taken = xSemaphoreTake(radioMutex_, timeout);
  return (taken == pdTRUE) ? RADIOLIB_ERR_NONE : ERR_TIMEOUT;
#else
  if (timeout == portMAX_DELAY) {
    radioMutex_.lock();
    return RADIOLIB_ERR_NONE;
  }
  const auto duration = std::chrono::milliseconds(timeout);
  if (radioMutex_.try_lock_for(duration)) {
    return RADIOLIB_ERR_NONE;
  }
  return ERR_TIMEOUT;
#endif
}

void RadioSX1262::unlockRadio() {
#if defined(ARDUINO)
  if (radioMutex_ != nullptr) {
    xSemaphoreGive(radioMutex_);
  }
#else
  radioMutex_.unlock();
#endif
}
