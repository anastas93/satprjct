#!/bin/sh
# Скрипт компилирует и запускает тест обмена ключами
set -e

# компилируем, определяя макрос KEY_EXCHANGE_TEST
g++ -std=c++17 -DARDUINO_ARCH_ESP32 -DKEY_EXCHANGE_TEST key_exchange_test.cpp selftest.cpp test_stubs/Arduino.cpp -I. -Itest_stubs -lmbedtls -lmbedcrypto -lmbedx509 -o key_exchange_test
./key_exchange_test
