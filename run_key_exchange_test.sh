#!/bin/sh
# Скрипт компилирует и запускает тест обмена ключами
set -e

g++ -std=c++17 -DARDUINO_ARCH_ESP32 key_exchange_test.cpp selftest.cpp test_stubs/Arduino.cpp -I. -Itest_stubs -lmbedtls -lmbedcrypto -lmbedx509 -o key_exchange_test
./key_exchange_test
