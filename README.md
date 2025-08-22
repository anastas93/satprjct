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
  - `serial_radio_control.ino` — пример настройки банков каналов, BW, SF, CR и мощности через Serial, вывода текущих параметров и отправки тестовых пакетов через `TxModule`.
- `TextConverter` — библиотека (`libs/text_converter/`) для преобразования UTF-8 текста в байты CP1251, используемая командой `TX`.
- `rs255223` — библиотека (`libs/rs255223/`) с обёртками `encode()` и `decode()` для кода Рида–Соломона RS(255,223).
- `byte_interleaver` — библиотека (`libs/byte_interleaver/`) с функциями `interleave()` и `deinterleave()` для байтового перемежения.
- `conv_codec` — библиотека (`libs/conv_codec/`) с функциями `encodeBits()` и `viterbiDecode()` для свёрточного кодирования.
- `bit_interleaver` — библиотека (`libs/bit_interleaver/`) с функциями `interleave()` и `deinterleave()` для битового перемежения.
- `scrambler` — библиотека (`libs/scrambler/`) с функциями `scramble()` и `descramble()` на основе LFSR
  (полином x^16 + x^14 + x^13 + x^11, стартовое значение 0xACE1).
- Для отладочных сообщений предусмотрен флаг `DefaultSettings::DEBUG` и уровни журналирования `DefaultSettings::LOG_LEVEL`. Доступны макросы `LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, `DEBUG_LOG` и их варианты с выводом значения (`*_VAL`) для фильтрации лишнего спама. Макросы на Arduino дополнительно вызывают `Serial.flush()`, чтобы не терять часть вывода.

Все сторонние библиотеки расположены в каталоге `libs/`.
Для корректной сборки в Arduino добавлен вспомогательный файл `libs_includes.cpp`,
который явно подключает реализации библиотек.

## API
### MessageBuffer
- `MessageBuffer(size_t capacity)` — создать буфер с максимальным количеством сообщений.
- `uint32_t enqueue(const uint8_t* data, size_t len)` — добавляет сообщение в буфер, при переполнении возвращает `0`.
- `size_t freeSlots() const` — получить число свободных слотов в очереди.
- `bool dropLast()` — удалить последнее сообщение (используется для отката).
- `bool hasPending() const` — проверяет наличие сообщений.
- `bool pop(uint32_t& id, std::vector<uint8_t>& out)` — извлекает сообщение и его идентификатор.

### PacketSplitter
- `PacketSplitter(PayloadMode mode, size_t custom = 0)` — создать делитель с выбранным режимом или произвольным размером блока.
- `void setMode(PayloadMode mode)` — сменить режим.
- `void setCustomSize(size_t custom)` — задать произвольный размер части в байтах.
- `uint32_t splitAndEnqueue(MessageBuffer& buf, const uint8_t* data, size_t len)` — разбить данные на части и занести их в буфер с предварительной проверкой свободных слотов и откатом при ошибке.

### PacketGatherer
- `PacketGatherer(PayloadMode mode, size_t custom = 0)` — создать собиратель с режимом или произвольным размером блока.
- `void reset()` — сбросить накопленные данные.
- `void add(const uint8_t* data, size_t len)` — добавить очередную часть.
- `bool isComplete() const` — проверить завершение сбора (актуально при фиксированном размере блока).
- `const std::vector<uint8_t>& get() const` — получить готовое сообщение.

### FrameHeader
- `bool encode(uint8_t* out, size_t out_len, const uint8_t* payload, size_t payload_len)` — записать заголовок в буфер и подсчитать CRC.
- `static bool decode(const uint8_t* data, size_t len, FrameHeader& out)` — разобрать заголовок из буфера и проверить CRC.

### TxModule
- `TxModule(IRadio& radio, const std::array<size_t,4>& capacities, PayloadMode mode)` — инициализация модуля с четырьмя очередями QoS.
- `void setPayloadMode(PayloadMode mode)` — сменить режим размера пакета.
- `uint32_t queue(const uint8_t* data, size_t len, uint8_t qos = 0)` — поместить сообщение в очередь выбранного класса QoS.
- `void loop()` — отправить первое сообщение, если оно есть.
- `void setSendPause(uint32_t pause_ms)` — задать паузу между отправками в миллисекундах.
- После `rs255223::encode()` выполняется `byte_interleaver::interleave()`, затем `conv_codec::encodeBits()`,
  при необходимости `bit_interleaver::interleave()` и `scrambler::scramble()`.

### RxModule
- `void setCallback(RxModule::Callback cb)` — установить обработчик входящих данных.
- `void onReceive(const uint8_t* data, size_t len)` — принять кадр, проверить CRC и передать полезные данные обработчику.
- Перед декодером RS выполняется обратная цепочка: `scrambler::descramble()`,
  `bit_interleaver::deinterleave()`, `conv_codec::viterbiDecode()`,
  затем `byte_interleaver::deinterleave()`.

### RadioSX1262
  - `bool begin()` — инициализация радиомодуля с автоматическим возвратом параметров к значениям по умолчанию.
- `void send(const uint8_t* data, size_t len)` — отправка пакета.
- `void loop()` — обработка готовности пакета и вызов колбэка в основном цикле.
- `void setReceiveCallback(IRadio::RxCallback cb)` — регистрация обработчика приёма.
- `bool setBank(ChannelBank bank)` — выбрать банк каналов (`EAST`, `WEST`, `TEST`).
- `bool setChannel(uint8_t ch)` — выбрать номер канала 0…9 и установить частоту приёма.
- `bool setBandwidth(float bw)` — задать ширину полосы в кГц.
- `bool setSpreadingFactor(int sf)` — указать фактор расширения 5…12.
- `bool setCodingRate(int cr)` — задать коэффициент кодирования 5…8.
- `bool setPower(uint8_t preset)` — установить уровень мощности из таблицы (-5…22 дБм).
- `void sendBeacon()` — отправить служебный пакет-маяк.
- `ChannelBank getBank() const`, `uint8_t getChannel() const`, `float getBandwidth() const`, `int getSpreadingFactor() const`, `int getCodingRate() const`, `int getPower() const`, `float getRxFrequency() const`, `float getTxFrequency() const` — получить текущие параметры.
  - `bool resetToDefaults()` — вернуть параметры радиомодуля к значениям по умолчанию.
  - Значения параметров по умолчанию заданы в файле `default_settings.h`.

### SerialProgramCollector
- `void resetBuffer()` — очистить буфер и начать новый сбор.
- `bool appendToBuffer(const String& line)` — добавить строку в общий буфер с проверкой переполнения.

### TextConverter
- `std::vector<uint8_t> utf8ToCp1251(const std::string& in)` — преобразует UTF-8 строку в байты CP1251.

### rs255223
- `void encode(const uint8_t* in, uint8_t* out)` — кодирует 223 байта в 255 байт.
- `bool decode(const uint8_t* in, uint8_t* out)` — декодирует 255 байт и возвращает 223 байта данных.

### conv_codec
- `void encodeBits(const uint8_t* in, size_t len, std::vector<uint8_t>& out)` — свёрточное кодирование (R=1/2).
- `bool viterbiDecode(const uint8_t* in, size_t len, std::vector<uint8_t>& out)` — декодирование алгоритмом Витерби.

### bit_interleaver
- `void interleave(uint8_t* buf, size_t len)` — битовое перемежение.
- `void deinterleave(uint8_t* buf, size_t len)` — обратное битовое перемежение.

## Пример последовательности вызовов
```
// Передача
шифрование -> PacketSplitter (223 байта) -> rs255223::encode ->
byte_interleaver::interleave -> conv_codec::encodeBits ->
bit_interleaver::interleave -> scrambler::scramble -> отправка

