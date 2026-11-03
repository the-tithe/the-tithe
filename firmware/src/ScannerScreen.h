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
//   N3 [=========---]  1,240 TO GO   7 LEFT  23c   the Night: Tithe progress
//   ===energy mix===========================
//   ^ VESPER KILN   NEW          NEAR  ..||
//   o CHROME SPIRE *             FAR   .|||       * the hint star
//   ...
//   G GLASS  ESC MENU               (NOT SAVING, in red, when nothing keeps the game)

namespace ScannerScreen {

constexpr int RowY0 = 27;
constexpr int RowH = 12;
constexpr int Rows = 8;
constexpr int MaxList = Sightings::MaxWells;   // every listed well, so sorting sees them all

inline uint32_t selectedId = 0;

// A number for the Tithe line: exact below 10,000, then 17.5K, 240K, 3.1M.
inline const char* shortNum(double v, char* buf, int cap) {
  if (v < 1e4) return UI::num(v, buf, cap);
  if (v < 1e5) snprintf(buf, cap, "%.1fK", floor(v / 100) / 10);
  else if (v < 1e6) snprintf(buf, cap, "%.0fK", floor(v / 1000));
  else if (v < 1e9) snprintf(buf, cap, "%.1fM", floor(v / 1e5) / 10);
  else return UI::num(v, buf, cap);
  return buf;
}
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
  // The right end first, so the left knows where it has to stop.
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
    rx -= 9 + UI::textW(b) + 6;
  }

  const WardKind w = sky.ward();
  if (sky.scans() == 0) {
    UI::text(g, 3, 3, st.deaf ? "DEAF" : "LISTENING", st.deaf ? UI::Rec : UI::Dim);
  } else {
    UI::text(g, 3, 3, wardName(w), UI::wardColor(w));
    int x = 3 + UI::textW(wardName(w)) + 2 * UI::CharW;
    // The ward only moves once two scans agree, but the count is this scan's: when
    // they disagree, the ward the count points to follows, dimmed.
    const WardKind next = sky.wardHeard();
    if (next != w) {
      UI::text(g, x - UI::CharW, 3, ">", UI::Dim);
      UI::text(g, x + UI::CharW, 3, wardName(next), UI::dimmed(UI::wardColor(next)));
      x += 2 * UI::CharW + UI::textW(wardName(next)) + 2 * UI::CharW;
    }
    snprintf(b, sizeof(b), "%d %s", sky.near(), sky.near() == 1 ? "WELL" : "WELLS");
    const int room = rx - (st.adrift ? 18 : 6);
    if (x + UI::textW(b) > room) snprintf(b, sizeof(b), "%d", sky.near());   // tight: the number alone
    UI::text(g, x, 3, b, UI::Text);
    if (st.adrift) {   // a drifting glyph waits in the glass: an outlined diamond
      const int cx = x + UI::textW(b) + 9, cy = 6;
      g.drawLine(cx, cy - 3, cx + 3, cy, Strays::Chalk);
      g.drawLine(cx + 3, cy, cx, cy + 3, Strays::Chalk);
      g.drawLine(cx, cy + 3, cx - 3, cy, Strays::Chalk);
      g.drawLine(cx - 3, cy, cx, cy - 3, Strays::Chalk);
    }
  }

  // The Night: how much of the Tithe is paid, how much Resonance is still to
  // go, and what's left to do it with. Ticks under the bar show where 6, 9 and
  // 11 spare visits would land (a tick the fill has passed is bright); a quiet
  // Deck keeps those to itself.
  if (st.inRun) {
    snprintf(b, sizeof(b), "N%d", st.night + 1);
    UI::text(g, 3, 13, b, UI::SelText);
    const int bw = 56;   // room for "98.7K TO GO" and "12 LEFT  999c" beside it
    const double frac = st.tithe > 0 ? st.total / st.tithe : 0;
    UI::bar(g, 22, 13, bw, 7, frac, UI::Accent);
    for (int k = 0; k < st.notches; k++) {
      int x = 23 + (int)((bw - 2) * st.notch[k]);
      if (x > 22 + bw - 2) x = 22 + bw - 2;
      g.drawFastVLine(x, 20, 2, frac >= st.notch[k] ? UI::SelText : UI::Dim);
    }
    const double toGo = ceil(st.tithe - st.total);
    if (toGo > 0) {
      char n[16];
      snprintf(b, sizeof(b), "%s TO GO", shortNum(toGo, n, sizeof(n)));
      UI::text(g, 22 + bw + 5, 13, b, UI::Text);
    }
    snprintf(b, sizeof(b), "%d LEFT  %dc", st.visitsLeft, st.coin);
    UI::textRight(g, UI::W - 3, 13, b, st.visitsLeft <= 2 ? UI::Rec : UI::Text);
  } else {
    UI::text(g, 3, 13, "NO WEEK BOUND: ESC > BIND A NEW WEEK", UI::Dim);
  }

  // How the energy around you splits, as one bar: what the wells give tonight.
  const int y = 23, x0 = 0, width = UI::W;
  int total = 0, near[EnergyCount] = {0};
  for (int e = 0; e < EnergyCount; e++) {
    near[(int)st.tonightOf((Energy)e)] += sky.energyNear((Energy)e);
    total += sky.energyNear((Energy)e);
  }
  if (total == 0) {
    g.drawFastHLine(x0, y, width, UI::Sep);
    return;
  }
  int x = x0, acc = 0;
  for (int e = 0; e < EnergyCount; e++) {
    const int c = near[e];
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
  const Energy te = st.tonightOf(w.energy);   // what it gives tonight
  const uint16_t ec = gone ? UI::dimmed(UI::energyColor(te)) : UI::energyColor(te);
  UI::energyGlyph(g, 4, y + 2, te, ec);
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

// `star`: the hint star's well is below the rows shown (1), above them (-1), or
// not off-screen (0). Off-screen, the footer says where, in place of G GLASS.
inline void footer(Canvas& g, const Status& st, int star = 0) {
  UI::footerLine(g);
  if (st.unsaved) UI::textRight(g, UI::W - 2, UI::FooterY, "NOT SAVING", UI::Rec);
  int x = 2;
  if (star) {
    UI::text(g, x, UI::FooterY, "*", UI::SelText);
    UI::text(g, x + 2 * UI::CharW, UI::FooterY, star > 0 ? "BELOW" : "ABOVE", UI::Dim);
    x += 2 * UI::CharW + UI::textW("BELOW") + 2 * UI::CharW;
  } else {
    x = UI::keyHint(g, x, "G", "GLASS", UI::Text);
  }
  UI::keyHint(g, x, "ESC", "MENU", UI::Text);
}

inline void empty(Canvas& g, const Sightings& sky, const Status& st, float t) {
  const int cx = UI::W / 2, cy = 68;
  for (int k = 0; k < 3; k++) {
    const float p = fmodf(t * 0.5f + k / 3.0f, 1.0f);
    g.drawCircle(cx, cy, 4 + (int)(p * 30), p < 0.5f ? UI::Sep : UI::Faint);
  }
  g.fillCircle(cx, cy, 2, UI::Accent);
  if (st.deaf) UI::textCenter(g, 106, "DEAF: THE RADIO DIDN'T START", UI::Rec);
  else UI::textCenter(g, 106, sky.scans() == 0 ? "LISTENING" : "NO WELLS IN RANGE", UI::Dim);
}

inline void draw(Canvas& g, const Sightings& sky, const Status& st, float t) {
  g.fillSprite(UI::Bg);
  header(g, sky, st, t);
  static const WellRec* list[MaxList];
  const int n = sky.listed(list, MaxList);
  int star = 0;
  if (n == 0) {
    empty(g, sky, st, t);
  } else {
    for (int i = 0; i < n && st.starId; i++)
      if (list[i]->id == st.starId) { star = i < top ? -1 : i >= top + Rows ? 1 : 0; break; }
    for (int r = 0; r < Rows && top + r < n; r++) {
      row(g, RowY0 + r * RowH, *list[top + r], sky, st, top + r == selIndex);
    }
    if (n > Rows) {   // where the visible rows sit in the whole list
      const int h = Rows * RowH;
      const int bh = h * Rows / n, by = RowY0 + (h - bh) * top / (n - Rows);
      g.drawFastVLine(UI::W - 1, by, bh, UI::Dim);
    }
  }
  footer(g, st, star);
}

} // namespace ScannerScreen
