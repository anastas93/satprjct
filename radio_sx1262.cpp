#include "radio_sx1262.h"
#include "default_settings.h"
#include <Arduino.h>
#include <cmath>
#include <array>
#include <cstring>
#include <type_traits>
#include <utility>
#include <cstdio>

#ifndef RADIOLIB_SX126X_IRQ_NONE
#define RADIOLIB_SX126X_IRQ_NONE 0U
#endif

#ifndef RADIOLIB_SX126X_IRQ_ALL
#define RADIOLIB_SX126X_IRQ_ALL 0xFFFFU
#endif

RadioSX1262* RadioSX1262::instance_ = nullptr; // инициализация статического указателя

// Максимальный размер пакета для SX1262
static constexpr size_t MAX_PACKET_SIZE = 245;

// Таблицы частот приёма и передачи для всех банков
// Восточный банк
static const float fRX_east_[10] = {
    251.900, 252.000, 253.800, 253.700, 257.000,
    257.100, 256.850, 258.650, 262.225, 262.325};
static const float fTX_east_[10] = {
    292.900, 293.100, 296.000, 294.700, 297.675,
    295.650, 297.850, 299.650, 295.825, 295.925};
// Западный банк
static const float fRX_west_[10] = {
    250.900, 252.050, 253.850, 262.175, 267.050,
    255.550, 251.950, 257.825, 260.125, 252.400};
static const float fTX_west_[10] = {
    308.300, 293.050, 294.850, 295.775, 308.050,
    296.550, 292.950, 297.075, 310.125, 309.700};
// Тестовый банк
static const float fRX_test_[10] = {
    250.000, 260.000, 270.000, 280.000, 290.000,
    300.000, 310.000, 433.000, 434.000, 446.000};
static const float fTX_test_[10] = {
    250.000, 260.000, 270.000, 280.000, 290.000,
    300.000, 310.000, 433.000, 434.000, 446.000};
// Полный банк из референса
static const float fRX_all_[167] = {
    243.625, 243.625, 243.800, 244.135, 244.275, 245.200, 245.800, 245.850, 245.950, 247.450,
    248.750, 248.825, 249.375, 249.400, 249.450, 249.450, 249.490, 249.530, 249.850, 249.850,
    249.890, 249.930, 250.090, 250.900, 251.275, 251.575, 251.600, 251.850, 251.900, 251.950,
    252.000, 252.050, 252.150, 252.200, 252.400, 252.450, 252.500, 252.550, 252.625, 253.550,
    253.600, 253.650, 253.700, 253.750, 253.800, 253.850, 253.850, 253.900, 254.000, 254.730,
    254.775, 254.830, 255.250, 255.350, 255.400, 255.450, 255.550, 255.550, 255.775, 256.450,
    256.600, 256.850, 256.900, 256.950, 257.000, 257.050, 257.100, 257.150, 257.150, 257.200,
    257.250, 257.300, 257.350, 257.500, 257.700, 257.775, 257.825, 257.900, 258.150, 258.350,
    258.450, 258.500, 258.550, 258.650, 259.000, 259.050, 259.975, 260.025, 260.075, 260.125,
    260.175, 260.375, 260.425, 260.425, 260.475, 260.525, 260.550, 260.575, 260.625, 260.675,
    260.675, 260.725, 260.900, 261.100, 261.100, 261.200, 262.000, 262.040, 262.075, 262.125,
    262.175, 262.175, 262.225, 262.275, 262.275, 262.325, 262.375, 262.425, 263.450, 263.500,
    263.575, 263.625, 263.625, 263.675, 263.725, 263.725, 263.775, 263.825, 263.875, 263.925,
    265.250, 265.350, 265.400, 265.450, 265.500, 265.550, 265.675, 265.850, 266.750, 266.850,
    266.900, 266.950, 267.050, 267.050, 267.100, 267.150, 267.200, 267.250, 267.400, 267.875,
    267.950, 268.000, 268.025, 268.050, 268.100, 268.150, 268.200, 268.250, 268.300, 268.350,
    268.400, 268.450, 269.700, 269.750, 269.800, 269.850, 269.950};
