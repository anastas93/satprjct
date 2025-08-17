
#pragma once
#include "freq_map.h"

// Объявления вспомогательных функций пинга
void SatPing();                    // асинхронный пинг текущего пресета
bool ChannelPing();                // проверка канала на текущем пресете
bool PresetPing(Bank bank, int preset); // проверка указанного пресета
void MassPing(Bank bank);          // проверка всех пресетов банка
