#pragma once
#include <M5GFX.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "Config.h"
#include "Well.h"

// The Deck's look, shared by every screen: colors, the energy glyphs,
// signal bars and the footer. Text is the built-in 6x8 font.

using Canvas = M5Canvas;

namespace UI {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr int W = Config::ScreenW;
constexpr int H = Config::ScreenH;
constexpr int CharW = 6;
constexpr int FooterSepY = H - 12;
constexpr int FooterY = H - 9;

constexpr uint16_t Bg = TFT_BLACK;
constexpr uint16_t Text = rgb(225, 228, 235);
constexpr uint16_t Dim = rgb(110, 116, 128);
constexpr uint16_t Faint = rgb(55, 60, 70);
// Themeable: each Grimoire page unlocks a Deck theme (setTheme below).
inline uint16_t Sep = rgb(40, 70, 80);
inline uint16_t SelBg = rgb(22, 34, 48);
inline uint16_t SelText = rgb(255, 225, 90);
inline uint16_t Accent = rgb(90, 220, 230);
inline uint16_t Sigil = rgb(190, 120, 255);
constexpr uint16_t Rec = rgb(255, 70, 70);
constexpr uint16_t Fresh = rgb(255, 225, 90);

constexpr uint16_t EnergyColor[EnergyCount] = {
  rgb(255, 140, 40),    // Ember
  rgb(60, 160, 255),    // Tide
  rgb(140, 255, 170),   // Gale
  rgb(190, 120, 255),   // Hush
  rgb(255, 230, 60),    // Static
};
constexpr uint16_t FactionColor[4] = {
  rgb(110, 220, 100),   // Hedge
  rgb(255, 180, 80),    // Hearth
  rgb(170, 200, 255),   // Warded
  rgb(255, 90, 200),    // Syndicate
};
constexpr uint16_t WardColor[WardCount] = {
  rgb(140, 150, 165),   // Hollow
  rgb(80, 200, 180),    // Fringe
  rgb(240, 220, 150),   // Quarter
  rgb(255, 80, 160),    // Nexus
};

// The Deck themes, one per Grimoire page: accent, separator, selection
// background, selection text, and the sigil's inner color.
struct Theme { const char* name; uint16_t accent, sep, selBg, selText, sigil; };
inline const Theme Themes[12] = {
  {"ASH", rgb(90, 220, 230), rgb(40, 70, 80), rgb(22, 34, 48), rgb(255, 225, 90), rgb(190, 120, 255)},
  {"EMBER", rgb(255, 150, 70), rgb(90, 45, 30), rgb(48, 24, 18), rgb(255, 220, 120), rgb(255, 90, 60)},
  {"BRASS", rgb(230, 190, 90), rgb(80, 65, 35), rgb(40, 32, 18), rgb(255, 240, 170), rgb(200, 150, 70)},
  {"MOSS", rgb(130, 220, 120), rgb(40, 75, 40), rgb(20, 38, 22), rgb(220, 255, 160), rgb(90, 180, 110)},
  {"HEARTH", rgb(255, 180, 100), rgb(85, 55, 35), rgb(44, 28, 20), rgb(255, 230, 170), rgb(240, 120, 80)},
  {"SEAL", rgb(170, 200, 255), rgb(50, 60, 90), rgb(24, 30, 50), rgb(230, 240, 255), rgb(120, 150, 255)},
  {"CHROME", rgb(210, 220, 230), rgb(70, 75, 85), rgb(34, 36, 42), rgb(255, 255, 255), rgb(255, 90, 200)},
  {"TIDE", rgb(80, 170, 255), rgb(30, 60, 95), rgb(16, 30, 52), rgb(170, 230, 255), rgb(60, 220, 210)},
  {"UMBRA", rgb(150, 130, 200), rgb(45, 38, 65), rgb(22, 18, 34), rgb(220, 200, 255), rgb(110, 80, 170)},
  {"MIDNIGHT", rgb(90, 120, 255), rgb(30, 35, 80), rgb(14, 16, 40), rgb(200, 210, 255), rgb(150, 90, 255)},
  {"GOLD LEAF", rgb(255, 210, 80), rgb(90, 70, 20), rgb(40, 32, 10), rgb(255, 245, 190), rgb(255, 170, 40)},
  {"WHITE", rgb(240, 240, 240), rgb(90, 90, 90), rgb(40, 40, 40), rgb(255, 255, 255), rgb(200, 200, 200)},
};
inline void setTheme(int i) {
  const Theme& t = Themes[i >= 0 && i < 12 ? i : 0];
  Accent = t.accent; Sep = t.sep; SelBg = t.selBg; SelText = t.selText; Sigil = t.sigil;
}

inline uint16_t energyColor(Energy e) { return EnergyColor[(int)e]; }
inline uint16_t energyColor(int e) { return EnergyColor[e]; }
inline uint16_t factionColor(Faction f) { return FactionColor[(int)f]; }
inline uint16_t wardColor(WardKind w) { return WardColor[(int)w]; }

// Halfway between a color and the background, for wells that dropped out.
inline uint16_t dimmed(uint16_t c) {
  const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((r * 2 / 5) << 11) | ((g * 2 / 5) << 5) | (b * 2 / 5));
}