// Приём
scrambler::descramble -> bit_interleaver::deinterleave ->
conv_codec::viterbiDecode -> byte_interleaver::deinterleave ->
rs255223::decode -> PacketGatherer -> обработка сообщения
```

## Что реализовано
- Базовая отправка и приём сообщений.
- Очереди сообщений по классам QoS с приоритетной обработкой.
- Делитель и собиратель пакетов с тремя режимами размера payload.
- Реализована настройка радиомодуля SX1262 и методы отправки/приёма.
- Обработка входящих пакетов через флаг `packetReady` и метод `loop()`.
- Приём строк по Serial и объединение их в буфер до 500 КБ.
- Добавлен заголовок кадра с контролем CRC и вставкой пилотов при передаче.
- Настройка полосы, фактора расширения, коэффициента кодирования и банка каналов с разнесением частот приёма и передачи через Serial.
- Тестовая отправка пакета через Serial с автоматическим переключением частоты TX.
- Настройка паузы между отправками пакетов в TxModule.
- Добавлено кодирование и декодирование RS(255,223) с байтовым интерливингом.
- Формирование 223-байтовых блоков через `PacketSplitter` перед RS-кодированием и сборка их в `RxModule` с помощью `PacketGatherer`.
- Внедрены свёрточное кодирование и Viterbi-декодирование,
  опциональный битовый интерливинг и скремблирование кадра перед передачей.
- Проверка максимального размера кадра перед отправкой (255 байт).
- Сброс параметров радиомодуля к значениям по умолчанию.
- Автоматическая установка этих параметров при запуске.
- Настройка мощности и команда INFO для отображения текущих параметров.
- Отправка служебного маяка для поиска устройств.
- Конвертация UTF-8 текста в CP1251 для команды TX.
- Файл `default_settings.h` с параметрами радиомодуля по умолчанию.
- Базовые тесты для буфера сообщений, делителя пакетов и формирования кадров.

## Что осталось сделать
  - Расширить управление радиомодулем (массовый пинг и т.д.).
  - Добавить подтверждения, шифрование и кодирование ошибок.
  - Реализовать обработку пилотов в RxPipeline (`updatePhaseFromPilot`).
  - Вернуть расширенные возможности исходного проекта по мере необходимости.
  - Уточнить формат пакетов (нумерация частей, контроль целостности) для надёжной сборки.
  - Завершить интеграцию RS-кода для сообщений произвольной длины.
  - Передавать точную длину последнего фрагмента при сборке сообщений.
  - Реализовать шифрование полезной нагрузки перед фрагментацией.
  - Добавить сохранение собранной программы и расширенную обработку команд завершения.
  - Покрыть тестами приём и обработку пилотов.
  - Реализовать поддержку других кодировок и символов в конвертере текста.
  - Внедрить планировщик WFQ для классов QoS.
  - Реализовать разделение кадра при превышении лимита размера.
  - Добавить тесты и настройку для битового интерливинга и свёрточного кодека.

