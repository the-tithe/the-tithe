#pragma once
#include <stdint.h>

// A small random number generator (PCG32) whose state lives in the save,
// so a run picks up exactly where it left off.

namespace Game {

struct Rng {
  uint64_t state = 0x853c49e6748fea9bULL;
  uint64_t inc = 0xda3e39cb94b95bdbULL;

  void seed(uint64_t s, uint64_t stream = 54) {
    state = 0;
    inc = (stream << 1u) | 1u;
    next();
    state += s;
    next();
  }
  uint32_t next() {
    const uint64_t old = state;
    state = old * 6364136223846793005ULL + inc;
    const uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    const uint32_t rot = (uint32_t)(old >> 59u);
    return (xs >> rot) | (xs << ((-rot) & 31));
  }
  // 0 .. n-1, without bias.
  uint32_t below(uint32_t n) {
    if (n <= 1) return 0;
    const uint32_t limit = -n % n;
    for (;;) {
      const uint32_t r = next();
      if (r >= limit) return r % n;
    }
  }
  double unit() { return next() * (1.0 / 4294967296.0); }   // [0, 1)
  bool chance(double p) { return unit() < p; }
  double uniform(double a, double b) { return a + (b - a) * unit(); }
  // Index picked with the given weights.
  int weighted(const double* w, int n) {
    double total = 0;
    for (int i = 0; i < n; i++) total += w[i];
    double r = unit() * total;
    for (int i = 0; i < n; i++) {
      if (r < w[i]) return i;
      r -= w[i];
    }
    return n - 1;
  }
};

} // namespace Game
