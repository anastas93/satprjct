
#pragma once
#include "encryptor.h"
#include "config.h"
#include "frame.h"
#include "crypto_spec.h"
#include <mbedtls/ccm.h>
#include <string.h>

class CcmEncryptor : public IEncryptor {
public:
  CcmEncryptor() { mbedtls_ccm_init(&ccm_); }
  ~CcmEncryptor() { mbedtls_ccm_free(&ccm_); }
  bool setKey(uint8_t kid, const uint8_t* key, size_t len) {
    if (!key || len != 16) return false;
    kid_ = kid;
    memcpy(key_, key, 16);
    // Сохраняем ключ и его CRC-16 в глобальном хранилище
    crypto_spec::setCurrentKey(key_);
    if (mbedtls_ccm_setkey(&ccm_, MBEDTLS_CIPHER_ID_AES, key_, 128) != 0) { key_set_ = false; return false; }
    key_set_ = true; return true;
  }
  void setEnabled(bool en) { enabled_ = en; }
  void setKid(uint8_t kid) { kid_ = kid; }
  uint8_t kid() const { return kid_; }
  // Expose the current AES key used for encryption.  This returns a
  // pointer to the 16‑byte key buffer.  It should only be used for
  // diagnostic or secure key exchange purposes.  The caller must
  // ensure the key is handled securely and not leaked over insecure
  // channels.
  const uint8_t* key() const { return key_; }
  bool isReady() const override { return enabled_ && key_set_; }

  bool encrypt(const uint8_t* plain, size_t plain_len,
               const uint8_t* aad, size_t aad_len,
               std::vector<uint8_t>& out) override {
    if (!isReady()) { out.assign(plain, plain+plain_len); return true; }
    uint8_t nonce[12]; buildNonce(aad, aad_len, nonce);
    out.resize(1 + plain_len + cfg::ENC_TAG_LEN);
    out[0] = kid_;
    uint8_t* c = out.data()+1;
    uint8_t* tag = out.data()+1+plain_len;
    int rc = mbedtls_ccm_encrypt_and_tag(&ccm_,
      plain_len, nonce, sizeof(nonce), aad, aad_len, plain, c, tag, cfg::ENC_TAG_LEN);
    return rc==0;
  }

  bool decrypt(const uint8_t* cipher, size_t cipher_len,
               const uint8_t* aad, size_t aad_len,
               std::vector<uint8_t>& out) override {
    if (!isReady()) { out.assign(cipher, cipher+cipher_len); return true; }
    if (cipher_len < 1 + cfg::ENC_TAG_LEN) return false;
    if (cipher[0] != kid_) return false;
    size_t clen = cipher_len - 1 - cfg::ENC_TAG_LEN;
    out.resize(clen);
    uint8_t nonce[12]; buildNonce(aad, aad_len, nonce);
    int rc = mbedtls_ccm_auth_decrypt(&ccm_,
      clen, nonce, sizeof(nonce), aad, aad_len, cipher+1, out.data(), cipher+1+clen, cfg::ENC_TAG_LEN);
    return rc==0;
  }
private:
  void buildNonce(const uint8_t* aad, size_t aad_len, uint8_t n[12]) const {
    memset(n, 0, 12);
    FrameHeader h{};
    if (!FrameHeader::decode(h, aad, aad_len)) return;
    n[0]=h.ver; n[1]=h.flags;
    n[2]=(uint8_t)(h.frag_idx & 0xFF); n[3]=(uint8_t)(h.frag_idx>>8);
    n[4]=(uint8_t)(h.frag_cnt & 0xFF); n[5]=(uint8_t)(h.frag_cnt>>8);
    n[6]=(uint8_t)(h.msg_id); n[7]=(uint8_t)(h.msg_id>>8);
    n[8]=(uint8_t)(h.msg_id>>16); n[9]=(uint8_t)(h.msg_id>>24);
    n[10]=(uint8_t)(h.payload_len); n[11]=(uint8_t)(h.payload_len>>8);
  }
  mbedtls_ccm_context ccm_;
  bool key_set_ = false;
  bool enabled_ = false;
  uint8_t kid_ = 0;
  uint8_t key_[16];
};
