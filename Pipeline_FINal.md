
# ESP32 LoRa Pipeline — API Reference (v1)

Цель: быстрый справочник по публичному API/командам/структурам проекта. Подходит для Arduino IDE (один скетч) и для подключения к «оригинальному» радио-коду.

---

## Состав и файлы
- **ESP32_LoRa_Pipeline.ino** — основной скетч, UART-команды, RadioLib-шим (SX1262/1268), интеграция конвейера.
- **freq_map.h** — банки/пресеты частот *(MAIN/RESERVE/TEST)*.
- **config.h** — настройки пайплайна (MTU, тайминги, метрики и пр.).
- **frame.h** — формат кадра, флаги, CRC16.
- **message_buffer.{h,cpp}** — очередь исходящих сообщений + назначение `msg_id`.
- **fragmenter.{h,cpp}** — резка сообщений на фрагменты под `LORA_MTU`.
- **encryptor.h** — интерфейс шифрования + `NoEncryptor` (заглушка, passthrough).
- **tx_pipeline.{h,cpp}** — отправка (ACK по умолчанию **OFF**).
- **rx_pipeline.{h,cpp}** — приём, дефрагментация, анти-дубликаты, колбэки.
- **metrics.h** — счётчики/метрики.
- **radio_adapter.h** — прототипы радио-функций (реализации в `.ino`).

---

## Конфигурация (`config.h`)
```cpp
static constexpr uint8_t  PIPE_VERSION           = 1;
static constexpr uint16_t LORA_MTU               = 255;  // SX126x payload лимит
static constexpr uint16_t MAX_MSG_SIZE           = 4096;
static constexpr uint16_t RX_ASSEMBLER_TTL_MS    = 15000;
static constexpr uint16_t MAX_OPEN_ASSEMBLERS    = 8;
static constexpr bool     ACK_ENABLED_DEFAULT    = false;
static constexpr bool     ENCRYPTION_ENABLED     = false;
static constexpr uint8_t  ENC_TAG_LEN            = 8;     // длина AEAD-тега (байт): 8/12/16 (по умолчанию 8)
static constexpr uint8_t  ENC_META_LEN           = 1;     // байты служебных данных шифрования (KID)
static constexpr bool     ENC_MODE_PER_FRAGMENT  = true;  // AEAD по фрагменту (дефолт)
static constexpr bool     STREAM_MODE_ENABLE     = false; // потоковая отправка больших данных (иначе STRICT)

static constexpr uint16_t SEND_RETRY_MS          = 1200;
static constexpr uint8_t  SEND_RETRY_COUNT       = 3;
static constexpr uint16_t INTER_FRAME_GAP_MS     = 25;
static constexpr uint8_t  RX_DUP_WINDOW          = 32;
static constexpr bool     PIPELINE_METRICS       = true;
/* --------- Buffer sizing (ESP-friendly) --------- */
static constexpr size_t   TX_BUF_MAX_BYTES        = 32 * 1024;  // общий лимит TX очереди
static constexpr uint16_t TX_BUF_MAX_MSGS         = 128;        // общий лимит сообщений
static constexpr size_t   TX_BUF_QOS_CAP_HIGH     = 16 * 1024;  // квоты по QoS
static constexpr size_t   TX_BUF_QOS_CAP_NORMAL   = 12 * 1024;
static constexpr size_t   TX_BUF_QOS_CAP_LOW      = 8  * 1024;

static constexpr size_t   RX_REASM_CAP_BYTES      = 48 * 1024;  // общий лимит памяти сборщика
static constexpr uint16_t RX_REASM_MAX_ASSEMBLERS = 8;          // сколько сообщений одновременно
static constexpr size_t   RX_REASM_PER_MSG_CAP    = 8 * 1024;   // кап на одно сообщение

static constexpr bool     USE_PSRAM_IF_AVAILABLE  = true;       // при наличии — размещать RX/TX буферы в PSRAM
/* ------------------------------------------------- */
```
Полезная нагрузка на один кадр = `LORA_MTU - sizeof(FrameHeader)` = **241 байт**.

---

