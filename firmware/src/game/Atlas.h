#pragma once
#include <stdint.h>
#include <string.h>

// The atlas: every well you've visited, across all runs. It answers the
// questions relics ask (have you been here before? on how many Nights?),
// holds the glyphs you've carved, and remembers which wells you've drawn
// dry tonight. Saved with the game (Save.h). Only device-keyed IDs, never
// a real network's name or hardware ID.

namespace Game {

// Set when a full atlas forgets a well to make room (the app says so once).
inline bool atlasForgot = false;

struct AtlasEntry {
  uint32_t id;          // device-keyed router hash (Well.h)
  uint32_t seed;        // world-keyed hash, for its name and prices
  uint32_t runNo;       // the run the marks below belong to
  uint16_t nights;      // bit per Night of that run you sold here (Overnight folds into bit 15)
  int16_t drawnNight;   // the Night you last drew here, -1 never (this run)
  int16_t soldNight;    // the Night you last sold here, -1 never (this run)
  int8_t glyph;         // Cat::Glyph carved here this run, -1 none
  uint8_t energy;       // what it gave last time, for the atlas screen
  uint8_t faction;
  uint8_t ward;         // the ward it was in last time, +1 (0: not known; this byte was padding)
  uint16_t visits;      // sales here, all runs
  uint32_t lastPlay;    // play-minute of the last visit, for choosing what to forget
};
static_assert(sizeof(AtlasEntry) == 28, "the atlas is saved as-is: nothing may move");

struct Atlas {
  static constexpr int Max = 400;
  AtlasEntry e[Max];
  uint16_t count;
  uint16_t pad;
  uint32_t unique;      // wells ever visited, including any forgotten to make room

  void clear() { memset(this, 0, sizeof(*this)); }

  AtlasEntry* find(uint32_t id) {
    for (int i = 0; i < count; i++) if (e[i].id == id) return &e[i];
    return nullptr;
  }
  const AtlasEntry* find(uint32_t id) const { return const_cast<Atlas*>(this)->find(id); }

  // The well a full atlas forgets: the one visited least, longest ago. Never one
  // from this week (`runNo`) while an older one is left, so this week's glyphs
  // and "drawn tonight" marks stay.
  AtlasEntry* forgettable(uint32_t runNo) {
    AtlasEntry* a = nullptr;
    for (int pass = 0; pass < 2 && !a; pass++)
      for (int i = 0; i < count; i++) {
        AtlasEntry& b = e[i];
        if (pass == 0 && b.runNo == runNo) continue;
        if (!a || b.visits < a->visits || (b.visits == a->visits && b.lastPlay < a->lastPlay)) a = &b;
      }
    return a;
  }

  // The entry for a well, made if needed. Marks from an older run are cleared.
  AtlasEntry& touch(uint32_t id, uint32_t seed, uint32_t runNo, uint32_t playMin) {
    AtlasEntry* a = find(id);
    if (!a) {
      if (count < Max) {
        a = &e[count++];
      } else {   // full: forget the well visited least, longest ago
        a = forgettable(runNo);
        atlasForgot = true;
      }
      memset(a, 0, sizeof(*a));
      a->id = id;
      a->seed = seed;
      a->runNo = runNo;
      a->drawnNight = a->soldNight = -1;
      a->glyph = -1;
    }
    if (a->runNo != runNo) {
      a->runNo = runNo;
      a->nights = 0;
      a->drawnNight = a->soldNight = -1;
      a->glyph = -1;
    }
    a->lastPlay = playMin;
    return *a;
  }

  static int nightBit(int night) { return night < 15 ? night : 15; }
};

// What you've done at each well, across every week: its best offering, all
// the Resonance you've given it, and when you first offered there. Kept apart
// from the atlas so older saves still load; matched to wells by id.
struct WellMemory {
  uint32_t id;
  float best;           // the loudest single offering here
  float total;          // all the Resonance offered here
  uint16_t firstRun;    // the week you first offered here (Run::runNo)
  uint8_t firstNight;   // and the Night, 0-based
  uint8_t pad;
};

struct WellBook {
  static constexpr int Max = Atlas::Max;
  uint16_t count;
  uint16_t pad;
  WellMemory m[Max];

  const WellMemory* find(uint32_t id) const {
    for (int i = 0; i < count; i++) if (m[i].id == id) return &m[i];
    return nullptr;
  }
  // Note an offering of `resonance` here. When full, the least-given well is forgotten.
  void note(uint32_t id, double resonance, uint32_t runNo, int night) {
    WellMemory* w = const_cast<WellMemory*>(find(id));
    if (!w) {
      if (count < Max) w = &m[count++];
      else {
        w = &m[0];
        for (int i = 1; i < count; i++) if (m[i].total < w->total) w = &m[i];
      }
      memset(w, 0, sizeof(*w));
      w->id = id;
      w->firstRun = (uint16_t)(runNo > 65535 ? 65535 : runNo);
      w->firstNight = (uint8_t)(night < 0 ? 0 : night > 255 ? 255 : night);
    }
    if (resonance > w->best) w->best = (float)resonance;
    w->total += (float)resonance;
  }
};

// The game's well book (the app points this at the save's), so the rules can tell
// a well the atlas forgot to make room from one never offered at. Forgets on its
// own, so a well both have forgotten does count as new again.
inline const WellBook* bookNow = nullptr;
inline bool offeredBefore(uint32_t id) { return bookNow && bookNow->find(id); }

} // namespace Game
