#pragma once
#include <stdint.h>
#include "WalkFormat.h"

// What the screens need to know about the Deck besides the wells.
struct Status {
  enum class Log : uint8_t { Off, On, NoCard, Failed };
  WalkFormat::Place place = WalkFormat::Place::None;
  Log log = Log::Off;
  uint32_t walk = 0;        // walk number on the card, 0 before the first
  uint32_t logBytes = 0;
  int battery = -1;         // percent, -1 when unknown
  bool listening = false;   // a scan is running
  bool muted = false;
  uint32_t nowMs = 0;
  float hushReveal = 1;     // 0..1: Hush wells only surface when you stand still
  float wardLeft = 0;       // seconds of ward left from a twist
  bool gyro = false;        // this Deck has a motion sensor
  bool awake = true;        // the screen is lit: not dimmed, not face-down
  bool deaf = false;        // the radio didn't start: nothing will ever be heard
  bool unsaved = false;     // nothing can keep the game (no flash area, no card that takes a save)
  // The run, for the scanner's header (App fills these in).
  bool inRun = false;
  int night = 0;            // 0-based
  double total = 0, tithe = 0;
  int visitsLeft = 0;
  int coin = 0;
  uint32_t starId = 0;      // the well the hint star marks, 0 none
  double notch[3] = {0, 0, 0};   // where 6, 9 and 11 spare visits land on the Tithe bar, 0..1
  int notches = 0;
  bool quiet = false;       // a high Pact: the Deck keeps its estimates to itself
  double heat = 0;          // shown as the Wardens' eye from 2 up
  bool wardens = false;     // the Wardens can come for you this week
  bool (*spent)(uint32_t id) = nullptr;   // drawn and offered tonight: nothing left to do there
  int (*glyph)(uint32_t id) = nullptr;     // the glyph carved there this week, -1 none
  bool adrift = false;      // a drifting glyph waits in the glass
  // The open well's Thirst for each energy you hold, before relics: 0 not held, -1 it won't take it.
  float wellThirst[5] = {0, 0, 0, 0, 0};
  int wellFocus = -1;        // the open well also gives your focus tonight: that energy, else -1
  bool wellHeld = false;     // the open well has faded past NEAR but is held in reach
  int wellFavor = 0;         // a favor you could take there: its reward in coin, else 0
  int wellFavorFaction = 0;
  const char* wellGlyph = nullptr;   // the open well's glyph this week, by name
  // The energy a well of each kind gives tonight (Glitch's shuffle, a Static storm), by its own energy.
  uint8_t tonight[5] = {0, 1, 2, 3, 4};
  int faction = -1;          // every well's faction tonight (Hedge in a Static storm), -1 its own
  Energy tonightOf(Energy e) const { return (Energy)tonight[(int)e < 5 ? (int)e : 0]; }
};
