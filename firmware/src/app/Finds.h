#pragma once
#include "../Strays.h"
#include "../Tune.h"
#include "AppState.h"
#include "Teach.h"

// What a walk turns up, from each scan: the naming words of the wells you
// pass, a well with the true name of one you've worked, the odd kinds of well
// in the field guide, and now and then a drifting glyph. All of it kept in the
// walks' block (game/Moments.h) and shown on the Grimoire's FOUND tab.
//
// Only wells that pass the fixed-well filter count. What a well is gets
// decided once, the first time it comes into reach on a walk, and nothing
// about it points anywhere. Sounds play at once; words wait until you stop
// and hold the Deck up to look (showHeld), so nothing asks you to read while
// you walk.

namespace App {

inline bool momentsDirty = false;           // the walks' block changed since it was saved
inline uint32_t momentsSavedAt = 0;         // play ms

// ---- Words that wait until you stop ------------------------------------------------

constexpr int HeldMax = 6;          // a dense first walk can turn up four kinds in minutes
constexpr float HeldStill = 0.8f;   // seconds held still before one shows
inline char heldText[HeldMax][40];
inline uint16_t heldColor[HeldMax];
inline bool heldFollows[HeldMax];   // shown right after the note before it, in the same stop
inline int nHeld = 0;
inline bool heldThisStop = false;   // one a stop, so each is seen

inline void dropHeld(int i) {
  nHeld--;
  memmove(heldText[i], heldText[i + 1], sizeof(heldText[0]) * (nHeld - i));
  memmove(&heldColor[i], &heldColor[i + 1], sizeof(heldColor[0]) * (nHeld - i));
  memmove(&heldFollows[i], &heldFollows[i + 1], sizeof(heldFollows[0]) * (nHeld - i));
}

// `follows`: a line that belongs with the one before it (a first-time note), so
// it comes straight after instead of waiting for the next stop.
inline void holdNote(const char* s, uint16_t c, bool follows) {
  if (nHeld == HeldMax) dropHeld(0);   // the oldest gives way
  snprintf(heldText[nHeld], sizeof(heldText[0]), "%s", s);
  heldColor[nHeld] = c;
  heldFollows[nHeld++] = follows;
}

inline void showHeld(const Ctx& c) {
  if (c.gyro && c.motion.stillFor < 0.5f) heldThisStop = false;
  if (!nHeld || toastT > 0 || nToastNext > 0 || (heldThisStop && !heldFollows[0]) || mode == Mode::Boot) return;
  if (c.gyro && !(c.motion.viewing() && c.motion.stillFor >= HeldStill)) return;
  toast(heldText[0], heldColor[0], 2.0f);
  dropHeld(0);
  heldThisStop = c.gyro;
}

// ---- The field guide ----------------------------------------------------------------

enum GuideKind : uint8_t { G_Old, G_Row, G_Namesake, G_Chorus, G_Locked, G_Crowded, G_Cold, G_LeftOn, G_Count };
inline const char* const GuideName[G_Count] = {
  "OLD WELL", "A ROW OF OLD WELLS", "NAMESAKE", "CHORUS", "LOCKED IN THE MARKET", "CROWDED CORNER", "COLD ONE", "LEFT ON",
};
// What listeners say about each kind.
inline const char* const GuideNote[G_Count] = {
  "A lamppost that never stopped talking.",
  "Still on, the whole row of them.",
  "One name, two wells. Nobody agrees.",
  "They finish each other's sentences.",
  "Locked up tight in the free stuff.",
  "Sixty voices, nobody listening.",
  "It still won't say its name.",
  "Nobody ever turned it off.",
};
constexpr int CrowdedWells = 60;
static_assert(G_Count == Game::Moments::GuideKinds, "one guide entry per kind");

inline bool guideMet(int k) { return (game.moments.guideGot >> k) & 1; }

// The first of a kind: kept with its true name. True if it's new.
inline bool meetGuide(int k, const WellRec& w) {
  Game::Moments& m = game.moments;
  if (guideMet(k)) return false;
  m.guideGot |= (uint8_t)(1u << k);
  m.guide[k] = Game::packName(w.seed, (int)w.faction);
  char b[40];
  snprintf(b, sizeof(b), "FOUND: %s", GuideName[k]);
  holdNote(b, UI::Accent);
  teachHeld(Taught::Find, "YOUR FINDS: SEE GRIMOIRE > FOUND");
  momentsDirty = true;
  return true;
}

// Wells in reach that pass the filter and share this one's network name, itself
// included; `old` counts only old wells.
inline int singingWith(const WellRec& w, const WellRec* const* list, int n, const Sightings& sky, bool old) {
  if (!w.nameKey) return 0;
  int k = 0;
  for (int i = 0; i < n; i++) {
    const WellRec& o = *list[i];
    k += sky.inRange(o) && o.nameKey == w.nameKey && WellRules::fixedish(o) && (!old || WellRules::isOldWell(o));
  }
  return k;
}

// ---- The naming words ---------------------------------------------------------------
// Bit i of words: the 64 first words, then each faction's 32 second words.

constexpr int WordsFirst = 64, WordsSecond = 32, WordColumns = 5;
inline int wordBit(int column, int i) { return column == 0 ? i : WordsFirst + (column - 1) * WordsSecond + i; }
inline int wordsIn(int column) { return column == 0 ? WordsFirst : WordsSecond; }
inline bool wordHeard(int column, int i) { return Game::getBit(game.moments.words, wordBit(column, i)); }
inline const char* wordText(int column, int i) {
  return column == 0 ? WellRules::Adjectives[i] : WellRules::Nouns[column - 1][i];
}
inline int wordsHeard(int column) {
  int k = 0;
  for (int i = 0; i < wordsIn(column); i++) k += wordHeard(column, i);
  return k;
}

inline void hearWords(const WellRec& w) {
  uint8_t* b = game.moments.words;
  const int a = wordBit(0, (int)(w.seed & 63)), s = wordBit(1 + (int)w.faction, (int)((w.seed >> 6) & 31));
  if (Game::getBit(b, a) && Game::getBit(b, s)) return;
  Game::setBit(b, a);
  Game::setBit(b, s);
  momentsDirty = true;
}

// ---- Namesakes ----------------------------------------------------------------------

constexpr int NamesakeVisits = 2;     // the worked well: offered at twice or more
inline bool namesakeThisWalk = false;   // at most one a walk

// A well you've worked, elsewhere, with this one's true name.
inline bool sharesAName(const WellRec& w) {
  const uint16_t p = Game::packName(w.seed, (int)w.faction);
  for (int i = 0; i < game.atlas.count; i++) {
    const Game::AtlasEntry& a = game.atlas.e[i];
    if (a.id != w.id && a.visits >= NamesakeVisits && Game::packName(a.seed, a.faction) == p) return true;
  }
  return false;
}
inline bool namesakeKept(uint32_t id) {
  const Game::Moments& m = game.moments;
  for (int i = 0; i < m.nNamesakes; i++) if (m.namesakes[i].id == id) return true;
  return false;
}
inline uint16_t weekNow() { return (uint16_t)(inRun() ? run().runNo : game.career.runs); }

inline void keepNamesake(WellRec& w) {
  Game::Moments& m = game.moments;
  if (m.nNamesakes == Game::Moments::MaxNamesakes) {   // full: the oldest gives way
    memmove(&m.namesakes[0], &m.namesakes[1], sizeof(m.namesakes[0]) * (m.nNamesakes - 1));
    m.nNamesakes--;
  }
  m.namesakes[m.nNamesakes++] = Game::Namesake{w.id, Game::packName(w.seed, (int)w.faction), weekNow()};
  w.namesake = true;
  namesakeThisWalk = true;
  m.guideGot |= (uint8_t)(1u << G_Namesake);   // the first also fills the guide's page, quietly
  if (!m.guide[G_Namesake]) m.guide[G_Namesake] = Game::packName(w.seed, (int)w.faction);
  char b[40];
  snprintf(b, sizeof(b), "NAMESAKE: %s", w.name);
  holdNote(b, UI::Accent);
  teachHeld(Taught::Namesake, "A NAMESAKE: SAME NAME, ANOTHER WELL", "= MARKS IT ON THE SCANNER");
  momentsDirty = true;
}

// ---- Drifting glyphs ----------------------------------------------------------------

struct Adrift {
  bool on;
  uint8_t shape;
  uint16_t hand;
  uint16_t name;       // always 0: a stray says nothing of where it was
  uint16_t week;
  uint8_t night;
  uint32_t since;      // play ms when it turned up
  float drawn;         // 0..1, how much of it has drawn itself
  bool rang;           // its rhythm has played
  float scatter;       // seconds since a flick broke it up; below 0, it hasn't
};
inline Adrift adrift{};

constexpr uint32_t StrayOdds = 10;          // of the wells where one could turn up, one in this many has one
constexpr uint16_t StrayAfter = 6;          // never-heard wells since the last one
constexpr uint32_t StrayWaitMs = 300000;    // it waits at least this long...
constexpr uint16_t StrayWaitWells = 15;     // ...and until this many more new wells have passed...
constexpr uint32_t StrayGoneMs = 1200000;   // ...or twenty minutes on known ground, where no new well ever comes
constexpr float StrayDrawSecs = 2.4f, StrayScatterSecs = 1.2f;

// The well you've sold at most, 0 for none: home, near enough.
inline uint32_t homeWell() {
  int most = 0;
  uint32_t id = 0;
  for (int i = 0; i < game.atlas.count; i++)
    if (game.atlas.e[i].visits > most) { most = game.atlas.e[i].visits; id = game.atlas.e[i].id; }
  return id;
}

// Whether one turns up by this well tonight: the same answer every time you ask
// (a reboot can't roll again), different on every Deck and every Night.
inline uint32_t strayRoll(uint32_t seed, uint32_t runNo, int night) {
  return Strays::mix(seed ^ Strays::mix(runNo * 0x9E3779B1u + (uint32_t)night * 0x85EBCA77u + Keys::deckTag()));
}

// One could turn up by `w`: out walking a quiet street on a Night, by a well
// you've never worked, home not in range, none yet tonight.
inline bool strayCould(const WellRec& w, const Sightings& sky, bool homeHere) {
  const Game::Moments& m = game.moments;
  const WardKind wk = sky.ward();
  if (!inRun() || game.phase != Game::Phase::Night || adrift.on || homeHere) return false;
  if (wk != WardKind::Hollow && wk != WardKind::Fringe) return false;
  if (m.newSinceStray < StrayAfter || game.atlas.find(w.id)) return false;
  return !(m.strayWeek == run().runNo && m.strayNight == run().night);
}

inline void startStray(int shape, uint16_t hand, uint16_t name) {
  Game::Moments& m = game.moments;
  adrift = Adrift{true, (uint8_t)shape, (uint16_t)(hand & Strays::HandMask), name, (uint16_t)run().runNo,
                  (uint8_t)run().night, game.playMs, 0, false, -1};
  m.strayWeek = (uint16_t)run().runNo;
  m.strayNight = (uint8_t)run().night;
  m.newSinceStray = 0;
  teachHeld(Taught::Glyph, "A GLYPH DRIFTS BY. PRESS G, HOLD STILL", "ONCE IT SETTLES, ENTER KEEPS IT");
  momentsDirty = true;
}

// Each frame: it draws itself while the glass is still, breaks up on a flick,
// and, away from the glass, waits a good while before it drifts off. True
// when it has a sound to play (`out`).
inline bool tickStray(const Ctx& c, float dt, Tune& out) {
  Adrift& a = adrift;
  if (!a.on) return false;
  if (a.scatter >= 0) {
    a.scatter += dt;
    if (a.scatter > StrayScatterSecs) a.on = false;
    return false;
  }
  if (mode == Mode::Glass) {
    if (c.gyro && c.motion.flick) {
      a.scatter = 0;
      out = Tunes::glyph(a.shape);
      return true;
    }
    if (c.st.hushReveal >= 1 && a.drawn < 1) {
      a.drawn += dt / StrayDrawSecs;
      if (a.drawn >= 1) {
        a.drawn = 1;
        a.rang = true;
        out = Tunes::glyph(a.shape);
        return true;
      }
    }
    return false;
  }
  const uint32_t waited = game.playMs - a.since;
  if ((waited >= StrayWaitMs && game.moments.newSinceStray >= StrayWaitWells) || waited >= StrayGoneMs) a.on = false;
  return false;
}

inline bool straySettled() { return adrift.on && adrift.scatter < 0 && adrift.drawn >= 1; }

// ENTER on a settled one: into the FOUND tab, exactly as drawn.
inline void keepStray() {
  Game::Moments& m = game.moments;
  if (m.nMarks == Game::Moments::MaxMarks) {   // full: the oldest gives way
    memmove(&m.marks[0], &m.marks[1], sizeof(m.marks[0]) * (m.nMarks - 1));
    m.nMarks--;
  }
  m.marks[m.nMarks++] = Game::Mark{adrift.hand, adrift.name, adrift.week, adrift.night, adrift.shape};
  if (m.marksKept < 0xFFFF) m.marksKept++;
  adrift.on = false;
  toast("KEPT", Strays::Chalk);
  save(true);
}

// ---- Each scan ----------------------------------------------------------------------

// What this scan turned up. True with a sound to play (`chime`), the rarest
// find's: a glyph, then a namesake, then a new kind for the guide.
inline bool noticeFinds(const Ctx& c, const WellRec* const* list, int n, Tune& chime) {
  Sightings& sky = c.sky;
  const uint32_t scan = sky.scans();
  Game::Moments& m = game.moments;
  if (scan == 1) namesakeThisWalk = false;   // the Deck just woke: a new walk
  const uint32_t home = homeWell();
  bool homeHere = false;
  int fixedHere = 0;
  const WellRec* loudest = nullptr;
  for (int i = 0; i < n; i++) {
    const WellRec& w = *list[i];
    if (w.lastScan != scan || !WellRules::fixedish(w)) continue;
    homeHere |= home && w.id == home;
    fixedHere++;
    if (!loudest && w.sawVisible) loudest = &w;
    if (w.firstScan == scan && w.neverHeard && m.newSinceStray < 0xFFFF) { m.newSinceStray++; momentsDirty = true; }
  }

  bool stray = false, namesake = false, guide = false;
  int namesakeEnergy = 0;
  const bool quiet = sky.ward() == WardKind::Hollow || sky.ward() == WardKind::Fringe;
  for (int i = 0; i < n; i++) {
    WellRec& w = *sky.edit(list[i]->id);
    if (w.lastScan != scan || !WellRules::fixedish(w)) continue;
    const bool named = w.energy != Energy::Hush || c.st.hushReveal >= 1;
    if (named) hearWords(w);
    // Coming into reach for the first time this walk: decided now, once.
    if (!w.judged && w.sig >= Game::ReachDbm) {
      w.judged = true;
      if (!namesakeThisWalk && w.neverHeard && w.energy != Energy::Hush && !namesakeKept(w.id) && sharesAName(w)) {
        keepNamesake(w);
        namesake = true;
        namesakeEnergy = (int)w.energy;
      }
      if (!stray && strayCould(w, sky, homeHere)) {
        const uint32_t h = strayRoll(w.seed, run().runNo, run().night);
        if (h % StrayOdds == 0) {
          startStray((int)((h >> 8) % Strays::Shapes), (uint16_t)(h >> 12), 0);
          stray = true;
        }
      }
    }
    // The odd kinds, each once for good.
    if (m.guideGot == 0xFF) continue;
    if (WellRules::isOldWell(w)) {
      guide |= meetGuide(G_Old, w);
      if (!guideMet(G_Row) && singingWith(w, list, n, sky, true) >= 3) guide |= meetGuide(G_Row, w);
    }
    if (!guideMet(G_Chorus) && singingWith(w, list, n, sky, false) >= 3) guide |= meetGuide(G_Chorus, w);
    if (w.faction == Faction::Warded && sky.near() >= 11 && sky.energyNear(Energy::Static) * 2 >= sky.near())
      guide |= meetGuide(G_Locked, w);
    if (w.energy == Energy::Hush && c.st.hushReveal >= 1 && quiet) guide |= meetGuide(G_Cold, w);
    if (w.faction == Faction::Hedge && w.maxGuard == 0 && quiet) guide |= meetGuide(G_LeftOn, w);
  }
  if (fixedHere >= CrowdedWells && loudest) guide |= meetGuide(G_Crowded, *loudest);

  if (stray || namesake || guide) save();
  if (stray) chime = Tunes::adrift();
  else if (namesake) chime = Tunes::namesake(namesakeEnergy);
  else if (guide) chime = Tunes::guide();
  return stray || namesake || guide;
}

} // namespace App
