#include "Arduino.h"
#include "encryptor_ccm.h"
#include "selftest.h"
#include "Preferences.h"
#include <mbedtls/ecdh.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <string.h>
#include <cstdio>
#include <cstdlib>

// Глобальные объекты как в основном проекте
CcmEncryptor g_ccm;
struct DummyTx { void setEncEnabled(bool) {} } g_tx;
uint8_t g_kid = 1;
enum class KeyStatus { Local = 0, Received = 1 };
KeyStatus g_key_status = KeyStatus::Local;
Preferences prefs; // заглушка NVS

// Копия функции performKeyExchange из проекта для хостового тестирования
bool performKeyExchange() {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  int ret = 0;
  bool ok = false;
  unsigned char secretA[32];
  unsigned char secretB[32];
  unsigned char hash[32];
  uint8_t newKey[16];
  uint8_t newKid = 0;
  unsigned char pubA[65];
  unsigned char pubB[65];
  size_t pubA_len = 0;
  size_t pubB_len = 0;
  size_t secretLenA = 0;
  size_t secretLenB = 0;
  char nameBuf[8];
  mbedtls_ecdh_context ctxA;
  mbedtls_ecdh_context ctxB;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_context entropy;
  const char* pers = "ecdh";
  mbedtls_ecdh_init(&ctxA);
  mbedtls_ecdh_init(&ctxB);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_entropy_init(&entropy);
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char*)pers, strlen(pers));
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_setup(&ctxA, MBEDTLS_ECP_DP_CURVE25519);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_setup(&ctxB, MBEDTLS_ECP_DP_CURVE25519);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_make_public(&ctxA, &pubA_len, pubA, sizeof(pubA),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_make_public(&ctxB, &pubB_len, pubB, sizeof(pubB),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_read_public(&ctxA, pubB, pubB_len);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_read_public(&ctxB, pubA, pubA_len);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_calc_secret(&ctxA, &secretLenA, secretA, sizeof(secretA),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) goto finish;
  ret = mbedtls_ecdh_calc_secret(&ctxB, &secretLenB, secretB, sizeof(secretB),
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) goto finish;
  if (secretLenA != secretLenB || secretLenA == 0 ||
      memcmp(secretA, secretB, secretLenA) != 0) {
    ret = -1;
    goto finish;
  }
  mbedtls_sha256(secretA, 32, hash, 0);
  memcpy(newKey, hash, 16);
  newKid = (uint8_t)((g_kid + 1) & 0xFF);
  if (newKid == 0) newKid = 1;
  g_kid = newKid;
  ok = g_ccm.setKey(g_kid, newKey, 16);
  g_ccm.setKid(g_kid);
  g_ccm.setEnabled(true);
  g_tx.setEncEnabled(true);
  snprintf(nameBuf, sizeof(nameBuf), "k%03u", (unsigned)g_kid);
  prefs.begin("lora", false);
  prefs.putBytes(nameBuf, newKey, 16);
  prefs.putUChar("kid", g_kid);
  prefs.putBool("enc", true);
  prefs.putBool("key_remote", false);
  prefs.end();
  g_key_status = KeyStatus::Local;
  ret = ok ? 0 : -1;
finish:
  mbedtls_ecdh_free(&ctxA);
  mbedtls_ecdh_free(&ctxB);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  return ret == 0;
#else
  // Заглушка для хостовой сборки без mbedTLS
  static bool seeded = false;
  if (!seeded) { srand((unsigned)micros()); seeded = true; }
  uint8_t newKey[16];
  for (int i = 0; i < 16; i++) newKey[i] = (uint8_t)(rand() & 0xFF);
  uint8_t newKid = (uint8_t)((g_kid + 1) & 0xFF);
  if (newKid == 0) newKid = 1;
  g_kid = newKid;
  bool ok = g_ccm.setKey(g_kid, newKey, 16);
  g_ccm.setKid(g_kid);
  g_ccm.setEnabled(true);
  g_tx.setEncEnabled(true);
  g_key_status = KeyStatus::Local;
  return ok;
#endif
}

int main() {
  Serial.begin(0);
  if (!performKeyExchange()) {
    printf("Обмен ключами не удался\n");
    return 1;
  }
  printf("KID=%u ключ=", g_kid);
  const uint8_t* key = g_ccm.key();
  for (int i = 0; i < 16; ++i) printf("%02X", key[i]);
  printf("\n");
  // Минимальный тест шифрования
  EncSelfTest_run(g_ccm, 32, Serial);
  return 0;
}

