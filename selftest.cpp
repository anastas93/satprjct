
#include "selftest.h"
#include <vector>
#include <string.h>

bool EncSelfTest_run(CcmEncryptor& ccm, size_t size, Print& out) {
  if (!ccm.isReady()) {
    out.println(F("ENC not ready: set KEY and enable ENC (ENC 1)"));
    return false;
  }
  std::vector<uint8_t> plain(size);
  for (size_t i=0;i<size;i++) plain[i] = (uint8_t)(i & 0xFF);

  FrameHeader hdr{};
  hdr.ver = cfg::PIPE_VERSION; hdr.flags = F_ENC;
  hdr.msg_id = 0xA55A1234; hdr.frag_idx = 0; hdr.frag_cnt = 1;
  hdr.payload_len = (uint16_t)plain.size(); hdr.crc16 = 0;
  std::vector<uint8_t> aad(reinterpret_cast<uint8_t*>(&hdr), reinterpret_cast<uint8_t*>(&hdr)+sizeof(FrameHeader));

  std::vector<uint8_t> enc, dec;
  unsigned long t0 = micros();
  bool ok1 = ccm.encrypt(plain.data(), plain.size(), aad.data(), aad.size(), enc);
  unsigned long t1 = micros();
  bool ok2 = ok1 && ccm.decrypt(enc.data(), enc.size(), aad.data(), aad.size(), dec);
  unsigned long t2 = micros();
  bool same = ok2 && (dec == plain);
  out.printf("ENCTEST size=%u enc=%s dec=%s time_enc=%luus time_dec=%luus\n",
             (unsigned)size, ok1? "OK":"FAIL", ok2? "OK":"FAIL", (t1 - t0), (t2 - t1));
  if (!same) {
    out.println(F("Mismatch!"));
    size_t n = plain.size() < dec.size() ? plain.size() : dec.size();
    size_t bad = 0;
    for (size_t i=0;i<n && bad<16;i++) if (plain[i]!=dec[i]) { out.printf(" idx=%u pt=%02X ct=%02X\n",(unsigned)i,plain[i],dec[i]); bad++; }
  }
  return same;
}

void EncSelfTest_battery(CcmEncryptor& ccm, Print& out) {
  const size_t sizes[] = {0,1,2,7,8,15,16,31,32,63,64,100,128,200,254,255,512,1000};
  for (size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) EncSelfTest_run(ccm, sizes[i], out);
}

void EncSelfTest_badKid(CcmEncryptor& ccm, Print& out) {
  if (!ccm.isReady()) { out.println(F("ENC not ready: set KEY and ENC 1")); return; }
  std::vector<uint8_t> plain(32);
  for (size_t i=0;i<plain.size();++i) plain[i] = (uint8_t)(i ^ 0x5A);

  FrameHeader hdr{};
  hdr.ver = cfg::PIPE_VERSION; hdr.flags = F_ENC; hdr.msg_id = 0xDEADBEEF;
  hdr.frag_idx=0; hdr.frag_cnt=1; hdr.payload_len=(uint16_t)plain.size(); hdr.crc16=0;
  std::vector<uint8_t> aad(reinterpret_cast<uint8_t*>(&hdr), reinterpret_cast<uint8_t*>(&hdr)+sizeof(FrameHeader));

  std::vector<uint8_t> enc, dec;
  if (!ccm.encrypt(plain.data(), plain.size(), aad.data(), aad.size(), enc)) { out.println(F("Encrypt failed")); return; }
  if (!enc.empty()) enc[0] ^= 0x01; // flip KID
  bool ok = ccm.decrypt(enc.data(), enc.size(), aad.data(), aad.size(), dec);
  out.printf("ENCTESTBAD wrong-KID dec=%s (expected FAIL)\n", ok? "OK":"FAIL");
}

void SelfTest_runAll(CcmEncryptor& ccm, Print& out) {
  out.println(F("SELFTEST: begin"));
  EncSelfTest_battery(ccm, out);
  out.println(F("SELFTEST: end"));
}
