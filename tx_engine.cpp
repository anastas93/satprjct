#include "tx_engine.h"
#include "radio_adapter.h"
#include "libs/qos_moderator.h"
#include "tdd_scheduler.h"
#include "event_queue.h"
#include <mutex>

// Внешние частоты, задаваемые основным скетчем
extern float g_freq_tx_mhz;
extern float g_freq_rx_mhz;

// Функция низкоуровневой передачи, реализуется в прошивке или заглушке
extern bool radioTransmit(const uint8_t* data, size_t len);

// Глобальный экземпляр движка и мьютекс
TxEngine g_tx_engine;
std::mutex gRadioMutex;

bool TxEngine::sendFrame(const uint8_t* data, size_t len, const TxOptions& opts) { // QOS: Высокий Низкоуровневая передача кадра через радио
  std::lock_guard<std::mutex> guard(gRadioMutex); // защищаем доступ к радио
  (void)opts.profile;  // профили пока не влияют на отправку
  (void)opts.scramble; // скремблер применяется на более ранних этапах

  // Всегда переустанавливаем частоту передатчика
  Radio_setFrequency(opts.freq_hz);
  // Сигнализируем о начале передачи
  queueEvent(evt_t::EVT_TX_START);
  bool ok = radioTransmit(data, len);
  // Сигнализируем о завершении передачи
  queueEvent(evt_t::EVT_TX_DONE);
  // Возвращаемся на частоту приёма
  Radio_setFrequency((uint32_t)(g_freq_rx_mhz * 1e6f));
  // Открываем приём на окно ACK
  Radio_forceRx(msToTicks(tdd::ackWindowMs + tdd::guardMs));
  return ok;
}

// Совместимый API для старых вызовов
bool Radio_sendRaw(const uint8_t* data, size_t len, Qos q) { // QOS: Высокий Совместимый API для отправки кадров в радио
  // Проверка приоритета через модератор QoS
  if (!Qos_allow(q)) return false;
  TxOptions o{};
  o.freq_hz = (uint32_t)(g_freq_tx_mhz * 1e6f);
  o.profile = 0;
  o.scramble = true;
  return g_tx_engine.sendFrame(data, len, o);
}
