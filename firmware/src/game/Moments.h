#pragma once
#include <stdint.h>
#include <string.h>
#include "Run.h"

// What the Deck keeps from your walks: every well it has ever heard, the
// wells that travelled with you, and the walk's collections (marks, names
// shared by two wells, the field guide, the naming words, walk ribbons, dawn
// tokens, streets you write in). Saved at the end of the game block (Save.h
// version 7). Only device-keyed IDs and the game's own names, never a real
// network's name or hardware ID, and nothing here ever leaves the Deck but
// in its own save.

namespace Game {

// Every well this Deck has ever heard: a Bloom filter over the device-keyed
// IDs. "Never heard" is always right. "Heard" is wrong for about 3 wells in
// 100 by 3,500 wells known (7 in 100 by 6,000), which only makes a new well
// pass as known now and then. It can't be read back into IDs.
struct KnownWells {
  static constexpr uint32_t Bits = 32768;
  static constexpr int K = 3;
  uint8_t bits[Bits / 8];

  static uint32_t slot(uint32_t id, int k) {
    uint32_t x = id + (uint32_t)k * 0x9E3779B9u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x & (Bits - 1);
  }
  bool has(uint32_t id) const {
    for (int k = 0; k < K; k++) {
      const uint32_t b = slot(id, k);
      if (!((bits[b >> 3] >> (b & 7)) & 1)) return false;
    }
    return true;
  }
  // Remember `id`. True if it was never heard before.
  bool add(uint32_t id) {
    const bool was = has(id);
    for (int k = 0; k < K; k++) {
      const uint32_t b = slot(id, k);
      bits[b >> 3] |= (uint8_t)(1u << (b & 7));
    }
    return !was;
  }
};

// A well's true name in 16 bits: its adjective, its noun and its faction, as
// Well.h's nameOf builds it. 0 is no name.
inline uint16_t packName(uint32_t seed, int faction) {
  return (uint16_t)(0x8000u | (seed & 63u) | (((seed >> 6) & 31u) << 6) | ((uint32_t)(faction & 3) << 11));
}
inline uint32_t nameSeed(uint16_t p) { return (p & 63u) | (((p >> 6) & 31u) << 6); }
inline int nameFaction(uint16_t p) { return (p >> 11) & 3; }

// A drifting glyph you kept.
struct Mark {
  uint16_t hand;    // how this one was drawn: the wobble of its strokes
  uint16_t name;    // always 0: a stray says nothing of where it was (kept for the save's layout)
  uint16_t week;    // Run::runNo
  uint8_t night;    // 0-based
  uint8_t shape;    // which of the six glyph shapes
};

// A well with the true name of one you've worked, decided once, at first sight.
struct Namesake {
  uint32_t id;      // the new well (device-keyed)
  uint16_t name;    // packName
  uint16_t week;
};

// One walk, drawn a sliver at a time. Each sliver covers 15 s << scale, and
// the scale doubles whenever the ribbon fills, so every walk fills it: a long
// walk folds into the same width as a short one and earns nothing extra.
struct Ribbon {
  static constexpr int Slivers = 60;
  static constexpr uint32_t SliverMs = 15000;
  static constexpr uint8_t Offer = 0x40, Hush = 0x80;   // bits of a sliver
  static constexpr uint8_t MarkFive = 1;                // bit of marks
  uint8_t s[Slivers];   // bits 0-1 the ward, 2-5 new wells, 6 an offering, 7 a Hush well came up
  uint8_t len;          // slivers so far
  uint8_t scale;
  uint8_t marks;        // bit per mark at the ribbon's end (all five energies)
  uint8_t night;        // the Night it was walked on, 1-based (0: not in a week, or an older save)

  static uint8_t sliver(int ward, int fresh, bool offer, bool hush) {
    return (uint8_t)((ward & 3) | ((fresh > 15 ? 15 : fresh) << 2) | (offer ? Offer : 0) | (hush ? Hush : 0));
  }
  static int ward(uint8_t v) { return v & 3; }
  static int fresh(uint8_t v) { return (v >> 2) & 15; }

  uint32_t sliverMs() const { return SliverMs << scale; }

  // Two slivers as one: the first one's ward, their new wells together, and
  // either one's offering or Hush well.
  static uint8_t merge(uint8_t a, uint8_t b) {
    return (uint8_t)(sliver(ward(a), fresh(a) + fresh(b), false, false) | ((a | b) & (Offer | Hush)));
  }

