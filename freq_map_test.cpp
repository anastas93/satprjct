#ifndef ARDUINO
// Юнит-тест проверяет, что пресеты частот совпадают с эталонными
// значениями и порядок RX/TX не перепутан.
#include <cstdio>
#include <cmath>
#include <vector>
#include "freq_map.h"

static bool eq(float a, float b) {
  return std::fabs(a - b) < 0.0001f;
}

int main() {
  // Эталонные значения из исходного проекта
  const std::vector<FreqPreset> ref_main = {
    {251.850f, 292.850f}, {257.775f, 311.375f}, {262.225f, 295.825f},
    {260.625f, 294.225f}, {262.125f, 295.725f}, {263.775f, 297.375f},
    {263.825f, 297.425f}, {263.925f, 297.525f}, {268.150f, 309.150f},
    {267.400f, 294.900f}
  };
  const std::vector<FreqPreset> ref_reserve = {
    {243.625f, 316.725f}, {248.825f, 294.375f}, {255.775f, 309.300f},
    {257.100f, 295.650f}, {258.350f, 299.350f}, {258.450f, 299.450f},
    {259.000f, 317.925f}, {263.625f, 297.225f}, {265.550f, 306.550f},
    {266.750f, 316.575f}
  };
  const std::vector<FreqPreset> ref_test = {
    {250.000f, 250.000f}, {260.000f, 260.000f}, {270.000f, 270.000f},
    {280.000f, 280.000f}, {290.000f, 290.000f}, {300.000f, 300.000f},
    {310.000f, 310.000f}, {433.000f, 433.000f}, {434.000f, 434.000f},
    {446.000f, 446.000f}
  };

  const auto& main = GetFreqTable(Bank::MAIN);
  const auto& reserve = GetFreqTable(Bank::RESERVE);
  const auto& test = GetFreqTable(Bank::TEST);

  bool ok = true;
  for (size_t i = 0; i < ref_main.size(); ++i) {
    if (!eq(main[i].rxMHz, ref_main[i].rxMHz) ||
        !eq(main[i].txMHz, ref_main[i].txMHz)) {
      std::printf("MAIN preset %zu mismatch\n", i);
      ok = false;
    }
    if (!eq(reserve[i].rxMHz, ref_reserve[i].rxMHz) ||
        !eq(reserve[i].txMHz, ref_reserve[i].txMHz)) {
      std::printf("RESERVE preset %zu mismatch\n", i);
      ok = false;
    }
    if (!eq(test[i].rxMHz, ref_test[i].rxMHz) ||
        !eq(test[i].txMHz, ref_test[i].txMHz)) {
      std::printf("TEST preset %zu mismatch\n", i);
      ok = false;
    }
  }
  std::printf("freq_map_test %s\n", ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
#endif // !ARDUINO