## Формат кадра (`frame.h`)
```cpp
#pragma pack(push, 1)
struct FrameHeader {
  uint8_t  ver;        // PIPE_VERSION
  uint8_t  flags;      // см. флаги ниже
  uint32_t msg_id;     // ID сообщения
  uint16_t frag_idx;   // индекс фрагмента (0..N-1)
  uint16_t frag_cnt;   // всего фрагментов (1 если без фрагмент.)
  uint16_t payload_len;// байт в этом кадре
  uint16_t crc16;      // CRC-16-CCITT(header c crc16=0 + payload)
};
#pragma pack(pop)

enum FrameFlags : uint8_t {
  F_ACK_REQ = 0x01,  // запрос ACK (на уровне сообщения)
  F_ACK     = 0x02,  // кадр-подтверждение
  F_ENC     = 0x04,  // payload зашифрован
  F_FRAG    = 0x08,  // кадр — часть фрагментации
  F_LAST    = 0x10,  // последний фрагмент
};
```
CRC — транспортная защита; криптоцелостность (когда появится) — через AEAD в `IEncryptor`.

---

## Публичные интерфейсы

### 1) Буфер исходящих (`message_buffer.h`)
```cpp
struct OutgoingMessage {
  uint32_t id;
  bool     ack_required;
  std::vector<uint8_t> data;
};

class MessageBuffer {
public:
  uint32_t enqueue(const uint8_t* data, size_t len, bool ack_required);
  bool     hasPending() const;
  bool     peekNext(OutgoingMessage& out);  // без удаления
  void     popFront();                      // удалить фронт (после отправки/ACK)
  void     markAcked(uint32_t msg_id);     // точечное удаление по id
};
```
Мини-обёртка для приложений:
```cpp
// см. .ino (пример): uint32_t sendPlain(const uint8_t* d, size_t n, bool ack=false);
```

### 2) Фрагментация (`fragmenter.h`)
```cpp
struct Fragment {
  FrameHeader hdr;
  std::vector<uint8_t> payload; // plaintext (до шифрования)
};

class Fragmenter {
public:
  std::vector<Fragment> split(uint32_t msg_id, const uint8_t* data, size_t len,
                              bool ack_required, uint16_t mtu=LORA_MTU);
};
```

### 3) Шифрование (`encryptor.h`)
```cpp
class IEncryptor {
public:
  virtual ~IEncryptor() = default;
  virtual bool encrypt(const uint8_t* plain, size_t plain_len,
                       const uint8_t* aad, size_t aad_len,
                       std::vector<uint8_t>& out) = 0;
  virtual bool decrypt(const uint8_t* cipher, size_t cipher_len,
                       const uint8_t* aad, size_t aad_len,
                       std::vector<uint8_t>& out) = 0;
  virtual bool isReady() const = 0;
};

class NoEncryptor : public IEncryptor { /* passthrough */ };
```
> AEAD (AES-CCM/GCM) подключается без изменения интерфейса. В AAD кладём заголовок (с `crc16=0`).

### 4) Отправка (`tx_pipeline.h`)
```cpp
class TxPipeline {
public:
  TxPipeline(MessageBuffer& buf, Fragmenter& frag, IEncryptor& enc);
  void loop();                 // вызывать в Arduino loop()
  void enableAck(bool v);      // ACK off/on (по умолчанию OFF)
  PipelineMetrics& metrics();  // счётчики

private:
  bool interFrameGap();        // пауза между кадрами
  void sendMessage(const OutgoingMessage& m);
};
```
Поведение при `ACK=OFF`: сообщение удаляется из очереди сразу после отправки всех фрагментов.

### 5) Приём (`rx_pipeline.h`)
```cpp
class RxPipeline {
public:
  using MsgCb = std::function<void(uint32_t, const uint8_t*, size_t)>;
  RxPipeline(IEncryptor& enc);
  void onReceive(const uint8_t* frame, size_t len);
  void setMessageCallback(MsgCb cb);
  PipelineMetrics& metrics();

private:
  // reassembly, dup-filter, GC по TTL
};
```
Анти-дубликаты: окно последних `RX_DUP_WINDOW` `msg_id`. Неполные сборки чистятся по `RX_ASSEMBLER_TTL_MS`.

### 6) Радио-хуки (реализованы в `.ino` через RadioLib)
```cpp
bool Radio_sendRaw(const uint8_t* data, size_t len);
bool Radio_setFrequency(uint32_t hz);
bool Radio_setBandwidth(uint32_t khz);
bool Radio_setSpreadingFactor(uint8_t sf);
bool Radio_setCodingRate(uint8_t cr4x);
bool Radio_setTxPower(int8_t dBm);
void Radio_onReceive(const uint8_t* data, size_t len);
```
> Подключено к **SX1262/1268**. Пины: `NSS=5, DIO1=26, NRST=27, BUSY=25`. **TCXO**: если `GPIO15 == LOW` → `2.4 MHz`, иначе `0`.

