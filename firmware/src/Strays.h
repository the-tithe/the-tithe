#pragma once
#include <stdint.h>
#include <string.h>

// A drifting glyph: one of the six glyph shapes, drawn a little wrong in a
// stranger's hand. Each shape is strokes on a 7x7 grid, in the order the
// gesture makes them; a hand (12 bits) slants the whole mark, shifts each
// stroke a pixel and leaves dry spots in the chalk. Pure logic: it draws
// into a Mask, which the glass and the FOUND tab put on screen, and which
// tests can check pixel by pixel.
//
// Rules every mark keeps, whatever the hand: only these six shapes, all one
// chalk white (Minted too), and nothing small and loose above the shape.
// Crumbs only ever fall below it, as Deep's do. valid() checks it.

namespace Strays {

constexpr int Shapes = 6;      // in Cat::Glyph order: Minted, Ringing, Echoing, Deep, Bound, Beacon
constexpr int Hands = 4096;    // every hand there is
constexpr uint16_t HandMask = Hands - 1;

// The one colour of every mark, and its dry spots: neutral chalk, never yellow.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t Chalk = rgb565(228, 230, 236);
constexpr uint16_t ChalkDry = rgb565(150, 152, 158);

// Strokes as cell pairs "xy", strokes split by '|'.
inline const char* const Strokes[Shapes] = {
  // Minted: pressing. The whorl from the middle out.
  "3243342332|3040516263645546362615040302112030",
  // Ringing: two prongs, then the stem struck down last.
  "0001021323|4041423323|23242526",
  // Echoing: a stroke, then the same stroke back.
  "0111213141516150|6353433323130314",
  // Deep: a shaft pressed down hard.
  "00010203040516|40414243444536|162636",
  // Bound: one line from the tail, never lifted.
  "6564534332231302011020314050616253",
  // Beacon: the ring, then its four ticks.
  "21314152535445352514131221|3130|5363|3536|1303",
};

struct Mask {
  static constexpr int W = 32, H = 32, O = 4;   // O: the grid's top-left inside the mask
  // 0 empty; else the step of the gesture that drew it (1..127), +128 for a dry spot.
  uint8_t px[H][W];
  int steps;   // the last step, crumbs included
  static int step(uint8_t v) { return v & 0x7F; }
  static bool dry(uint8_t v) { return v & 0x80; }
};

inline uint32_t mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352Du;
  x ^= x >> 15;
  x *= 0x846CA68Bu;
  x ^= x >> 16;
  return x;
}

// What a hand does: how far the mark leans, and each stroke's shift.
inline int slantOf(uint16_t hand) { return (int)(hand % 5) - 2; }
inline int lean(int slant, int y, int pitch) {   // px the row at grid y moves right
  const int v = slant * pitch * (3 - y);
  return v >= 0 ? (v + 4) / 9 : -((-v + 4) / 9);
}

inline void stamp(Mask& m, int x, int y, int size, uint8_t step) {
  for (int dy = 0; dy < size; dy++)
    for (int dx = 0; dx < size; dx++) {
      const int px = x + dx, py = y + dy;
      if (px < 0 || py < 0 || px >= Mask::W || py >= Mask::H) continue;
      if (!m.px[py][px]) m.px[py][px] = step;
    }
}

// A line of stamps from one cell to the next, so a stroke never has a gap.
inline void line(Mask& m, int x0, int y0, int x1, int y1, int size, uint8_t step) {
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
  const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  for (;;) {
    stamp(m, x0, y0, size, step);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 < dx) { err += dx; y0 += sy; }
  }
}

