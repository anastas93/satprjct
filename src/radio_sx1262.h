#pragma once
#include <RadioLib.h>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "radio_interface.h"
#include "channel_bank.h"

// Вспомогательные признаки для определения доступного варианта IRQ-API в RadioLib
namespace radio_sx1262_detail {

template <typename T, typename = void>
struct HasIrqFlagsApi : std::false_type {};

template <typename T>
struct HasIrqFlagsApi<
    T,
    std::void_t<decltype(std::declval<T&>().getIrqFlags()),
                decltype(std::declval<T&>().clearIrqFlags(RADIOLIB_SX126X_IRQ_ALL))>>
    : std::true_type {};

template <typename T, typename = void>
struct HasZeroArgIrqStatusApi : std::false_type {};

template <typename T>
struct HasZeroArgIrqStatusApi<
    T,
    std::void_t<decltype(std::declval<T&>().getIrqStatus())>> : std::true_type {};

template <typename T, typename = void>
struct HasPointerIrqStatusApi : std::false_type {};

template <typename T>
struct HasPointerIrqStatusApi<
    T,
    std::void_t<decltype(
        std::declval<T&>().getIrqStatus(static_cast<uint16_t*>(nullptr)))>>
    : std::true_type {};

// Вызов варианта getIrqStatus() без аргументов, если он доступен
template <typename Radio>
auto CallZeroArgGetIrqStatus(Radio& radio, int)
    -> decltype(radio.getIrqStatus()) {
  return radio.getIrqStatus();
}

template <typename Radio>
uint16_t CallZeroArgGetIrqStatus(Radio&, long) {
  return 0U;
}

// Вызов варианта getIrqStatus(uint16_t*), если он доступен
template <typename Radio>
auto CallPointerGetIrqStatus(Radio& radio, uint16_t* dest, int)
    -> decltype(radio.getIrqStatus(dest)) {
  return radio.getIrqStatus(dest);
}

template <typename Radio>
int16_t CallPointerGetIrqStatus(Radio&, uint16_t*, long) {
  return RADIOLIB_ERR_NONE;
}

} // namespace radio_sx1262_detail

// Реализация радиоинтерфейса на базе SX1262
class RadioSX1262 : public IRadio {
public:
  RadioSX1262();
  // Инициализация модуля и установка параметров по умолчанию
  bool begin();
  // Отправка данных
  void send(const uint8_t* data, size_t len) override;
  // Выполнение пинга с ожиданием эха
  bool ping(const uint8_t* data, size_t len,
            uint8_t* response, size_t responseCapacity,
            size_t& receivedLen, uint32_t timeoutUs,
            uint32_t& elapsedUs);
  // Отправка служебного маяка
  void sendBeacon();
  // Обработка готовности пакета в основном цикле
  void loop();
  // Установка колбэка приёма
  void setReceiveCallback(RxCallback cb) override;
  // Возвращение в режим приёма
  void ensureReceiveMode() override;
  // Получение параметров последнего принятого пакета
  float getLastSnr() const;  // последний SNR
  float getLastRssi() const; // последний RSSI
  // Получить случайный байт
  uint8_t randomByte();
  // Получить последний код ошибки RadioLib
  int16_t getLastErrorCode() const { return lastError_; }
    // Выбор банка каналов (EAST, WEST, TEST, ALL)
    bool setBank(ChannelBank bank);
    // Выбор канала 0-166 в зависимости от банка
    bool setChannel(uint8_t ch);
  // Установка ширины полосы пропускания
  bool setBandwidth(float bw);
  // Установка фактора расширения
  bool setSpreadingFactor(int sf);
  // Установка коэффициента кодирования
  bool setCodingRate(int cr);
  // Установка уровня мощности по индексу пресета
  bool setPower(uint8_t preset);
  // Включение режима повышенного усиления приёмника
  bool setRxBoostedGainMode(bool enabled);
  // Получение текущих параметров
    ChannelBank getBank() const { return bank_; }
    uint8_t getChannel() const { return channel_; }
    // Получить количество каналов в текущем банке
    uint16_t getBankSize() const { return BANK_CHANNELS_[static_cast<int>(bank_)]; }
  // Получить количество каналов в произвольном банке
  static uint16_t bankSize(ChannelBank bank);
  // Получить частоту приёма для канала в указанном банке
  static float bankRx(ChannelBank bank, uint16_t ch);
  // Получить частоту передачи для канала в указанном банке
  static float bankTx(ChannelBank bank, uint16_t ch);
  float getBandwidth() const { return BW_[bw_preset_]; }
  int getSpreadingFactor() const { return SF_[sf_preset_]; }
  int getCodingRate() const { return CR_[cr_preset_]; }
  int getPower() const { return Pwr_[pw_preset_]; }
  bool isRxBoostedGainEnabled() const { return rxBoostedGainEnabled_; }
    float getRxFrequency() const { return fRX_bank_[static_cast<int>(bank_)][channel_]; }
    float getTxFrequency() const { return fTX_bank_[static_cast<int>(bank_)][channel_]; }
  // Сброс параметров к значениям по умолчанию с перезапуском приёма
  bool resetToDefaults();

