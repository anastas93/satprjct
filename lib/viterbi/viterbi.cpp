#include "viterbi.h"
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace vit {
namespace {
  constexpr uint8_t G1 = 0x79; // 171_8
  constexpr uint8_t G2 = 0x5B; // 133_8
  constexpr int K = 7;
  constexpr int STATES = 1 << (K-1); // 64

  inline uint8_t parity(uint8_t x) {
    x ^= x>>4; x ^= x>>2; x ^= x>>1; return x&1;
  }

  struct Trans { uint8_t next; uint8_t out; };
  std::array<Trans,STATES*2> trans; // [state*2 + bit]
  bool init_done=false;
  void init() {
    if (init_done) return;
    for (int s=0;s<STATES;s++) {
      for (int b=0;b<2;b++) {
        uint8_t reg = ((s<<1)|b) & 0x7F; // 7 bits
        uint8_t o0 = parity(reg & G1);
        uint8_t o1 = parity(reg & G2);
        Trans t{ (uint8_t)(reg & 0x3F), (uint8_t)((o0<<1)|o1) };
        trans[s*2 + b] = t;
      }
    }
    init_done=true;
  }

  inline uint8_t get_bit(const uint8_t* data, size_t bit) {
    return (data[bit>>3] >> (7-(bit&7))) & 1;
  }
}

void encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
  // Проверяем корректность указателя на входные данные
  if (len > 0 && data == nullptr) {
    throw std::invalid_argument("vit::encode: data == nullptr при положительной длине");
  }
  init();
  out.clear();
  std::vector<uint8_t> bits; bits.reserve(len*16);
  uint8_t state=0;
  for (size_t i=0;i<len*8;i++) {
    uint8_t b = get_bit(data,i);
    Trans t = trans[state*2 + b];
    state = t.next;
    bits.push_back(t.out>>1);
    bits.push_back(t.out &1);
  }
  // упаковываем в байты
  out.resize((bits.size()+7)/8);
  if (!out.empty()) {
    std::memset(out.data(),0,out.size());
  }
  for (size_t i=0;i<bits.size();i++) {
    out[i>>3] |= bits[i] << (7-(i&7));
  }
}

bool decode(const uint8_t* in, size_t len, std::vector<uint8_t>& out) {
  // Проверяем корректность входных аргументов
  if (len > 0 && in == nullptr) {
    out.clear();
    return false;
  }
  init();
  size_t total_bits = len*8;
  if (total_bits %2 !=0) return false;
  size_t steps = total_bits/2;
  out.clear();
  std::array<uint16_t,STATES> metric, new_metric;
  metric.fill(std::numeric_limits<uint16_t>::max()/2); metric[0]=0;
  std::vector<uint8_t> decisions(steps*STATES);
  for (size_t t=0;t<steps;t++) {
    uint8_t r0 = get_bit(in,2*t);
    uint8_t r1 = get_bit(in,2*t+1);
    new_metric.fill(std::numeric_limits<uint16_t>::max()/2);
    for (int s=0;s<STATES;s++) {
      uint16_t pm = metric[s];
      if (pm >= (std::numeric_limits<uint16_t>::max()/2)) continue;
      for (int b=0;b<2;b++) {
        Trans tr = trans[s*2 + b];
        uint8_t eo0 = tr.out>>1; uint8_t eo1 = tr.out&1;
        uint16_t br = (r0!=eo0) + (r1!=eo1);
        uint16_t m = pm + br;
        int ns = tr.next;
        if (m < new_metric[ns]) {
          new_metric[ns]=m;
          decisions[t*STATES + ns] = (uint8_t)((b<<6)|s);
        }
      }
    }
    metric = new_metric;
  }
  // трассировка
  int state=0; // предполагаем обнуление регистров
  std::vector<uint8_t> bits(steps);
  for (int t=(int)steps-1;t>=0;t--) {
    uint8_t d = decisions[t*STATES + state];
    uint8_t bit = d>>6; uint8_t prev = d & 0x3F;
    bits[t]=bit; state=prev;
  }
  // упаковываем биты в байты
  out.resize((steps+7)/8);
  if (!out.empty()) {
    std::memset(out.data(),0,out.size());
  }
  for (size_t i=0;i<steps;i++) out[i>>3]|=bits[i]<<(7-(i&7));
  return true;
}

}

