#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Atlas.h"
#include "Career.h"
#include "Log.h"
#include "Moments.h"
#include "Rules.h"
#include "Run.h"
#include "Shop.h"

// Everything the game keeps, as one block: the career, the run in progress,
// the atlas, recent sales and an open shop. Saved after every change.
//
// The block starts with a header: a magic number, a format version, the size,
// a fingerprint of the catalog, and a checksum, so a half-written or damaged
// save is caught and the backup is used instead. A save is also refused if it
// was made with a different catalog (its relic numbers would point at the
// wrong relics), and every count and number in it is range-checked on load.
// Change the layout of anything in Saved and you must bump Version (and add an
// upgrade if old saves should still load).

namespace Game {

// Where the player is in the game, so a restart comes back to the same screen.
enum class Phase : uint8_t { NoRun, Night, NightEnd, Shop, Won, Lost };

// The HEARD log: a bit per Overheard line (Lore::Overheard) and per Collector
// line (Lore::Collector) ever caught. Added in version 6.
struct HeardLog {
  uint8_t radio[32];   // room for 256 lines
  uint8_t stall[8];    // room for 64 lines
};

template <int RC>
struct SavedT {
  CareerT<RC> career;
  RunT<RC> run;
  Atlas atlas;
  RecentSales recent;
  Shop shop;
  NightEnd lastEnd;     // the summary shown between a Night and its shop
  Phase phase;
  uint8_t pad[3];
  uint32_t playMs;      // total play time, for the hold's leak and the atlas
  Log log;              // the week's Nights and the records (added in version 2)
  uint8_t seen[(RC + 7) / 8];   // relics ever seen at a stall or held (added in version 3)
  WellBook wells;       // each well's best and total offerings (added in version 4)
  HeardLog heard;       // what you've overheard and what the Collector has said (added in version 6)
  Moments moments;      // what the Deck keeps from your walks (added in version 7)
};
using Saved = SavedT<RelicIdCap>;

namespace SaveFormat {

constexpr uint32_t Magic = 0x4B434544;   // "DECK"
constexpr uint16_t Version = 7;
// Versions 1 to 4 were made with the 72-relic catalog, whose relic arrays were
// exactly 72 long. They load through Saved72 and are upgraded (see upgrade()).
// Version 1 had no log at the end, version 2 no relics seen, version 3 no well
// book, and version 4 was the whole block but the HEARD log. Version 5 has room for
// RelicIdCap relics, version 6 adds the HEARD log, and version 7 the walks' block.
constexpr uint32_t Catalog72 = 0x05F69FABu;   // Cat::CatalogHash of the 72-relic catalog
// Adding relics changes Cat::CatalogHash but not the layout (up to RelicIdCap). Keep loading saves
// from the catalog before by accepting its hash in decode(), as Catalog72 is here.
// Renaming a Pact changes the hash and nothing else: the catalog with Salt at the second rung.
constexpr uint32_t CatalogSalt = 0xC2B8F990u;
using Saved72 = SavedT<72>;
constexpr size_t V1Size = offsetof(Saved72, log);
constexpr size_t V2Size = offsetof(Saved72, seen);
// Version 3's block ended at `seen`, rounded up to the block's alignment.
constexpr size_t V3Size = (offsetof(Saved72, seen) + sizeof(Saved72::seen) + alignof(Saved72) - 1) / alignof(Saved72) * alignof(Saved72);
// Versions 4 and 5 ended at the well book, rounded up the same way.
template <class S>
constexpr size_t beforeHeard() { return (offsetof(S, heard) + alignof(S) - 1) / alignof(S) * alignof(S); }
constexpr size_t V4Size = beforeHeard<Saved72>();
constexpr size_t V5Size = beforeHeard<Saved>();
// Version 6 ended at the HEARD log, rounded up the same way.
template <class S>
constexpr size_t beforeMoments() { return (offsetof(S, moments) + alignof(S) - 1) / alignof(S) * alignof(S); }
constexpr size_t V6Size = beforeMoments<Saved>();

struct Header {
  uint32_t magic;
  uint16_t version;
  uint16_t headerSize;
  uint32_t size;        // bytes of Saved that follow
  uint32_t catalog;     // Cat::CatalogHash when it was saved
  uint32_t crc;         // CRC-32 of those bytes
};

inline uint32_t crc32(const uint8_t* p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

constexpr size_t Bytes = sizeof(Header) + sizeof(Saved);

// The header for `s`, for writing it out in two parts without a copy.
inline Header headerFor(const Saved& s) {
  Header h{Magic, Version, (uint16_t)sizeof(Header), (uint32_t)sizeof(Saved), Cat::CatalogHash, 0};
  h.crc = crc32((const uint8_t*)&s, sizeof(Saved));
  return h;
}

// Write `s` into `out` (Bytes long).
inline void encode(const Saved& s, uint8_t* out) {
  const Header h = headerFor(s);
  memcpy(out, &h, sizeof(h));
  memcpy(out + sizeof(h), &s, sizeof(Saved));
}

enum class Load : uint8_t { Ok, Empty, BadMagic, NewerVersion, WrongSize, BadChecksum, OtherCatalog, Invalid };

inline const char* loadText(Load l) {
  switch (l) {
    case Load::Ok: return "ok";
    case Load::Empty: return "no save";
    case Load::BadMagic: return "not a Deck save";
    case Load::NewerVersion: return "made by a newer Deck";
    case Load::WrongSize: return "wrong size";
    case Load::BadChecksum: return "damaged";
    case Load::OtherCatalog: return "made with a different catalog";
    case Load::Invalid: return "holds impossible values";
  }
  return "?";
}

// Every count and catalog number in a save is in range.
inline bool valid(const Saved& s) {
  const Run& r = s.run;
  if (r.nRelics > Run::MaxRelics || r.nTrinkets > TrinketSlots || r.nSpells > 4 || r.nGlyphTokens > 4) return false;
  for (int i = 0; i < r.nRelics; i++) {
    const OwnedRelic& x = r.relics[i];
    if (x.id >= Cat::RelicCount || x.ed >= Cat::EditionCount || x.ed < -1 || x.tier > 3) return false;
  }
  for (int i = 0; i < r.nTrinkets; i++) if (r.trinkets[i] >= Cat::TrinketCount) return false;
  for (int i = 0; i < r.nSpells; i++) if (r.spells[i] >= Cat::SpellCount) return false;
  for (int i = 0; i < r.nGlyphTokens; i++) if (r.glyphTokens[i] >= Cat::GlyphCount) return false;
  if (r.calling >= Cat::CallingCount || r.pact >= Cat::PactCount || r.challenge >= Cat::ChallengeCount ||
      r.challenge < -1 || r.focus > 2 || r.lastE >= EnergyN || r.eclipse >= Cat::EclipseCount)
    return false;
  if (r.lastSpell >= Cat::SpellCount || r.n.omen >= Cat::OmenCount || r.n.eclipse >= Cat::EclipseCount) return false;
  for (int i = 0; i < Nights; i++) if (r.omens[i] > Cat::OmenCount) return false;
  if (s.atlas.count > Atlas::Max || s.recent.n > RecentSales::Max || s.recent.head >= RecentSales::Max) return false;
  for (int i = 0; i < s.atlas.count; i++) if (s.atlas.e[i].glyph >= Cat::GlyphCount || s.atlas.e[i].energy >= EnergyN) return false;
  if (s.shop.nRelics > Shop::MaxRelics || s.shop.nCons > Shop::MaxCons) return false;
  for (int i = 0; i < s.shop.nRelics; i++) if (s.shop.relics[i].id >= Cat::RelicCount) return false;
  if (s.career.nQueue > sizeof(s.career.queue) / sizeof(s.career.queue[0]) || s.career.theme >= 12) return false;
  if ((uint8_t)s.phase > (uint8_t)Phase::Lost) return false;
  if (s.wells.count > WellBook::Max) return false;
  if (!s.moments.valid()) return false;
  return true;
}

// Copy a save made with room for A relics into one with room for B. Everything
// but the relic-sized arrays is the same, so most of it copies whole.
template <int A, int B>
inline void upgrade(const SavedT<A>& o, SavedT<B>& n) {
  memset(&n, 0, sizeof(n));
  auto bits = [](uint8_t* to, size_t toN, const uint8_t* from, size_t fromN) { memcpy(to, from, fromN < toN ? fromN : toN); };
  const CareerT<A>& oc = o.career;
  CareerT<B>& nc = n.career;
  nc.runs = oc.runs;
  nc.wins = oc.wins;
  memcpy(nc.stats, oc.stats, sizeof(nc.stats));
  bits(nc.relics, sizeof(nc.relics), oc.relics, sizeof(oc.relics));
  memcpy(nc.trinkets, oc.trinkets, sizeof(nc.trinkets));
  memcpy(nc.spells, oc.spells, sizeof(nc.spells));
  nc.callings = oc.callings;
  bits(nc.wonWith, sizeof(nc.wonWith), oc.wonWith, sizeof(oc.wonWith));
  nc.schoolWins = oc.schoolWins;
  nc.callingsWon = oc.callingsWon;
  memcpy(nc.seals, oc.seals, sizeof(nc.seals));
  nc.bestPact = oc.bestPact;
  nc.eclipsesSurvived = oc.eclipsesSurvived;
  nc.pages = oc.pages;
  nc.theme = oc.theme;
  nc.challengesWon = oc.challengesWon;
  nc.channels = oc.channels;
  memcpy(nc.queue, oc.queue, sizeof(nc.queue));
  nc.nQueue = oc.nQueue;
  memcpy(nc.revealed, oc.revealed, sizeof(nc.revealed));
  nc.nRevealed = oc.nRevealed;
  // The run: the same before and after its growing-relic values.
  constexpr size_t head = offsetof(RunT<A>, state), tailA = offsetof(RunT<A>, coin), tailB = offsetof(RunT<B>, coin);
  static_assert(head == offsetof(RunT<B>, state), "the run's head moved");
  static_assert(sizeof(RunT<A>) - tailA == sizeof(RunT<B>) - tailB, "the run's tail changed");
  memcpy((uint8_t*)&n.run, (const uint8_t*)&o.run, head);
  for (int i = 0; i < A && i < B; i++) n.run.state[i] = o.run.state[i];
  memcpy((uint8_t*)&n.run + tailB, (const uint8_t*)&o.run + tailA, sizeof(RunT<A>) - tailA);
  n.atlas = o.atlas;
  n.recent = o.recent;
  n.shop = o.shop;
  n.lastEnd = o.lastEnd;
  n.phase = o.phase;
  n.playMs = o.playMs;
  n.log = o.log;
  bits(n.seen, sizeof(n.seen), o.seen, sizeof(o.seen));
  n.wells = o.wells;
  n.heard = o.heard;
  n.moments = o.moments;
}

// Read a save made by encode(). On anything but Ok, `s` is left alone.
inline Load decode(const uint8_t* in, size_t n, Saved& s) {
  if (n == 0) return Load::Empty;
  if (n < sizeof(Header)) return Load::WrongSize;
  Header h;
  memcpy(&h, in, sizeof(h));
  if (h.magic != Magic) return Load::BadMagic;
  if (h.version > Version) return Load::NewerVersion;
  const bool current = h.catalog == Cat::CatalogHash || h.catalog == CatalogSalt;   // the same layout
  if (!current && h.catalog != Catalog72) return Load::OtherCatalog;
  // Older versions load with the newer parts empty.
  const size_t want = current ? (h.version == Version ? sizeof(Saved) : h.version == 6 ? V6Size : h.version == 5 ? V5Size : 0)
                      : h.version == 1 ? V1Size : h.version == 2 ? V2Size : h.version == 3 ? V3Size
                      : h.version == 4 ? V4Size : 0;
  if (h.headerSize != sizeof(Header) || want == 0 || h.size != want || n < sizeof(Header) + h.size)
    return Load::WrongSize;
  if (crc32(in + sizeof(Header), h.size) != h.crc) return Load::BadChecksum;
  // Check a copy, so a bad save leaves the game alone. Borrowed from the heap
  // only while loading: it's too big for the stack, and too rarely needed to
  // keep for good.
  Saved* check = (Saved*)malloc(sizeof(Saved));
  if (!check) return Load::WrongSize;
  if (current) {
    memset(check, 0, sizeof(Saved));
    memcpy(check, in + sizeof(Header), h.size);
    if (h.version < 6) memset(&check->heard, 0, sizeof(check->heard));   // the old tail padding isn't a log
    if (h.version < 7) memset(&check->moments, 0, sizeof(check->moments));   // nor the walks' block
  } else {   // a 72-relic save: read it as one, then move it into today's layout
    Saved72* old = (Saved72*)malloc(sizeof(Saved72));
    if (!old) { free(check); return Load::WrongSize; }
    memset(old, 0, sizeof(Saved72));
    memcpy(old, in + sizeof(Header), h.size);
    if (h.version < 4) memset(&old->wells, 0, sizeof(old->wells));   // the old tail padding isn't a well book
    memset(&old->heard, 0, sizeof(old->heard));                         // nor a HEARD log
    memset(&old->moments, 0, sizeof(old->moments));                     // nor the walks' block
    upgrade(*old, *check);
    free(old);
  }
  const bool ok = valid(*check);
  if (ok) memcpy(&s, check, sizeof(Saved));
  free(check);
  return ok ? Load::Ok : Load::Invalid;
}

} // namespace SaveFormat
} // namespace Game