  // Статический вывод активных флагов IRQ SX1262 в лог
  static void logIrqFlags(uint32_t flags);
  // Принудительная проверка и вывод отложенных IRQ-логов (обходной вызов из внешнего кода)
  void flushPendingIrqLog();
  // Установка внешнего обработчика, который будет получать готовую строку IRQ-лога
  using IrqLogCallback = void(*)(const char* message, uint32_t uptimeMs);
  void setIrqLogCallback(IrqLogCallback cb);

private:
  static void onDio1Static();            // статический обработчик прерывания
  void handleDio1();                     // обработка приёма
  void processPendingIrqLog();           // перенос логов IRQ из контекста прерывания

  // Непосредственная установка частоты
  bool setFrequency(float freq);

  // Запуск приёма с повторными попытками и логированием
  bool startReceiveWithRetry(const char* context);

  // Обёртка над SX1262 с публичным доступом к очистке IRQ-статуса
  struct PublicSX1262 : public SX1262 {
    using SX1262::SX1262;
    using SX1262::clearIrqStatus;

    // Универсальный доступ к флагам IRQ с учётом варианта API библиотеки
    uint32_t getIrqFlags() {
      if constexpr (radio_sx1262_detail::HasIrqFlagsApi<SX1262>::value) {
        return SX1262::getIrqFlags();
      } else if constexpr (radio_sx1262_detail::HasZeroArgIrqStatusApi<SX1262>::value) {
        return static_cast<uint32_t>(
            radio_sx1262_detail::CallZeroArgGetIrqStatus(
                static_cast<SX1262&>(*this), 0));
      } else if constexpr (radio_sx1262_detail::HasPointerIrqStatusApi<SX1262>::value) {
        uint16_t legacyFlags = 0;
        int16_t state = radio_sx1262_detail::CallPointerGetIrqStatus(
            static_cast<SX1262&>(*this), &legacyFlags, 0);
        return (state == RADIOLIB_ERR_NONE) ? legacyFlags : 0U;
      } else {
        return 0U;
      }
    }

    // Очистка флагов IRQ через доступный механизм
    int16_t clearIrqFlags(uint32_t mask) {
      if constexpr (radio_sx1262_detail::HasIrqFlagsApi<SX1262>::value) {
        return SX1262::clearIrqFlags(mask);
      } else {
        (void)mask;
        return SX1262::clearIrqStatus();
      }
    }

    // Совместимость с API, ожидающим возвращение статуса без аргументов
    uint16_t getIrqStatus() {
      if constexpr (radio_sx1262_detail::HasZeroArgIrqStatusApi<SX1262>::value) {
        return radio_sx1262_detail::CallZeroArgGetIrqStatus(
            static_cast<SX1262&>(*this), 0);
      } else {
        return static_cast<uint16_t>(getIrqFlags());
      }
    }

    // Совместимость с API, ожидающим указатель на буфер
    int16_t getIrqStatus(uint16_t* dest) {
      if constexpr (radio_sx1262_detail::HasPointerIrqStatusApi<SX1262>::value) {
        return radio_sx1262_detail::CallPointerGetIrqStatus(
            static_cast<SX1262&>(*this), dest, 0);
      } else {
        if (dest) {
          *dest = static_cast<uint16_t>(getIrqFlags());
        }
        return RADIOLIB_ERR_NONE;
      }
    }
  };

  PublicSX1262 radio_;                   // экземпляр радиомодуля
  RxCallback rx_cb_;                     // пользовательский колбэк
  IrqLogCallback irqCallback_ = nullptr; // внешнее уведомление об IRQ-логе
  static RadioSX1262* instance_;         // указатель на текущий объект
  static bool irqLoggerStarted_;         // отметка, что стартовый лог уже выведен
  volatile bool packetReady_ = false;    // флаг готовности пакета
  volatile bool irqNeedsRead_ = false;   // требуется ли чтение IRQ-регистров в основном потоке
  volatile bool irqLogPending_ = false;  // требуется ли вывести отложенный лог IRQ
  volatile uint32_t pendingIrqFlags_ = 0;          // сохранённые флаги IRQ из ISR
  volatile int16_t pendingIrqClearState_ = RADIOLIB_ERR_NONE; // результат очистки IRQ

  ChannelBank bank_ = ChannelBank::EAST; // текущий банк
  uint8_t channel_ = 0;                  // текущий канал
  int pw_preset_ = 9;
  int bw_preset_ = 3;
  int sf_preset_ = 2;
  int cr_preset_ = 0;
  float tcxo_ = 0;
  bool rxBoostedGainEnabled_ = false;

  float lastSnr_ = 0.0f;     // SNR последнего пакета
  float lastRssi_ = 0.0f;    // RSSI последнего пакета
  int16_t lastError_ = RADIOLIB_ERR_NONE; // последний код ошибки RadioLib

    // Таблицы частот для всех банков
    static const float* fRX_bank_[5];
    static const float* fTX_bank_[5];
    static const uint16_t BANK_CHANNELS_[5];
  static const int8_t Pwr_[10];
  static const float BW_[5];
  static const int8_t SF_[8];
  static const int8_t CR_[4];
};

