#!/bin/sh
# Полный цикл: генерация → рассылка → подтверждение → активация
set -e

# генерация ключа и тест шифрования
g++ -std=c++17 -DARDUINO_ARCH_ESP32 -DKEY_EXCHANGE_TEST key_exchange_test.cpp selftest.cpp message_buffer.cpp test_stubs/Arduino.cpp crypto_spec.cpp encryptor_ccm.cpp -I. -Itest_stubs -lmbedtls -lmbedcrypto -lmbedx509 -o key_exchange_test
./key_exchange_test

# рассылка ключа с подтверждением и активацией
g++ -std=c++17 key_rotation_test.cpp crypto_spec.cpp -I. -o key_rotation_test
./key_rotation_test
