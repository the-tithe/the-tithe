#pragma once
#include "../Tune.h"
#include "AppState.h"
#include "Finds.h"
#include "Walks.h"

// What the Deck does with each scan as you walk: it remembers every well it
// has ever heard, sets aside wells that travel with you, notices what the walk
// turns up (Finds.h), and plays the tune of each well that comes into reach.
// Sound only: words wait until you stop, so it's safe while you're walking.
//
// The walk's sounds belong to the walk: they only play on the scanner's
// screens (the list, the glass, a well's page, the walk log), never over the
// draw, the show, the dawn or the stall, which have sounds of their own. A
// find's chime is rarer and plays on any quiet screen too.

namespace App {

inline Sequencer tunes;
inline uint32_t soundMs = 1;   // the sequencer's clock, moved on by update()

// A tune cued to play once the cursor has rested on it (the atlas), so
// scrolling through a list doesn't stutter one clipped note per row.
constexpr uint32_t CueRestMs = 400;
inline Tune cuedTune{};
inline uint32_t cuedAt = 0;
inline bool cued = false;

inline void playTune(const Tune& t) { tunes.play(t, soundMs); }
inline void cueTune(const Tune& t) { cuedTune = t; cuedAt = soundMs; cued = true; }
inline void tickTunes(float dt) {
  soundMs += (uint32_t)(dt * 1000);
  if (cued && soundMs - cuedAt >= CueRestMs) { cued = false; playTune(cuedTune); }
  tunes.tick(soundMs, [](int hz, int ms) { beep(hz, ms); });
}

// A well you keep: offered at three times or more. Its tune comes home.
inline bool keptWell(uint32_t id) {
  const Game::AtlasEntry* a = game.atlas.find(id);
  return a && a->visits >= 3;
}
// A well you've worked: offered at, ever.
inline bool workedWell(uint32_t id) {
  const Game::AtlasEntry* a = game.atlas.find(id);
  return a && a->visits >= 1;
}

inline void playWellTune(uint32_t seed, int energy, uint32_t id) { playTune(Tunes::of(seed, energy, keptWell(id))); }
inline void cueWellTune(uint32_t seed, int energy, uint32_t id) { cueTune(Tunes::of(seed, energy, keptWell(id))); }

// The walk's sounds, at most a tune every 8 seconds and a chirp every 15.
constexpr uint32_t TuneGapMs = 8000, ChirpGapMs = 15000;
constexpr uint32_t FiveWindowMs = 300000;   // all five energies in reach within five minutes
constexpr int TunesFree = 10;               // a walk's first tunes: every well sings; after that only the ones worth it
inline uint32_t tunedAt = 0, chirpedAt = 0;
inline uint32_t fiveAt[Game::EnergyN] = {0};
inline bool fivePlayed = false;             // once a walk
inline int tunesThisWalk = 0;
inline uint32_t sangId = 0, sangAt = 0;     // the well that sang last, and when (soundMs), for a mark on the scanner

inline bool soundDue(uint32_t at, uint32_t gap) { return at == 0 || soundMs - at >= gap; }

// Where the walk's sounds belong.
inline bool listeningScreen() { return mode == Mode::Scanner || mode == Mode::Glass || mode == Mode::Well || mode == Mode::Walk; }
// Past the walk's free tunes, a well sings only if it's never been heard by
// this Deck, or you've worked it.
inline bool worthATune(const WellRec& w) {
  return tunesThisWalk < TunesFree || w.neverHeard || workedWell(w.id);
}

// Every energy's well in reach within the window: their chord, once a walk.
inline bool fiveInReach() {
  for (int e = 0; e < Game::EnergyN; e++)
    if (fiveAt[e] == 0 || soundMs - fiveAt[e] > FiveWindowMs) return false;
  return true;
}

// After each full scan.
inline void afterScan(const Ctx& c) {
  Sightings& sky = c.sky;
  const uint32_t scan = sky.scans();
  Game::Moments& m = game.moments;
  static const WellRec* list[Sightings::MaxWells];
  const int n = sky.listed(list, Sightings::MaxWells);
  const bool listening = listeningScreen();
  if (scan == 1) tunesThisWalk = 0;
  const WellRec* tune = nullptr;    // the strongest well come into reach, still to play
  const WellRec* fresh = nullptr;   // the strongest well first heard this scan
  for (int i = 0; i < n; i++) {
    WellRec& w = *sky.edit(list[i]->id);
    if (w.lastScan != scan) continue;
    if (w.firstScan == scan) {
      if (m.travelled(w.id)) w.travelling = true;   // it walked with you before
      if (m.known.add(w.id)) { w.neverHeard = true; momentsDirty = true; }
    }
    if (w.travelling && !m.travelled(w.id)) { m.noteTraveller(w.id); momentsDirty = true; }
    if (!WellRules::fixedish(w)) continue;
    if (w.firstScan == scan && scan > 1 && !fresh) fresh = &w;
    if (w.sig < Game::ReachDbm || w.tuned) continue;
    // Already in reach when the Deck woke, or while you were busy elsewhere: not passing by.
    if (scan <= 1 || !listening) { w.tuned = true; continue; }
    if (w.energy == Energy::Hush && c.gyro && c.motion.stillFor < 1) continue;   // a moving body drowns them
    fiveAt[(int)w.energy] = soundMs;
    if (!tune && worthATune(w)) tune = &w;
  }
  Tune chime{}, keep{};
  const bool found = noticeFinds(c, list, n, chime);
  const bool asked = walksAfterScan(c, list, n, keep);
  if (found && !ceremonyScreen()) {   // a find's sound comes first; the tune waits its turn
    playTune(chime);
    tunedAt = chirpedAt = soundMs;
  } else if (!listening) {
    // The words still wait for a stop; only the walk's sounds are skipped here.
  } else if (asked) {   // a street you keep: its loudest well's tune, come home
    playTune(keep);
    tunedAt = chirpedAt = soundMs;
  } else if (!fivePlayed && fiveInReach()) {
    fivePlayed = ribbonNow.five = true;
    playTune(Tunes::five());
    tunedAt = chirpedAt = soundMs;
  } else if (tune && soundDue(tunedAt, TuneGapMs)) {
    sky.edit(tune->id)->tuned = true;
    playWellTune(tune->seed, (int)tune->energy, tune->id);
    tunedAt = chirpedAt = soundMs;
    tunesThisWalk++;
    sangId = tune->id;
    sangAt = soundMs;
  } else if (fresh && fresh->neverHeard && soundDue(chirpedAt, ChirpGapMs) && soundDue(tunedAt, TuneGapMs)) {
    playTune(Tunes::chirp((int)fresh->energy));
    chirpedAt = soundMs;
  }
  // The walks' block changes with almost every scan: save it once a minute.
  if (momentsDirty && (momentsSavedAt == 0 || game.playMs - momentsSavedAt > 60000)) {
    momentsDirty = false;
    momentsSavedAt = game.playMs;
    save();
  }
}

} // namespace App
