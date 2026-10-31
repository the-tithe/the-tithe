#pragma once
#include "Effects.h"
#include "UI.h"
#include "Press.h"
#include "Sightings.h"
#include "Status.h"
#include "Strays.h"

// The main view: the wells around you, strongest first.
//
//   FRINGE  7 WELLS                   87% (o)   ward, count, battery, listening
//   N3 [=======-------------]   7 LEFT  23c       the Night: Tithe progress
//   ===energy mix===========================
//   ^ VESPER KILN   NEW          NEAR  ..||
//   o CHROME SPIRE *             FAR   .|||       * the hint star
//   ...
//   TAB STREET  R REC W3  G GLASS  ESC MENU

namespace ScannerScreen {

constexpr int RowY0 = 27;
constexpr int RowH = 12;
constexpr int Rows = 8;
constexpr int MaxList = Sightings::MaxWells;   // every listed well, so sorting sees them all

inline uint32_t selectedId = 0;
inline int selIndex = 0;
inline int top = 0;

// Moves the selection. Returns the ID of the well to open, or 0.
inline uint32_t update(const Press& p, const Sightings& sky) {
  static const WellRec* list[MaxList];   // static: 1.3 KB off the stack
  const int n = sky.listed(list, MaxList);
  if (n == 0) { selIndex = 0; selectedId = 0; top = 0; return 0; }
  // Follow the selected well as the list reorders; if it dropped out,
  // stay on the same row.
  int i = 0;
  while (i < n && list[i]->id != selectedId) i++;
  if (i == n) i = selIndex < n ? selIndex : n - 1;
  if (p.down && i < n - 1) i++;
  if (p.up && i > 0) i--;
  if (p.left) i = 0;        // the top: the nearest well
  if (p.right) i = n - 1;   // the bottom
  selIndex = i;
  selectedId = list[i]->id;
  if (i < top) top = i;
  if (i >= top + Rows) top = i - Rows + 1;
  if (top > n - Rows) top = n - Rows > 0 ? n - Rows : 0;
  return p.enter ? selectedId : 0;
}

inline void header(Canvas& g, const Sightings& sky, const Status& st, float t) {
  char b[24];
  const WardKind w = sky.ward();
  if (sky.scans() == 0) {
    UI::text(g, 3, 3, "LISTENING", UI::Dim);
  } else {
    UI::text(g, 3, 3, wardName(w), UI::wardColor(w));
    snprintf(b, sizeof(b), "%d %s", sky.near(), sky.near() == 1 ? "WELL" : "WELLS");
    const int x = 3 + UI::textW(wardName(w)) + 2 * UI::CharW;
    UI::text(g, x, 3, b, UI::Text);
    if (st.adrift) {   // a drifting glyph waits in the glass: an outlined diamond
      const int cx = x + UI::textW(b) + 9, cy = 6;
      g.drawLine(cx, cy - 3, cx + 3, cy, Strays::Chalk);
      g.drawLine(cx + 3, cy, cx, cy + 3, Strays::Chalk);
      g.drawLine(cx, cy + 3, cx - 3, cy, Strays::Chalk);
      g.drawLine(cx - 3, cy, cx, cy - 3, Strays::Chalk);
    }
  }
  UI::listening(g, UI::W - 7, 6, t, st.listening);
  int rx = UI::W - 16;
  if (st.battery >= 0) {
    snprintf(b, sizeof(b), "%d%%", st.battery);
    UI::textRight(g, rx, 3, b, st.battery <= 15 ? UI::Rec : UI::Dim);
    rx -= UI::textW(b) + 8;
  }
  if (st.wardLeft > 0) { Effects::wardIcon(g, rx - 2, 6, UI::FactionColor[(int)Faction::Warded]); rx -= 12; }
  if (st.inRun && st.wardens && st.heat >= 2) {   // the Wardens' eye, and how hot you are
    const uint16_t ec = st.heat >= 3 ? UI::Rec : UI::dimmed(UI::Rec);
    Effects::eyeIcon(g, rx - 2, 6, ec);
    snprintf(b, sizeof(b), "%.1f", st.heat);
    UI::textRight(g, rx - 9, 3, b, ec);
  }

  // The Night: how much of the Tithe is paid, and what's left to do it with.
  // After the first offering, about how many more this build needs, and ticks
  // under the bar where 6, 9 and 11 spare visits would land (a tick the fill
  // has passed is bright). A quiet Deck keeps those to itself, and the bar
  // takes the room.
  if (st.inRun) {
    snprintf(b, sizeof(b), "N%d", st.night + 1);
    UI::text(g, 3, 13, b, UI::SelText);
    const int bw = st.quiet ? 132 : 84;
    const double frac = st.tithe > 0 ? st.total / st.tithe : 0;
    UI::bar(g, 22, 13, bw, 7, frac, UI::Accent);
    for (int k = 0; k < st.notches; k++) {
      int x = 23 + (int)((bw - 2) * st.notch[k]);
      if (x > 22 + bw - 2) x = 22 + bw - 2;
      g.drawFastVLine(x, 20, 2, frac >= st.notch[k] ? UI::SelText : UI::Dim);
    }
    if (st.more > 0) {
      snprintf(b, sizeof(b), "~%d MORE", st.more);
      UI::text(g, 22 + bw + 6, 13, b, st.more > st.visitsLeft ? UI::Rec : UI::Dim);
    }
    snprintf(b, sizeof(b), "%d LEFT  %dc", st.visitsLeft, st.coin);
    UI::textRight(g, UI::W - 3, 13, b, st.visitsLeft <= 2 ? UI::Rec : UI::Text);
  } else {
    UI::text(g, 3, 13, "NO WEEK BOUND: ESC > BIND A NEW WEEK", UI::Dim);
  }

  // How the energy around you splits, as one bar.
  const int y = 23, x0 = 0, width = UI::W;
  int total = 0;
  for (int e = 0; e < EnergyCount; e++) total += sky.energyNear((Energy)e);
  if (total == 0) {
    g.drawFastHLine(x0, y, width, UI::Sep);
    return;
  }
  int x = x0, acc = 0;
  for (int e = 0; e < EnergyCount; e++) {
    const int c = sky.energyNear((Energy)e);
    if (!c) continue;
    acc += c;
    const int x1 = x0 + width * acc / total;
    g.fillRect(x, y, x1 - x - (acc < total ? 1 : 0), 2, UI::energyColor((Energy)e));
    x = x1;
  }
}

inline void row(Canvas& g, int y, const WellRec& w, const Sightings& sky, const Status& st, bool sel) {
  const bool lost = !sky.inRange(w);
  const bool gone = lost || (st.spent && st.spent(w.id));   // out of range, or nothing left to do there tonight
  if (sel) g.fillRect(0, y, UI::W, RowH - 1, UI::SelBg);
  const uint16_t ec = gone ? UI::dimmed(UI::energyColor(w.energy)) : UI::energyColor(w.energy);
  UI::energyGlyph(g, 4, y + 2, w.energy, ec);
  // Hush wells only show their names to someone standing still.
  if (w.energy == Energy::Hush && st.hushReveal < 1) {
    char veil[17];
    UI::veiledName(w.name, st.hushReveal, veil);
    UI::text(g, 15, y + 2, veil, UI::dimmed(UI::energyColor(Energy::Hush)));
  } else {
    UI::text(g, 15, y + 2, w.name, gone ? UI::Dim : sel ? UI::SelText : UI::Text);
  }
  int tagX = 15 + UI::textW(w.name) + 6;
  if (w.id == st.starId && !gone) {   // the hint star: where your build makes the most
    UI::text(g, tagX, y + 2, "*", UI::SelText);
    tagX += 12;
  }
  if (st.glyph && st.glyph(w.id) >= 0) {   // a glyph you carved here this week
    const int cx = tagX + 3, cy = y + 6;
    const uint16_t gc = gone ? UI::Dim : UI::Accent;
    g.fillTriangle(cx - 3, cy, cx, cy - 3, cx + 3, cy, gc);
    g.fillTriangle(cx - 3, cy, cx, cy + 3, cx + 3, cy, gc);
    tagX += 12;
  }
  if (w.namesake) {   // the true name of a well you've worked
    UI::text(g, tagX, y + 2, "=", gone ? UI::Dim : UI::Accent);
    tagX += 12;
  }
  if (sky.isFresh(w) && !gone && tagX + UI::textW("FRESH") + 2 <= 168) UI::text(g, tagX, y + 2, "FRESH", UI::Fresh);   // not when three tags already sit before the reach word
  const UI::Reach r = UI::reachOf(w.sig);
  const uint16_t rc = gone ? UI::Faint : UI::reachColor(r);
  UI::textRight(g, 216, y + 2, lost ? "LOST" : UI::reachName(r), gone ? UI::Dim : rc);
  UI::signalBars(g, 221, y + 10, r, gone ? UI::Dim : rc);
}

// The R hint's word for each state of the walk log: REC, red while it records (the
// walk's number is on the walk screen, which also spells the card's trouble out).
inline const char* recWord(Status::Log log, uint16_t& color) {
  color = UI::Dim;
  switch (log) {
    case Status::Log::On: color = UI::Rec; return "REC";
    case Status::Log::NoCard: return "NO SD";
    case Status::Log::Failed: color = UI::Rec; return "SD ERR";
    default: return "REC";
  }
}
// The TAB hint's word: the place, or PLACE when none is set.
inline const char* placeWord(WalkFormat::Place place) {
  return place == WalkFormat::Place::None ? "PLACE" : WalkFormat::placeLabel(place);
}

// The footer's four hints, 240 px wide at most: with the longest place and a card
// error they wouldn't fit, so G GLASS gives way (ESC MENU is the only way to the menu).
inline int footerWidth(WalkFormat::Place place, Status::Log log, bool withGlass) {
  uint16_t c;
  const auto hint = [](const char* key, const char* what) { return UI::textW(key) + UI::CharW + UI::textW(what); };
  return 2 + hint("TAB", placeWord(place)) + 12 + hint("R", recWord(log, c)) + 12 +
         (withGlass ? hint("G", "GLASS") + 12 : 0) + hint("ESC", "MENU");
}

inline void footer(Canvas& g, const Status& st) {
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "TAB", placeWord(st.place), st.place == WalkFormat::Place::None ? UI::Dim : UI::Accent);
  uint16_t rc;
  const char* rec = recWord(st.log, rc);
  x = UI::keyHint(g, x, "R", rec, rc);
  if (footerWidth(st.place, st.log, true) <= UI::W) x = UI::keyHint(g, x, "G", "GLASS", UI::Text);
  UI::keyHint(g, x, "ESC", "MENU", UI::Text);
}

