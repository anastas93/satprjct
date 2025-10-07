#pragma once

#include <cstdint>

namespace LoRaRadioLibSettings {

// Структура описывает параметры драйвера SX1262, управляемые через RadioLib.
// Все поля снабжены подробными комментариями с указанием их влияния.
struct SX1262DriverOptions {
  bool useDio2AsRfSwitch = true;      // Использовать ли вывод DIO2 в качестве RF-переключателя
  bool useDio3ForTcxo = false;        // Управлять ли внешним TCXO через DIO3
  float tcxoVoltage = 0.0f;           // Напряжение питания TCXO, В (0 — TCXO не используется)
  bool enableRegulatorLDO = true;    // Принудительно включить LDO-регулятор питания
  bool enableRegulatorDCDC = false;   // Принудительно включить DC-DC регулятор
  bool autoLdro = true;               // Автоматическое управление низким коэффициентом дьюти-цикла (LDRO)
  bool implicitHeader = true;         // Режим implicit header (фиксированная длина полезной нагрузки)
  uint8_t implicitPayloadLength = 32; // Длина полезной нагрузки в режиме implicit header
  bool enableCrc = false;             // Включение аппаратного CRC пакета
  bool invertIq = false;              // Инверсия I/Q при приёме/передаче
  bool publicNetwork = true;          // Использовать ли публичное LoRa-синхрослово
  uint16_t syncWord = 0x34;           // Пользовательское синхрослово LoRa
  uint16_t preambleLength = 24;      // Длина преамбулы в символах
  bool rxBoostedGain = true;          // Усиленный режим LNA при приёме
};

// Набор параметров по умолчанию, применяемый при инициализации радиомодуля.
// Имя константы содержит суффикс OPTIONS, чтобы избежать конфликта с макросами Arduino (DEFAULT).
inline constexpr SX1262DriverOptions DEFAULT_OPTIONS{};

// Удобные псевдонимы для часто используемых параметров.
inline constexpr bool DEFAULT_RX_BOOSTED_GAIN = DEFAULT_OPTIONS.rxBoostedGain;         // Режим усиленного приёма
inline constexpr uint16_t DEFAULT_PREAMBLE_LENGTH = DEFAULT_OPTIONS.preambleLength;    // Длина преамбулы по умолчанию

} // namespace LoRaRadioLibSettings