// A color faded toward black: k = 1 is the color, 0 is black.
inline uint16_t fade(uint16_t c, float k) {
  if (k <= 0) return 0;
  if (k > 1) k = 1;
  const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((int)(r * k) << 11) | ((int)(g * k) << 5) | (int)(b * k));
}

// ---- Signal -------------------------------------------------------------

enum class Reach : uint8_t { Faint, Far, Near, InReach };

inline Reach reachOf(float dbm) {
  if (dbm >= -55) return Reach::InReach;
  if (dbm >= -67) return Reach::Near;
  if (dbm >= WellRules::FaintDbm) return Reach::Far;
  return Reach::Faint;
}
inline const char* reachName(Reach r) {
  static const char* const k[] = {"FAINT", "FAR", "NEAR", "IN REACH"};
  return k[(int)r];
}
inline uint16_t reachColor(Reach r) {
  static const uint16_t k[] = {rgb(110, 116, 128), rgb(150, 160, 175), rgb(210, 215, 225), rgb(90, 220, 230)};
  return k[(int)r];
}

// Four bars, 3 px wide, rising left to right. x,y is the bottom-left corner.
inline void signalBars(Canvas& g, int x, int y, Reach r, uint16_t on) {
  for (int i = 0; i < 4; i++) {
    const int h = 2 + i * 2;
    g.fillRect(x + i * 4, y - h, 3, h, i <= (int)r ? on : Faint);
  }
}

// ---- Glyphs -------------------------------------------------------------
// 7x7, drawn at x,y (top-left). One shape per energy, so the list still
// reads without color.

inline void energyGlyph(Canvas& g, int x, int y, Energy e, uint16_t c) {
  switch (e) {
    case Energy::Ember:    // a flame: a triangle pointing up
      g.fillTriangle(x + 3, y, x, y + 6, x + 6, y + 6, c);
      break;
    case Energy::Tide:     // a full drop
      g.fillCircle(x + 3, y + 3, 3, c);
      break;
    case Energy::Gale:     // a diamond
      g.fillTriangle(x + 3, y, x, y + 3, x + 6, y + 3, c);
      g.fillTriangle(x, y + 3, x + 6, y + 3, x + 3, y + 6, c);
      break;
    case Energy::Hush:     // an empty ring
      g.drawCircle(x + 3, y + 3, 3, c);
      break;
    case Energy::Static:   // a spark
      g.drawLine(x, y, x + 6, y + 6, c);
      g.drawLine(x + 6, y, x, y + 6, c);
      g.drawFastHLine(x, y + 3, 7, c);
      break;
  }
}