---

## UART-команды (встроенные)
Быстрые как в оригинале:
- `B0..B4` — выбрать банк (`MAIN/RESERVE/TEST`)
- `P0..P9` — выбрать пресет (0..9), берётся `fRX[preset]` из `freq_map.h`
- `~wa` / `~w<ms>` — авто/фикс окно пинга (флаг для логики)
- `~ka0` / `~ka1` — ACK OFF/ON *(в текущей сборке — no-op)*

Тест/служебные:
- `SIMPLE` — отправить `"ping"`
- `LARGE [size]` — отправить большой буфер (по умолчанию 1200 байт), проверка фрагментации
- `METRICS` — печать счётчиков
- `VERSION` — печать версии/MTU/полезной нагрузки

(Дополнительно в примере: `@<text>` — широковещательно, `#<dst> <text>` — адресно — пока без кодирования адресата в полезной нагрузке.)

---

## Метрики (`metrics.h`)
```cpp
struct PipelineMetrics {
  uint32_t tx_frames, tx_bytes, tx_msgs;
  uint32_t rx_frames_ok, rx_bytes, rx_msgs_ok;
  uint32_t rx_dup_msgs, rx_crc_fail, rx_assem_drop;
  uint32_t rx_frags;
};
```
Печать см. в `.ino` → `printMetrics()`.

---

## Типовой порядок работы
**Отправка:** `enqueue()` → `TxPipeline::loop()` → `Fragmenter` → (Encrypt) → CRC → `Radio_sendRaw`.  
**Приём:** IRQ/колбэк радио → `Radio_onReceive()` → CRC → (Decrypt) → reassembly → `msg_cb(id,data,len)`.

---

## Замены/расширения
- Включение реального шифрования: заменить `NoEncryptor` на AES-CCM/mbedTLS, `ENCRYPTION_ENABLED=true`.
- Включение ACK: `enableAck(true)` + реализация простого окна/ретраев.
- NVS-профили: можно добавить `SAVE/LOAD` для BANK/PRESET/SF/BW/CR/POWER.

---

## Быстрый FAQ
**Q:** Почему полезная нагрузка 241 B?  
**A:** SX126x FIFO 256 B, наш заголовок 14 B → 255–14 = 241 B полезных данных на кадр.

**Q:** Где подключить приём из радио?  
**A:** В твоём RX-обработчике вызвать `Radio_onReceive(rxBuf, rxLen)` — он отдаст кадр в `RxPipeline`.

**Q:** Почему ACK не работает?  
**A:** По требованию сейчас отключён. Включим через `enableAck(true)` и реализуем простые подтверждения позже.

---

## Контакты точек интеграции
- Радио инициализируется в `.ino` через RadioLib (SX1262/1268). Пины и TCXO логика — как в оригинальном проекте.
- Частоты берутся из `freq_map.h` (флоаты МГц). Для настройщика RadioLib конвертируются в Гц.


---

## Частотные банки (как в оригинале `freq_map.h`)

**MAIN**
| Preset | RX (MHz) | TX (MHz) |
|---:|---:|---:|
| 0 | 251.850 | 292.850 |
| 1 | 257.775 | 311.375 |
| 2 | 262.225 | 295.825 |
| 3 | 260.625 | 294.225 |
| 4 | 262.125 | 295.725 |
| 5 | 263.775 | 297.375 |
| 6 | 263.825 | 297.425 |
| 7 | 263.925 | 297.525 |
| 8 | 268.150 | 309.150 |
| 9 | 267.400 | 294.900 |

**RESERVE**
| Preset | RX (MHz) | TX (MHz) |
|---:|---:|---:|
| 0 | 243.625 | 316.725 |
| 1 | 248.825 | 294.375 |
| 2 | 255.775 | 309.300 |
| 3 | 257.100 | 295.650 |
| 4 | 258.350 | 299.350 |
| 5 | 258.450 | 299.450 |
| 6 | 259.000 | 317.925 |
| 7 | 263.625 | 297.225 |
| 8 | 265.550 | 306.550 |
| 9 | 266.750 | 316.575 |


