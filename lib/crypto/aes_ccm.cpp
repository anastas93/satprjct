#include "aes_ccm.h"
#include "../mbedtls/ccm.h"

// Шифрование в режиме AES-CCM
bool encrypt_ccm(const uint8_t* key, size_t key_len,
                 const uint8_t* nonce, size_t nonce_len,
                 const uint8_t* aad, size_t aad_len,
                 const uint8_t* input, size_t input_len,
                 std::vector<uint8_t>& output,
                 std::vector<uint8_t>& tag, size_t tag_len) {
  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);
  if (mbedtls_ccm_setkey(&ctx, key, key_len * 8) != 0) {
    mbedtls_ccm_free(&ctx);
    return false;
  }
  output.resize(input_len);
  tag.resize(tag_len);
  int ret = mbedtls_ccm_encrypt_and_tag(&ctx, input_len,
                                        nonce, nonce_len,
                                        aad, aad_len,
                                        input, output.data(),
                                        tag.data(), tag_len);
  mbedtls_ccm_free(&ctx);
  return ret == 0;
}

// Расшифрование и проверка тега
bool decrypt_ccm(const uint8_t* key, size_t key_len,
                 const uint8_t* nonce, size_t nonce_len,
                 const uint8_t* aad, size_t aad_len,
                 const uint8_t* input, size_t input_len,
                 const uint8_t* tag, size_t tag_len,
                 std::vector<uint8_t>& output) {
  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);
  if (mbedtls_ccm_setkey(&ctx, key, key_len * 8) != 0) {
    mbedtls_ccm_free(&ctx);
    return false;
  }
  output.resize(input_len);
  int ret = mbedtls_ccm_auth_decrypt(&ctx, input_len,
                                     nonce, nonce_len,
                                     aad, aad_len,
                                     input, output.data(),
                                     tag, tag_len);
  mbedtls_ccm_free(&ctx);
  return ret == 0;
}
