
# ESP32 LoRa Pipeline — Track D
- ACK/ретраи, разнос RX/TX, пресеты, метрики, NVS
- AES-CCM (тег 8 B), `ENC/KID/KEY`
- Локальные тесты `ENCTEST`, `ENCTEST_BAD`
- **Персистентный msg_id в NVS** (исключение повторного нонса)
- **Анти-replay** (окно и порог старых ID)

Команды (добавленные):
- `ENCTEST [size]` — локальный self-test шифрования/дешифрования
- `ENCTEST_BAD [size]` — негативный тест (неверный KID)
- `IDRESET [val]` — установить текущий msg_id (для тестов)
- `REPLAYCLR` — логическое очищение окна replay (no-op видимый)
