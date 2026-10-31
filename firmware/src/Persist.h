#pragma once
#include <Preferences.h>
#include <esp_random.h>
#include "Well.h"
#include "game/RealClock.h"

// What the Deck keeps in its own flash between power-ups: its private key
// (Well.h), the last walk number, whether sound is on, and the last note of
// nearby routers' timers (game/RealClock.h, routers hashed). The key never
// leaves the Deck, and nothing here goes on the SD card.

namespace Persist {

inline Preferences prefs;
inline bool muted = false;
inline uint32_t lastWalk = 0;

// Call after the radio is on: the hardware random generator is only truly
// random while the radio runs.
inline void begin() {
  prefs.begin("deck", false);
  if (prefs.getBytes("key", Keys::device, 16) != 16) {
    esp_fill_random(Keys::device, 16);
    prefs.putBytes("key", Keys::device, 16);
  }
  muted = prefs.getBool("mute", false);
  lastWalk = prefs.getUInt("walk", 0);
}

inline void setMuted(bool m) {
  muted = m;
  prefs.putBool("mute", m);
}

inline void setLastWalk(uint32_t w) {
  lastWalk = w;
  prefs.putUInt("walk", w);
}

inline bool clockNote(Game::ClockNote& n) {
  return prefs.getBytes("clock", &n, sizeof(n)) == sizeof(n) && n.valid();
}

inline void setClockNote(const Game::ClockNote& n) { prefs.putBytes("clock", &n, sizeof(n)); }

} // namespace Persist
