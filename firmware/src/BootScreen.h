#pragma once
#include "UI.h"

// The Deck waking up: its sigil draws itself while the game's title and the
// boot log type out beside it. About two seconds; any key skips it.

namespace BootScreen {

constexpr float Length = 2.4f;

struct Info {
  bool card;      // SD card mounted
  bool ears;      // WiFi radio started
  bool balance;   // motion sensor (Cardputer ADV only)
  // The Deck's key was just made. Expected on a first boot; with a save to load
  // it means every well the save knows now reads as a stranger.
  bool keysNew = false;
  bool keysLost = false;
  const char* cardWord = nullptr;   // the card's state in a word (SDCard::word), else from `card`
};

// Draws the arc of a circle from angle 0 to `frac` of a full turn.
inline void arc(Canvas& g, int cx, int cy, int r, float frac, uint16_t c) {
  const int steps = (int)(frac * r * 7);
  for (int i = 0; i <= steps; i++) {
    const float a = -1.5708f + 6.2832f * i / (r * 7.0f);
    g.drawPixel(cx + (int)lroundf(r * cosf(a)), cy + (int)lroundf(r * sinf(a)), c);
  }
}

inline void line(Canvas& g, float x0, float y0, float x1, float y1, float frac, uint16_t c) {
  if (frac <= 0) return;
  if (frac > 1) frac = 1;
  g.drawLine((int)x0, (int)y0, (int)(x0 + (x1 - x0) * frac), (int)(y0 + (y1 - y0) * frac), c);
}

inline float phase(float t, float start, float len) {
  const float p = (t - start) / len;
  return p < 0 ? 0 : p > 1 ? 1 : p;
}

inline void sigil(Canvas& g, int cx, int cy, float t) {
  const uint16_t outer = UI::Accent, inner = UI::Sigil;
  arc(g, cx, cy, 44, phase(t, 0.0f, 0.7f), outer);
  arc(g, cx, cy, 40, phase(t, 0.15f, 0.7f), UI::dimmed(outer));
  // A triangle inside, one side at a time.
  float px[3], py[3];
  for (int k = 0; k < 3; k++) {
    const float a = -1.5708f + k * 2.0944f;
    px[k] = cx + 40 * cosf(a);
    py[k] = cy + 40 * sinf(a);
  }
  for (int k = 0; k < 3; k++) {
    line(g, px[k], py[k], px[(k + 1) % 3], py[(k + 1) % 3], phase(t, 0.5f + k * 0.15f, 0.25f), inner);
  }
  // Four ticks on the outer ring, like a compass.
  for (int k = 0; k < 4; k++) {
    const float a = k * 1.5708f;
    line(g, cx + 44 * cosf(a), cy + 44 * sinf(a), cx + 50 * cosf(a), cy + 50 * sinf(a),
         phase(t, 0.9f + k * 0.05f, 0.1f), outer);
  }
  // The eye in the middle opens last.
  const float e = phase(t, 1.1f, 0.3f);
  if (e > 0) {
    const int r = (int)(9 * e);
    g.drawEllipse(cx, cy + 4, 12, r ? r : 1, outer);
    g.fillCircle(cx, cy + 4, (int)(4 * e), inner);
  }
}

inline void draw(Canvas& g, float t, const Info& info) {
  g.fillSprite(UI::Bg);
  sigil(g, 56, 67, t);

  struct Line { const char* label; const char* value; bool ok; };
  const Line lines[] = {
    {"KEYS", info.keysNew ? "NEW" : "SEALED", !info.keysLost},
    {"GRIMOIRE", info.cardWord ? info.cardWord : info.card ? "SD OK" : "NO CARD", info.card},
    {"EARS", info.ears ? "OPEN" : "DEAF", info.ears},
    {"BALANCE", info.balance ? "TRUE" : "NONE", true},
  };
  const int x = 118;
  if (t > 0.1f) {   // the game's title, and under it the firmware's version
    UI::text(g, x, 14, "THE TITHE", UI::SelText, 2);
    UI::text(g, x, 32, Config::Version, UI::Dim);
  }
  for (int i = 0; i < 4; i++) {
    const float at = 0.45f + i * 0.3f;
    if (t < at) break;
    const int y = 46 + i * 12;
    UI::text(g, x, y, ">", UI::Dim);
    UI::text(g, x + 12, y, lines[i].label, UI::Text);
    // No motion sensor isn't a fault: the Deck plays without the moves.
    const uint16_t vc = !lines[i].ok ? UI::Rec : i == 3 && !info.balance ? UI::Dim : UI::Accent;
    if (t > at + 0.2f) UI::textRight(g, UI::W - 6, y, lines[i].value, vc);
  }
  if (t > 1.75f) {
    const int y = 46 + 4 * 12 + 6;
    UI::text(g, x, y, ">", UI::Dim);
    char b[24];
    const int dots = ((int)((t - 1.75f) * 6)) % 4;
    snprintf(b, sizeof(b), "LISTENING%.*s", dots, "...");
    UI::text(g, x + 12, y, b, UI::Accent);
  }
}

// The boot stops here: the save can't be loaded and mustn't be written over.
inline void drawStop(Canvas& g, const char* what, const char* todo) {
  g.fillSprite(UI::Bg);
  UI::textCenter(g, 20, "THE TITHE", UI::SelText, 2);
  UI::textCenter(g, 40, Config::Version, UI::Dim);
  UI::textCenter(g, 62, what, UI::Rec);
  UI::textCenter(g, 76, todo, UI::Text);
  UI::textCenter(g, 98, "THE SAVE IS KEPT AS IT IS", UI::Dim);
}

} // namespace BootScreen
