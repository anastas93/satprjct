# Функции отправки и приоритеты

Этот документ перечисляет все функции проекта, способные отправлять данные наружу. Для каждой функции указан рекомендуемый приоритет для последующей модерации через механизм QoS.

| Файл | Функция | Назначение | Приоритет |
|------|---------|------------|-----------|
| `tx_engine.cpp` | `TxEngine::sendFrame` | Низкоуровневая передача кадра через радио. | Высокий |
| `tx_engine.cpp` | `Radio_sendRaw` | Совместимый API для отправки кадров в радио (учитывает Qos). | Высокий |
| `tx_pipeline.cpp` | `TxPipeline::sendMessageFragments` | Фрагментация и отправка сообщения по кадрам. | Средний |
| `rx_pipeline.cpp` | `RxPipeline::sendAck` | Отправка ACK при приёме сообщений. | Высокий |
| `ESP32_LoRa_Pipeline.ino` | `handleSend` | HTTP‑обработчик отправки текстового сообщения. | Нормальный |
| `ESP32_LoRa_Pipeline.ino` | `handleSendQ` | HTTP‑обработчик отправки сообщения с явным QoS. | Зависит от параметра |
| `ESP32_LoRa_Pipeline.ino` | `sendKeyRequest` | Постановка в очередь запроса ключа. | Высокий |
| `ESP32_LoRa_Pipeline.ino` | `sendKeyResponse` | Постановка в очередь ответа с ключом. | Высокий |
| `web_script.h` | `sendParam` | Настройка параметров радио и протокола (`setbw`, `setsf`, `setcr`, `settxp` и др.). | Нормальный |
| `web_script.h` | `sendPerTh`, `sendEbn0Th` | Установка порогов PER и Eb/N0. | Нормальный |
| `web_script.h` | `sendChat`/`chatWs.send` | Отправка текстового сообщения через чат. | Нормальный |
| `web_script.h` | `sendLarge` (`/large`) | Отправка большого сообщения. | Нормальный |
| `web_script.h` | `sendQBtn`, `largeQBtn` | Сообщения с явным приоритетом. | Зависит от параметра |
| `web_script.h` | `ping`, `channelping`, `presetping`, `massping`, `satping` | Диагностические ping‑команды. | Низкий |
| `web_script.h` | `simple`, `enctest`, `enctestbad`, `selftest`, `msgid` | Тестовые запросы. | Низкий |
| `web_script.h` | `keytest`, `keyreq`, `keysend`, `keydh`, `setkey`, `setkid`, `keystatus` | Операции с ключами. | Высокий |
| `web_script.h` | `idreset`, `replayclr` | Очистка защитных счётчиков. | Высокий |
| `web_script.h` | `qosmode`, `qos` | Управление и статистика QoS. | Нормальный |
| `web_script.h` | `tddApplyBtn` (`/tdd`) | Настройка TDD‑планировщика. | Нормальный |
| `web_script.h` | `metrics`, `linkdiag`, `frames` | Получение диагностических данных. | Низкий |
| `web_script.h` | `archivelist`, `archiverestore` | Работа с архивом сообщений. | Нормальный |
| `web_script.h` | `setack`, `setdup`, `setrxboost`, `toggleenc` | Переключение режимов протокола. | Нормальный |

## Примечания
- Все перечисленные функции вызывают `Qos_allow`, блокирующий низкоприоритетные отправки при наличии более важных.
- Дополнительные источники отправки данных следует добавлять в таблицу по мере появления.
