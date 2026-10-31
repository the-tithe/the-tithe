#pragma once
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "CatalogData.h"
#include "Run.h"

// Everything that lasts between runs: lifetime stats, what's unlocked, Pact
// seals, Grimoire pages, and the queue of things earned but not yet
// revealed (the Grimoire reveals up to 2 a Binding). Matches play_career()
// and unlocked_by() in tools/balance_sim.py.

namespace Game {

enum class Kind : uint8_t { Relic, Trinket, Spell, Calling };

template <int RC>
struct CareerT {
  uint32_t runs, wins;
  uint32_t stats[(int)Cat::Stat::Count];   // totals, or best values for "best" stats
  uint8_t relics[(RC + 7) / 8];
  uint8_t trinkets[(Cat::TrinketCount + 7) / 8];
  uint8_t spells[(Cat::SpellCount + 7) / 8];
  uint16_t callings;                       // bit per Cat::Calling
  uint8_t wonWith[(RC + 7) / 8];
  uint32_t schoolWins;                     // bit per school with 2+ relics in a win
  uint16_t callingsWon;
  int8_t seals[Cat::CallingCount];         // best Pact won with each Calling, -1 none
  int8_t bestPact;
  uint8_t eclipsesSurvived;                // bit per Eclipse
  uint16_t pages;                          // bit per Grimoire page
  uint8_t theme;                           // the Deck theme in use (a page index)
  uint16_t challengesWon;                  // bit per Challenge won
  uint16_t channels;                       // bit per channel ever sold on
  // Earned, waiting for the end of a Binding to be revealed.
  struct Pending { uint8_t kind, id; } queue[24];
  uint8_t nQueue;
  // Revealed at the end of the last Binding, for the Grimoire screen.
  Pending revealed[UnlocksPerRun + 2];
  uint8_t nRevealed;
  uint8_t pad[2];
};
using Career = CareerT<RelicIdCap>;

inline bool getBit(const uint8_t* b, int i) { return (b[i >> 3] >> (i & 7)) & 1; }
inline void setBit(uint8_t* b, int i) { b[i >> 3] |= (uint8_t)(1u << (i & 7)); }

inline bool unlocked(const Career& c, Kind k, int id) {
  switch (k) {
    case Kind::Relic: return getBit(c.relics, id);
    case Kind::Trinket: return getBit(c.trinkets, id);
    case Kind::Spell: return getBit(c.spells, id);
    case Kind::Calling: return (c.callings >> id) & 1;
  }
  return false;
}

inline void unlock(Career& c, Kind k, int id) {
  switch (k) {
    case Kind::Relic: setBit(c.relics, id); break;
    case Kind::Trinket: setBit(c.trinkets, id); break;
    case Kind::Spell: setBit(c.spells, id); break;
    case Kind::Calling: c.callings |= (uint16_t)(1u << id); break;
  }
}

// A fresh career: the starting pool, the three starting Callings, page one.
inline void newCareer(Career& c) {
  memset(&c, 0, sizeof(c));
  for (int i = 0; i < Cat::RelicCount; i++)
    if (Cat::Relics[i].unlock == Cat::Stat::None && !Cat::Relics[i].legendary) setBit(c.relics, i);
  for (int i = 0; i < Cat::TrinketCount; i++)
    if (Cat::Trinkets[i].unlock == Cat::Stat::None) setBit(c.trinkets, i);
  for (int i = 0; i < Cat::SpellCount; i++)
    if (Cat::Spells[i].unlock == Cat::Stat::None) setBit(c.spells, i);
  for (int i = 0; i < Cat::CallingCount; i++)
    if (Cat::Callings[i].unlock == Cat::Stat::None) c.callings |= (uint16_t)(1u << i);
  for (int i = 0; i < Cat::CallingCount; i++) c.seals[i] = -1;
  c.bestPact = -1;
  c.pages = 1;   // "The first Deck"
}

// The week under way, if any, and the atlas's count of wells ever offered at.
// The app sets both each frame (like playMinNow), so every counter the player
// reads mid-week (rank, "PAY 10 TITHES 6/10", "FIND 100 DIFFERENT WELLS")
// already includes this week's Nights instead of jumping at the week's end.
// A run's numbers fold into the career for good in endRun, which clears
// `active` first, so nothing is ever counted twice.
inline const Run* runNow = nullptr;
inline uint32_t wellsNow = 0;

inline uint32_t careerStat(const Career& c, Cat::Stat s) {
  const uint32_t have = c.stats[(int)s];
  if (!runNow || !runNow->active || s == Cat::Stat::None) return have;
  const uint32_t live = runNow->stats[(int)s];
  if (s == Cat::Stat::unique_wells) return have > wellsNow ? have : wellsNow;
  if (Cat::StatIsBest[(int)s]) return have > live ? have : live;
  return have + live;
}

// Has the career met an unlock condition? `arg` names a relic (win_with) or
// a school (school_win).
inline bool met(const Career& c, Cat::Stat s, int32_t need, int arg) {
  using S = Cat::Stat;
  switch (s) {
    case S::None: return true;
    case S::win_with: return arg >= 0 && getBit(c.wonWith, arg);
    case S::school_win: return arg >= 0 && ((c.schoolWins >> arg) & 1);
    case S::callings_won: return bits(c.callingsWon) >= need;
    case S::pact_win: return c.bestPact >= need - 1;
    case S::channels_seen: return bits(c.channels) >= need;
    case S::wins: return c.wins >= (uint32_t)need;
    case S::eclipses_survived: return bits(c.eclipsesSurvived) >= need;
    default: return careerStat(c, s) >= (uint32_t)need;
  }
}

// Where the career stands on an unlock's condition, in the same units as `need`.
inline uint32_t goalNow(const Career& c, Cat::Stat s) {
  using S = Cat::Stat;
  switch (s) {
    case S::callings_won: return bits(c.callingsWon);
    case S::channels_seen: return bits(c.channels | (runNow && runNow->active ? runNow->channels : 0));
    case S::wins: return c.wins;
    case S::eclipses_survived: return bits(c.eclipsesSurvived);
    default: return careerStat(c, s);
  }
}

// Unlocks nobody wrote down: no hint, no progress, until you stumble on them.
inline bool secretGoal(Cat::Stat s) {
  using S = Cat::Stat;
  return s == S::low_battery_close || s == S::broke_shop || s == S::same_well_nights;
}

// Goals that measure how well you play rather than how often you press a key.
// Only these show progress ("12/25"); grindable ones (dowse 30 times) don't.
inline bool realGoal(Cat::Stat s) {
  using S = Cat::Stat;
  switch (s) {
    case S::max_networks: case S::big_sale_nexus: case S::hollows_one_run: case S::ward_changes_night:
    case S::new_wells_run: case S::new_wells_night: case S::unique_wells: case S::attune_level:
    case S::kinds_night: case S::channels_seen: case S::fast_closes: case S::nights_paid:
    case S::max_coin: case S::big_units: case S::wins: case S::op_nights: case S::rares_bought:
    case S::rares_one_run: case S::callings_won: case S::standing_hedge: case S::standing_hearth:
    case S::standing_warded: case S::eclipses_survived: case S::overnight_best: case S::editions_owned:
      return true;
    default: return false;
  }
}

// What opens a locked thing, in capitals for the screen: its condition, with
// progress for real goals, or a shrug for a secret.
inline const char* goalText(const Career& c, Cat::Stat s, int32_t need, const char* unlockText, char* b, int cap) {
  if (secretGoal(s)) { snprintf(b, cap, "NOBODY WROTE THIS ONE DOWN."); return b; }
  int k = 0;
  for (const char* q = unlockText; *q && k < cap - 1; q++) b[k++] = (char)toupper((unsigned char)*q);
  b[k] = 0;
  if (realGoal(s) && need > 1) {
    uint32_t now = goalNow(c, s);
    if (now > (uint32_t)need) now = (uint32_t)need;
    char x[16], y[16];
    auto commas = [](uint32_t v, char* o) {
      char t[16];
      const int n = snprintf(t, sizeof(t), "%u", (unsigned)v);
      int j = 0;
      for (int i = 0; i < n; i++) { if (i && (n - i) % 3 == 0) o[j++] = ','; o[j++] = t[i]; }
      o[j] = 0;
    };
    commas(now, x);
    commas((uint32_t)need, y);
    snprintf(b + k, cap - k, " (%s/%s)", x, y);
  }
  return b;
}

// Fold a finished run's numbers into the career.
inline void mergeRun(Career& c, const Run& r) {
  for (int i = 1; i < (int)Cat::Stat::Count; i++) {
    if (Cat::StatIsBest[i]) {
      if (r.stats[i] > c.stats[i]) c.stats[i] = r.stats[i];
    } else {
      c.stats[i] += r.stats[i];
    }
  }
  c.channels |= r.channels;
  c.runs++;
}

// Look for newly earned unlocks and queue them, in catalog order.
inline void queueEarned(Career& c) {
  auto queued = [&](Kind k, int id) {
    for (int i = 0; i < c.nQueue; i++) if (c.queue[i].kind == (uint8_t)k && c.queue[i].id == id) return true;
    return false;
  };
  auto consider = [&](Kind k, int id, Cat::Stat s, int32_t need, int arg) {
    if (s == Cat::Stat::None || unlocked(c, k, id) || queued(k, id)) return;
    if (c.nQueue >= sizeof(c.queue) / sizeof(c.queue[0])) return;
    if (met(c, s, need, arg)) c.queue[c.nQueue++] = {(uint8_t)k, (uint8_t)id};
  };
  for (int i = 0; i < Cat::RelicCount; i++)
    consider(Kind::Relic, i, Cat::Relics[i].unlock, Cat::Relics[i].need, Cat::Relics[i].arg);
  for (int i = 0; i < Cat::TrinketCount; i++)
    consider(Kind::Trinket, i, Cat::Trinkets[i].unlock, Cat::Trinkets[i].need, -1);
  for (int i = 0; i < Cat::SpellCount; i++)
    consider(Kind::Spell, i, Cat::Spells[i].unlock, Cat::Spells[i].need, -1);
  for (int i = 0; i < Cat::CallingCount; i++)
    consider(Kind::Calling, i, Cat::Callings[i].unlock, Cat::Callings[i].need, -1);
}

// The end of a Binding: reveal up to UnlocksPerRun queued unlocks, and open
// any Grimoire pages now earned. Returns how many things were revealed.
inline int reveal(Career& c) {
  queueEarned(c);
  c.nRevealed = 0;
  while (c.nQueue > 0 && c.nRevealed < UnlocksPerRun) {
    const auto p = c.queue[0];
    memmove(c.queue, c.queue + 1, (c.nQueue - 1) * sizeof(c.queue[0]));
    c.nQueue--;
    unlock(c, (Kind)p.kind, p.id);
    c.revealed[c.nRevealed++] = p;
  }
  for (int i = 0; i < Cat::PageCount; i++) {
    if ((c.pages >> i) & 1) continue;
    if (met(c, Cat::Pages[i].unlock, Cat::Pages[i].need, -1)) c.pages |= (uint16_t)(1u << i);
  }
  return c.nRevealed;
}

// Rank: a title from Tithes paid over the whole career.
constexpr uint32_t RankAt[] = {0, 3, 10, 25, 50, 100, 200};
inline const char* const RankNames[] = {"LISTENER", "DOWSER", "WELL-HAND", "WELLWRIGHT",
                                        "TITHE-KEEPER", "LAMP", "OLD ONE"};
inline int rankIndex(const Career& c) {
  const uint32_t paid = careerStat(c, Cat::Stat::nights_paid);
  int k = 0;
  for (int i = 0; i < 7; i++) if (paid >= RankAt[i]) k = i;
  return k;
}
inline const char* rankName(const Career& c) { return RankNames[rankIndex(c)]; }

} // namespace Game
