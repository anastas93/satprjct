# Минимальный модуль передачи и приёма

В форке оставлены только базовые компоненты:

- `MessageBuffer` — простой буфер сообщений с ограничением по размеру.
- `PacketSplitter` — делитель пакетов с тремя режимами размера полезной нагрузки.
- `PacketGatherer` — собиратель пакетов обратно в сообщение.
- `TxModule` — передача данных через интерфейс `IRadio`.
- `RxModule` — приём данных и передача их в пользовательский колбэк.
- `IRadio` описан в `radio_interface.h`.
- `RadioSX1262` — конкретная реализация интерфейса на базе модуля SX1262.
- `serial_program_collector.ino` — пример приёма строк по Serial и сборки их в один буфер.

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

### TxModule
- `TxModule(IRadio& radio, MessageBuffer& buf, PayloadMode mode)` — инициализация модуля.
- `void setPayloadMode(PayloadMode mode)` — сменить режим размера пакета.
- `uint32_t queue(const uint8_t* data, size_t len)` — поместить сообщение в очередь.
- `void loop()` — отправить первое сообщение, если оно есть.

### RxModule
- `void setCallback(RxModule::Callback cb)` — установить обработчик входящих данных.
- `void onReceive(const uint8_t* data, size_t len)` — передать принятый пакет обработчику.

### RadioSX1262
- `bool begin()` — инициализация радиомодуля с преднастройками.
- `void send(const uint8_t* data, size_t len)` — отправка пакета.
- `void setReceiveCallback(IRadio::RxCallback cb)` — регистрация обработчика приёма.
- `bool setFrequency(float mhz)` — установить рабочую частоту.
- `bool setBandwidth(float khz)` — установить ширину полосы.
- `bool setSpreadingFactor(int sf)` — установить фактор расширения спектра.

### SerialProgramCollector
- `void resetBuffer()` — очистить буфер и начать новый сбор.
- `bool appendToBuffer(const String& line)` — добавить строку в общий буфер с проверкой переполнения.
- Команды `SET_FREQ`, `SET_BW`, `SET_SF` позволяют изменить настройки радиомодуля.

## Что реализовано
- Базовая отправка и приём сообщений.
- Очередь сообщений без приоритета.
- Делитель и собиратель пакетов с тремя режимами размера payload.
- Реализована настройка радиомодуля SX1262 и методы отправки/приёма.
- Приём строк по Serial и объединение их в буфер до 500 КБ.
- Настройка частоты, BW и SF через команды Serial.
- Тесты для MessageBuffer и PacketSplitter.

## Что осталось сделать
- Расширить управление радиомодулем (мощность, банки частот, маяк и т.д.).
- Добавить подтверждения, шифрование и кодирование ошибок.
- Вернуть расширенные возможности исходного проекта по мере необходимости.
- Уточнить формат пакетов (нумерация частей, контроль целостности) для надёжной сборки.
- Добавить сохранение собранной программы и расширенную обработку команд завершения.
- Написать тесты для остальных компонентов.