**TEST**
| Preset | RX (MHz) | TX (MHz) |
|---:|---:|---:|
| 0 | 250.000 | 250.000 |
| 1 | 260.000 | 260.000 |
| 2 | 270.000 | 270.000 |
| 3 | 280.000 | 280.000 |
| 4 | 290.000 | 290.000 |
| 5 | 300.000 | 300.000 |
| 6 | 310.000 | 310.000 |
| 7 | 433.000 | 433.000 |
| 8 | 434.000 | 434.000 |
| 9 | 446.000 | 446.000 |

---

## Блочная схема работы системы

```
[Источники данных: UART, сенсоры, внешние интерфейсы, тестовые генераторы]
        |
        v
[Буфер сообщений]
  - Присваивает ID каждому сообщению
  - Хранит сообщения до успешной отправки
  - Удаляет после отправки или получения ACK (если включён)
        |
        v
[Блок сегментации сообщений]
  - Разделяет длинные сообщения на блоки по MTU (как в оригинале)
  - Маркирует блоки порядковыми номерами
        |
        v
[Блок шифрования]
  - Сейчас заглушка (в будущем AES-CCM/GCM или другой алгоритм)
        |
        v
[Блок отправки LoRa]
  - Используем без изменений из оригинального примера
  - Передача по радиоканалу
        |
        v
[Эфир]
        |
        v
[Приём LoRa]
  - Приём радиоблоков
  - Передаём в блок дешифрования
        |
        v
[Блок дешифрования]
  - Сейчас заглушка
        |
        v
[Сборка длинных сообщений]
  - Склеивает блоки обратно в полное сообщение
          |
        v
  [Буфер сообщений]
  - Присваивает ID каждому сообщению
  - Хранит сообщения
        |
        v
[Выдача в целевой интерфейс]
  - UART, память, обработчики команд
```

**Примечания:**
- ACK по умолчанию выключен.
- Приём и отправка работают симметрично (зеркальная обработка).
- MTU — как в оригинальном коде LoRa-примера.

---

## Блочная схема (конвейер TX/RX)

```
[Источники данных: UART | сенсоры | задачи | протоколы] 
          │
          ▼
[Буфер сообщений]
  • назначает msg_id каждому сообщению
  • хранит до фактической отправки
  • при ACK=OFF — удаляет после отправки
  • при ACK=ON  — удаляет после получения подтверждения
          │
          ▼
[Фрагментация]
  • режет сообщение на куски по (LORA_MTU - sizeof(FrameHeader))
  • выставляет флаги F_FRAG / F_LAST, считает frag_cnt/frag_idx
          │
          ▼
[Шифрование]
  • интерфейс IEncryptor (AEAD)
  • сейчас NoEncryptor (прозрачная копия)
  • при включении шифрования → флаг F_ENC, AAD = FrameHeader (crc16=0)
          │
          ▼
[Передача LoRa (из примера, без правок)]
  • Radio_sendRaw() отправляет кадр как есть
  • CRC16-CCITT считается на заголовок (crc16=0) + payload
──────────────────────────────────────────────────────────
                ЭФИР / РАДИОКАНАЛ
──────────────────────────────────────────────────────────
[Приём LoRa]
  • Radio_onReceive() отдаёт кадр в RxPipeline
          │
          ▼
[Дешифрование]
  • NoEncryptor → passthrough
  • (в будущем: проверка тега AEAD)
          │
          ▼
[Сборка фрагментов]
  • по msg_id и frag_idx/frag_cnt собирает полное сообщение
  • окно анти-дубликатов по msg_id
  • TTL на незавершённые сборки
          │
          ▼
[Колбэк приложения]
  • onMessage(msg_id, data*, len) — готовые полезные данные
```

### Примечания
- **Несколько источников данных** → все пишут в **общий буфер сообщений** (`MessageBuffer::enqueue()`), где назначается `msg_id`.
- Политика хранения:
  - **ACK=OFF** (по умолчанию): удаляем запись из буфера сразу после успешной отправки всех фрагментов.
  - **ACK=ON** (позже): держим в буфере до получения подтверждения (кадр/сообщение ACK).
- **Фрагментация** использует `LORA_MTU` (по умолчанию 255 B) и наш заголовок 14 B → **241 B/кадр**.
- **Шифрование** подключается плагином (`IEncryptor`), без изменения остальных блоков.
- Блок **LoRa** берём из твоего оригинального примера: в проекте это тонкие функции `Radio_*`, вызывающие библиотеку радио.
- На RX стороне порядок обратный: приём → дешифр → сборка → колбэк.

---

## TCXO (Temperature Compensated Crystal Oscillator)

