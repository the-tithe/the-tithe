#pragma once
#include "Dowse.h"
#include "Motion.h"
#include "Press.h"
#include "Sightings.h"
#include "Status.h"
#include "UI.h"

// Dowsing one well: turn slowly in a circle, and the needle swings toward
// it. The dial turns with you, so "up" on the dial is always the way you
// face. Each spoke is one slice of the circle, longer where the well
// sounded stronger.

namespace DowseScreen {

constexpr int Cx = 62, Cy = 70, R = 46;

inline Dowse dowse;
inline uint32_t target = 0;
inline float heardAt = -10;   // seconds since start, for the reading flash

inline void begin(uint32_t id) {
  target = id;
  dowse.reset();
}

inline void sample(float heading, int rssi) { dowse.add(heading, rssi); }

// Returns true to leave. ENTER starts the circle again.
inline bool update(const Press& p) {
  if (p.enter) dowse.reset();
  return p.back;
}

// Where an absolute heading lands on the dial, given the way you face.
inline void dialPoint(float heading, float facing, float r, int& x, int& y) {
  const float a = Motion::wrap(heading - facing);
  x = Cx + (int)lroundf(r * sinf(a));
  y = Cy - (int)lroundf(r * cosf(a));
}

inline const char* directionWord(float rel) {
  const float d = fabsf(rel) / Motion::DegToRad;
  if (d < 25) return "AHEAD";
  if (d > 150) return "BEHIND";
  return rel > 0 ? "RIGHT" : "LEFT";
}

inline void draw(Canvas& g, const Sightings& sky, const Motion& m, const Status& st, float t) {
  g.fillSprite(UI::Bg);
  const WellRec* w = sky.byId(target);
  if (!st.gyro) {
    UI::textCenter(g, 52, "THIS DECK CAN'T DOWSE", UI::Text);
    UI::textCenter(g, 66, "IT HAS NO GYRO", UI::Dim);
    UI::footerLine(g);
    UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
    return;
  }
  const uint16_t ec = w ? UI::energyColor(w->energy) : UI::Accent;

  // The dial.
  g.drawCircle(Cx, Cy, R, UI::Sep);
  g.drawCircle(Cx, Cy, R + 2, UI::Faint);
  g.fillTriangle(Cx, Cy - R - 7, Cx - 4, Cy - R - 1, Cx + 4, Cy - R - 1, UI::Text);   // you face this way
  float lo = 0, hi = -200;
  for (int b = 0; b < Dowse::Bins; b++) {
    if (!dowse.has(b)) continue;
    const float a = dowse.average(b);
    if (lo == 0 || a < lo) lo = a;
    if (a > hi) hi = a;
  }
  for (int b = 0; b < Dowse::Bins; b++) {
    int x0, y0, x1, y1;
    const float h = Dowse::binCenter(b);
    if (!dowse.has(b)) {
      dialPoint(h, m.heading, 30, x0, y0);
      g.drawPixel(x0, y0, UI::Dim);
      continue;
    }
    const float f = hi > lo ? (dowse.average(b) - lo) / (hi - lo) : 0.5f;
    dialPoint(h, m.heading, 12, x0, y0);
    dialPoint(h, m.heading, 16 + 26 * f, x1, y1);
    g.drawLine(x0, y0, x1, y1, f > 0.5f ? ec : UI::dimmed(ec));
  }
  g.fillCircle(Cx, Cy, 3, UI::Text);

  // The needle, once the circle is done.
  const bool ready = dowse.ready();
  const float pull = dowse.contrast();
  const bool trusted = ready && pull >= 2.5f;
  if (trusted) {
    const float rel = Motion::wrap(dowse.bearing() - m.heading);
    const float wob = 0.04f * sinf(t * 9.0f);   // a dowsing rod never sits quite still
    int x, y, xl, yl, xr, yr;
    dialPoint(dowse.bearing() + wob, m.heading, R - 3, x, y);
    dialPoint(dowse.bearing() + wob - 0.35f, m.heading, 9, xl, yl);
    dialPoint(dowse.bearing() + wob + 0.35f, m.heading, 9, xr, yr);
    g.fillTriangle(x, y, xl, yl, xr, yr, ec);
    g.fillCircle(Cx, Cy, 3, ec);
    (void)rel;
  }

  // Right side: the well, then what to do or where it is.
  const int x0 = 124;
  if (w) {
    UI::energyGlyph(g, x0, 6, w->energy, ec);
    UI::text(g, x0 + 11, 6, w->name, UI::Text);
  }
  if (!ready) {
    UI::text(g, x0, 28, "TURN SLOWLY", UI::Text, 1);
    UI::text(g, x0, 39, "IN A CIRCLE", UI::Text, 1);
    char b[24];
    snprintf(b, sizeof(b), "%d OF %d SLICES", dowse.covered(), Dowse::Bins);
    UI::text(g, x0, 56, b, UI::Dim);
  } else if (!trusted) {
    UI::text(g, x0, 28, "NO PULL", UI::Text, 2);
    UI::text(g, x0, 50, "TRY AGAIN, OR", UI::Dim);
    UI::text(g, x0, 61, "MOVE CLOSER", UI::Dim);
  } else {
    const float rel = Motion::wrap(dowse.bearing() - m.heading);
    UI::text(g, x0, 26, directionWord(rel), ec, 2);
    char b[24];
    const int deg = (int)lroundf(fabsf(rel) / Motion::DegToRad);
    if (deg < 25) snprintf(b, sizeof(b), "STRAIGHT ON");
    else if (deg > 150) snprintf(b, sizeof(b), "TURN AROUND");
    else snprintf(b, sizeof(b), "%d DEG %s", deg, rel > 0 ? "RIGHT" : "LEFT");
    UI::text(g, x0, 46, b, UI::Text);
    UI::text(g, x0, 60, pull >= 6 ? "STRONG PULL" : "FAINT PULL", pull >= 6 ? UI::Accent : UI::Dim);
  }

  // Live signal, flashing as each reading comes in.
  if (dowse.samples() > 0) {
    char b[16];
    snprintf(b, sizeof(b), "%d dBm", dowse.last());
    const UI::Reach r = UI::reachOf((float)dowse.last());
    UI::signalBars(g, x0, 98, r, UI::reachColor(r));
    UI::text(g, x0 + 22, 91, b, t - heardAt < 0.15f ? UI::Text : UI::Dim);
  }

  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  UI::keyHint(g, x, "ENTER", "AGAIN", UI::Text);
}

} // namespace DowseScreen
