#include "ccm.h"
#include <string.h>

// Вспомогательная функция для обнуления памяти
static void zeroize(void* v, size_t n) {
  volatile uint8_t* p = (volatile uint8_t*)v;
  while (n--) *p++ = 0;
}

void mbedtls_ccm_init(mbedtls_ccm_context* ctx) {
  mbedtls_aes_init(&ctx->aes);
}

int mbedtls_ccm_setkey(mbedtls_ccm_context* ctx, const unsigned char* key, unsigned int keybits) {
  return mbedtls_aes_setkey_enc(&ctx->aes, key, keybits);
}

void mbedtls_ccm_free(mbedtls_ccm_context* ctx) {
  mbedtls_aes_free(&ctx->aes);
  zeroize(ctx, sizeof(*ctx));
}

static int ccm_auth_crypt(mbedtls_ccm_context* ctx, int mode, size_t length,
                          const unsigned char* iv, size_t iv_len,
                          const unsigned char* add, size_t add_len,
                          const unsigned char* input, unsigned char* output,
                          unsigned char* tag, size_t tag_len) {
  if (tag_len < 4 || tag_len > 16 || (tag_len & 1))
    return MBEDTLS_ERR_CCM_BAD_INPUT;
  if (iv_len < 7 || iv_len > 13)
    return MBEDTLS_ERR_CCM_BAD_INPUT;
  if (add_len >= 0xFF00)
    return MBEDTLS_ERR_CCM_BAD_INPUT;

  unsigned char q = 16 - 1 - (unsigned char)iv_len;
  unsigned char b[16];
  unsigned char y[16];
  unsigned char ctr[16];
  unsigned char i;

  memset(b, 0, 16);
  b[0] = (unsigned char)(((add_len > 0) << 6) | (((tag_len - 2) / 2) << 3) | (q - 1));
  memcpy(b + 1, iv, iv_len);
  size_t len_left = length;
  for (i = 0; i < q; i++, len_left >>= 8)
    b[15 - i] = (unsigned char)(len_left & 0xFF);
  if (len_left > 0)
    return MBEDTLS_ERR_CCM_BAD_INPUT;

  memset(y, 0, 16);
  for (i = 0; i < 16; ++i) y[i] ^= b[i];
  mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, y, y);

  if (add_len > 0) {
    memset(b, 0, 16);
    b[0] = (unsigned char)((add_len >> 8) & 0xFF);
    b[1] = (unsigned char)(add_len & 0xFF);
    size_t use = add_len < 14 ? add_len : 14;
    memcpy(b + 2, add, use);
    add += use;
    add_len -= use;
    for (i = 0; i < 16; ++i) y[i] ^= b[i];
    mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, y, y);
    while (add_len > 0) {
      memset(b, 0, 16);
      use = add_len > 16 ? 16 : add_len;
      memcpy(b, add, use);
      add += use;
      add_len -= use;
      for (i = 0; i < 16; ++i) y[i] ^= b[i];
      mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, y, y);
    }
  }

  ctr[0] = q - 1;
  memcpy(ctr + 1, iv, iv_len);
  memset(ctr + 1 + iv_len, 0, q);
  ctr[15] = 1;

  const unsigned char* src = input;
  unsigned char* dst = output;
  len_left = length;
  while (len_left > 0) {
    size_t use = len_left > 16 ? 16 : len_left;
    if (mode == 0) { // encrypt
      memset(b, 0, 16);
      memcpy(b, src, use);
      for (i = 0; i < 16; ++i) y[i] ^= b[i];
      mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, y, y);
    }
    unsigned char stream[16];
    memcpy(stream, ctr, 16);
    mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, stream, stream);
    for (i = 0; i < use; ++i)
      dst[i] = src[i] ^ stream[i];
    if (mode != 0) { // decrypt
      memset(b, 0, 16);
      memcpy(b, dst, use);
      for (i = 0; i < 16; ++i) y[i] ^= b[i];
      mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, y, y);
    }
    dst += use;
    src += use;
    len_left -= use;
    for (i = 0; i < q; i++)
      if (++ctr[15 - i] != 0) break;
  }

  for (i = 0; i < q; i++)
    ctr[15 - i] = 0;
  unsigned char s0[16];
  mbedtls_aes_crypt_ecb(&ctx->aes, MBEDTLS_AES_ENCRYPT, ctr, s0);
  for (i = 0; i < tag_len; ++i)
    tag[i] = y[i] ^ s0[i];
  zeroize(s0, sizeof(s0));
  return 0;
}

int mbedtls_ccm_encrypt_and_tag(mbedtls_ccm_context* ctx, size_t length,
                                const unsigned char* iv, size_t iv_len,
                                const unsigned char* add, size_t add_len,
                                const unsigned char* input, unsigned char* output,
                                unsigned char* tag, size_t tag_len) {
  return ccm_auth_crypt(ctx, 0, length, iv, iv_len, add, add_len, input, output, tag, tag_len);
}

int mbedtls_ccm_auth_decrypt(mbedtls_ccm_context* ctx, size_t length,
                             const unsigned char* iv, size_t iv_len,
                             const unsigned char* add, size_t add_len,
                             const unsigned char* input, unsigned char* output,
                             const unsigned char* tag, size_t tag_len) {
  unsigned char check[16];
  int ret = ccm_auth_crypt(ctx, 1, length, iv, iv_len, add, add_len, input, output, check, tag_len);
  if (ret != 0) {
    return ret;
  }
  int diff = 0;
  for (size_t i = 0; i < tag_len; ++i)
    diff |= check[i] ^ tag[i];
  if (diff != 0) {
    zeroize(output, length);
    return MBEDTLS_ERR_CCM_AUTH_FAILED;
  }
  return 0;
}