// Draw `shape` in `hand` at `pitch` px per grid cell (3 in the glass, 2 for
// the Grimoire). `shift` false draws every stroke where the shape has it.
inline void render(int shape, uint16_t hand, int pitch, Mask& m, bool shift = true) {
  memset(&m, 0, sizeof(m));
  hand &= HandMask;
  const int size = pitch - 1, slant = slantOf(hand);
  const char* s = Strokes[shape % Shapes];
  int stroke = 0, step = 0, px = 0, py = 0;
  bool first = true;
  int wx = 0, wy = 0;
  for (; *s; s++) {
    if (*s == '|') { stroke++; first = true; continue; }
    if (first) {   // each stroke shifts by 0 or 1 px each way, so strokes always still touch
      const uint32_t h = mix(hand * 31u + (uint32_t)stroke * 977u + 11u);
      wx = shift ? (int)(h & 1) : 0;
      wy = shift ? (int)((h >> 1) & 1) : 0;
    }
    const int cx = s[0] - '0', cy = s[1] - '0';
    s++;
    const int x = Mask::O + cx * pitch + lean(slant, cy, pitch) + wx, y = Mask::O + cy * pitch + wy;
    step++;
    if (first) stamp(m, x, y, size, (uint8_t)step);
    else line(m, px, py, x, y, size, (uint8_t)step);
    px = x;
    py = y;
    first = false;
  }
  // Dry spots: chalk skipping here and there, a little more in some hands.
  const uint32_t dryness = 5 + (hand >> 9);   // one pixel in 5 to 12
  for (int y = 0; y < Mask::H; y++)
    for (int x = 0; x < Mask::W; x++)
      if (m.px[y][x] && mix((uint32_t)(y * Mask::W + x) * 2654435761u ^ hand) % dryness == 0) m.px[y][x] |= 0x80;
  // Deep's crumbs: two or three bits of chalk that snapped off, fallen below it.
  if (shape == 3) {
    int bottom = 0, left = Mask::W, right = 0;
    for (int y = 0; y < Mask::H; y++)
      for (int x = 0; x < Mask::W; x++)
        if (m.px[y][x]) { bottom = y; if (x < left) left = x; if (x > right) right = x; }
    const int n = 2 + (int)((hand >> 3) & 1);
    const int span = right - left + 3;
    for (int i = 0; i < n; i++) {
      const uint32_t h = mix(hand * 7u + (uint32_t)i * 131u + 5u);
      const int x = left - 1 + (int)(h % (uint32_t)span), y = bottom + 2 + (int)((h >> 8) % 3);
      if (x >= 0 && x < Mask::W && y < Mask::H) m.px[y][x] = (uint8_t)(step + 1 + i);
    }
    step += n;
  }
  m.steps = step;
}

// The rule at the top of this file, for one mask. Small pieces (no
// bigger than two cells each way) are crumbs: each must sit wholly below
// every bigger piece of the mark. There must be at least one bigger piece.
inline bool valid(const Mask& m, int pitch) {
  static uint8_t seen[Mask::H][Mask::W];
  memset(seen, 0, sizeof(seen));
  struct Piece { int top, bottom, left, right; };
  Piece pieces[24];
  int n = 0;
  static int16_t stack[Mask::W * Mask::H];
  for (int y0 = 0; y0 < Mask::H; y0++)
    for (int x0 = 0; x0 < Mask::W; x0++) {
      if (!m.px[y0][x0] || seen[y0][x0]) continue;
      if (n == 24) return false;
      Piece p{y0, y0, x0, x0};
      int sp = 0;
      stack[sp++] = (int16_t)(y0 * Mask::W + x0);
      seen[y0][x0] = 1;
      while (sp) {
        const int k = stack[--sp], x = k % Mask::W, y = k / Mask::W;
        if (y < p.top) p.top = y;
        if (y > p.bottom) p.bottom = y;
        if (x < p.left) p.left = x;
        if (x > p.right) p.right = x;
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= Mask::W || ny >= Mask::H || seen[ny][nx] || !m.px[ny][nx]) continue;
            seen[ny][nx] = 1;
            stack[sp++] = (int16_t)(ny * Mask::W + nx);
          }
      }
      pieces[n++] = p;
    }
  auto small = [&](const Piece& p) { return p.right - p.left < 2 * pitch && p.bottom - p.top < 2 * pitch; };
  int lowest = -1;
  for (int i = 0; i < n; i++) if (!small(pieces[i]) && pieces[i].bottom > lowest) lowest = pieces[i].bottom;
  if (lowest < 0) return false;
  for (int i = 0; i < n; i++) if (small(pieces[i]) && pieces[i].top <= lowest) return false;
  return true;
}

// A mark as it's shown: the hand's version, or, if that ever broke the rule,
// the same shape with its strokes where they belong.
inline void make(int shape, uint16_t hand, int pitch, Mask& m) {
  render(shape, hand, pitch, m);
  if (!valid(m, pitch)) render(shape, hand, pitch, m, false);
}

} // namespace Strays