static const float fTX_all_[167] = {
    316.725, 300.400, 298.200, 296.075, 300.250, 312.850, 298.650, 314.230, 299.400, 298.800,
    306.900, 294.375, 316.975, 300.975, 299.000, 312.750, 313.950, 318.280, 316.250, 298.830,
    300.500, 308.750, 312.600, 308.300, 296.500, 308.450, 298.225, 292.850, 292.900, 292.950,
    293.100, 293.050, 293.150, 299.150, 309.700, 309.750, 309.800, 309.850, 309.925, 294.550,
    295.950, 294.650, 294.700, 294.750, 296.000, 294.850, 294.850, 307.500, 298.630, 312.550,
    310.800, 296.200, 302.425, 296.350, 296.400, 296.450, 296.550, 296.550, 309.300, 313.850,
    305.950, 297.850, 296.100, 297.950, 297.675, 298.050, 295.650, 298.150, 298.150, 308.800,
    309.475, 309.725, 307.200, 311.350, 316.150, 311.375, 297.075, 298.000, 293.200, 299.350,
    299.450, 299.500, 299.550, 299.650, 317.925, 317.975, 310.050, 310.225, 310.275, 310.125,
    310.325, 292.975, 297.575, 294.025, 294.075, 294.125, 296.775, 294.175, 294.225, 294.475,
    294.275, 294.325, 313.900, 298.380, 298.700, 294.950, 314.200, 307.075, 306.975, 295.725,
    297.025, 295.775, 295.825, 295.875, 300.275, 295.925, 295.975, 296.025, 311.400, 309.875,
    297.175, 297.225, 297.225, 297.275, 297.325, 297.325, 297.375, 297.425, 297.475, 297.525,
    306.250, 306.350, 294.425, 306.450, 302.525, 306.550, 306.675, 306.850, 316.575, 307.850,
    297.625, 307.950, 308.050, 308.050, 308.100, 308.150, 308.200, 308.250, 294.900, 310.375,
    310.450, 310.475, 309.025, 310.550, 310.600, 309.150, 296.050, 309.250, 309.300, 309.350,
    295.900, 309.450, 309.925, 310.750, 310.025, 310.850, 310.950};

// Домашний банк: короткий набор частот из полного списка ALL
static const float fRX_home_[7] = {
    263.450, 257.700, 257.200, 256.450, 267.250, 250.090, 249.850};
static const float fTX_home_[7] = {
    311.400, 316.150, 308.800, 398.800, 308.250, 312.600, 298.830};

const float* RadioSX1262::fRX_bank_[5] = {
    fRX_east_, fRX_west_, fRX_test_, fRX_all_, fRX_home_};
const float* RadioSX1262::fTX_bank_[5] = {
    fTX_east_, fTX_west_, fTX_test_, fTX_all_, fTX_home_};
const uint16_t RadioSX1262::BANK_CHANNELS_[5] = {10, 10, 10, 167, 7};

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

const int8_t RadioSX1262::Pwr_[10] = {-5, -2, 1, 4, 7, 10, 13, 16, 19, 22};
const float RadioSX1262::BW_[5] = {7.81, 10.42, 15.63, 20.83, 31.25};
const int8_t RadioSX1262::SF_[8] = {5, 6, 7, 8, 9, 10, 11, 12};
const int8_t RadioSX1262::CR_[4] = {5, 6, 7, 8};

RadioSX1262::RadioSX1262() : radio_(new Module(5, 26, 27, 25)) {}

bool RadioSX1262::begin() {
  instance_ = this;               // сохраняем указатель на объект
  return resetToDefaults();       // применяем настройки по умолчанию
}

void RadioSX1262::send(const uint8_t* data, size_t len) {
  if (!data || len == 0) {                   // проверка указателя и длины
    DEBUG_LOG("RadioSX1262: пустая передача");
    return;
  }
  if (len > MAX_PACKET_SIZE) {               // проверка аппаратного лимита
    LOG_ERROR_VAL("RadioSX1262: превышен лимит длины=", len);
    return;
  }
  float freq_tx = fTX_bank_[static_cast<int>(bank_)][channel_];
  float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];
  DEBUG_LOG_VAL("RadioSX1262: отправка длины=", len);
  radio_.setFrequency(freq_tx);              // переключаемся на TX частоту
  int state = radio_.transmit((uint8_t*)data, len); // отправляем пакет
  if (state != RADIOLIB_ERR_NONE) {          // проверка кода ошибки
    LOG_ERROR_VAL("RadioSX1262: ошибка передачи, код=", state);
    return;                                  // выходим без смены частоты
  }
  radio_.setFrequency(freq_rx);              // возвращаем частоту приёма
  radio_.startReceive();                     // и продолжаем слушать эфир
  DEBUG_LOG("RadioSX1262: запуск приёма после завершения передачи");
  DEBUG_LOG("RadioSX1262: передача завершена");
}

