#pragma once
#include "UI.h"
#include "Press.h"
#include "Sightings.h"
#include "Status.h"

// One well up close, for walking toward it: its signal over the last 48
// scans, and whether you're getting warmer or colder.

namespace WellScreen {

enum class Trend : int8_t { Colder = -1, Steady = 0, Warmer = 1 };

// Compares the last 3 scans it was heard in with the 3 before them.
inline Trend trendOf(const WellRec& w, uint32_t scanNo) {
  int got = 0, recent = 0, older = 0;
  const uint32_t span = scanNo < (uint32_t)WellRec::Hist ? scanNo : (uint32_t)WellRec::Hist;
  for (uint32_t s = scanNo, k = 0; k < span && got < 6; k++, s--) {
    const int8_t v = w.hist[s % WellRec::Hist];
    if (v == Sightings::Absent) continue;
    if (got < 3) recent += v; else older += v;
    got++;
  }
  if (got < 6) return Trend::Steady;
  const int diff = (recent - older) / 3;
  if (diff >= 3) return Trend::Warmer;
  if (diff <= -3) return Trend::Colder;
  return Trend::Steady;
}

inline bool update(const Press& p) { return p.back; }
inline bool wantsDowse(const Press& p) { return p.ch == 'd'; }

inline void graph(Canvas& g, int x, int y, int w, int h, const WellRec& wr, uint32_t scanNo) {
  constexpr int Lo = -95, Hi = -35;
  for (int db = -85; db <= -45; db += 10) {   // faint guide lines
    const int gy = y + h - (db - Lo) * h / (Hi - Lo);
    for (int gx = x; gx < x + w; gx += 4) g.drawPixel(gx, gy, UI::Faint);
  }
  const int n = WellRec::Hist;
  const int bw = w / n;
  x += w - n * bw;   // the newest scan sits at the right edge
  const uint16_t c = UI::energyColor(wr.energy);
  for (int k = 0; k < n; k++) {   // oldest on the left
    const uint32_t s = scanNo - (uint32_t)(n - 1 - k);
    const int bx = x + k * bw;
    if (s == 0 || s > scanNo || s < wr.firstScan) continue;
    const int8_t v = wr.hist[s % n];
    if (v == Sightings::Absent) {
      g.drawPixel(bx + bw / 2, y + h - 1, UI::Dim);
      continue;
    }
    int bh = (v - Lo) * h / (Hi - Lo);
    if (bh < 1) bh = 1;
    if (bh > h) bh = h;
    g.fillRect(bx, y + h - bh, bw - 1, bh, k == n - 1 ? UI::Text : c);
  }
}

inline void draw(Canvas& g, const Sightings& sky, uint32_t id, const Status& st, float t) {
  g.fillSprite(UI::Bg);
  const WellRec* w = sky.byId(id);
  if (!w) {
    UI::textCenter(g, 60, "THE WELL IS GONE", UI::Dim);
    UI::footerLine(g);
    UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
    return;
  }
  char b[40];
  if (w->energy == Energy::Hush && st.hushReveal < 1) {
    char veil[17];
    UI::veiledName(w->name, st.hushReveal, veil);
    UI::text(g, 4, 4, veil, UI::dimmed(UI::energyColor(Energy::Hush)), 2);
  } else {
    UI::text(g, 4, 4, w->name, UI::Text, 2);
  }

  int x = 4;
  UI::energyGlyph(g, x, 24, w->energy, UI::energyColor(w->energy));
  x += 11;
  UI::text(g, x, 24, energyName(w->energy), UI::energyColor(w->energy));
  x += UI::textW(energyName(w->energy)) + 2 * UI::CharW;
  if (st.wellFocus >= 0) {   // it also gives your focus tonight
    const Energy f = (Energy)st.wellFocus;
    UI::energyGlyph(g, x, 24, f, UI::energyColor(f));
    x += 11;
    UI::text(g, x, 24, energyName(f), UI::energyColor(f));
    x += UI::textW(energyName(f)) + 2 * UI::CharW;
  }
  UI::text(g, x, 24, factionName(w->faction), UI::factionColor(w->faction));
  if (st.wellFavor > 0) {   // a favor you could take here, instead of the channel
    snprintf(b, sizeof(b), "FAVOR +%dc", st.wellFavor);
    UI::textRight(g, UI::W - 4, 24, b, UI::factionColor((Faction)st.wellFavorFaction));
  } else if (st.wellGlyph) {   // the glyph you carved here
    char n[16];
    int k = 0;
    for (const char* q = st.wellGlyph; *q && k < 15; q++) n[k++] = (char)toupper(*q);
    n[k] = 0;
    snprintf(b, sizeof(b), "%s GLYPH", n);
    UI::textRight(g, UI::W - 4, 24, b, UI::Accent);
  } else {
    snprintf(b, sizeof(b), "CH %u", w->channel);
    UI::textRight(g, UI::W - 4, 24, b, UI::Dim);
  }

  if (w->namesake) {   // decided when it came into reach: a well you've worked has its name
    graph(g, 4, 37, UI::W - 8, 20, *w, sky.scans());
    UI::text(g, 4, 63, "SAME NAME AS A WELL YOU'VE WORKED.", UI::Text);
    UI::text(g, 4, 74, "ELSEWHERE.", UI::Text);
  } else {
    graph(g, 4, 37, UI::W - 8, 44, *w, sky.scans());
  }

  const bool gone = !sky.inRange(*w);
  const UI::Reach r = UI::reachOf(w->sig);
  UI::text(g, 4, 88, gone ? "LOST" : UI::reachName(r), gone ? UI::Dim : UI::reachColor(r), 2);
  if (!gone) {
    const Trend tr = trendOf(*w, sky.scans());
    const char* word = tr == Trend::Warmer ? "WARMER" : tr == Trend::Colder ? "COLDER" : "STEADY";
    const uint16_t c = tr == Trend::Warmer ? UI::energyColor(Energy::Ember)
                       : tr == Trend::Colder ? UI::energyColor(Energy::Tide) : UI::Dim;
    const int wx = UI::W - 4 - UI::textW(word);
    UI::text(g, wx, 92, word, c);
    const int ax = wx - 10, ay = 95;
    if (tr == Trend::Warmer) g.fillTriangle(ax, ay - 4, ax - 4, ay + 3, ax + 4, ay + 3, c);
    if (tr == Trend::Colder) g.fillTriangle(ax, ay + 4, ax - 4, ay - 3, ax + 4, ay - 3, c);
  }

  const uint32_t mins = (st.nowMs - w->firstMs) / 60000;
  char times[16];
  if (w->seen == 1) snprintf(times, sizeof(times), "ONCE");
  else snprintf(times, sizeof(times), "%u TIMES", w->seen);
  bool holding = false;
  for (int e = 0; e < EnergyCount; e++) holding |= st.wellThirst[e] != 0;
  if (holding) {   // its Thirst for what you're carrying, where you stand now
    UI::text(g, 4, 110, "THIRST", UI::Dim);
    int x = 4 + 7 * UI::CharW;
    for (int e = 0; e < EnergyCount; e++) {
      const float t = st.wellThirst[e];
      if (t == 0) continue;
      UI::energyGlyph(g, x, 110, (Energy)e, UI::energyColor((Energy)e));
      if (t < 0) snprintf(b, sizeof(b), "NO");
      else snprintf(b, sizeof(b), "%.1f", t);
      UI::text(g, x + 9, 110, b, t < 0 ? UI::Rec : UI::ThirstColor);
      x += 9 + UI::textW(b) + 8;
    }
  } else {
    if (mins == 0) snprintf(b, sizeof(b), "HEARD %s, FIRST JUST NOW", times);
    else snprintf(b, sizeof(b), "HEARD %s, FIRST %lu MIN AGO", times, (unsigned long)mins);
    UI::text(g, 4, 110, b, UI::Dim);
  }

  // What the keys do here; a lost well can't be visited or dowsed. (The signal
  // is the bars and the reach word above; the dowse keeps the dBm number.)
  UI::footerLine(g);
  int kx = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  if (st.inRun && !gone) kx = UI::keyHint(g, kx, "ENTER", "VISIT", UI::Text);
  if (st.gyro && !gone) UI::keyHint(g, kx, "D", "DOWSE", UI::Text);
  UI::textRight(g, UI::W - 2, UI::FooterY, "WELL", UI::Text);   // against the right edge: with every hint up it just fits
  UI::textRight(g, UI::W - 2 - UI::textW("WELL") - UI::CharW, UI::FooterY, "< >", UI::Dim);
  (void)t;
}

} // namespace WellScreen