  // Add the next sliver. A full ribbon first folds in half, each pair of
  // slivers becoming one that covers twice as long.
  void push(uint8_t v) {
    if (len >= Slivers) {
      for (int i = 0; i < Slivers / 2; i++) s[i] = merge(s[2 * i], s[2 * i + 1]);
      len = Slivers / 2;
      if (scale < 16) scale++;
    }
    s[len++] = v;
  }

  // The sliver drawn at pixel x of a ribbon `width` wide: every walk fills it.
  int at(int x, int width) const { return len ? x * len / width : -1; }
};

// A street you wrote in under the others, in a hand made from how you typed.
struct OwnStreet {
  char who[2];          // your initials
  char street[12];      // what you called it (no 0 at the end when it's full)
  uint16_t week;        // shown as WK n
  uint16_t color;       // the hand: its colour, letter spacing and wobble
  uint8_t advance;
  int8_t wobble;
  uint8_t crossed;      // you crossed it out
  uint8_t pad;
};

// Wells first heard together on a walk, to notice when you walk the same
// stretch again on another Night.
struct Stretch {
  static constexpr int Max = 8;
  uint16_t ids[Max];    // the low half of each well's device-keyed ID, 0 for none
  uint16_t nights[3];   // the distinct Nights it was walked: runNo * 16 + Night
  uint8_t nNights;
  uint8_t asked;        // the Deck has offered to add it to your streets

  static uint16_t low(uint32_t id) { return (uint16_t)id ? (uint16_t)id : 1; }
  static uint16_t nightCode(uint32_t runNo, int night) { return (uint16_t)(runNo * 16u + ((unsigned)night & 15u)); }
  bool used() const { return ids[0] != 0; }
  bool has(uint32_t id) const {
    const uint16_t k = low(id);
    for (int i = 0; i < Max && ids[i]; i++) if (ids[i] == k) return true;
    return false;
  }
  bool walkedOn(uint16_t code) const {
    for (int i = 0; i < nNights; i++) if (nights[i] == code) return true;
    return false;
  }
  // Walked on another Night. True if that's new.
  bool walk(uint16_t code) {
    if (nNights >= 3 || walkedOn(code)) return false;
    nights[nNights++] = code;
    return true;
  }
};

// ---- Dawn tokens: a picture of where you walked, never more coin ----------------

enum TokenKind : uint8_t {
  T_Subway, T_Arcade, T_Laundromat, T_CarWash, T_Parking, T_Bus, T_Ferry, T_Stranger, TokenCount,
};
constexpr uint32_t StrangerOdds = 40;   // about one dawn in this many brings one nobody recognises

// The kind from tonight's offerings (Moments' tally): the faction and the kind
// of ward you offered at most, and `roll` (the same for a Night) to vary it.
// Every kind turns up on ordinary streets, not only the quiet ones. Very
// rarely, whatever the Night was like, one nobody recognises.
inline int dawnToken(const uint8_t byFaction[4], const uint8_t byWard[4], const uint8_t byEnergy[5], uint32_t roll) {
  if (roll % StrangerOdds == 0) return T_Stranger;
  int f = 0, w = 0, total = 0;
  for (int i = 1; i < 4; i++) {
    if (byFaction[i] > byFaction[f]) f = i;
    if (byWard[i] > byWard[w]) w = i;
  }
  for (int i = 0; i < 5; i++) total += byEnergy[i];
  if (total == 0) return T_Subway;
  static const uint8_t kinds[4][4][2] = {
    // Hollow, Fringe, Quarter, Nexus
    {{T_CarWash, T_Ferry}, {T_Arcade, T_CarWash}, {T_Arcade, T_Subway}, {T_Arcade, T_Subway}},              // Hedge
    {{T_Laundromat, T_Ferry}, {T_Laundromat, T_Laundromat}, {T_Laundromat, T_Bus}, {T_Subway, T_Laundromat}},  // Hearth
    {{T_Ferry, T_Parking}, {T_Bus, T_Ferry}, {T_Bus, T_Subway}, {T_Bus, T_Parking}},                          // Warded
    {{T_Parking, T_CarWash}, {T_Parking, T_Bus}, {T_Parking, T_Bus}, {T_Parking, T_Subway}},                  // Syndicate
  };
  return kinds[f][w][(roll >> 2) & 1];
}

// ---- Your own hand, from how you typed --------------------------------------------

// Its colour, letter spacing and wobble from the gaps between your key
// presses (ms): quick typing writes close, uneven typing wobbles.
inline void handFrom(const uint16_t* gaps, int n, uint16_t& color, uint8_t& advance, int8_t& wobble) {
  static const uint16_t inks[6] = {0x9694, 0xC51C, 0xAE4D, 0xF4B1, 0xD4AB, 0xA55E};   // sage, lilac, moss, rose, copper, periwinkle
  uint32_t mean = 260, dev = 0;
  if (n > 0) {
    uint32_t sum = 0;
    for (int i = 0; i < n; i++) sum += gaps[i];
    mean = sum / (uint32_t)n;
    for (int i = 0; i < n; i++) dev += gaps[i] > mean ? gaps[i] - mean : mean - gaps[i];
    dev /= (uint32_t)n;
  }
  advance = mean < 180 ? 5 : mean < 420 ? 6 : 7;
  wobble = dev * 3 < mean ? 0 : (n & 1) ? 1 : -1;
  color = inks[(mean / 40 + dev / 25) % 6];
}

struct Moments {
  KnownWells known;