bool RadioSX1262::ping(const uint8_t* data, size_t len,
                       uint8_t* response, size_t responseCapacity,
                       size_t& receivedLen, uint32_t timeoutUs,
                       uint32_t& elapsedUs) {
  receivedLen = 0;                                        // сбрасываем длину ответа
  elapsedUs = 0;                                          // сбрасываем время
  if (!data || len == 0 || !response || responseCapacity == 0) {
    return false;                                         // некорректные аргументы
  }

  float freq_tx = fTX_bank_[static_cast<int>(bank_)][channel_];
  float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];

  if (!setFrequency(freq_tx)) {                           // не удалось установить TX
    return false;
  }
  int state = radio_.transmit((uint8_t*)data, len);       // отправляем пакет
  if (state != RADIOLIB_ERR_NONE) {                       // ошибка передачи
    setFrequency(freq_rx);
    radio_.startReceive();
    return false;
  }
  if (!setFrequency(freq_rx)) {                           // возвращаем RX частоту
    radio_.startReceive();
    return false;
  }

  packetReady_ = false;                                   // очищаем флаг готовности
  radio_.startReceive();                                  // слушаем эфир
  DEBUG_LOG("RadioSX1262: запуск ожидания ответа после пинга");
  DEBUG_LOG_VAL("RadioSX1262: таймаут ожидания, мкс=", timeoutUs);
  uint32_t start = micros();                              // стартовое время ожидания
  std::array<uint8_t, 256> buf{};                         // временный буфер
  bool waitingLogged = false;                             // отметка первого ожидания

  while ((micros() - start) < timeoutUs) {                // ждём ответ с таймаутом
    if (!packetReady_) {                                  // пакета пока нет
      if (!waitingLogged) {                               // фиксируем начало ожидания
        DEBUG_LOG("RadioSX1262: ожидание готовности пакета");
        waitingLogged = true;
      }
      delay(1);
      continue;
    }
    packetReady_ = false;                                 // сбрасываем флаг
    waitingLogged = false;                                // разрешаем повторный лог
    size_t pktLen = radio_.getPacketLength();             // длина принятого пакета
    if (pktLen == 0 || pktLen > buf.size()) {             // защита от мусора
      radio_.startReceive();
      DEBUG_LOG("RadioSX1262: перезапуск приёма после некорректной длины пакета");
      continue;
    }
    int readState = radio_.readData(buf.data(), pktLen);  // читаем данные
    radio_.startReceive();                                // сразу возобновляем приём
    DEBUG_LOG("RadioSX1262: перезапуск приёма после чтения пакета");
    if (readState != RADIOLIB_ERR_NONE) {                 // не удалось прочитать
      continue;
    }
    lastSnr_ = radio_.getSNR();                           // сохраняем параметры
    lastRssi_ = radio_.getRSSI();
    if (pktLen == len && std::memcmp(buf.data(), data, len) == 0) {
      if (pktLen <= responseCapacity) {                   // помещается в буфер ответа
        std::memcpy(response, buf.data(), pktLen);
        receivedLen = pktLen;
        elapsedUs = micros() - start;                     // фиксируем время пролёта
        return true;                                      // пинг отработал
      }
      break;                                              // буфер мал, считаем тайм-аутом
    }
    if (rx_cb_) {                                         // чужой пакет передаём дальше
      rx_cb_(buf.data(), pktLen);
    }
  }

  elapsedUs = micros() - start;                           // время ожидания на выходе
  return false;                                           // пинг не удался
}

void RadioSX1262::setReceiveCallback(RxCallback cb) { rx_cb_ = cb; }

