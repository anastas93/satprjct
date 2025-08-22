# Минимальный модуль передачи и приёма

В форке оставлены только базовые компоненты:

- `MessageBuffer` — простой буфер сообщений с ограничением по размеру.
- `PacketSplitter` — делитель пакетов с тремя режимами размера полезной нагрузки.
- `PacketGatherer` — собиратель пакетов обратно в сообщение.
- `TxModule` — передача данных через интерфейс `IRadio`.
- `RxModule` — приём данных и передача их в пользовательский колбэк.
- `FrameHeader` — заголовок кадра с функциями `encode()` и `decode()`.
- `IRadio` описан в `radio_interface.h`.
- `RadioSX1262` — конкретная реализация интерфейса на базе модуля SX1262.
- `SerialProgramCollector` — библиотека для приёма строк по Serial и сборки их в один буфер (`libs/serial_program_collector/`).
- `serial_radio_control.ino` — пример настройки частоты, BW и SF через Serial.

Все сторонние библиотеки расположены в каталоге `libs/`.

## API
### MessageBuffer
- `MessageBuffer(size_t capacity)` — создать буфер с максимальным количеством сообщений.
- `uint32_t enqueue(const uint8_t* data, size_t len)` — добавляет сообщение в буфер, при переполнении возвращает `0`.
- `bool hasPending() const` — проверяет наличие сообщений.
- `bool pop(uint32_t& id, std::vector<uint8_t>& out)` — извлекает сообщение и его идентификатор.

### PacketSplitter
- `PacketSplitter(PayloadMode mode)` — создать делитель с выбранным режимом (`SMALL`, `MEDIUM`, `LARGE`).
- `void setMode(PayloadMode mode)` — сменить режим.
- `uint32_t splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len)` — разбить данные на части и занести их в буфер.

### PacketGatherer
- `PacketGatherer(PayloadMode mode)` — создать собиратель.
- `void reset()` — сбросить накопленные данные.
- `void add(const uint8_t* data, size_t len)` — добавить очередную часть.
- `bool isComplete() const` — проверить завершение сбора.
- `const std::vector<uint8_t>& get() const` — получить готовое сообщение.

### FrameHeader
- `bool encode(uint8_t* out, size_t out_len, const uint8_t* payload, size_t payload_len)` — записать заголовок в буфер и подсчитать CRC.
- `static bool decode(const uint8_t* data, size_t len, FrameHeader& out)` — разобрать заголовок из буфера и проверить CRC.

### TxModule
- `TxModule(IRadio& radio, MessageBuffer& buf, PayloadMode mode)` — инициализация модуля.
- `void setPayloadMode(PayloadMode mode)` — сменить режим размера пакета.
- `uint32_t queue(const uint8_t* data, size_t len)` — поместить сообщение в очередь.
- `void loop()` — отправить первое сообщение, если оно есть.
- `void setSendPause(uint32_t pause_ms)` — задать паузу между отправками в миллисекундах.

### RxModule
- `void setCallback(RxModule::Callback cb)` — установить обработчик входящих данных.
- `void onReceive(const uint8_t* data, size_t len)` — принять кадр, проверить CRC и передать полезные данные обработчику.

### RadioSX1262
- `bool begin()` — инициализация радиомодуля с преднастройками.
- `void send(const uint8_t* data, size_t len)` — отправка пакета.
- `void setReceiveCallback(IRadio::RxCallback cb)` — регистрация обработчика приёма.
- `bool setFrequency(float freq)` — установить рабочую частоту в МГц.
- `bool setBandwidth(float bw)` — задать ширину полосы в кГц.
- `bool setSpreadingFactor(int sf)` — указать фактор расширения 5…12.

### SerialProgramCollector
- `void resetBuffer()` — очистить буфер и начать новый сбор.
- `bool appendToBuffer(const String& line)` — добавить строку в общий буфер с проверкой переполнения.

## Что реализовано
- Базовая отправка и приём сообщений.
- Очередь сообщений без приоритета.
- Делитель и собиратель пакетов с тремя режмами размера payload.
- Реализована настройка радиомодуля SX1262 и методы отправки/приёма.
- Приём строк по Serial и объединение их в буфер до 500 КБ.
- Добавлен заголовок кадра с контролем CRC и вставкой пилотов при передаче.
- Настройка частоты, полосы и фактора расширения через Serial.
- Настройка паузы между отправками пакетов в TxModule.
- Базовые тесты для буфера сообщений, делителя пакетов и формирования кадров.

## Что осталось сделать
- Расширить управление радиомодулем (мощность, банки частот, маяк и т.д.).
- Добавить подтверждения, шифрование и кодирование ошибок.
- Реализовать обработку пилотов в RxPipeline (`updatePhaseFromPilot`).
- Вернуть расширенные возможности исходного проекта по мере необходимости.
- Уточнить формат пакетов (нумерация частей, контроль целостности) для надёжной сборки.
- Добавить сохранение собранной программы и расширенную обработку команд завершения.
- Покрыть тестами приём и обработку пилотов.

