# Отчёт о прогоне тестов

Дата: 01.10.2025

## Подготовка окружения
- Установлен пакет `libsodium-dev` для криптографических тестов (`apt-get install -y libsodium-dev`).

## Команды и результаты

| Тест | Команда | Статус | Примечание |
|------|---------|--------|------------|
| test_key_transfer | `g++ -std=c++17 -I. -Ilibs tests/test_key_transfer.cpp libs_includes.cpp message_buffer.cpp -lsodium -o tests/build_test_key_transfer`<br>`./tests/build_test_key_transfer` | Успех | Вывод `OK`. |
| test_enct | `g++ -std=c++17 -I. -Ilibs tests/test_enct.cpp libs_includes.cpp message_buffer.cpp -lsodium -o tests/build_test_enct`<br>`./tests/build_test_enct` | Успех | Вывод `OK`. |
| test_full_flow | `g++ -std=c++17 -I. -Ilibs tests/test_full_flow.cpp tx_module.cpp rx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_full_flow`<br>`./tests/build_test_full_flow` | Успех | Подтверждено сообщение `OK`. |
| test_keyloader | `g++ -std=c++17 -I. -Ilibs tests/test_keyloader.cpp libs_includes.cpp message_buffer.cpp -lsodium -o tests/build_test_keyloader`<br>`./tests/build_test_keyloader` | Успех | Подтверждено сообщение `OK`. |
| test_text_converter | `g++ -std=c++17 -I. -Ilibs tests/test_text_converter.cpp libs_includes.cpp message_buffer.cpp -lsodium -o tests/build_test_text_converter`<br>`./tests/build_test_text_converter` | Успех | Подтверждено сообщение `OK`. |
| test_packet_splitter | `g++ -std=c++17 -I. -Ilibs tests/test_packet_splitter.cpp message_buffer.cpp tx_module.cpp libs_includes.cpp -lsodium -pthread -o tests/build_test_packet_splitter`<br>`./tests/build_test_packet_splitter` | Успех | Подтверждено сообщение `OK`. |
| test_pilot_marker | `g++ -std=c++17 -I. -Ilibs tests/test_pilot_marker.cpp tx_module.cpp rx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_pilot_marker`<br>`./tests/build_test_pilot_marker` | Падение | Аварийное завершение: `Assertion 'radio.history.size() == before_ack + 1' failed`. |
| test_processing_without_send | `g++ -std=c++17 -I. -Ilibs tests/test_processing_without_send.cpp tx_module.cpp rx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_processing_without_send`<br>`./tests/build_test_processing_without_send` | Успех | Подтверждено сообщение `OK`. |
| test_message_buffer | `g++ -std=c++17 -I. -Ilibs tests/test_message_buffer.cpp message_buffer.cpp libs_includes.cpp -lsodium -o tests/build_test_message_buffer`<br>`./tests/build_test_message_buffer` | Успех | Подтверждено сообщение `OK`. |
| test_large_packet_capacity | `g++ -std=c++17 -I. -Ilibs tests/test_large_packet_capacity.cpp message_buffer.cpp tx_module.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_large_packet_capacity`<br>`./tests/build_test_large_packet_capacity` | Успех | Вывод завершился без ошибок. |
| test_received_buffer | `g++ -std=c++17 -I. -Ilibs tests/test_received_buffer.cpp libs_includes.cpp message_buffer.cpp -lsodium -o tests/build_test_received_buffer`<br>`./tests/build_test_received_buffer` | Успех | Подтверждено сообщение `OK`. |
| test_rx_received_buffer | `g++ -std=c++17 -I. -Ilibs tests/test_rx_received_buffer.cpp rx_module.cpp tx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_rx_received_buffer`<br>`./tests/build_test_rx_received_buffer` | Успех | Подтверждено сообщение `OK`. |
| test_split_reassembly | `g++ -std=c++17 -I. -Ilibs tests/test_split_reassembly.cpp tx_module.cpp rx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_split_reassembly`<br>`./tests/build_test_split_reassembly` | Успех | Сборка и выполнение прошли без ошибок. |
| test_rx_profiling | `g++ -std=c++17 -I. -Ilibs tests/test_rx_profiling.cpp rx_module.cpp tx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_rx_profiling`<br>`./tests/build_test_rx_profiling` | Падение | Аварийное завершение: `Assertion 'received.size() == expected_plain.size()' failed`. |
| test_tx_module | `g++ -std=c++17 -I. -Ilibs tests/test_tx_module.cpp tx_module.cpp message_buffer.cpp libs_includes.cpp -pthread -lsodium -o tests/build_test_tx_module` | Не собрано | Ошибка компиляции: конфликт объявлений `ackPayload`. |

