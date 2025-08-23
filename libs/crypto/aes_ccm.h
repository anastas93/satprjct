#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Используется упрощённая реализация AES-CCM на базе функций mbedTLS
// encrypt_ccm() шифрует данные и формирует тег аутентичности
// decrypt_ccm() проверяет тег и расшифровывает данные
bool encrypt_ccm(const uint8_t* key, size_t key_len,
                 const uint8_t* nonce, size_t nonce_len,
                 const uint8_t* aad, size_t aad_len,
                 const uint8_t* input, size_t input_len,
                 std::vector<uint8_t>& output,
                 std::vector<uint8_t>& tag, size_t tag_len);

bool decrypt_ccm(const uint8_t* key, size_t key_len,
                 const uint8_t* nonce, size_t nonce_len,
                 const uint8_t* aad, size_t aad_len,
                 const uint8_t* input, size_t input_len,
                 const uint8_t* tag, size_t tag_len,
                 std::vector<uint8_t>& output);
