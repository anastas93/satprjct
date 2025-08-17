
#include <Arduino.h>
#include <string.h>
#include "RadioLib.h"
#include "freq_map.h"

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_system.h>
#endif

extern SX1262 radio;
extern float g_freq_rx_mhz;
extern float g_freq_tx_mhz;
extern int   g_preset;

void SatPing() {
  // Формируем 5‑байтный пакет пинга
  uint8_t ping[5];
  uint8_t rx[5];
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();          // случайные байты
  ping[0] = ping[1] ^ ping[2];           // идентификатор пакета

  // Определяем адрес устройства по eFUSE MAC, как в оригинале
  uint8_t address = 0x01;
#if defined(ESP32) || defined(ESP_PLATFORM)
  uint64_t mac = ESP.getEfuseMac();
  address = ((mac >> 8) & 0xFF) ^ (mac & 0xFF);
#endif
  ping[3] = address;
  ping[4] = 0x00;

  // Сообщаем текущие частоты RX/TX
  Serial.print(F("~&Server: RX:"));
  Serial.print(g_freq_rx_mhz, 3);
  Serial.print(F("/TX:"));
  Serial.print(g_freq_tx_mhz, 3);
  Serial.println(F("~"));

  // Передаём пинг на частоте TX
  radio.setFrequency(g_freq_tx_mhz);
  radio.transmit(ping, 5);

  // Измеряем время прохождения туда-обратно
  uint32_t t1 = micros();
  radio.setFrequency(g_freq_rx_mhz);      // переходим на приём
  int state = radio.receive(rx, 5);       // ждём ответ
  uint32_t t2 = micros();

  bool ok = (state == RADIOLIB_ERR_NONE) || (memcmp(ping, rx, 5) == 0);
  if (ok) {
    uint32_t dt = t2 - t1;
    // расстояние в км: (t [мкс] -> с) * c / 2 / 1000
    double dist_km = ((double)dt * 1e-6 * 299792458.0 / 2.0) / 1000.0;
    double ms_time = (double)dt * 0.001;
    Serial.print(F("Ping>OK RSSI:"));
    Serial.print(radio.getRSSI());
    Serial.print(F(" dBm/SNR:"));
    Serial.print(radio.getSNR());
    Serial.print(F(" dB distance:~"));
    Serial.print(dist_km, 3);
    Serial.print(F("km   time:"));
    Serial.print(ms_time, 2);
    Serial.println(F("ms"));
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("CRC error!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  // Небольшая задержка и возврат модуля в режим приёма
  delay(150);
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
  radio.transmit(ping, 5);

  radio.setFrequency(g_freq_rx_mhz);  // слушаем на текущей RX
  int state = radio.receive(rx, 5);
  bool ok = (state == RADIOLIB_ERR_NONE) || (memcmp(ping, rx, 5) == 0);
  radio.setFrequency(g_freq_rx_mhz);  // возвращаем приём
  radio.startReceive();
  return ok;
}

// Пинг указанного пресета из заданного банка
bool PresetPing(Bank bank, int preset) {
  const FreqPreset* tbl = nullptr;
  switch (bank) {
    case Bank::MAIN:    tbl = FREQ_MAIN;    break;
    case Bank::RESERVE: tbl = FREQ_RESERVE; break;
    case Bank::TEST:    tbl = FREQ_TEST;    break;
  }
  if (!tbl || preset < 0 || preset >= 10) return false;

  uint8_t ping[5];
  uint8_t rx[5];
  ping[1] = radio.randomByte();
  ping[2] = radio.randomByte();
  ping[0] = ping[1] ^ ping[2];
  ping[3] = 0;
  ping[4] = 0;

  radio.setFrequency(tbl[preset].txMHz);
  radio.transmit(ping, 5);
  radio.setFrequency(tbl[preset].rxMHz);
  int state = radio.receive(rx, 5);
  bool ok = (state == RADIOLIB_ERR_NONE) || (memcmp(ping, rx, 5) == 0);
  radio.setFrequency(g_freq_rx_mhz);  // возвращаем исходную частоту
  radio.startReceive();
  return ok;
}

// Обход всех пресетов выбранного банка и вывод результата в консоль
void MassPing(Bank bank) {
  const FreqPreset* tbl = nullptr;
  switch (bank) {
    case Bank::MAIN:    tbl = FREQ_MAIN;    break;
    case Bank::RESERVE: tbl = FREQ_RESERVE; break;
    case Bank::TEST:    tbl = FREQ_TEST;    break;
  }
  if (!tbl) return;

  int found = 0;
  for (int i = 0; i < 10; ++i) {
    Serial.print(F("RX:"));
    Serial.print(tbl[i].rxMHz, 3);
    Serial.print(F("/TX:"));
    Serial.print(tbl[i].txMHz, 3);
    Serial.print(F(" "));

    bool ok = PresetPing(bank, i);
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
    delay(150);
  }
  Serial.print(F("Found: "));
  Serial.println(found);
  radio.setFrequency(g_freq_rx_mhz);  // возвращаемся на рабочую частоту
  radio.startReceive();
}