// ---- Text ---------------------------------------------------------------

inline void text(Canvas& g, int x, int y, const char* s, uint16_t c, int size = 1) {
  g.setTextSize(size);
  g.setTextColor(c);
  g.setCursor(x, y);
  g.print(s);
}
inline int textW(const char* s, int size = 1) { return (int)strlen(s) * CharW * size; }
inline void textRight(Canvas& g, int xRight, int y, const char* s, uint16_t c, int size = 1) {
  text(g, xRight - textW(s, size), y, s, c, size);
}
inline void textCenter(Canvas& g, int y, const char* s, uint16_t c, int size = 1) {
  text(g, (W - textW(s, size)) / 2, y, s, c, size);
}

// A Hush well's name as it surfaces: the first `reveal` share of its
// letters, the rest still under water.
inline void veiledName(const char* name, float reveal, char out[17]) {
  const int len = (int)strlen(name), shown = (int)(len * reveal);
  for (int i = 0; i < len; i++) out[i] = i < shown || name[i] == ' ' ? name[i] : '~';
  out[len] = 0;
}

// A whole number with thousands commas: 26000 -> "26,000". Big ones shorten.
inline const char* num(double v, char* buf, int cap) {
  if (v < 0) v = 0;
  if (v >= 1e12) { snprintf(buf, cap, "%.1fT", v / 1e12); return buf; }
  if (v >= 1e9) { snprintf(buf, cap, "%.1fB", v / 1e9); return buf; }
  if (v >= 1e7) { snprintf(buf, cap, "%.1fM", v / 1e6); return buf; }
  char raw[24];
  snprintf(raw, sizeof(raw), "%.0f", v);
  const int len = (int)strlen(raw);
  int o = 0;
  for (int i = 0; i < len && o < cap - 1; i++) {
    buf[o++] = raw[i];
    const int left = len - 1 - i;
    if (left > 0 && left % 3 == 0 && o < cap - 1) buf[o++] = ',';
  }
  buf[o] = 0;
  return buf;
}

// ---- The three factors of an offering, one color each, everywhere ---------

constexpr uint16_t UnitsColor = rgb(255, 120, 210);
constexpr uint16_t ThirstColor = rgb(70, 220, 240);
constexpr uint16_t AmpColor = rgb(255, 75, 85);

// The factor a word names ("Amp", "UNITS", "thirst,"), or false.
inline bool factorWord(const char* w, int n, uint16_t& c) {
  char k[8];
  int m = 0;
  for (int i = 0; i < n && m < 7; i++) {
    const char ch = w[i];
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) k[m++] = (char)(ch | 0x20);
    else break;
  }
  k[m] = 0;
  if (!strcmp(k, "amp")) { c = AmpColor; return true; }
  if (!strcmp(k, "units") || !strcmp(k, "unit")) { c = UnitsColor; return true; }
  if (!strcmp(k, "thirst")) { c = ThirstColor; return true; }
  return false;
}

// A word that's a change to a factor: "+2", "x1.5", "-1", "3".
inline bool amountWord(const char* w, int n) {
  if (n == 0) return false;
  const char ch = w[0];
  return ch == '+' || ch == '-' || (ch >= '0' && ch <= '9') || ((ch == 'x' || ch == 'X') && n > 1 && w[1] >= '0' && w[1] <= '9');
}

// One line, with each factor word and the amount before it in that factor's color.
inline void textFactors(Canvas& g, int x, int y, const char* s, uint16_t c) {
  const int len = (int)strlen(s);
  int i = 0;
  while (i < len) {
    int j = i;
    while (j < len && s[j] != ' ') j++;
    uint16_t wc = c, fc;
    if (factorWord(s + i, j - i, fc)) wc = fc;
    else if (amountWord(s + i, j - i)) {
      int k = j;
      while (k < len && s[k] == ' ') k++;
      int e = k;
      while (e < len && s[e] != ' ') e++;
      if (e > k && factorWord(s + k, e - k, fc)) wc = fc;
    }
    char w[48];
    const int n = j + 1 - i < (int)sizeof(w) ? j + 1 - i : (int)sizeof(w) - 1;   // the word and its space
    memcpy(w, s + i, n);
    w[n] = 0;
    text(g, x + i * CharW, y, w, wc);
    i = j + 1;
  }
}

