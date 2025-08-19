#!/bin/sh
# Скрипт компилирует и запускает тест обмена ключами
set -e

# компилируем, определяя макрос KEY_EXCHANGE_TEST
# после перемещения crypto_spec в корень исправляем путь
g++ -std=c++17 -DARDUINO_ARCH_ESP32 -DKEY_EXCHANGE_TEST key_exchange_test.cpp selftest.cpp message_buffer.cpp test_stubs/Arduino.cpp crypto_spec.cpp encryptor_ccm.cpp -I. -Itest_stubs -lmbedtls -lmbedcrypto -lmbedx509 -o key_exchange_test
./key_exchange_test

# тестируем полный цикл ротации ключа (упрощённая эмуляция)
g++ -std=c++17 key_rotation_test.cpp -o key_rotation_test
./key_rotation_test
