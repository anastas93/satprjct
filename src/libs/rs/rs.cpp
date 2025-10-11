#include "rs.h"
#include <array>
#include <cstring>

// Поле GF(256) с примитивным полиномом x^8 + x^4 + x^3 + x^2 + 1 (0x11d)
namespace {
  constexpr uint8_t PRIM = 0x1d; // без старшего бита
  std::array<uint8_t,512> gf_exp{};
  std::array<uint8_t,256> gf_log{};
  bool tables_ready = false;

  inline uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (a==0 || b==0) return 0;
    return gf_exp[ gf_log[a] + gf_log[b] ];
  }

  void init_tables() {
    if (tables_ready) return;
    uint8_t x = 1;
    for (int i=0;i<255;i++) {
      gf_exp[i] = x;
      gf_log[x] = i;
      x <<= 1;
      if (x & 0x100) x ^= PRIM;
    }
    for (int i=255;i<512;i++) gf_exp[i] = gf_exp[i-255];
    tables_ready = true;
  }

  // Генераторный полином для (255,223)
  std::array<uint8_t,33> gen{}; // degree 32
  void init_gen() {
    init_tables();
    gen.fill(0); gen[0]=1;
    for (int i=0;i<32;i++) {
      uint8_t root = gf_exp[i];
      for (int j=i+1;j>0;j--) {
        uint8_t a = gen[j];
        uint8_t b = gf_mul(gen[j-1], root);
        gen[j] = a ^ b;
      }
    }
  }
}

namespace rs255 {

void encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  init_gen();
  out.resize(len + 32);
  if (len) std::memcpy(out.data(), data, len);
  std::array<uint8_t,32> parity{};
  for (size_t i=0;i<len;i++) {
    uint8_t feedback = out[i] ^ parity[0];
    // сдвиг регистра
    for (int j=0;j<31;j++) parity[j] = parity[j+1] ^ gf_mul(feedback, gen[j+1]);
    parity[31] = gf_mul(feedback, gen[32]);
  }
  std::memcpy(out.data()+len, parity.data(), 32);
}

// Простая реализация декодера Берлекампа-Мэсси
bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out, int& corrected) {
  init_gen();
  corrected = 0;
  if (len < 32) return false;
  size_t n = len;
  const size_t npar = 32;
  out.assign(in, in+n);

  // syndromes
  std::array<uint8_t,32> syn{};
  bool has_err=false;
  for (size_t i=0;i<npar;i++) {
    uint8_t s=0;
    for (size_t j=0;j<n;j++) {
      s = out[j] ^ gf_mul(s, gf_exp[i]);
    }
    syn[i]=s;
    if (s) has_err=true;
  }
  if (!has_err) { out.resize(n-npar); return true; }

  // Берлекэмп-Мэсси
  std::array<uint8_t,33> err_loc{}; err_loc[0]=1;
  std::array<uint8_t,33> old_loc{}; old_loc[0]=1;
  int L=0; int m=1;
  for (size_t i=0;i<npar;i++) {
    uint8_t delta = syn[i];
    for (int j=1;j<=L;j++) delta ^= gf_mul(err_loc[j], syn[i-j]);
    old_loc[m] = 0;
    if (delta) {
      if (2*L <= (int)i) {
        auto tmp = err_loc;
        for (size_t j=0;j<err_loc.size();j++) err_loc[j] ^= gf_mul(old_loc[j], delta);
        old_loc = tmp;
        L = i + 1 - L;
      } else {
        for (size_t j=0;j<err_loc.size();j++) err_loc[j] ^= gf_mul(old_loc[j], delta);
      }
    }
    m++;
  }
  // Поиск корней
  std::vector<int> err_pos;
  for (int i=0;i<(int)n;i++) {
    uint8_t x = 1;
    for (int j=1;j<=L;j++) x ^= gf_mul(err_loc[j], gf_exp[(i*j)%255]);
    if (x==0) err_pos.push_back(n-1-i);
  }
  if (err_pos.empty()) return false;

  // Дляни нахождения величин ошибок используем простой алгоритм Форни
  for (int pos : err_pos) {
    // вычисление суммы
    uint8_t num = 0;
    for (size_t i=0;i<npar;i++) {
      uint8_t term = gf_mul(syn[i], gf_exp[(255 - ((pos)*((int)i+1)))%255]);
      num ^= term;
    }
    uint8_t denom = 0;
    for (int i=1;i<=L;i+=2) {
      denom ^= gf_mul(err_loc[i], gf_exp[(i*(255-pos))%255]);
    }
    if (denom==0) return false;
    uint8_t err = gf_mul(num, gf_exp[255 - gf_log[denom]]);
    out[pos] ^= err; corrected++;
  }
  out.resize(n-npar);
  return true;
}

}