void RadioSX1262::ensureReceiveMode() {
  const float freq_rx = fRX_bank_[static_cast<int>(bank_)][channel_];
  radio_.setFrequency(freq_rx);
  radio_.startReceive();
  DEBUG_LOG("RadioSX1262: принудительный переход в режим приёма на частоте %.3f МГц",
            static_cast<double>(freq_rx));
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
  return state == RADIOLIB_ERR_NONE;          // возвращаем успех
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

bool RadioSX1262::resetToDefaults() {
  // возвращаем все параметры к значениям из файла default_settings.h
  bank_ = DefaultSettings::BANK;       // банк каналов
  channel_ = DefaultSettings::CHANNEL; // канал
  pw_preset_ = DefaultSettings::POWER_PRESET; // мощность
  bw_preset_ = DefaultSettings::BW_PRESET;    // полоса
  sf_preset_ = DefaultSettings::SF_PRESET;    // фактор расширения
  cr_preset_ = DefaultSettings::CR_PRESET;    // коэффициент кодирования

  int state = radio_.begin(
      fRX_bank_[static_cast<int>(bank_)][channel_],
      BW_[bw_preset_], SF_[sf_preset_], CR_[cr_preset_],
      0x18, Pwr_[pw_preset_], DefaultSettings::PREAMBLE_LENGTH, tcxo_, false);
  if (state != RADIOLIB_ERR_NONE) {
    return false;                                         // ошибка инициализации
  }
  radio_.setDio1Action(onDio1Static);                     // колбэк приёма
  if (!setRxBoostedGainMode(DefaultSettings::RX_BOOSTED_GAIN)) { // применение усиления по умолчанию
    rxBoostedGainEnabled_ = false;                         // фиксируем фактическое состояние
    LOG_WARN("RadioSX1262: не удалось установить RX boosted gain");
  }
  radio_.startReceive();                                  // начинаем приём
  DEBUG_LOG("RadioSX1262: запуск приёма после применения настроек по умолчанию на частоте %.3f МГц",
            static_cast<double>(fRX_bank_[static_cast<int>(bank_)][channel_]));
  return true;
}

void RadioSX1262::onDio1Static() {
  if (instance_) {
    instance_->handleDio1();
  }
}

void RadioSX1262::logIrqFlags(uint32_t flags) {
  // Карта соответствия аппаратных флагов IRQ их человекочитаемым именам
  static constexpr struct {
    uint32_t mask;
    const char* name;
  } kIrqMap[] = {
      {RADIOLIB_SX126X_IRQ_TX_DONE, "TX_DONE"},
      {RADIOLIB_SX126X_IRQ_RX_DONE, "RX_DONE"},
      {RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED, "PREAMBLE_DETECTED"},
      {RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID, "SYNCWORD_VALID"},
      {RADIOLIB_SX126X_IRQ_HEADER_VALID, "HEADER_VALID"},
      {RADIOLIB_SX126X_IRQ_HEADER_ERR, "HEADER_ERR"},
      {RADIOLIB_SX126X_IRQ_CRC_ERR, "CRC_ERR"},
      {RADIOLIB_SX126X_IRQ_TIMEOUT, "RX_TX_TIMEOUT"},
      {RADIOLIB_SX126X_IRQ_CAD_DONE, "CAD_DONE"},
      {RADIOLIB_SX126X_IRQ_CAD_DETECTED, "CAD_DETECTED"},
  };

  const uint32_t effectiveMask = flags & 0xFFFFU; // нормализуем маску к 16 битам
  if (effectiveMask == RADIOLIB_SX126X_IRQ_NONE) {
    DEBUG_LOG("RadioSX1262: IRQ-флаги отсутствуют (маска=0x0000)");
    return;
  }

  // Накапливаем человекочитаемые флаги в статическом буфере без динамических выделений
  char knownFlags[128];
  knownFlags[0] = '\0';
  size_t offset = 0U;
  uint32_t knownMask = 0U; // совокупная маска известных флагов
  for (const auto& entry : kIrqMap) {
    if ((effectiveMask & entry.mask) == 0U) {
      continue;
    }
    const char* format = (offset == 0U) ? "%s" : " | %s"; // добавляем разделитель при необходимости
    const int written = std::snprintf(knownFlags + offset,
                                      sizeof(knownFlags) - offset,
                                      format, entry.name);
    if (written < 0) {
      knownFlags[offset] = '\0'; // аварийно завершаем строку при ошибке форматирования
      break;
    }
    const size_t writtenSize = static_cast<size_t>(written);
    if (writtenSize >= sizeof(knownFlags) - offset) {
      offset = sizeof(knownFlags) - 1U; // защита от выхода за границы буфера
    } else {
      offset += writtenSize;
    }
    knownMask |= entry.mask; // учитываем распознанный флаг
  }

  const uint32_t unknownMask = effectiveMask & ~knownMask; // маска неизвестных флагов
  char message[224];
  if (knownFlags[0] != '\0') {
    if (unknownMask != 0U) {
      std::snprintf(message, sizeof(message),
                    "RadioSX1262: IRQ=0x%04lX, расшифровка: [%s], неизвестные биты=0x%04lX",
                    static_cast<unsigned long>(effectiveMask),
                    knownFlags,
                    static_cast<unsigned long>(unknownMask));
    } else {
      std::snprintf(message, sizeof(message),
                    "RadioSX1262: IRQ=0x%04lX, расшифровка: [%s]",
                    static_cast<unsigned long>(effectiveMask),
                    knownFlags);
    }
  } else {
    std::snprintf(message, sizeof(message),
                  "RadioSX1262: IRQ=0x%04lX, известные флаги отсутствуют",
                  static_cast<unsigned long>(effectiveMask));
  }

  DEBUG_LOG("%s", message);
}

void RadioSX1262::handleDio1() {
  uint32_t irqFlags = radio_.getIrqFlags();  // универсальное чтение флагов IRQ
  pendingIrqFlags_ = irqFlags;               // сохраняем флаги для вывода в основном потоке
  pendingIrqClearState_ =
      radio_.clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL); // сохраняем код очистки IRQ
  irqLogPending_ = true;                     // помечаем, что требуется вывод в loop()

  packetReady_ = true;                     // устанавливаем флаг готовности пакета
}