  // Wells that travelled with you on a walk, so they're never a find on the next.
  static constexpr int MaxTravellers = 16;
  uint32_t travellers[MaxTravellers];
  uint8_t nTravellers, nextTraveller;

  // Drifting glyphs: the ones kept, and when the last one turned up.
  uint8_t nMarks;
  uint8_t strayNight;       // the Night of the last one, in week strayWeek
  uint16_t strayWeek;
  uint16_t newSinceStray;   // never-heard wells since then
  uint16_t marksKept;       // all ever kept, including any let go to make room
  static constexpr int MaxMarks = 24;
  Mark marks[MaxMarks];

  // Namesakes.
  static constexpr int MaxNamesakes = 32;
  Namesake namesakes[MaxNamesakes];
  uint8_t nNamesakes;

  // The field guide: a bit per kind of odd well met, and the first one's true name.
  uint8_t guideGot;
  static constexpr int GuideKinds = 8;
  uint16_t guide[GuideKinds];

  // The naming words heard: 64 first words, then 32 second words per faction.
  uint8_t words[24];

  // The last walks' ribbons, the newest at nextRibbon - 1.
  static constexpr int MaxRibbons = 7;
  Ribbon ribbons[MaxRibbons];
  uint8_t nRibbons, nextRibbon;

  // Dawn tokens: the one shown at the latest dawn (kind + 1, 0 none), and every one kept, by kind.
  uint8_t lastToken;
  uint8_t pad0;
  static constexpr int TokenKinds = 10;
  uint16_t tokens[TokenKinds];
  // Tonight's offerings, by faction, ward and energy, for the dawn's token.
  uint16_t tallyWeek;
  uint8_t tallyNight;
  uint8_t pad1;
  uint8_t byFaction[4], byWard[4], byEnergy[5];
  uint8_t pad2[3];

  // Streets you've written in, and stretches you might keep.
  static constexpr int MaxStreets = 4;
  OwnStreet streets[MaxStreets];
  uint8_t nStreets;
  uint8_t pad3[3];
  static constexpr int MaxStretches = 16;
  Stretch stretches[MaxStretches];

  uint16_t keepName;   // a stretch you keep, waiting to be written in: its loudest well's true name (packName)
  uint16_t taught;     // a bit per first-time note already shown (App::Taught), so each says its line once
  PortentChoice portent;   // the dawn's Portents still to choose between (was spare bytes, always zero before)
  uint8_t spare[23];   // zero: room for a later field without moving the rest

  // A well that travelled with you on an earlier walk.
  bool travelled(uint32_t id) const {
    for (int i = 0; i < nTravellers; i++) if (travellers[i] == id) return true;
    return false;
  }
  // Remember one, the oldest forgotten when full.
  void noteTraveller(uint32_t id) {
    if (travelled(id)) return;
    travellers[nextTraveller] = id;
    nextTraveller = (uint8_t)((nextTraveller + 1) % MaxTravellers);
    if (nTravellers < MaxTravellers) nTravellers++;
  }

  // Every count and index is in range.
  bool valid() const {
    if (nTravellers > MaxTravellers || nextTraveller >= MaxTravellers || nMarks > MaxMarks ||
        nNamesakes > MaxNamesakes || nRibbons > MaxRibbons || nextRibbon >= MaxRibbons || lastToken > TokenKinds ||
        nStreets > MaxStreets)
      return false;
    for (int i = 0; i < MaxRibbons; i++) if (ribbons[i].len > Ribbon::Slivers) return false;
    for (int i = 0; i < MaxStretches; i++) if (stretches[i].nNights > 3) return false;
    for (int i = 0; i < MaxMarks; i++) if (marks[i].shape >= 6) return false;
    if (portent.pending >= 8) return false;
    for (int i = 0; i < 3; i++) if (portent.alt[i] > Cat::PortentCount) return false;
    return true;
  }
};
static_assert(sizeof(Moments) == 5664, "the walks' block is part of the save: its layout can't move");

} // namespace Game
