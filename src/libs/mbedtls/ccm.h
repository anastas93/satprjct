#pragma once

#include <stddef.h>
#include <stdint.h>
#include "aes.h"

#define MBEDTLS_ERR_CCM_BAD_INPUT   -0x000E
#define MBEDTLS_ERR_CCM_AUTH_FAILED -0x0012

// Контекст CCM содержит контекст AES
typedef struct {
  mbedtls_aes_context aes;
} mbedtls_ccm_context;

void mbedtls_ccm_init(mbedtls_ccm_context* ctx);
int mbedtls_ccm_setkey(mbedtls_ccm_context* ctx, const unsigned char* key, unsigned int keybits);
void mbedtls_ccm_free(mbedtls_ccm_context* ctx);

int mbedtls_ccm_encrypt_and_tag(mbedtls_ccm_context* ctx, size_t length,
                                const unsigned char* iv, size_t iv_len,
                                const unsigned char* add, size_t add_len,
                                const unsigned char* input, unsigned char* output,
                                unsigned char* tag, size_t tag_len);

int mbedtls_ccm_auth_decrypt(mbedtls_ccm_context* ctx, size_t length,
                             const unsigned char* iv, size_t iv_len,
                             const unsigned char* add, size_t add_len,
                             const unsigned char* input, unsigned char* output,
                             const unsigned char* tag, size_t tag_len);

