#pragma once
#include "AppState.h"

// One short line the first time each new thing happens, saying what it is and
// which key to press, then never again. Which ones have been said is a bit per
// career in the walks' block (Moments::taught), so a new Deck starts over.
//
// Two ways to say it: a held note waits until the Deck is still and held up
// (for things that happen while you walk), a toast sits on a screen you're
// already reading.

namespace App {

enum class Taught : uint8_t { Glyph, Namesake, Find, Token, Mercy, Favor, Recap, Street, OffFor, Count };
static_assert((int)Taught::Count <= 16, "Moments::taught has sixteen bits");

inline void holdNote(const char* s, uint16_t c, bool follows = false);   // Finds.h

inline bool taught(Taught t) { return (game.moments.taught >> (int)t) & 1; }

// Marks it said. True the first time.
inline bool teachOnce(Taught t) {
  if (taught(t)) return false;
  game.moments.taught |= (uint16_t)(1u << (int)t);
  save();
  return true;
}

// One or two lines that wait until you stop, following whatever note came just before.
inline void teachHeld(Taught t, const char* a, const char* b = nullptr) {
  if (!teachOnce(t)) return;
  holdNote(a, UI::Text, true);
  if (b) holdNote(b, UI::Text, true);
}

// One or two lines on the screen you're reading, after any toast already up.
inline void teachToast(Taught t, const char* a, const char* b = nullptr) {
  if (!teachOnce(t)) return;
  toastAfter(a, UI::Text, 2.5f);
  if (b) toastAfter(b, UI::Text, 2.5f);
}

} // namespace App
