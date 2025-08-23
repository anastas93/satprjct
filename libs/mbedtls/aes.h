#pragma once

#include <stdint.h>
#include <stddef.h>

#define MBEDTLS_AES_ENCRYPT 1

// Контекст AES с развёрнутыми ключами
typedef struct {
  uint8_t round_key[240]; // максимум для AES-256
  uint8_t nr;             // число раундов
} mbedtls_aes_context;

// Инициализация контекста
void mbedtls_aes_init(mbedtls_aes_context* ctx);

// Очистка контекста
void mbedtls_aes_free(mbedtls_aes_context* ctx);

// Установка ключа для шифрования (128/192/256 бит)
int mbedtls_aes_setkey_enc(mbedtls_aes_context* ctx, const unsigned char* key, unsigned int keybits);

// Шифрование одного блока (ECB)
int mbedtls_aes_crypt_ecb(mbedtls_aes_context* ctx, int mode,
                          const unsigned char input[16], unsigned char output[16]);

