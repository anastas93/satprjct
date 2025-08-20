
#include <Arduino.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include "RadioLib.h"
#include "freq_map.h"
#include "libs/ccsds_link/fec.h" // функции помехоустойчивого кодирования
#include "radio_adapter.h"
#include "fragmenter.h"   // работа с фрагментами
#include "satping.h"  // описание структур PingOptions и PingStats

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_system.h>
#endif

extern SX1262 radio;
extern float g_freq_rx_mhz;
extern float g_freq_tx_mhz;
extern volatile bool txDoneFlag;  // флаг завершения передачи из обработчика IRQ

// Преобразование режима FEC в строку для вывода
static const char* FecModeName(PingFecMode m) {
  switch (m) {
    case PingFecMode::FEC_OFF: return "off";
    case PingFecMode::FEC_RS_VIT: return "rs_vit";
    case PingFecMode::FEC_LDPC: return "ldpc";
    case PingFecMode::FEC_REPEAT2: return "repeat2";
    default: return "unknown";
  }
}

void SatPing() {
  // Устанавливаем частоту передачи, как в оригинальном коде
  radio.setFrequency(g_freq_tx_mhz);
  uint8_t ping[5];
  uint8_t rx_ping[5];
  bool err_ping = 1;

  // Формируем 5‑байтный пакет пинга
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();      // случайные байты
  ping[0] = ping[1] ^ ping[2];       // идентификатор

  // Определяем адрес устройства по eFUSE MAC
  uint8_t address = 0x01;
#if defined(ESP32) || defined(ESP_PLATFORM)
  uint64_t mac = ESP.getEfuseMac();
  address = ((mac >> 8) & 0xFF) ^ (mac & 0xFF);
#endif
  ping[3] = address;
  ping[4] = 0;

  // Выводим текущие частоты RX/TX
  Serial.print(F("RX:"));
  Serial.print(g_freq_rx_mhz, 3);
  Serial.print(F("/TX:"));
  Serial.println(g_freq_tx_mhz, 3);

  // Передаём пакет асинхронно и ждём завершения по флагу
  radio.startTransmit(ping, 5);
  while (!txDoneFlag) {
    delay(1);
  }
  txDoneFlag = false;               // сбрасываем флаг окончания TX
  radio.finishTransmit();           // фиксируем завершение передачи
  uint32_t time1 = micros();        // фиксируем момент завершения передачи

  delay(2);                         // короткая защита перед приёмом
  radio.setFrequency(g_freq_rx_mhz); // переходим на приём
  radio.startReceive();             // включаем режим RX
  int state = radio.receive(rx_ping, 5);  // ждём ответ
  uint32_t time2 = micros();

  err_ping = memcmp(ping, rx_ping, 5);
  if (state == RADIOLIB_ERR_NONE || err_ping == 0) {
    // пакет был успешно принят
    uint32_t time3 = time2 - time1;
    Serial.print(F("Ping>OK RSSI:"));
    Serial.print(radio.getRSSI());
    Serial.print(F(" dBm/SNR:"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));

    uint32_t len = (((time3 * 0.000001) * 299792458 / 2) / 1000);
    Serial.print(F("distance:~"));
    Serial.print(len);
    Serial.print(F("km   time:"));
    Serial.print(time3 * 0.001);
    Serial.println(F("ms"));
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // пакет был получен, но повреждён
    Serial.println(F("CRC error!"));
  } else {
    // произошла ошибка
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  // Возвращаемся на частоту приёма
  delay(3);                      // короткий guard вместо длинной паузы
  radio.setFrequency(g_freq_rx_mhz);
  radio.startReceive();          // возвращаем радио в режим RX
}

// Детальный пинг с разбивкой по этапам и временем каждого шага
void SatPingTrace() {
  radio.setFrequency(g_freq_tx_mhz);     // работаем на частоте передачи

  uint8_t ping[5];
  uint8_t rx_ping[5];

  uint32_t t_start = micros();           // время начала операции

  // Формируем пакет пинга
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();           // случайные байты
  ping[0] = ping[1] ^ ping[2];            // идентификатор

  uint8_t address = 0x01;
#if defined(ESP32) || defined(ESP_PLATFORM)
  uint64_t mac = ESP.getEfuseMac();
  address = ((mac >> 8) & 0xFF) ^ (mac & 0xFF);
#endif
  ping[3] = address;
  ping[4] = 0;

  uint32_t t_pkt = micros();
  Serial.print(F("[")); Serial.print((t_pkt - t_start) / 1000.0, 3);
  Serial.println(F(" ms] пакет сформирован"));

  // Передаём пакет и ждём окончания
  radio.startTransmit(ping, 5);
  while (!txDoneFlag) {
    delay(1);
  }
  txDoneFlag = false;
  radio.finishTransmit();
  uint32_t t_tx = micros();
  Serial.print(F("[")); Serial.print((t_tx - t_start) / 1000.0, 3);
  Serial.println(F(" ms] передача завершена"));

  // Переходим на приём
  delay(2);
  radio.setFrequency(g_freq_rx_mhz);
  radio.startReceive();
  uint32_t t_rx_start = micros();
  Serial.print(F("[")); Serial.print((t_rx_start - t_start) / 1000.0, 3);
  Serial.println(F(" ms] ожидание ответа"));

  int state = radio.receive(rx_ping, 5);
  uint32_t t_rx_end = micros();
  Serial.print(F("[")); Serial.print((t_rx_end - t_start) / 1000.0, 3);
  Serial.println(F(" ms] приём завершён"));

  bool ok = (state == RADIOLIB_ERR_NONE) || (memcmp(ping, rx_ping, 5) == 0);
  if (ok) {
    Serial.print(F("Ping>OK RSSI:"));
    Serial.print(radio.getRSSI());
    Serial.print(F(" dBm/SNR:"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("CRC error!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  Serial.print(F("Итого RTT: "));
  Serial.print((t_rx_end - t_tx) / 1000.0, 3);
  Serial.println(F(" ms"));

  // Возврат на рабочую частоту приёма
  delay(3);
  radio.setFrequency(g_freq_rx_mhz);
  radio.startReceive();
}

// Проверка канала на текущем пресете без вывода подробной информации
bool ChannelPing() {
  uint8_t ping[5];
  uint8_t rx[5];

  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();       // случайные байты
  ping[0] = ping[1] ^ ping[2];        // идентификатор
  ping[3] = 0;                        // адрес несущественен
  ping[4] = 0;

  radio.setFrequency(g_freq_tx_mhz);  // передаём на текущей TX
  radio.startTransmit(ping, 5);
  while (!txDoneFlag) {
    delay(1);
  }
  txDoneFlag = false;
  radio.finishTransmit();             // переводим радио из режима TX
  delay(2);                           // короткий guard

  radio.setFrequency(g_freq_rx_mhz);  // слушаем на текущей RX
  radio.startReceive();               // переходим в режим приёма
  int state = radio.receive(rx, 5);
  bool ok = (state == RADIOLIB_ERR_NONE) || (memcmp(ping, rx, 5) == 0);
  radio.setFrequency(g_freq_rx_mhz);  // возвращаем приём
  radio.startReceive();
  return ok;
}

// Пинг указанного пресета из заданного банка
bool PresetPing(Bank bank, int preset) {
  // Получаем нужный банк частот как вектор
  const auto& tbl = GetFreqTable(bank);
  if (preset < 0 || preset >= (int)tbl.size()) return false;

  uint8_t ping[5];
  uint8_t rx[5];
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();
  ping[0] = ping[1] ^ ping[2];
  ping[3] = 0;
  ping[4] = 0;

  radio.setFrequency(tbl[preset].txMHz);
  radio.startTransmit(ping, 5);
  while (!txDoneFlag) {
    delay(1);
  }
  txDoneFlag = false;
  radio.finishTransmit();               // завершение TX перед приёмом
  delay(2);                             // пауза перед приёмом

  radio.setFrequency(tbl[preset].rxMHz);
  radio.startReceive();
  int state = radio.receive(rx, 5);
  bool ok = (state == RADIOLIB_ERR_NONE) || (memcmp(ping, rx, 5) == 0);
  radio.setFrequency(g_freq_rx_mhz);  // возвращаем исходную частоту
  radio.startReceive();
  return ok;
}

// Обход всех пресетов выбранного банка и вывод результата в консоль
void MassPing(Bank bank) {
  const auto& tbl = GetFreqTable(bank);
  if (tbl.empty()) return;

  int found = 0;
  for (size_t i = 0; i < tbl.size(); ++i) {
    Serial.print(F("RX:"));
    Serial.print(tbl[i].rxMHz, 3);
    Serial.print(F("/TX:"));
    Serial.print(tbl[i].txMHz, 3);
    Serial.print(F(" "));

    bool ok = PresetPing(bank, static_cast<int>(i));
    if (ok) {
      Serial.print(F("OK RSSI:"));
      Serial.print(radio.getRSSI());
      Serial.print(F(" dBm/SNR:"));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));
      found++;
    } else {
      Serial.println(F("Bad"));
    }
    delay(3);   // короткая пауза между пресетами
  }
  Serial.print(F("Found: "));
  Serial.println(found);
  radio.setFrequency(g_freq_rx_mhz);  // возвращаемся на рабочую частоту
  radio.startReceive();
}

// Пакетный пинг с расширенной статистикой
void SatPingRun(const PingOptions& opts, PingStats& stats) {
  // Сохраняем параметры запуска
  stats.fec_mode = opts.fec_mode;
  stats.frag_size = opts.frag_size;

  uint32_t seq = 0;                  // порядковый номер пакета
  uint32_t start_ms = millis();      // время начала теста
  Fragmenter fr;                     // инструмент для резки сообщений на фрагменты

  while ((opts.count == 0 || seq < opts.count) &&
         (opts.duration_min == 0 || (millis() - start_ms) < opts.duration_min * 60000UL)) {

    // формируем сообщение целиком
    std::vector<uint8_t> msg(opts.frag_size);
    for (uint32_t i = 0; i < opts.frag_size; ++i) msg[i] = radio.randomByte();

    // режем на фрагменты при необходимости
    std::vector<Fragment> frags;
    if (opts.frag_size > FRAME_PAYLOAD_MAX) {
      frags = fr.split(seq, msg.data(), msg.size(), false, FRAME_PAYLOAD_MAX);
    } else {
      Fragment f{};
      f.hdr.frag_idx = 0; f.hdr.frag_cnt = 1;
      f.payload = msg;
      frags.push_back(std::move(f));
    }

    uint32_t msg_t1 = micros();                 // начало RTT всего сообщения
    std::vector<uint32_t> frtt;                  // RTT отдельных фрагментов
    bool msg_success = true;                    // флаг успешной доставки всех фрагментов

    for (size_t fi = 0; fi < frags.size(); ++fi) {
      Fragment& frag = frags[fi];
      if (frag.payload.size() >= 3) frag.payload[0] = frag.payload[1] ^ frag.payload[2];

      bool success = false;
      const char* drop_reason = "timeout"; // причина отказа
      uint8_t attempt = 0;
      while (!success && attempt <= opts.retries) {
        std::vector<uint8_t> txbuf;     // буфер для передачи (с учётом FEC)
        int corrected = 0;              // число исправленных символов

        // кодирование по выбранному режиму
        if (opts.fec_mode == PingFecMode::FEC_RS_VIT) {
          fec_encode_rs_viterbi(frag.payload.data(), frag.payload.size(), txbuf);
        } else if (opts.fec_mode == PingFecMode::FEC_LDPC) {
          fec_encode_ldpc(frag.payload.data(), frag.payload.size(), txbuf);
        } else if (opts.fec_mode == PingFecMode::FEC_REPEAT2) {
          fec_encode_repeat(frag.payload.data(), frag.payload.size(), txbuf);
        } else {
          txbuf = frag.payload;
        }

        // передача одного фрагмента
        radio.setFrequency(g_freq_tx_mhz);
        radio.startTransmit(txbuf.data(), txbuf.size());
        while (!txDoneFlag) {
          delay(1);
        }
        txDoneFlag = false;
        radio.finishTransmit();               // выходим из режима передачи
        stats.sent++;
        if (attempt > 0) stats.retransmits++;
        stats.tx_bytes += txbuf.size();

        // короткая пауза и переход на приём
        delay(2);
        std::vector<uint8_t> rxbuf(txbuf.size());
        radio.setFrequency(g_freq_rx_mhz);
        radio.startReceive();
        uint32_t t1 = micros();
        int state = RADIOLIB_ERR_RX_TIMEOUT;
        while ((micros() - t1) / 1000 < opts.timeout_ms) {
          if (radio.available() == RADIOLIB_ERR_NONE) {
            state = radio.readData(rxbuf.data(), rxbuf.size());
            break;
          }
          delay(1);
        }
        uint32_t t2 = micros();

        if (state == RADIOLIB_ERR_NONE) {
          bool ok = false;
          const uint8_t* cmp = rxbuf.data();
          size_t cmp_len = rxbuf.size();
          std::vector<uint8_t> decoded;
          if (opts.fec_mode == PingFecMode::FEC_RS_VIT) {
            ok = fec_decode_rs_viterbi(rxbuf.data(), rxbuf.size(), decoded, corrected);
            cmp = decoded.data();
            cmp_len = decoded.size();
          } else if (opts.fec_mode == PingFecMode::FEC_LDPC) {
            ok = fec_decode_ldpc(rxbuf.data(), rxbuf.size(), decoded, corrected);
            cmp = decoded.data();
            cmp_len = decoded.size();
          } else if (opts.fec_mode == PingFecMode::FEC_REPEAT2) {
            ok = fec_decode_repeat(rxbuf.data(), rxbuf.size(), decoded);
            cmp = decoded.data();
            cmp_len = decoded.size();
          } else {
            ok = true;
          }
          if (ok && cmp_len == frag.payload.size() &&
              memcmp(frag.payload.data(), cmp, frag.payload.size()) == 0) {
            // получен корректный ответ
            uint32_t rtt = (t2 - t1) / 1000;
            stats.received++;
            stats.payload_bytes += frag.payload.size();
            frtt.push_back(rtt);
            float ebn0 = Radio_readEbN0();
            Serial.print(F("seq=")); Serial.print(seq);
            Serial.print(F(" frag=")); Serial.print((int)fi);
            Serial.print(F(" rtt=")); Serial.print(rtt); Serial.print(F("ms snr="));
            Serial.print(radio.getSNR()); Serial.print(F("dB ebn0=")); Serial.print(ebn0);
            Serial.print(F("dB fec_corr=")); Serial.print(corrected);
            Serial.print(F(" fec_mode="));
            Serial.print(FecModeName(stats.fec_mode));
            Serial.print(F(" frag_size=")); Serial.print(stats.frag_size);
            Serial.println(F(" drop_reason=ok"));
            success = true;
          } else {
            stats.fec_fail++;
            drop_reason = "fec_fail";
          }
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
          stats.crc_fail++;
          drop_reason = "crc_fail";
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
          stats.timeout++;
          drop_reason = "timeout";
        } else {
          stats.no_ack++;
          drop_reason = "no_ack";
        }

        attempt++;
      }

      if (!success) {
        Serial.print(F("seq=")); Serial.print(seq);
        Serial.print(F(" frag=")); Serial.print((int)fi);
        Serial.print(F(" fec_mode="));
        Serial.print(FecModeName(stats.fec_mode));
        Serial.print(F(" frag_size=")); Serial.print(stats.frag_size);
        Serial.print(F(" drop_reason=")); Serial.println(drop_reason);
        msg_success = false;
        break; // прекращаем отправку остальных фрагментов
      }
    }

    uint32_t msg_t2 = micros();
    if (msg_success) {
      stats.rtt.push_back((msg_t2 - msg_t1) / 1000);
      stats.rtt_frag.push_back(frtt);
    }

    seq++;
    delay(opts.interval_ms);
  }

  // итоги
  Serial.println(F("--- satlink ping statistics ---"));
  Serial.print(F("fec_mode=")); Serial.print(FecModeName(stats.fec_mode));
  Serial.print(F(" frag_size=")); Serial.println(stats.frag_size);
  Serial.print(stats.sent); Serial.print(F(" packets transmitted, "));
  Serial.print(stats.received); Serial.print(F(" received, "));
  uint32_t lost = stats.sent - stats.received;
  Serial.print(lost); Serial.print(F(" lost ("));
  if (stats.sent) Serial.print((lost * 100) / stats.sent); else Serial.print(0);
  Serial.println(F("% loss)"));

  if (!stats.rtt.empty()) {
    std::sort(stats.rtt.begin(), stats.rtt.end());
    uint32_t rtt_min = stats.rtt.front();
    uint32_t rtt_max = stats.rtt.back();
    uint32_t rtt_p50 = stats.rtt[stats.rtt.size() / 2];
    uint32_t rtt_p95 = stats.rtt[stats.rtt.size() * 95 / 100];
    uint64_t sum = 0; for (uint32_t v : stats.rtt) sum += v;
    uint32_t rtt_avg = sum / stats.rtt.size();
    Serial.print(F("rtt min/avg/p50/p95/max = "));
    Serial.print(rtt_min); Serial.print('/');
    Serial.print(rtt_avg); Serial.print('/');
    Serial.print(rtt_p50); Serial.print('/');
    Serial.print(rtt_p95); Serial.print('/');
    Serial.print(rtt_max); Serial.println(F(" ms"));
  }

  // Перцентили RTT для отдельных фрагментов
  if (!stats.rtt_frag.empty()) {
    std::vector<uint32_t> all_frag_rtt;
    for (const auto& v : stats.rtt_frag)
      all_frag_rtt.insert(all_frag_rtt.end(), v.begin(), v.end());
    if (!all_frag_rtt.empty()) {
      std::sort(all_frag_rtt.begin(), all_frag_rtt.end());
      uint32_t frtt_min = all_frag_rtt.front();
      uint32_t frtt_max = all_frag_rtt.back();
      uint32_t frtt_p50 = all_frag_rtt[all_frag_rtt.size() / 2];
      uint32_t frtt_p95 = all_frag_rtt[all_frag_rtt.size() * 95 / 100];
      uint64_t fsum = 0; for (uint32_t v : all_frag_rtt) fsum += v;
      uint32_t frtt_avg = fsum / all_frag_rtt.size();
      Serial.print(F("frag rtt min/avg/p50/p95/max = "));
      Serial.print(frtt_min); Serial.print('/');
      Serial.print(frtt_avg); Serial.print('/');
      Serial.print(frtt_p50); Serial.print('/');
      Serial.print(frtt_p95); Serial.print('/');
      Serial.print(frtt_max); Serial.println(F(" ms"));
    }
  }

  double duration_ms = millis() - start_ms;
  double goodput = 0;
  if (duration_ms > 0) goodput = (stats.payload_bytes * 8.0) / duration_ms; // kbps
  double overhead = 0;
  if (stats.tx_bytes > 0) overhead = (double)(stats.tx_bytes - stats.payload_bytes) * 100.0 / stats.tx_bytes;

  Serial.print(F("Goodput: ")); Serial.print(goodput, 1); Serial.print(F(" kbps (payload), Overhead: "));
  Serial.print(overhead, 1); Serial.println(F("%"));
  Serial.println(F("Errors:"));
  Serial.print(F("  CRC-fail: ")); Serial.println(stats.crc_fail);
  Serial.print(F("  FEC-fail: ")); Serial.println(stats.fec_fail);
  Serial.print(F("  Timeout: ")); Serial.println(stats.timeout);
  Serial.print(F("  No-ack: ")); Serial.println(stats.no_ack);
  Serial.print(F("  Retransmits: ")); Serial.println(stats.retransmits);
}

