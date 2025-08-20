
// Карта частот. Для расширяемости и удобства работы используем std::vector,
// чтобы можно было добавлять произвольное число пресетов без изменения API.
#pragma once
#include <stdint.h>
#include <vector>

enum class Bank : uint8_t { MAIN = 0, RESERVE = 1, TEST = 2 };

// Пара частот приёма и передачи (в мегагерцах)
struct FreqPreset {
  float rxMHz;
  float txMHz;
};

// Основной банк частот
static const std::vector<FreqPreset> FREQ_MAIN = {
  {251.850f, 292.850f}, {257.775f, 311.375f}, {262.225f, 295.825f},
  {260.625f, 294.225f}, {262.125f, 295.725f}, {263.775f, 297.375f},
  {263.825f, 297.425f}, {263.925f, 297.525f}, {268.150f, 309.150f},
  {267.400f, 294.900f}
};

// Резервный банк частот
static const std::vector<FreqPreset> FREQ_RESERVE = {
  {243.625f, 316.725f}, {248.825f, 294.375f}, {255.775f, 309.300f},
  {257.100f, 295.650f}, {258.350f, 299.350f}, {258.450f, 299.450f},
  {259.000f, 317.925f}, {263.625f, 297.225f}, {265.550f, 306.550f},
  {266.750f, 316.575f}
};

// Тестовый банк: RX и TX совпадают
static const std::vector<FreqPreset> FREQ_TEST = {
  {250.000f, 250.000f}, {260.000f, 260.000f}, {270.000f, 270.000f},
  {280.000f, 280.000f}, {290.000f, 290.000f}, {300.000f, 300.000f},
  {310.000f, 310.000f}, {433.000f, 433.000f}, {434.000f, 434.000f},
  {446.000f, 446.000f}
};

// Возвращает ссылку на вектор пресетов для указанного банка
inline const std::vector<FreqPreset>& GetFreqTable(Bank bank) {
  switch (bank) {
    case Bank::MAIN:
      return FREQ_MAIN;
    case Bank::RESERVE:
      return FREQ_RESERVE;
    case Bank::TEST:
      return FREQ_TEST;
  }
  // По умолчанию возвращаем основной банк, чтобы избежать UB
  return FREQ_MAIN;
}
