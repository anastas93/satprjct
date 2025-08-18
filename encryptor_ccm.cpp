#include "encryptor_ccm.h"
#include <string.h>

// Конструктор и деструктор по умолчанию
CcmEncryptor::CcmEncryptor() {}
CcmEncryptor::~CcmEncryptor() {}

bool CcmEncryptor::setKey(uint8_t kid, const uint8_t* key, size_t len) {
  if (!key || len != 16) return false;
  KeyInfo& k = keys_[kid];
  // Перинициализируем контекст на случай повторной установки
  mbedtls_ccm_free(&k.ctx);
  mbedtls_ccm_init(&k.ctx);
  memcpy(k.key, key, 16);
  if (mbedtls_ccm_setkey(&k.ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0) {
    keys_.erase(kid);
    return false;
  }
  if (kid == active_kid_) crypto_spec::setCurrentKey(k.key);
  return true;
}

void CcmEncryptor::setEnabled(bool en) { enabled_ = en; }

void CcmEncryptor::setActiveKid(uint8_t kid) {
  active_kid_ = kid;
  auto it = keys_.find(kid);
  if (it != keys_.end()) {
    // Обновляем глобальный ключ для диагностики
    crypto_spec::setCurrentKey(it->second.key);
  }
}

const uint8_t* CcmEncryptor::key(uint8_t kid) const {
  auto it = keys_.find(kid);
  return (it != keys_.end()) ? it->second.key : nullptr;
}

bool CcmEncryptor::isReady() const {
  return enabled_ && keys_.find(active_kid_) != keys_.end();
}

bool CcmEncryptor::encrypt(const uint8_t* plain, size_t plain_len,
                           const uint8_t* aad, size_t aad_len,
                           std::vector<uint8_t>& out) {
  auto it = keys_.find(active_kid_);
  if (!isReady() || it == keys_.end()) {
    out.assign(plain, plain + plain_len);
    return true;
  }
  uint8_t nonce[12];
  buildNonce(aad, aad_len, nonce);
  out.resize(1 + plain_len + cfg::ENC_TAG_LEN);
  out[0] = active_kid_;
  uint8_t* c = out.data() + 1;
  uint8_t* tag = out.data() + 1 + plain_len;
  int rc = mbedtls_ccm_encrypt_and_tag(&it->second.ctx,
      plain_len, nonce, sizeof(nonce), aad, aad_len,
      plain, c, tag, cfg::ENC_TAG_LEN);
  return rc == 0;
}

bool CcmEncryptor::decrypt(const uint8_t* cipher, size_t cipher_len,
                           const uint8_t* aad, size_t aad_len,
                           std::vector<uint8_t>& out) {
  if (!enabled_) {
    out.assign(cipher, cipher + cipher_len);
    return true;
  }
  if (cipher_len < 1 + cfg::ENC_TAG_LEN) return false;
  uint8_t kid = cipher[0];
  auto it = keys_.find(kid);
  if (it == keys_.end()) return false;
  size_t clen = cipher_len - 1 - cfg::ENC_TAG_LEN;
  out.resize(clen);
  uint8_t nonce[12];
  buildNonce(aad, aad_len, nonce);
  int rc = mbedtls_ccm_auth_decrypt(&it->second.ctx,
      clen, nonce, sizeof(nonce), aad, aad_len,
      cipher + 1, out.data(), cipher + 1 + clen, cfg::ENC_TAG_LEN);
  return rc == 0;
}

void CcmEncryptor::buildNonce(const uint8_t* aad, size_t aad_len, uint8_t n[12]) const {
  memset(n, 0, 12);
  FrameHeader h{};
  if (!FrameHeader::decode(h, aad, aad_len)) return;
  n[0] = h.ver; n[1] = h.flags;
  n[2] = (uint8_t)(h.frag_idx & 0xFF); n[3] = (uint8_t)(h.frag_idx >> 8);
  n[4] = (uint8_t)(h.frag_cnt & 0xFF); n[5] = (uint8_t)(h.frag_cnt >> 8);
  n[6] = (uint8_t)(h.msg_id); n[7] = (uint8_t)(h.msg_id >> 8);
  n[8] = (uint8_t)(h.msg_id >> 16); n[9] = (uint8_t)(h.msg_id >> 24);
  n[10] = (uint8_t)(h.payload_len); n[11] = (uint8_t)(h.payload_len >> 8);
}

