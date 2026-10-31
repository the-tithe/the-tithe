#pragma once
#include "UI.h"

// Short overlays for the Deck's gestures: a pulse ripple (flick), a ward
// sigil (twist) and the eye opening (picked up, or turned face-up). Each
// takes the seconds since it started and draws nothing once it's done.

namespace Effects {

constexpr float PulseLen = 0.9f;
constexpr float WardLen = 1.3f;
constexpr float EyeLen = 0.8f;

inline void pulse(Canvas& g, float t) {
  if (t < 0 || t > PulseLen) return;
  const float p = t / PulseLen;
  for (int k = 0; k < 3; k++) {
    const float q = p - k * 0.12f;
    if (q <= 0 || q >= 1) continue;
    const int r = 6 + (int)(q * 150);
    g.drawCircle(UI::W / 2, UI::H / 2, r, q < 0.5f ? UI::Accent : UI::dimmed(UI::Accent));
  }
}

// A six-pointed ward: two triangles in a ring, growing in and fading.
inline void ward(Canvas& g, float t) {
  if (t < 0 || t > WardLen) return;
  const float p = t / WardLen;
  const uint16_t c = p < 0.6f ? UI::FactionColor[2] : UI::dimmed(UI::FactionColor[2]);
  const int cx = UI::W / 2, cy = UI::H / 2;
  const float r = 18 + 30 * (p < 0.3f ? p / 0.3f : 1.0f);
  const float spin = p * 1.2f;
  g.drawCircle(cx, cy, (int)r + 4, c);
  for (int tri = 0; tri < 2; tri++) {
    float x[3], y[3];
    for (int k = 0; k < 3; k++) {
      const float a = spin + tri * 3.14159f + k * 2.0944f;
      x[k] = cx + r * sinf(a);
      y[k] = cy - r * cosf(a);
    }
    for (int k = 0; k < 3; k++) g.drawLine((int)x[k], (int)y[k], (int)x[(k + 1) % 3], (int)y[(k + 1) % 3], c);
  }
}

// The Deck's eye opening: an almond that widens, a pupil, then fades.
inline void eye(Canvas& g, float t) {
  if (t < 0 || t > EyeLen) return;
  const float p = t / EyeLen;
  const float open = p < 0.4f ? p / 0.4f : 1.0f;
  const uint16_t c = p < 0.7f ? UI::Accent : UI::dimmed(UI::Accent);
  const int cx = UI::W / 2, cy = UI::H / 2;
  const int h = 1 + (int)(16 * open);
  g.fillEllipse(cx, cy, 34, h, UI::Bg);
  g.drawEllipse(cx, cy, 34, h, c);
  if (open > 0.5f) g.fillCircle(cx, cy, (int)(9 * open), UI::EnergyColor[(int)Energy::Hush]);
}

// Small hexagon for the header while a ward holds.
// The Wardens' eye, for the header once Heat runs high.
inline void eyeIcon(Canvas& g, int cx, int cy, uint16_t c) {
  g.drawEllipse(cx, cy, 4, 2, c);
  g.fillRect(cx - 1, cy - 1, 2, 3, c);
}

inline void wardIcon(Canvas& g, int cx, int cy, uint16_t c) {
  for (int k = 0; k < 6; k++) {
    const float a0 = k * 1.0472f, a1 = (k + 1) * 1.0472f;
    g.drawLine(cx + (int)lroundf(4 * sinf(a0)), cy - (int)lroundf(4 * cosf(a0)),
               cx + (int)lroundf(4 * sinf(a1)), cy - (int)lroundf(4 * cosf(a1)), c);
  }
}

} // namespace Effects
