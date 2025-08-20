# Функции отправки и приоритеты

Документ генерируется автоматически скриптом `update_qos_senders.py`.

| Файл | Функция | Описание | Приоритет |
|------|---------|----------|-----------|
| `ESP32_LoRa_Pipeline.ino` | `handleSend` | HTTP-обработчик отправки текстового сообщения | Нормальный |
| `ESP32_LoRa_Pipeline.ino` | `handleSendQ` | HTTP-обработчик отправки сообщения с явным QoS | Зависит |
| `ESP32_LoRa_Pipeline.ino` | `sendKeyRequest` | Постановка в очередь запроса ключа | Высокий |
| `ESP32_LoRa_Pipeline.ino` | `sendKeyResponse` | Постановка в очередь ответа с ключом | Высокий |
| `ack_sender.cpp` | `AckSender::send` | Формирование и отправка ACK | Высокий |
| `rx_pipeline.cpp` | `RxPipeline::sendAck` | Отправка ACK при приёме сообщений | Высокий |
| `tx_engine.cpp` | `Radio_sendRaw` | Совместимый API для отправки кадров в радио | Высокий |
| `tx_engine.cpp` | `TxEngine::sendFrame` | Низкоуровневая передача кадра через радио | Высокий |
| `tx_pipeline.cpp` | `TxPipeline::sendMessageFragments` | Подготовка и отправка кадров | Средний |
| `web_script.h` | `ackChk` | Включение/отключение ACK | Нормальный |
| `web_script.h` | `archivelist` | Список архива сообщений | Нормальный |
| `web_script.h` | `archiverestore` | Восстановление архива сообщений | Нормальный |
| `web_script.h` | `channelping` | Диагностический ping канала | Низкий |
| `web_script.h` | `encTestBtn` | Тест шифрования | Низкий |
| `web_script.h` | `enctestbad` | Негативный тест шифрования | Низкий |
| `web_script.h` | `idreset` | Сброс счётчиков идентификаторов | Высокий |
| `web_script.h` | `keydh` | Диффи-Хеллман | Высокий |
| `web_script.h` | `keyreq` | Запрос ключа | Высокий |
| `web_script.h` | `keysend` | Отправка ключа | Высокий |
| `web_script.h` | `keytest` | Тест ключей | Высокий |
| `web_script.h` | `large` | Отправка большого сообщения | Нормальный |
| `web_script.h` | `largeq` | Отправка большого сообщения с явным QoS | Зависит |
| `web_script.h` | `loadFrames` | Получение последних кадров | Низкий |
| `web_script.h` | `massPingBtn` | Массовый ping | Низкий |
| `web_script.h` | `metrics` | Получение метрик | Низкий |
| `web_script.h` | `msgIdBtn` | Управление счётчиком сообщений | Низкий |
| `web_script.h` | `ping` | Диагностический ping | Низкий |
| `web_script.h` | `presetPingBtn` | Диагностический ping пресета | Низкий |
| `web_script.h` | `qos` | Статистика QoS | Нормальный |
| `web_script.h` | `qosmode` | Управление режимом QoS | Нормальный |
| `web_script.h` | `replayclr` | Очистка счётчиков реплеев | Высокий |
| `web_script.h` | `satRunBtn` | Расширенный SatPing | Низкий |
| `web_script.h` | `selftest` | Тестовый запрос selftest | Низкий |
| `web_script.h` | `sendParam` | Настройка параметров радио | Нормальный |
| `web_script.h` | `sendq` | Сообщение с явным приоритетом | Зависит |
| `web_script.h` | `setdup` | Переключение режима дублирования | Нормальный |
| `web_script.h` | `setebn0th` | Установка порогов Eb/N0 | Нормальный |
| `web_script.h` | `setkey` | Установка ключа шифрования | Высокий |
| `web_script.h` | `setkid` | Активация ключевого идентификатора | Высокий |
| `web_script.h` | `setkid` | Установка идентификатора ключа | Высокий |
| `web_script.h` | `setperth` | Установка порогов PER | Нормальный |
| `web_script.h` | `setrxboost` | Переключение усиления приёмника | Нормальный |
| `web_script.h` | `simple` | Тестовый запрос simple | Низкий |
| `web_script.h` | `tddApplyBtn` | Настройка TDD-планировщика | Нормальный |
| `web_script.h` | `toggleenc` | Переключение шифрования | Нормальный |
| `web_script.h` | `updateKeyStatus` | Получение статуса ключа | Высокий |
| `web_script.h` | `updateLinkDiag` | Получение диагностических данных | Низкий |
