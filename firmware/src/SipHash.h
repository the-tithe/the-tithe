#pragma once
#include <stddef.h>
#include <stdint.h>

// SipHash-2-4, a keyed hash: without the key, the output says nothing about
// the input. The Deck uses it to turn router IDs and network names into
// numbers that can't be traced back to the real network.

namespace SipHash {

inline uint64_t rotl(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }

inline uint64_t load64(const uint8_t* p) {
  uint64_t v = 0;
  for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
  return v;
}

inline uint64_t hash(const uint8_t key[16], const uint8_t* in, size_t len) {
  const uint64_t k0 = load64(key), k1 = load64(key + 8);
  uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
  uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
  uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
  uint64_t v3 = 0x7465646279746573ULL ^ k1;
  auto round = [&] {
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
  };
  const size_t whole = len - (len % 8);
  for (size_t i = 0; i < whole; i += 8) {
    const uint64_t m = load64(in + i);
    v3 ^= m; round(); round(); v0 ^= m;
  }
  uint64_t b = ((uint64_t)len) << 56;
  for (size_t i = 0; i < len % 8; i++) b |= ((uint64_t)in[whole + i]) << (8 * i);
  v3 ^= b; round(); round(); v0 ^= b;
  v2 ^= 0xff;
  round(); round(); round(); round();
  return v0 ^ v1 ^ v2 ^ v3;
}

} // namespace SipHash