В оригинальном проекте модуль использует внешний TCXO для опорной частоты LoRa-чипа.  
Это даёт:
- Повышенную стабильность частоты при изменении температуры.
- Меньший дрейф частоты → меньше ошибок синхронизации.
- Возможность работы на узких полосах (низкий BW) без срыва приёма.

**Реализация в коде**  
- Перед инициализацией радио на пин питания TCXO подаётся HIGH.
- Задержка для прогрева — ~5–10 мс.
- Только после этого вызывается `begin()` радио.
- В оригинале включение TCXO делается в `ensureRadioBegun()`:
  ```cpp
  pinMode(PIN_TCXO_VCC, OUTPUT);
  digitalWrite(PIN_TCXO_VCC, HIGH);
  delay(10); // прогрев TCXO
  // затем begin()
  ```

**Питание TCXO**
- В схеме модуль TCXO питается от GPIO ESP32 (например, `PIN_TCXO_VCC`).
- Это позволяет отключать его в режиме сна для экономии энергии.

**Примечания**
- Время прогрева зависит от конкретной модели TCXO (обычно 2–10 мс).
- Если использовать автопробуждение LoRa-чипа — TCXO всё равно должен быть стабилен к моменту передачи/приёма.

---

## TCXO (из оригинала) — отдельная справка

**Логика автодетекта (как в исходном проекте):**
- На старте настраивается `GPIO15` как `INPUT_PULLUP`.
- Если `digitalRead(15) == LOW`, считаем что на модуле установлен TCXO **2.4 MHz**.
- Иначе — TCXO **отключён** (`0 MHz`).

Пример (фрагмент инициализации в RadioLib‑шиме):
```cpp
float tcxo = 0.0f;
pinMode(15, INPUT_PULLUP);
if (digitalRead(15) == LOW) {
  tcxo = 2.4f;              // TCXO 2.4 MHz — как в оригинале
}
int st = g_radio.begin(433.0, 125.0, 7, 5, 0x18, 14, 10, tcxo, false);
```

**Как управлять TCXO:**
- Аппаратно: перемычкой/сигналом, который тянет `GPIO15` к GND → включится профиль **2.4 MHz**.
- Программно (если нужно жёстко задать):
  - В `.ino` вместо автодетекта можно напрямую поставить `float tcxo = 2.4f;` (всегда включён) либо `0.0f;` (всегда выключен).

**Зачем это нужно:** SX1262/1268 с TCXO получает стабильную опорную частоту → меньший дрейф/лучше точность частоты, особенно на узких полосах (BW 62.5 кГц) и при низких температурах.

> Примечание: следи за питанием TCXO по даташиту модуля (ток и напряжение TCXO), а также за временем прогрева TCXO перед передачей (обычно десятки миллисекунд).
### Основные команды
| Команда      | Аргументы              | Действие |
|--------------|------------------------|----------|
| `HELP`       | —                      | Печатает список всех доступных команд. |
| `SEND`       | `<text>`               | Отправить текстовое сообщение. |
| `B`          | `<bank>`               | Установить банк частот (0–4). См. раздел **Частоты**. |
| `P`          | `<preset>`             | Установить номер пресета частот внутри банка (0–9). |
| `FREQ`       | `<MHz>`                | Установить частоту вручную (в МГц). |
| `BW`         | `<kHz>`                | Установить полосу пропускания (7.81f, 10.42f, 15.63f, 20.83f, 31.25f)(BW). |
| `SF`         | `<7..12>`              | Установить фактор расширения (Spreading Factor). |
| `CR`         | `<5..8>`                | Установить коэффициент кодирования (Coding Rate). |
| `TXP`        | `<dBm>`                | Установить мощность передачи. |
| `METRICS`    | —                      | Печатает статистику работы конвейера (успехи, ошибки, потери). |
| `VER`        | —                      | Печатает версию прошивки. |
| `TEST1`      | —                      | Тест №1: Отправка короткого сообщения. |
| `TEST2`      | —                      | Тест №2: Отправка длинного сообщения (с фрагментацией). |

### Примеры использования
```
B 2        // выбрать EAST банк
P 7        // выбрать 7-й пресет в банке
SF 9       // spreading factor 9
BW 125     // 125 кГц
CR 5       // coding rate 4/5
TXP 14     // мощность 14 дБм
SEND Hello // отправка "Hello"
TEST1      // запуск теста 1
TEST2      // запуск теста 2
```