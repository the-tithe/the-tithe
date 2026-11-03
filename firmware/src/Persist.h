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
inline bool keyNew = false;   // no key was stored: this boot made one
inline uint32_t titheAt = 0;   // where the Tithe's own flash area was when it last mounted, 0 never (SaveStore::begin)
inline bool keyKept = true;    // the key made this boot was stored (a full settings area can refuse it)

// Read what's kept. False when there's no key yet: then makeKey() once the
// radio is on.
inline bool begin() {
  prefs.begin("deck", false);
  muted = prefs.getBool("mute", false);
  lastWalk = prefs.getUInt("walk", 0);
  titheAt = prefs.getUInt("lfsat", 0);
  return prefs.getBytes("key", Keys::device, 16) == 16;
}

// A new key. Call after the radio is on: the hardware random generator is only
// truly random while the radio runs.
inline void makeKey() {
  esp_fill_random(Keys::device, 16);
  keyKept = prefs.putBytes("key", Keys::device, 16) == 16;
  keyNew = true;
}

inline void noteTitheMounted(uint32_t at) {
  if (titheAt == at) return;
  titheAt = at;
  prefs.putUInt("lfsat", at);
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
