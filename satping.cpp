
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

  // Передаём пакет и измеряем время
  radio.transmit(ping, 5);
  uint32_t time1 = micros();
  radio.setFrequency(g_freq_rx_mhz);      // переходим на приём
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
  delay(150);
  radio.setFrequency(g_freq_rx_mhz);
  // radio.startReceive();  // в оригинале не вызывался
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
