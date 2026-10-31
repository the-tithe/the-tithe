#pragma once
#include "../Strays.h"
#include "AppState.h"
#include "Finds.h"
#include "Walks.h"

// The Grimoire's FOUND tab: what your walks turn up (Finds.h). A short list
// of sections; ENTER opens one, ESC comes back to the list.
//
//   MARKS      the drifting glyphs you kept, as they were drawn
//   WELLS      the field guide: odd kinds of well, blank until met
//   NAMESAKES  wells with the true name of one you'd worked
//   WORDS      the words true names are made of, one column a page
//   TOKENS     the token that came with each dawn's coin, by kind
//   WALKS      the ribbons of your last seven walks

namespace App {

enum FoundSection : uint8_t { FS_Marks, FS_Wells, FS_Namesakes, FS_Words, FS_Tokens, FS_Walks, FS_Count };
inline const char* const FoundName[FS_Count] = {"MARKS", "ODD WELLS", "NAMESAKES", "WORDS", "TOKENS", "WALKS"};
// One line on each, under the list.
inline const char* const FoundAbout[FS_Count] = {
  "GLYPHS YOU KEPT FROM THE GLASS", "ODD KINDS OF WELL, DRAWN WHEN MET", "WELLS NAMED LIKE ONE YOU'VE WORKED",
  "WHAT TRUE NAMES ARE MADE OF", "WHAT CAME WITH EACH DAWN'S COIN", "YOUR LAST SEVEN WALKS, AS RIBBONS",
};
inline int foundOpen = -1;   // the section open, -1 the list
inline int foundSel = 0;     // the cursor inside it (the page, on WORDS)

inline const char* foundTitle() { return foundOpen < 0 ? "FOUND" : FoundName[foundOpen]; }

inline int guideCount() { return Game::bits(game.moments.guideGot); }
inline int wordsCount() {
  int k = 0;
  for (int col = 0; col < WordColumns; col++) k += wordsHeard(col);
  return k;
}
inline int tokensKept() {
  int n = 0;
  for (int k = 0; k < Game::TokenCount; k++) n += game.moments.tokens[k];
  return n;
}
inline int ribbonsDrawn();

inline uint16_t newestMark(int i) { return (uint16_t)(game.moments.nMarks - 1 - i); }   // newest first

// ---- The field guide's drawings -------------------------------------------------
// Small pixel pictures, one per kind, drawn 1 px a cell.

struct Icon { uint8_t w, h; const char* rows[7]; };
inline const Icon GuideIcon[G_Count] = {
  {5, 7, {"#...#", ".#.#.", ".#.#.", "#...#", "..#..", "..#..", "..#.."}},                      // a radio on a pole
  {17, 7, {"#...#.#...#.#...#", ".#.#...#.#...#.#.", ".#.#...#.#...#.#.", "#...#.#...#.#...#",
           "..#.....#.....#..", "..#.....#.....#..", "..#.....#.....#.."}},                       // three of them
  {13, 5, {".###.....###.", "#...#...#...#", "#...#####...#", "#...#...#...#", ".###.....###."}},   // two rings, one stroke
  {13, 5, {".###.###.###.", "#...#...#...#", "#...#...#...#", "#...#...#...#", ".###.###.###."}},   // rings in a chain
  {19, 7, {".......#####.......", "#...#.#.....#.#...#", ".#.#..#.###.#..#.#.", "#####.#.#.#.#.#####",
           ".#.#..#.###.#..#.#.", "#...#.#.....#.#...#", ".......#####......."}},                     // locked among sparks
  {9, 7, {"#.#.#.#.#", ".#.#.#.#.", "#.#.#.#.#", ".#.#.#.#.", "#.#.#.#.#", ".#.#.#.#.", "#########"}},  // a crowd in a corner
  {7, 7, {"..###..", ".#...#.", "#..#..#", "#.###.#", "#..#..#", ".#...#.", "..###.."}},          // a cold ring
  {7, 7, {"...#...", ".#.#.#.", "#..#..#", "#.....#", "#.....#", ".#...#.", "..###.."}},          // a switch left on
};
inline uint16_t guideColor(int k) {
  switch (k) {
    case G_Old: case G_Row: case G_LeftOn: return UI::EnergyColor[(int)Energy::Static];
    case G_Locked: return UI::FactionColor[(int)Faction::Warded];
    case G_Chorus: return UI::FactionColor[(int)Faction::Syndicate];
    case G_Crowded: return UI::WardColor[(int)WardKind::Nexus];
    case G_Cold: return UI::EnergyColor[(int)Energy::Hush];
    default: return UI::Accent;
  }
}
inline void drawIcon(Canvas& g, int x, int y, const Icon& ic, uint16_t c) {
  for (int r = 0; r < ic.h; r++)
    for (int k = 0; k < ic.w; k++)
      if (ic.rows[r][k] == '#') g.drawPixel(x + k, y + r, c);
}

// A kept glyph, its middle at x,y, 2 px a cell.
inline void drawMark(Canvas& g, int x, int y, const Game::Mark& mk) {
  static Strays::Mask m;
  Strays::make(mk.shape, mk.hand, 2, m);
  int left = Strays::Mask::W, right = 0, top = Strays::Mask::H, bottom = 0;
  for (int py = 0; py < Strays::Mask::H; py++)
    for (int px = 0; px < Strays::Mask::W; px++)
      if (m.px[py][px]) {
        if (px < left) left = px;
        if (px > right) right = px;
        if (py < top) top = py;
        if (py > bottom) bottom = py;
      }
  const int ox = x - (left + right) / 2, oy = y - (top + bottom) / 2;
  for (int py = top; py <= bottom; py++)
    for (int px = left; px <= right; px++)
      if (m.px[py][px]) g.drawPixel(ox + px, oy + py, Strays::Mask::dry(m.px[py][px]) ? Strays::ChalkDry : Strays::Chalk);
}

inline void trueName(uint16_t packed, char out[17]) {
  if (!packed) { out[0] = 0; return; }
  WellRules::nameOf(Game::nameSeed(packed), (Faction)Game::nameFaction(packed), out);
}

// ---- The list --------------------------------------------------------------------

inline void foundValue(int s, char* b, int cap) {
  const Game::Moments& m = game.moments;
  int v = 0;
  switch (s) {
    case FS_Marks: v = m.nMarks; snprintf(b, cap, "%d", v); break;
    case FS_Wells: v = guideCount(); snprintf(b, cap, "%d OF %d", v, (int)G_Count); break;
    case FS_Namesakes: v = m.nNamesakes; snprintf(b, cap, "%d", v); break;
    case FS_Tokens: v = tokensKept(); snprintf(b, cap, "%d", v); break;
    case FS_Walks: v = ribbonsDrawn(); snprintf(b, cap, "%d", v); break;
    default: v = wordsCount(); snprintf(b, cap, "%d OF %d", v, WordsFirst + 4 * WordsSecond); break;
  }
  if (v == 0) snprintf(b, cap, "-");
}

inline void drawFoundList(Canvas& g) {
  char b[24];
  for (int i = 0; i < FS_Count; i++) {
    const int y = 17 + i * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    UI::text(g, 6, y, FoundName[i], i == sel ? UI::SelText : UI::Text);
    foundValue(i, b, sizeof(b));
    UI::textRight(g, UI::W - 4, y, b, b[0] == '-' ? UI::Dim : UI::Accent);
  }
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  UI::text(g, 4, 103, FoundAbout[sel], UI::Dim);
}

// ---- MARKS: a grid of every glyph kept, newest first -------------------------------

constexpr int MarkCols = 8, MarkRows = 3, MarkW = 30, MarkH = 27, MarkY0 = 15;

inline void drawMarks(Canvas& g) {
  const Game::Moments& m = game.moments;
  bool shape[Strays::Shapes] = {false};
  int shapes = 0;
  for (int i = 0; i < m.nMarks; i++) if (!shape[m.marks[i].shape]) { shape[m.marks[i].shape] = true; shapes++; }
  char b[40];
  snprintf(b, sizeof(b), "%d OF %d SHAPES", shapes, Strays::Shapes);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  if (m.nMarks == 0) { UI::text(g, 6, 17, "NOTHING YET", UI::Dim); return; }
  if (foundSel >= m.nMarks) foundSel = m.nMarks - 1;
  for (int i = 0; i < m.nMarks && i < MarkCols * MarkRows; i++) {
    const int x = (i % MarkCols) * MarkW, y = MarkY0 + (i / MarkCols) * MarkH;
    if (i == foundSel) g.fillRect(x, y, MarkW, MarkH, UI::SelBg);
    drawMark(g, x + MarkW / 2, y + MarkH / 2, m.marks[newestMark(i)]);
  }
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  const Game::Mark& k = m.marks[newestMark(foundSel)];
  char name[16];
  upper(Cat::Glyphs[k.shape].name, name, sizeof(name));
  snprintf(b, sizeof(b), "%s, ADRIFT", name);   // its shape is a glyph's, but nobody carved it
  UI::text(g, 4, 103, b, UI::Text);
  snprintf(b, sizeof(b), "WEEK %u, NIGHT %u", (unsigned)k.week, (unsigned)k.night + 1);
  UI::textRight(g, UI::W - 4, 103, b, UI::Dim);
}

// ---- WELLS: the field guide ------------------------------------------------------

inline void drawGuide(Canvas& g) {
  const Game::Moments& m = game.moments;
  char b[24];
  snprintf(b, sizeof(b), "%d OF %d", guideCount(), (int)G_Count);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  for (int k = 0; k < G_Count; k++) {
    const int y = 17 + k * 10;
    if (k == foundSel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    if (guideMet(k)) {
      const Icon& ic = GuideIcon[k];
      drawIcon(g, 4, y + (7 - ic.h) / 2, ic, guideColor(k));
      UI::text(g, 28, y, GuideName[k], k == foundSel ? UI::SelText : UI::Text);
    } else {   // a blank where its drawing will go
      for (int i = 0; i < 7; i += 2) {
        g.drawPixel(4 + i, y, UI::Faint);
        g.drawPixel(4 + i, y + 6, UI::Faint);
        g.drawPixel(4, y + i, UI::Faint);
        g.drawPixel(10, y + i, UI::Faint);
      }
      UI::text(g, 28, y, "? ? ? ? ?", k == foundSel ? UI::Dim : UI::Faint);
    }
  }
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  if (!guideMet(foundSel)) return;
  char n[17];
  UI::text(g, 4, 103, GuideNote[foundSel], UI::Text);
  trueName(m.guide[foundSel], n);
  UI::text(g, 4, 113, n, UI::Dim);
}

// ---- NAMESAKES: newest first ------------------------------------------------------

constexpr int NamesakeRows = 10;

inline void drawNamesakes(Canvas& g) {
  const Game::Moments& m = game.moments;
  char b[24], n[17];
  snprintf(b, sizeof(b), "%d", (int)m.nNamesakes);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  if (m.nNamesakes == 0) { UI::text(g, 6, 17, "NOTHING YET", UI::Dim); return; }
  if (foundSel >= m.nNamesakes) foundSel = m.nNamesakes - 1;
  if (foundSel < top) top = foundSel;
  if (foundSel >= top + NamesakeRows) top = foundSel - NamesakeRows + 1;
  for (int i = top; i < m.nNamesakes && i < top + NamesakeRows; i++) {
    const int y = 17 + (i - top) * 10;
    const Game::Namesake& k = m.namesakes[m.nNamesakes - 1 - i];
    if (i == foundSel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    trueName(k.name, n);
    UI::text(g, 6, y, n, i == foundSel ? UI::SelText : UI::Text);
    snprintf(b, sizeof(b), "WEEK %u", (unsigned)k.week);
    UI::textRight(g, UI::W - 4, y, b, UI::Dim);
  }
  if (top > 0) g.fillTriangle(UI::W - 3, 15, UI::W - 5, 17, UI::W - 1, 17, UI::Dim);
  if (top + NamesakeRows < m.nNamesakes) g.fillTriangle(UI::W - 3, 117, UI::W - 5, 115, UI::W - 1, 115, UI::Dim);
}

// ---- WORDS: each blank shows only how long its word is --------------------------
// Six pages: the 64 first words in two halves, then each faction's second words.

constexpr int WordPages = WordColumns + 1;
inline int wordPageColumn(int page) { return page <= 1 ? 0 : page - 1; }
inline int wordPageFrom(int page) { return page == 1 ? WordsFirst / 2 : 0; }
inline int wordPageCount(int page) { return page <= 1 ? WordsFirst / 2 : WordsSecond; }

inline const char* wordPageName(int page) {
  static const char* const k[WordPages] = {"FIRST 1 OF 2", "FIRST 2 OF 2", "HEDGE", "HEARTH", "WARDED", "SYNDICATE"};
  return k[page];
}

inline void drawWords(Canvas& g) {
  const int page = foundSel, col = wordPageColumn(page), from = wordPageFrom(page), n = wordPageCount(page);
  const int got = wordsHeard(col), all = wordsIn(col);
  char b[32];
  snprintf(b, sizeof(b), "%s  %d OF %d", wordPageName(page), got, all);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  if (got == all) g.drawFastHLine(UI::W - 3 - UI::textW(b), 11, UI::textW(b), UI::Accent);   // a full column: a line under it
  // Four columns of eight, a word a row.
  const int down = 8, across = 60, rowH = 10;
  for (int k = 0; k < n; k++) {
    const int i = from + k;
    const int x = 3 + (k / down) * across, y = 17 + (k % down) * rowH;
    const char* w = wordText(col, i);
    if (wordHeard(col, i)) {
      UI::text(g, x, y, w, UI::Text);
    } else {
      char dash[12];
      const int len = (int)strlen(w);
      for (int k = 0; k < len && k < 11; k++) dash[k] = '-';
      dash[len < 11 ? len : 11] = 0;
      UI::text(g, x, y, dash, UI::Faint);
    }
  }
}

// ---- TOKENS: a sock of every dawn's token, by kind --------------------------------

constexpr int TokenCols = 5, TokenW = 48, TokenH = 41, TokenY0 = 15;

inline void drawTokens(Canvas& g) {
  const Game::Moments& m = game.moments;
  char b[32];
  snprintf(b, sizeof(b), "%d", tokensKept());
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  for (int k = 0; k < Game::TokenCount; k++) {
    const int x = (k % TokenCols) * TokenW, y = TokenY0 + (k / TokenCols) * TokenH;
    if (k == foundSel) g.fillRect(x, y, TokenW, TokenH, UI::SelBg);
    if (m.tokens[k]) {
      drawToken(g, x + (TokenW - 18) / 2, y + 5, k, 2);
      snprintf(b, sizeof(b), "%u", (unsigned)m.tokens[k]);
      UI::text(g, x + (TokenW - UI::textW(b)) / 2, y + 28, b, k == foundSel ? UI::SelText : UI::Text);
    } else {
      g.drawCircle(x + TokenW / 2, y + 13, 8, UI::Faint);
    }
  }
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  if (m.tokens[foundSel]) UI::text(g, 4, 103, TokenName[foundSel], UI::Text);
  else UI::text(g, 4, 103, "? ? ? ? ?", UI::Faint);
}

// ---- WALKS: the ribbons of the last seven walks, newest first -----------------------

constexpr int RibbonX = 22, RibbonW = 192, RibbonY0 = 19, RibbonPitch = 13, RibbonH = 5;

inline const Game::Ribbon& ribbonAt(int i) {   // 0 the newest
  const Game::Moments& m = game.moments;
  return m.ribbons[(m.nextRibbon + 2 * Game::Moments::MaxRibbons - 1 - i) % Game::Moments::MaxRibbons];
}
// Walks that drew something: a walk too short for a sliver kept its place but has nothing to show.
inline int ribbonsDrawn() {
  int n = 0;
  for (int i = 0; i < game.moments.nRibbons; i++) n += ribbonAt(i).len > 0;
  return n;
}

// One walk: each sliver in its ward's colour, brighter where many new wells
// were, a tick above where you offered, a fleck below where a Hush well came
// up, and at its end five dots if all five energies sang together.
inline void drawRibbon(Canvas& g, int y, const Game::Ribbon& r) {
  if (!r.len) return;
  for (int x = 0; x < RibbonW; x++) {
    const uint8_t v = r.s[r.at(x, RibbonW)];
    const float bright = 0.45f + 0.55f * (Game::Ribbon::fresh(v) > 6 ? 1 : Game::Ribbon::fresh(v) / 6.0f);
    g.drawFastVLine(RibbonX + x, y, RibbonH, UI::fade(UI::WardColor[Game::Ribbon::ward(v)], bright));
  }
  for (int i = 0; i < r.len; i++) {   // one mark per sliver, in its middle
    const int x = RibbonX + (2 * i + 1) * RibbonW / (2 * r.len);
    if (r.s[i] & Game::Ribbon::Offer) g.drawFastVLine(x, y - 3, 2, UI::Text);
    if (r.s[i] & Game::Ribbon::Hush) g.drawPixel(x, y + RibbonH + 1, UI::EnergyColor[(int)Energy::Hush]);
  }
  if (r.marks & Game::Ribbon::MarkFive) {
    static const int up[5] = {3, 1, 0, 2, 4};   // as the chord rises
    for (int e = 0; e < 5; e++) g.fillRect(RibbonX + RibbonW + 4 + e * 4, y + 1, 2, 3, UI::EnergyColor[up[e]]);
  }
}

inline void drawWalks(Canvas& g) {
  const Game::Moments& m = game.moments;
  char b[8];
  // What the marks mean: a tick where you offered, a fleck where a Hush well came up.
  int lx = UI::W - 3 - UI::textW("HUSH");
  UI::text(g, lx, 3, "HUSH", UI::Dim);
  g.drawPixel(lx - 4, 7, UI::EnergyColor[(int)Energy::Hush]);
  lx -= 12 + UI::textW("OFFER");
  UI::text(g, lx, 3, "OFFER", UI::Dim);
  g.drawFastVLine(lx - 4, 4, 2, UI::Text);
  if (ribbonsDrawn() == 0) { UI::text(g, 6, 17, "NOTHING YET", UI::Dim); return; }
  int row = 0;
  for (int i = 0; i < m.nRibbons; i++) {
    const Game::Ribbon& r = ribbonAt(i);
    if (!r.len) continue;
    const int y = RibbonY0 + row++ * RibbonPitch;
    if (r.night) {   // the Night it was walked on
      snprintf(b, sizeof(b), "N%d", r.night);
      UI::text(g, 4, y - 1, b, UI::Dim);
    }
    drawRibbon(g, y, r);
  }
  int x = 4;
  for (int w = 0; w < WardCount; w++) {
    UI::text(g, x, 112, wardName((WardKind)w), UI::WardColor[w]);
    x += UI::textW(wardName((WardKind)w)) + UI::CharW;
  }
}

// ---- Keys and drawing -------------------------------------------------------------

// Keys on the FOUND tab. True when FOUND used the press; left and right on the
// list, and ESC there, are the Grimoire's.
inline bool updateFound(const Press& p) {
  const Game::Moments& m = game.moments;
  if (foundOpen < 0) {
    if (p.down && sel < FS_Count - 1) { sel++; return true; }
    if (p.up && sel > 0) { sel--; return true; }
    if (p.enter) {
      foundOpen = sel;
      foundSel = 0;
      top = 0;
      if (sel == FS_Tokens && m.lastToken) foundSel = m.lastToken - 1;   // on the token the last dawn brought
      return true;
    }
    return p.down || p.up;
  }
  if (p.back) { sel = foundOpen; foundOpen = -1; top = 0; return true; }
  switch (foundOpen) {
    case FS_Marks: {
      const int n = m.nMarks < MarkCols * MarkRows ? m.nMarks : MarkCols * MarkRows;
      int to = foundSel + (p.right ? 1 : p.left ? -1 : p.down ? MarkCols : p.up ? -MarkCols : 0);
      if (to >= 0 && to < n) foundSel = to;
      break;
    }
    case FS_Wells:
      if (p.down && foundSel < G_Count - 1) foundSel++;
      if (p.up && foundSel > 0) foundSel--;
      break;
    case FS_Namesakes:
      if (p.down && foundSel < m.nNamesakes - 1) foundSel++;
      if (p.up && foundSel > 0) foundSel--;
      break;
    case FS_Tokens: {
      const int to = foundSel + (p.right ? 1 : p.left ? -1 : p.down ? TokenCols : p.up ? -TokenCols : 0);
      if (to >= 0 && to < Game::TokenCount) foundSel = to;
      break;
    }
    case FS_Walks: break;
    default:
      if (p.left || p.right) foundSel = (foundSel + (p.right ? 1 : WordPages - 1)) % WordPages;
      break;
  }
  return true;
}

inline void drawFound(Canvas& g) {
  switch (foundOpen) {
    case FS_Marks: drawMarks(g); break;
    case FS_Wells: drawGuide(g); break;
    case FS_Namesakes: drawNamesakes(g); break;
    case FS_Words: drawWords(g); break;
    case FS_Tokens: drawTokens(g); break;
    case FS_Walks: drawWalks(g); break;
    default: drawFoundList(g); break;
  }
}

// The footer's key hints. Returns false on the list, where the Grimoire's own apply.
inline bool foundFooter(Canvas& g) {
  if (foundOpen < 0) return false;
  const int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  if (foundOpen == FS_Words) UI::keyHint(g, x, "< >", wordPageName((foundSel + 1) % WordPages), UI::Text);
  return true;
}

} // namespace App