// Text wrapped at word breaks to `width` pixels. Returns the lines used.
inline int wrap(Canvas& g, int x, int y, int width, const char* s, uint16_t c, int maxLines = 9, int lineH = 10,
                int skip = 0, bool factors = false) {
  const int perLine = width / CharW;
  int lines = 0;
  while (*s && lines < maxLines + skip) {
    int len = (int)strlen(s);
    int take = len <= perLine ? len : perLine;
    if (len > perLine) {
      int sp = take;
      while (sp > 0 && s[sp] != ' ') sp--;
      if (sp > 0) take = sp;
    }
    char line[48];
    const int n = take < (int)sizeof(line) - 1 ? take : (int)sizeof(line) - 1;
    memcpy(line, s, n);
    line[n] = 0;
    if (lines >= skip) {
      if (factors) textFactors(g, x, y + (lines - skip) * lineH, line, c);
      else text(g, x, y + (lines - skip) * lineH, line, c);
    }
    lines++;
    s += take;
    while (*s == ' ') s++;
  }
  return lines - skip;
}

// How many lines `s` wraps to at `width` pixels.
inline int wrapLines(int width, const char* s) {
  const int perLine = width / CharW;
  int lines = 0;
  while (*s) {
    int len = (int)strlen(s), take = len <= perLine ? len : perLine;
    if (len > perLine) { int sp = take; while (sp > 0 && s[sp] != ' ') sp--; if (sp > 0) take = sp; }
    lines++;
    s += take;
    while (*s == ' ') s++;
  }
  return lines;
}

// A thin progress bar; `frac` past 1 fills it and glows.
inline void bar(Canvas& g, int x, int y, int w, int h, double frac, uint16_t c) {
  g.drawRect(x, y, w, h, Faint);
  const double f = frac < 0 ? 0 : frac > 1 ? 1 : frac;
  if (f > 0) g.fillRect(x + 1, y + 1, (int)((w - 2) * f), h - 2, frac >= 1 ? SelText : c);
}

inline uint16_t rarityColor(int rarity) {
  static const uint16_t k[4] = {rgb(200, 205, 215), rgb(90, 200, 120), rgb(110, 160, 255), rgb(255, 200, 70)};
  return k[rarity & 3];
}

// A key and what it does, like "TAB STREET". Returns the x after it.
inline int keyHint(Canvas& g, int x, const char* key, const char* what, uint16_t c) {
  text(g, x, FooterY, key, Dim);
  x += textW(key) + CharW;
  text(g, x, FooterY, what, c);
  return x + textW(what) + 2 * CharW;
}

inline void footerLine(Canvas& g) { g.drawFastHLine(0, FooterSepY, W, Sep); }

inline void battery(Canvas& g, int pct) {
  if (pct < 0) return;
  char b[8];
  snprintf(b, sizeof(b), "%d%%", pct);
  textRight(g, W - 2, FooterY, b, pct <= 15 ? Rec : Dim);
}

// A small ring that pulses while the Deck is listening.
inline void listening(Canvas& g, int cx, int cy, float t, bool active) {
  if (!active) {
    g.drawCircle(cx, cy, 2, Faint);
    return;
  }
  const float p = fmodf(t * 1.6f, 1.0f);
  g.drawCircle(cx, cy, 1 + (int)(p * 4), p < 0.6f ? Accent : dimmed(Accent));
  g.fillCircle(cx, cy, 1, Accent);
}

} // namespace UI
