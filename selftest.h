
#pragma once
#include <Arduino.h>
#include "encryptor_ccm.h"
#include "frame.h"
#include "config.h"

bool EncSelfTest_run(CcmEncryptor& ccm, size_t size, Print& out);
void EncSelfTest_battery(CcmEncryptor& ccm, Print& out);
void EncSelfTest_badKid(CcmEncryptor& ccm, Print& out);
void SelfTest_runAll(CcmEncryptor& ccm, Print& out);
// Проверка архивации и восстановления буфера сообщений на целевом устройстве
bool MsgBufSelfTest_run(Print& out);
