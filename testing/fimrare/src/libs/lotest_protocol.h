#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace Lotest {

// Тип кадра протокола Lotest
enum class FrameType : uint8_t {
  Data = 0,   // DATA с полезной нагрузкой
  Ack = 1,    // ACK c BITMAP16
  Fin = 2,    // FIN с CRC-16 всего файла
  Parity = 3  // HARQ-паритет RS(15, 11)
};

// Размер фиксированного кадра в байтах
constexpr size_t kFrameSize = 8;
// Размер полезной нагрузки DATA/FIN/PAR в байтах
constexpr size_t kPayloadSize = 5;
// Размер окна ARQ (ACK каждые 8 пакетов)
constexpr size_t kArqWindow = 8;
// Количество символов данных в коде RS(15, 11)
constexpr size_t kRsDataSymbols = 11;
// Количество символов паритета в коде RS(15, 11)
constexpr size_t kRsParitySymbols = 4;

// Флаги кадра
struct FrameFlags {
  static constexpr uint8_t kNeedParity = 0x04; // для ACK: запросить PAR пакеты
};

// Структура кадра протокола Lotest
struct Frame {
  std::array<uint8_t, kFrameSize> bytes{};

  FrameType type() const;
  void setType(FrameType type);

  uint8_t sequence() const;
  void setSequence(uint8_t seq);

  uint8_t payloadSize() const;
  void setPayloadSize(uint8_t size);

  uint8_t flags() const;
  void setFlags(uint8_t value);

  std::array<uint8_t, kPayloadSize> payload() const;
  void setPayload(const uint8_t* data, size_t len);
};

// Настройки уровня протокола
struct Config {
  bool enableInterleaving = false;  // перестановка последовательности
  uint8_t interleaveStep = 4;       // шаг перестановки (SEQ += step)
  bool enableHarq = false;          // генерация HARQ-паритета
  bool forcePhyFec = false;         // включить FEC на PHY-уровне
  bool enablePayloadCrc8 = false;   // опциональный CRC-8 на DATA (уменьшает payload до 4 байт)
};

// Информация об ACK
struct AckBitmap {
  uint8_t baseSeq = 0;              // первый SEQ окна
  uint8_t count = 0;                // количество кадров в окне
  uint16_t bitmap = 0;              // BITMAP16 наличия кадров
  bool needParity = false;          // получателю требуются PAR кадры
};

// Состояние HARQ-группы
struct HarqGroup {
  uint8_t baseSeq = 0;                                // SEQ первого кадра группы
  std::vector<std::array<uint8_t, kPayloadSize>> data; // накопленные DATA payload
};

// Результат обработки входящего кадра
struct ProcessResult {
  bool accepted = false;               // кадр принят в окно
  bool completedMessage = false;       // сообщение собрано целиком
  std::vector<uint8_t> assembled;      // собранная полезная нагрузка
  std::optional<AckBitmap> ack;        // сформированный ACK
  bool isAck = false;                  // входящий кадр — ACK
  AckBitmap ackInfo;                   // если входящий кадр ACK — его содержимое
};

// Подготовка перестановки SEQ для окна ARQ
std::vector<uint8_t> buildInterleavingOrder(size_t count, uint8_t step);

// CRC-16/CCITT-FALSE
uint16_t crc16(const uint8_t* data, size_t len, uint16_t initial = 0xFFFF);
uint16_t crc16(const std::vector<uint8_t>& data, uint16_t initial = 0xFFFF);

// CRC-8/Dallas-Maxim
uint8_t crc8(const uint8_t* data, size_t len, uint8_t initial = 0x00);

// Преобразование кадра ACK в структуру
std::optional<AckBitmap> parseAck(const Frame& frame);
// Формирование кадра ACK
Frame buildAck(const AckBitmap& ack);

// Класс для работы с кодом RS(15, 11)
class ReedSolomon1511 {
public:
  ReedSolomon1511();
  // Вычисление 4 байтов паритета для 11 байтов данных
  std::array<uint8_t, kRsParitySymbols> encode(const std::array<uint8_t, kRsDataSymbols>& data) const;

private:
  std::array<uint8_t, 256> alphaTo_{}; // таблица анти-логарифмов
  std::array<uint8_t, 256> indexOf_{}; // таблица логарифмов
  std::array<uint8_t, kRsParitySymbols + 1> generator_{}; // порождающий многочлен

  static uint8_t gfMul(uint8_t a, uint8_t b, const std::array<uint8_t, 256>& alphaTo,
                       const std::array<uint8_t, 256>& indexOf);
};

} // namespace Lotest

