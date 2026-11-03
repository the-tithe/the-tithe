#pragma once
#include "Motion.h"
#include "Press.h"
#include "Sightings.h"
#include "Status.h"
#include "Strays.h"
#include "UI.h"

// The scrying glass: the wells around you floating in a bowl of dark
// water. Stronger wells float near the middle. Tilt the Deck and they
// drift, the near ones most, like things under water. Walk and the water
// ripples; stand still and it clears, and the Hush wells surface.
//
// A well you've dowsed sits at its real direction (up is the way you
// face) and turns with you. The rest float at a direction of their own,
// which means nothing.
//
// Now and then a drifting glyph floats here too. It isn't a well: it can't
// be opened or dowsed, and it drifts to the emptiest side of the bowl, away
// from the wells. Hold still and it draws itself, stroke by stroke.

namespace GlassScreen {

constexpr int Cx = 120, Cy = 68, R = 50;
constexpr int MaxShown = 24;
constexpr uint32_t BearingKeepMs = 180000;   // a dowsed direction holds for 3 minutes

inline uint32_t selectedId = 0;

// The drifting glyph, filled in by the app each frame. `drawn` 0..1 as it
// draws itself; `scatter` the seconds since a flick broke it up, below 0 whole.
struct Drift { bool on; uint8_t shape; uint16_t hand; float drawn; float scatter; };
inline Drift drift{};

inline bool pinned(const WellRec& w, uint32_t nowMs) { return w.dowsed && nowMs - w.dowsedMs < BearingKeepMs; }

inline bool visible(const WellRec& w, const Status& st) {
  return w.energy != Energy::Hush || st.hushReveal > 0.05f;
}

// Moves the selection. Returns the well to open, or 0.
inline uint32_t update(const Press& p, const Sightings& sky, const Status& st) {
  static const WellRec* list[Sightings::MaxWells];   // static: 1.3 KB off the stack
  int n = sky.listed(list, Sightings::MaxWells);
  if (n > MaxShown) n = MaxShown;
  int k = 0;
  for (int i = 0; i < n; i++) if (visible(*list[i], st)) list[k++] = list[i];
  n = k;
  if (n == 0) { selectedId = 0; return 0; }
  int i = 0;
  while (i < n && list[i]->id != selectedId) i++;
  if (i == n) i = 0;
  if (p.down || p.right) i = (i + 1) % n;
  if (p.up || p.left) i = (i + n - 1) % n;
  selectedId = list[i]->id;
  return p.enter ? selectedId : 0;
}

// Where a well floats in the glass.
inline void place(const WellRec& w, const Motion& m, const Status& st, float t, float& x, float& y) {
  float depth = (-40 - w.sig) / 50.0f;   // 0 near the middle .. 1 at the rim
  if (depth < 0) depth = 0;
  if (depth > 1) depth = 1;
  const float r = 8 + depth * (R - 13);
  float a;
  if (pinned(w, st.nowMs)) {
    a = Motion::wrap(w.bearing - m.heading);
  } else {
    a = (w.seed >> 8 & 0xFFFF) / 65536.0f * 6.2832f + 0.12f * sinf(t * 0.21f + (w.seed & 0xFF));
  }
  x = Cx + r * sinf(a);
  y = Cy - r * cosf(a);
  // Parallax: near wells drift further when the glass tips.
  const float near = 1 - depth;
  x += m.tiltX() * (6 + 10 * near);
  y += m.tiltY() * (5 + 8 * near);
  // Ripples while the Deck moves.
  const float rough = 1 - m.steadiness;
  x += rough * 2.5f * sinf(t * 7.3f + (w.seed & 63));
  y += rough * 2.5f * cosf(t * 6.1f + (w.seed >> 6 & 63));
}

// The drifting glyph at x,y (its middle): faint while it drifts, bright where
// it has drawn itself, and flying apart into pixels after a flick.
inline void drawDrift(Canvas& g, int x, int y) {
  static Strays::Mask mk;
  Strays::make(drift.shape, drift.hand, 3, mk);
  int left = Strays::Mask::W, right = 0, top = Strays::Mask::H, bottom = 0;
  for (int py = 0; py < Strays::Mask::H; py++)
    for (int px = 0; px < Strays::Mask::W; px++)
      if (mk.px[py][px]) {
        if (px < left) left = px;
        if (px > right) right = px;
        if (py < top) top = py;
        if (py > bottom) bottom = py;
      }
  const int ox = x - (left + right) / 2, oy = y - (top + bottom) / 2;
  const int upTo = (int)(drift.drawn * mk.steps + 0.5f);
  const float s = drift.scatter;
  for (int py = top; py <= bottom; py++)
    for (int px = left; px <= right; px++) {
      const uint8_t v = mk.px[py][px];
      if (!v) continue;
      const bool bright = Strays::Mask::step(v) <= upTo;
      uint16_t c = Strays::Mask::dry(v) ? Strays::ChalkDry : Strays::Chalk;
      if (!bright) c = UI::fade(c, 0.4f);
      int dx = ox + px, dy = oy + py;
      if (s >= 0) {   // each pixel flies off its own way, and falls
        const uint32_t h = Strays::mix((uint32_t)(py * 64 + px) * 2654435761u);
        const float vx = ((int)(h & 63) - 32) * 1.1f, vy = -(float)((h >> 6) & 31) - 6;
        dx += (int)(vx * s);
        dy += (int)(vy * s + 70 * s * s);
        c = UI::fade(c, 1 - s / 1.2f);
      }
      if (dx >= 0 && dx < UI::W && dy >= 0 && dy < UI::FooterSepY) g.drawPixel(dx, dy, c);
    }
}

inline void draw(Canvas& g, const Sightings& sky, const Motion& m, const Status& st, float t) {
  g.fillSprite(UI::Bg);
  // The bowl, with faint rings that shimmer more the more you move.
  g.fillCircle(Cx, Cy, R, UI::rgb(6, 10, 18));
  const float rough = st.gyro ? 1 - m.steadiness : 0;
  for (int k = 1; k <= 3; k++) {
    const float wob = rough * 3 * sinf(t * 3 + k * 1.7f);
    g.drawCircle(Cx, Cy, (int)(k * R / 4 + wob), UI::rgb(16, 26, 40));
  }
  g.drawCircle(Cx, Cy, R, UI::Sep);
  g.drawCircle(Cx, Cy, R + 2, UI::Faint);
  // Stillness fills the rim, clockwise from the top; full means the glass is clear.
  const float clear = st.hushReveal;
  if (st.gyro && clear > 0) {
    const int steps = (int)(clear * 120);
    for (int i = 0; i <= steps; i++) {
      const float a = i / 120.0f * 6.2832f;
      g.drawPixel(Cx + (int)lroundf((R + 2) * sinf(a)), Cy - (int)lroundf((R + 2) * cosf(a)),
                  clear >= 1 ? UI::EnergyColor[(int)Energy::Hush] : UI::Accent);
    }
  }
  g.fillTriangle(Cx, Cy - R - 7, Cx - 3, Cy - R - 2, Cx + 3, Cy - R - 2, UI::Dim);   // the way you face
  g.fillCircle(Cx, Cy, 1, UI::Dim);

  static const WellRec* list[Sightings::MaxWells];
  int n = sky.listed(list, Sightings::MaxWells);
  if (n > MaxShown) n = MaxShown;
  const WellRec* sel = nullptr;
  const bool settled = drift.on && drift.scatter < 0 && drift.drawn >= 1;
  float wx[MaxShown], wy[MaxShown];   // where the wells sit, to float the glyph clear of them
  int nw = 0;
  for (int i = n - 1; i >= 0; i--) {   // faint ones first, so strong ones sit on top
    const WellRec& w = *list[i];
    if (!visible(w, st)) continue;
    float x, y;
    place(w, m, st, t, x, y);
    wx[nw] = x;
    wy[nw++] = y;
    const Energy te = st.tonightOf(w.energy);   // what it gives tonight
    uint16_t c = UI::energyColor(te);
    if (!sky.inRange(w)) c = UI::dimmed(c);
    if (w.energy == Energy::Hush && st.hushReveal < 1) c = UI::dimmed(c);
    UI::energyGlyph(g, (int)x - 3, (int)y - 3, te, c);
    if (pinned(w, st.nowMs)) {   // a tick on the rim toward a dowsed well
      const float a = Motion::wrap(w.bearing - m.heading);
      g.drawLine(Cx + (int)((R - 3) * sinf(a)), Cy - (int)((R - 3) * cosf(a)), Cx + (int)((R + 4) * sinf(a)),
                 Cy - (int)((R + 4) * cosf(a)), c);
    }
    if (w.id == selectedId) {
      sel = &w;
      if (!settled) g.drawCircle((int)x, (int)y, 6, UI::SelText);
    }
  }
  if (drift.on) {
    // The emptiest side of the bowl, easing round as the wells move; it bobs less as it settles.
    static float angle = 0;
    static bool placed = false;
    static float reach = 26;
    float want = (drift.hand & 1023) / 1024.0f * 6.2832f, wantR = 26, best = -1;
    for (int k = 0; k < 32 && nw; k++)
      for (int rr = 14; rr <= 36; rr += 4) {
        const float a = k * 6.2832f / 32, px = Cx + rr * sinf(a), py = Cy - rr * cosf(a);
        float room = 1e9f;
        for (int i = 0; i < nw; i++) {
          const float d = (wx[i] - px) * (wx[i] - px) + (wy[i] - py) * (wy[i] - py);
          if (d < room) room = d;
        }
        if (room > best) { best = room; want = a; wantR = (float)rr; }
      }
    angle = placed ? angle + Motion::wrap(want - angle) * 0.05f : want;
    reach = placed ? reach + (wantR - reach) * 0.05f : wantR;
    placed = true;
    const float loose = 1 - drift.drawn;
    const float a = angle + 0.15f * sinf(t * 0.27f) * loose, r = reach + 2 * sinf(t * 0.5f) * loose;
    float x = Cx + r * sinf(a), y = Cy - r * cosf(a);
    x += (m.tiltX() * 8 + rough * 2.5f * sinf(t * 5.3f)) * loose;
    y += (m.tiltY() * 6 + rough * 2.5f * cosf(t * 4.7f)) * loose;
    drawDrift(g, (int)x, (int)y);
  }

  // Header: the ward on the left, the chosen well on the right.
  const WardKind wk = sky.ward();
  char b[24];
  UI::text(g, 3, 3, wardName(wk), UI::wardColor(wk));
  snprintf(b, sizeof(b), "%d", sky.near());
  UI::text(g, 3, 13, b, UI::Text);
  if (settled) {
    UI::textRight(g, UI::W - 3, 3, "ADRIFT", Strays::Chalk);
  } else if (sel) {
    UI::textRight(g, UI::W - 3, 3, sel->name, UI::SelText);
    const UI::Reach r = UI::reachOf(sel->sig);
    UI::textRight(g, UI::W - 3, 13, pinned(*sel, st.nowMs) ? "DOWSED" : UI::reachName(r),
                  pinned(*sel, st.nowMs) ? UI::energyColor(sel->energy) : UI::reachColor(r));
  }
  if (st.gyro && clear < 1 && rough > 0.4f) UI::text(g, 3, 108, "HOLD STILL", UI::Dim);

  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "LIST", UI::Text);
  UI::keyHint(g, x, "ENTER", settled ? "KEEP" : "OPEN", UI::Text);
  UI::battery(g, st.battery);
}

} // namespace GlassScreen
