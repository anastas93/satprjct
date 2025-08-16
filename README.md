# ESP32 LoRa Pipeline — Track D
- ACK/ретраи, разнос RX/TX, пресеты, метрики, NVS
- AES-CCM (тег 8 B), `ENC/KID/KEY`
- Локальные тесты `ENCTEST`, `ENCTEST_BAD`
- **Персистентный msg_id в NVS** (исключение повторного нонса)
- **Анти-replay** (окно и порог старых ID)

## Используемые библиотеки
- [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) — базовые классы (`Arduino.h`, `Preferences`, `WiFi`, `WebServer`)
- [RadioLib](https://github.com/jgromes/RadioLib) — драйвер SX1262 и функции LoRa
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) — криптография (AES‑CCM, ECDH и др.)

## Установка и запуск
### Требования
- Arduino IDE 1.8+ или PlatformIO
- Пакет плат ESP32 (например, `esp32` от Espressif)
- Библиотека [LoRa](https://github.com/sandeepmistry/arduino-LoRa) для Arduino

### Компиляция и прошивка
1. Откройте `ESP32_LoRa_Pipeline.ino` в Arduino IDE.
2. В меню **Tools → Board** выберите **ESP32 Dev Module**.
3. Подключите плату и нажмите **Upload** для компиляции и прошивки.

### Пример запуска
1. После загрузки устройство поднимает точку доступа `ESP32-LoRa`/`12345678`.
2. Подключитесь к Wi‑Fi и откройте в браузере `http://192.168.4.1/` для доступа к веб‑интерфейсу.

## Команды
| Команда | Параметры | Назначение | Пример ответа |
|--------|-----------|------------|---------------|
| `ENCTEST [size]` | `size` — размер тестового блока в байтах | Проверка шифрования и дешифрования | `ENCTEST size=32 enc=OK dec=OK time_enc=600us time_dec=700us` |
| `ENCTEST_BAD [size]` | `size` — размер блока | Шифрование с подменой KID, должно завершиться ошибкой | `ENCTESTBAD wrong-KID dec=FAIL (expected FAIL)` |
| `IDRESET [val]` | `val` — новое значение `msg_id` | Установить счётчик сообщений, например для воспроизведения кадров | `*SYS:* msg_id set to 0` |
| `REPLAYCLR` | — | Сбрасывает окно анти‑replay | `*SYS:* replay window cleared` |

Пример использования:

```
> ENCTEST 32
ENCTEST size=32 enc=OK dec=OK time_enc=...us time_dec=...us

> ENCTEST_BAD 32
ENCTESTBAD wrong-KID dec=FAIL (expected FAIL)
```

## Тестирование
Подробное руководство по ручной проверке веб‑интерфейса и радиомодуля описано в [TESTS.md](TESTS.md).
Базовые самотесты шифрования выполняются командами `ENCTEST` и `ENCTEST_BAD`; полный сценарий приведён в `TESTS.md`.
Дополнительно доступен автономный тест совместимости `compat_ack_test`,
который проверяет согласованность формата пакетов и обмен ACK.
Компиляция:
```
g++ -std=c++17 compat_ack_test.cpp message_buffer.cpp fragmenter.cpp \
    tx_pipeline.cpp rx_pipeline.cpp frame_log.cpp test_stubs/Arduino.cpp \
    -I. -Itest_stubs -o compat_ack_test
./compat_ack_test
```
