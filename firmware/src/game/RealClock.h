#pragma once
#include <stddef.h>
#include <stdint.h>
#include "game/Atlas.h"
#include "game/Game.h"

// The Deck has no clock that runs while it's off, but routers do: every
// beacon carries the router's own timer, in microseconds since it started.
// The Deck notes a few routers' timers as it goes, and reads them again when
// it's switched on. The difference says how long it was off, so wells keep
// settling while it is. Routers are kept only as device-keyed hashes.

namespace Game {

struct ClockMark {
  uint32_t id;        // Keys::deviceHash of the router's BSSID
  uint8_t channel;
  uint8_t pad[3];
  uint64_t tsf;       // the router's timer, microseconds
};

struct ClockNote {
  static constexpr int Max = 8;
  static constexpr uint32_t Magic = 0x4B4C4F43;   // "CLOK"
  uint32_t magic;
  uint32_t playMs;    // the game's play clock when the note was taken
  uint8_t n;
  uint8_t pad[3];
  ClockMark m[Max];
  uint32_t check;

  uint32_t sum() const {
    const uint8_t* p = (const uint8_t*)this;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < offsetof(ClockNote, check); i++) h = (h ^ p[i]) * 16777619u;
    return h;
  }
  void seal() { magic = Magic; check = sum(); }
  bool valid() const { return magic == Magic && n <= Max && check == sum(); }
};

// Real seconds since the note, from the routers heard again now, or -1 if
// none of them match. A router that restarted reads lower than it did and is
// skipped; the median of the rest wins, so one odd router can't swing it.
inline int64_t secondsSince(const ClockNote& was, const ClockMark* now, int n) {
  constexpr int64_t Cap = 60LL * 24 * 3600;   // two months: anything longer is a bad reading
  int64_t secs[ClockNote::Max];
  int k = 0;
  for (int i = 0; i < was.n && k < ClockNote::Max; i++) {
    for (int j = 0; j < n; j++) {
      if (now[j].id != was.m[i].id || now[j].tsf < was.m[i].tsf) continue;
      const int64_t s = (int64_t)((now[j].tsf - was.m[i].tsf) / 1000000ULL);
      if (s <= Cap) secs[k++] = s;
      break;
    }
  }
  if (k == 0) return -1;
  for (int a = 1; a < k; a++)
    for (int b = a; b > 0 && secs[b] < secs[b - 1]; b--) { const int64_t t = secs[b]; secs[b] = secs[b - 1]; secs[b - 1] = t; }
  return secs[(k - 1) / 2];
}

// Minutes the Deck was off: the real time since the note, less the play time
// the loaded save already counts after it (or plus play time it lost).
inline uint32_t offMinutes(const ClockNote& was, uint32_t savedPlayMs, int64_t secs) {
  if (secs < 0) return 0;
  const int64_t ms = secs * 1000 + (int64_t)was.playMs - (int64_t)savedPlayMs;
  return ms > 0 ? (uint32_t)(ms / 60000) : 0;
}

// Let `minutes` pass for every well still settling. Returns how many settled.
// Only what settling needs comes off lastPlay, never the whole night away: a
// well that settles lands exactly at "settled just now", so the atlas's MOST
// RECENT order (and which wells it forgets first when full) stays as it was.
inline int settleFor(Atlas& atlas, uint32_t minutes, uint32_t playMin) {
  int settledNow = 0;
  for (int i = 0; i < atlas.count; i++) {
    AtlasEntry& a = atlas.e[i];
    if (a.lastPlay + SettleMin <= playMin) continue;
    const uint32_t left = a.lastPlay + SettleMin - playMin;   // minutes it still needs
    const uint32_t off = minutes < left ? minutes : left;
    a.lastPlay = a.lastPlay > off ? a.lastPlay - off : 0;
    settledNow += a.lastPlay + SettleMin <= playMin;
  }
  return settledNow;
}

// Wells still settling at play minute `playMin`.
inline int settling(const Atlas& atlas, uint32_t playMin) {
  int k = 0;
  for (int i = 0; i < atlas.count; i++) k += atlas.e[i].lastPlay + SettleMin > playMin;
  return k;
}

} // namespace Game
