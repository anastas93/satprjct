#pragma once

#include <cstdint>

namespace LoRaRadioLibSettings {

// Структура описывает параметры драйвера SX1262, управляемые через RadioLib.
// Для каждого поля приведено краткое описание его назначения.
struct SX1262DriverOptions {
  float bandwidthKhz = 15.63f;        // Ширина полосы пропускания (кГц).
                                      // Меньшая полоса повышает чувствительность и увеличивает время передачи.
  uint8_t spreadingFactor = 7;        // Фактор расширения SF (5..12).
                                      // Увеличение повышает устойчивость, но снижает скорость.
  uint8_t codingRateDenom = 5;        // Делитель коэффициента кодирования CR (5..8).
                                      // CR=4/denom, задаёт избыточность и помехоустойчивость.
  int8_t lowPowerDbm = -5;            // Низкий уровень мощности передачи (dBm) для щадящего режима.
  int8_t highPowerDbm = 22;           // Высокий уровень мощности передачи (dBm) для максимальной дальности.
  bool useDio2AsRfSwitch = true;      // Включение аппаратного RF-переключателя на выводе DIO2.
  bool useDio3ForTcxo = false;        // Разрешение питания внешнего TCXO через вывод DIO3.
  float tcxoVoltage = 0.0f;           // Напряжение питания TCXO (В). Значение 0 отключает управление TCXO.
  bool enableRegulatorLDO = true;     // Принудительный перевод радиочасти в режим LDO-регулятора.
  bool enableRegulatorDCDC = false;   // Принудительный перевод радиочасти в режим DC-DC (если поддерживается аппаратно).
  bool autoLdro = true;               // Автоматический выбор оптимизации для низких скоростей передачи (LDRO).
  bool implicitHeader = true;         // Использовать фиксированный размер пакета (implicit header) вместо стандартного заголовка.
  uint8_t implicitPayloadLength = 8;  // Размер полезной нагрузки при implicit header (байты).
  bool enableCrc = true;              // Добавлять ли аппаратный CRC в конец LoRa-пакета.
  bool invertIq = false;              // Инверсия фаз (I/Q) для совместимости с определёнными сетями.
  bool publicNetwork = true;          // Использовать стандартное публичное синхрослово LoRa (true) либо приватное (false).
  uint16_t syncWord = 0x34;           // Значение синхрослова LoRa (8 или 16 бит в зависимости от режима).
  uint16_t preambleLength = 12;       // Длина преамбулы в символах — влияет на время пробуждения приёмника.
  bool rxBoostedGain = true;          // Режим усиленного каскада LNA при приёме для повышения чувствительности.
};

// Набор параметров по умолчанию, применяемый при инициализации радиомодуля.
// Имя константы содержит суффикс OPTIONS, чтобы избежать конфликта с макросами Arduino (DEFAULT).
#if __cplusplus >= 201703L
inline constexpr SX1262DriverOptions DEFAULT_OPTIONS{};
inline constexpr bool DEFAULT_RX_BOOSTED_GAIN = DEFAULT_OPTIONS.rxBoostedGain;         // Режим усиленного приёма
inline constexpr uint16_t DEFAULT_PREAMBLE_LENGTH = DEFAULT_OPTIONS.preambleLength;    // Длина преамбулы по умолчанию
#else
constexpr SX1262DriverOptions DEFAULT_OPTIONS{};                                       // отдельная копия в каждом переводимом модуле
constexpr bool DEFAULT_RX_BOOSTED_GAIN = DEFAULT_OPTIONS.rxBoostedGain;                // Режим усиленного приёма без inline
constexpr uint16_t DEFAULT_PREAMBLE_LENGTH = DEFAULT_OPTIONS.preambleLength;           // Длина преамбулы по умолчанию без inline
#endif

} // namespace LoRaRadioLibSettings