void RadioSX1262::processPendingIrqLog() {
  bool hasPending = false;
  uint32_t flags = 0;
  int16_t clearState = RADIOLIB_ERR_NONE;

#if defined(ARDUINO)
  noInterrupts();
#endif
  if (irqLogPending_) {
    hasPending = true;
    flags = pendingIrqFlags_;
    clearState = pendingIrqClearState_;
    irqLogPending_ = false; // сбрасываем флаг только при чтении актуальных данных
  }
#if defined(ARDUINO)
  interrupts();
#endif

  if (!hasPending) {
    return; // нет отложенных событий для вывода
  }

  if (flags == RADIOLIB_SX126X_IRQ_NONE) {
    DEBUG_LOG("RadioSX1262: событие DIO1 без активных флагов IRQ");
  } else {
    logIrqFlags(flags); // выводим только активные флаги
  }

  if (clearState != RADIOLIB_ERR_NONE) {
    LOG_WARN_VAL("RadioSX1262: не удалось очистить статус IRQ, код=", clearState);
  }

  DEBUG_LOG("RadioSX1262: событие DIO1, модуль сообщает о готовности пакета");
}

// Проверка флага готовности и чтение данных
void RadioSX1262::loop() {
  processPendingIrqLog();                  // отложенный вывод статусов IRQ
  if (!packetReady_) {                      // пакет пока не готов
    return;
  }
  size_t len = radio_.getPacketLength();
  // При завершении передачи длина пакета может быть мусорной и вызвать
  // выделение огромного буфера, что приводит к перезагрузке
  if (len == 0 || len > 256) {
    packetReady_ = false;                   // сбрасываем флаг
    radio_.startReceive();                  // возобновляем приём
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
  }
  packetReady_ = false;                     // пакет обработан
  radio_.startReceive();                    // снова слушаем эфир
  DEBUG_LOG("RadioSX1262: перезапуск приёма после обработки пакета в loop");
}

void RadioSX1262::sendBeacon() {
  uint8_t beacon[15]{0};                  // буфер маяка
  beacon[1] = randomByte();               // случайный байт 1
  beacon[2] = randomByte();               // случайный байт 2
  beacon[0] = beacon[1] ^ beacon[2];      // идентификатор = XOR
  const char text[6] = {'B','E','A','C','O','N'}; // надпись "BEACON"
  memcpy(&beacon[9], text, 6);            // вставка текста
  send(beacon, sizeof(beacon));           // отправляем как обычный пакет
}
