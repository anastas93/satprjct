
#pragma once
#include "freq_map.h"
#include <vector>

// Режимы кодирования/повтора
enum class PingFecMode : uint8_t {
  FEC_OFF = 0,     // без FEC
  FEC_RS_VIT = 1,  // RS(255,223)+Viterbi
  FEC_LDPC = 2,    // LDPC (заглушка)
  FEC_REPEAT2 = 3  // простой повтор кадров
};

// Параметры запуска расширенного пинга
struct PingOptions {
  uint32_t count = 0;            // сколько пакетов послать, 0 = бесконечно
  uint32_t interval_ms = 1000;   // интервал между пакетами
  uint32_t timeout_ms = 3000;    // таймаут ожидания ответа
  uint32_t frag_size = 5;        // размер полезной нагрузки
  uint32_t duration_min = 0;     // длительность прогона в минутах, 0 = игнорировать
  PingFecMode fec_mode = PingFecMode::FEC_OFF; // режим FEC
  uint8_t retries = 0;           // число повторов при потере (ARQ)
};

// Аггрегированная статистика работы пинга
struct PingStats {
  uint32_t sent = 0;         // отправлено кадров (с учётом ретраев)
  uint32_t received = 0;     // принято кадров
  uint32_t crc_fail = 0;     // ошибок CRC
  uint32_t fec_fail = 0;     // ошибок FEC
  uint32_t no_ack = 0;       // отсутствие ACK
  uint32_t timeout = 0;      // таймауты
  uint32_t retransmits = 0;  // количество ретраев
  uint64_t tx_bytes = 0;     // всего передано байт (с учётом служебных)
  uint64_t payload_bytes = 0;// полезная нагрузка, успешно доставленная
  std::vector<uint32_t> rtt; // RTT каждого сообщения в мс
  std::vector<std::vector<uint32_t>> rtt_frag; // RTT отдельных фрагментов
  PingFecMode fec_mode = PingFecMode::FEC_OFF; // использованный режим FEC
  uint32_t frag_size = 0;    // размер полезной нагрузки в байтах
};

// Объявления вспомогательных функций пинга
void SatPing();                    // асинхронный пинг текущего пресета
void SatPingTrace();               // детальный пинг с разбором по этапам
bool ChannelPing();                // проверка канала на текущем пресете
bool PresetPing(Bank bank, int preset); // проверка указанного пресета
void MassPing(Bank bank);          // проверка всех пресетов банка

// Расширенный пакетный режим пинга
void SatPingRun(const PingOptions& opts, PingStats& stats);