inline void empty(Canvas& g, const Sightings& sky, float t) {
  const int cx = UI::W / 2, cy = 68;
  for (int k = 0; k < 3; k++) {
    const float p = fmodf(t * 0.5f + k / 3.0f, 1.0f);
    g.drawCircle(cx, cy, 4 + (int)(p * 30), p < 0.5f ? UI::Sep : UI::Faint);
  }
  g.fillCircle(cx, cy, 2, UI::Accent);
  UI::textCenter(g, 106, sky.scans() == 0 ? "LISTENING" : "NO WELLS IN RANGE", UI::Dim);
}

inline void draw(Canvas& g, const Sightings& sky, const Status& st, float t) {
  g.fillSprite(UI::Bg);
  header(g, sky, st, t);
  static const WellRec* list[MaxList];
  const int n = sky.listed(list, MaxList);
  if (n == 0) {
    empty(g, sky, t);
  } else {
    for (int r = 0; r < Rows && top + r < n; r++) {
      row(g, RowY0 + r * RowH, *list[top + r], sky, st, top + r == selIndex);
    }
    if (n > Rows) {   // where the visible rows sit in the whole list
      const int h = Rows * RowH;
      const int bh = h * Rows / n, by = RowY0 + (h - bh) * top / (n - Rows);
      g.drawFastVLine(UI::W - 1, by, bh, UI::Dim);
    }
  }
  footer(g, st);
}

} // namespace ScannerScreen
