#pragma once
#include "../ScannerScreen.h"
#include "../Tune.h"
#include "AppState.h"
#include "Finds.h"

// What the Deck makes of a whole walk, kept in the walks' block (game/Moments.h):
//
//   the recap   walk with the Deck put away, then take it out and hold it
//               still: a card says what you passed
//   the ribbon  each walk drawn as a thin ribbon in the colours of the wards
//               you passed, folded to the same width however long it was
//   the token   each dawn's coin comes with a token whose kind follows where
//               you offered that Night
//   stretches   wells first heard together; walk the same ones on three
//               Nights and the Deck asks if it's a street you keep
//
// Only wells that pass the fixed-well filter count or get named. Words wait
// until you stop and hold the Deck up to look.

namespace App {

// ---- The recap after a pocket walk -------------------------------------------------

constexpr float RecapAway = 5;           // seconds put away before footsteps count
constexpr float RecapWalk = 120;         // seconds walked put away, for a card
constexpr float RecapInUse = 20;         // held up this long while out: not a pocket walk any more
constexpr float RecapCardSecs = 12;

struct Recap {
  bool on;              // a pocket walk under way
  uint32_t fromScan;    // wells first heard after this scan were passed on it
  float pocketed;       // seconds walked with the Deck put away
  float viewed;         // seconds held up since
  uint8_t wards;        // bit per kind of ward passed through
  // The card, once you stop.
  bool card;
  float cardT;
  int passed;           // wells passed (fixed ones only)
  int fresh, hidden, wardKinds;   // of them never heard by this Deck (NEW), and hidden
  uint32_t loudId;      // the loudest well passed, 0 for none with a name
  bool loudNear;        // ...and still in range when the card came up: ENTER opens it
  char loudName[17];
};
inline Recap recap{};

// What a pocket walk passed: never-heard wells, hidden ones, and the loudest
// named one. Only wells that pass the filter count, so a phone that walked
// along, or one that passed you, is never on the card.
inline void recapCount(const Sightings& sky, uint32_t fromScan, Recap& out) {
  out.passed = out.fresh = out.hidden = 0;
  out.loudId = 0;
  out.loudNear = false;
  out.loudName[0] = 0;
  int loud = -128;
  for (int i = 0; i < sky.size(); i++) {
    const WellRec& w = sky.at(i);
    if (w.firstScan <= fromScan || !WellRules::fixedish(w)) continue;
    out.passed++;
    out.fresh += w.neverHeard;
    out.hidden += !w.sawVisible;
    if (w.sawVisible && w.peak > loud) {
      loud = w.peak;
      out.loudId = w.id;
      out.loudNear = sky.inRange(w);
      snprintf(out.loudName, sizeof(out.loudName), "%s", w.name);
    }
  }
}

inline bool recapScreen() { return mode == Mode::Scanner || mode == Mode::Glass || mode == Mode::Well; }

// Each frame. True when the card has just come up, with the loudest well's tune (`out`).
// "Stopped" means the Deck held still AND the footsteps over: the step counter
// runs on for about a second after the last step, so the card waits for it
// rather than coming up and going in the same breath.
inline bool tickRecap(const Ctx& c, float dt, Tune& out) {
  Recap& r = recap;
  if (!c.gyro) return false;
  const Motion& m = c.motion;
  if (r.card) {
    r.cardT += dt;
    if (r.cardT > RecapCardSecs || m.walking() || !recapScreen()) r.card = false;
    return false;
  }
  if (m.awayFor > RecapAway && m.walking()) {
    if (!r.on) r = Recap{true, c.sky.scans(), 0, 0, (uint8_t)(1u << (int)c.sky.ward())};
    r.pocketed += dt;
    r.viewed = 0;
  }
  if (!r.on || !m.viewing()) return false;
  r.viewed += dt;
  if (r.viewed > RecapInUse) { r.on = false; return false; }
  if (m.stillFor < HeldStill || m.walking() || toastT > 0 || !recapScreen()) return false;
  r.on = false;   // you've stopped: this walk's card, or none
  if (r.pocketed < RecapWalk) return false;
  recapCount(c.sky, r.fromScan, r);
  if (r.passed < 1) return false;
  r.wardKinds = Game::bits(r.wards);
  r.card = true;
  r.cardT = 0;
  if (const WellRec* w = r.loudId ? c.sky.byId(r.loudId) : nullptr) {
    const Game::AtlasEntry* a = game.atlas.find(w->id);
    out = Tunes::of(w->seed, (int)w->energy, a && a->visits >= 3);
    return true;
  }
  return false;
}

// The loudest well passed is still in range, so the card can open it.
inline bool recapCanOpen(const Ctx& c) {
  const WellRec* w = recap.loudId ? c.sky.byId(recap.loudId) : nullptr;
  return w && c.sky.inRange(*w);
}

// Keys while the card is up: ESC puts it away, ENTER opens the loudest well
// (or, with nothing to open, puts the card away too). Any other key is the
// screen's, and the card stays. True when the press was the card's.
inline bool recapPress(const Ctx& c, const Press& p) {
  if (!recap.card || !(p.back || p.enter)) return false;
  recap.card = false;
  if (p.enter && recapCanOpen(c)) {
    wellId = recap.loudId;
    ScannerScreen::selectedId = wellId;
    if (mode != Mode::Well) listMode = mode;
    go(Mode::Well);
  } else if (p.enter || p.back) {
    recap.card = false;
  }
  teachToast(Taught::Recap, "THE DECK KEPT LISTENING IN YOUR POCKET");
  return true;
}

inline void drawRecap(Canvas& g, const Ctx& c) {
  if (!recap.card) return;
  const Recap& r = recap;
  const bool canOpen = recapCanOpen(c);
  constexpr int X = 24, Y = 36, W = UI::W - 2 * X;
  const int H = r.loudId ? 46 : 34;
  g.fillRect(X, Y, W, H, UI::SelBg);
  g.drawRect(X, Y, W, H, UI::Sep);
  UI::textCenter(g, Y + 6, "WHILE YOU WALKED", UI::SelText);
  char b[40];
  snprintf(b, sizeof(b), "%d NEW  %d HIDDEN  %d WARD%s", r.fresh, r.hidden, r.wardKinds, r.wardKinds == 1 ? "" : "S");
  UI::textCenter(g, Y + 19, b, UI::Text);
  if (r.loudId) {
    snprintf(b, sizeof(b), "LOUDEST: %s", r.loudName);
    UI::textCenter(g, Y + 31, b, UI::Accent);
  }
  g.fillRect(0, UI::FooterSepY + 1, UI::W, UI::H - UI::FooterSepY - 1, UI::Bg);
  UI::footerLine(g);
  const int x = UI::keyHint(g, 2, "ESC", "CLOSE", UI::Text);
  if (canOpen) UI::keyHint(g, x, "ENTER", "OPEN IT", UI::Text);
  else if (r.loudId) UI::text(g, x, UI::FooterY, "OUT OF REACH", UI::Dim);
}

// ---- The ribbon of each walk --------------------------------------------------------

constexpr float RibbonStartWalk = 60;    // seconds of walking before a walk gets a ribbon
constexpr float RibbonIdle = 1200;       // twenty minutes without a step ends it
constexpr int RibbonNoGyroNew = 10;      // without a motion sensor: never-heard wells instead

struct RibbonNow {
  bool open;            // drawing the newest ribbon in game.moments
  float t;              // seconds into this sliver
  float idle;           // seconds since a footstep
  float walkAtEnd;      // Motion::walkTotal when the last one ended
  int newSoFar;         // never-heard wells this walk, for a Deck with no motion sensor
  uint16_t wards[4];    // scans in each kind of ward, this sliver
  uint8_t fresh;        // never-heard wells, this sliver
  bool offer, hush;
  bool five;            // all five energies sang together on this walk
};
inline RibbonNow ribbonNow{};

inline Game::Ribbon& ribbonOpen() {
  const Game::Moments& m = game.moments;
  return game.moments.ribbons[(m.nextRibbon + Game::Moments::MaxRibbons - 1) % Game::Moments::MaxRibbons];
}

inline void startRibbon() {
  Game::Moments& m = game.moments;
  m.ribbons[m.nextRibbon] = Game::Ribbon{};
  if (inRun() && game.phase == Game::Phase::Night) m.ribbons[m.nextRibbon].night = (uint8_t)(run().night + 1 > 255 ? 255 : run().night + 1);
  m.nextRibbon = (uint8_t)((m.nextRibbon + 1) % Game::Moments::MaxRibbons);
  if (m.nRibbons < Game::Moments::MaxRibbons) m.nRibbons++;
  ribbonNow.open = true;
  ribbonNow.t = ribbonNow.idle = 0;
  momentsDirty = true;
}

// This sliver's ward is the one it heard most.
inline void endSliver(const Ctx& c) {
  RibbonNow& n = ribbonNow;
  int w = (int)c.sky.ward();
  for (int k = 0, most = 0; k < 4; k++) if (n.wards[k] > most) { most = n.wards[k]; w = k; }
  Game::Ribbon& r = ribbonOpen();
  r.push(Game::Ribbon::sliver(w, n.fresh, n.offer, n.hush));
  if (n.five) r.marks |= Game::Ribbon::MarkFive;
  memset(n.wards, 0, sizeof(n.wards));
  n.fresh = 0;
  n.offer = n.hush = false;
  n.t = 0;
  momentsDirty = true;
}

// Each frame: time on a walk moves the ribbon on. With a motion sensor, only
// time near footsteps counts, and twenty minutes without one ends the walk.
inline void tickRibbon(const Ctx& c, float dt) {
  RibbonNow& n = ribbonNow;
  if (n.open && game.moments.nRibbons == 0) n.open = false;   // a new game
  if (!n.open) {
    const bool walked = c.gyro ? c.motion.walkTotal - n.walkAtEnd >= RibbonStartWalk : n.newSoFar >= RibbonNoGyroNew;
    if (!walked) return;
    startRibbon();
  }
  if (c.gyro) {
    n.idle = c.motion.walking() ? 0 : n.idle + dt;
    if (n.idle >= RibbonIdle) {
      if (n.t > 0) endSliver(c);
      n.open = n.five = false;
      n.walkAtEnd = c.motion.walkTotal;
      return;
    }
    if (c.motion.walkedRecently() <= 0) return;
  }
  n.t += dt;
  if (n.t * 1000 >= ribbonOpen().sliverMs()) endSliver(c);
}

// The Deck just woke: a new walk, and a new ribbon once you've walked a little.
inline void newRibbonWalk(const Ctx& c) {
  ribbonNow = RibbonNow{};
  ribbonNow.walkAtEnd = c.motion.walkTotal;
}

// ---- Dawn tokens ---------------------------------------------------------------------

inline const char* const TokenName[Game::TokenCount] = {
  "SUBWAY", "ARCADE", "LAUNDROMAT", "CAR WASH", "PARKING", "BUS", "FERRY", "NOT FROM HERE",
};

// Each token 9 px across: r its rim, b its face, m what's struck on it, . nothing.
struct TokenArt { uint16_t rim, face, mark; const char* rows[9]; };
inline const TokenArt Tokens[Game::TokenCount] = {
  {UI::rgb(235, 200, 100), UI::rgb(200, 160, 60), 0,   // subway: a Y cut through it
   {"..rrrrr..", ".rbbbbbr.", "rbb.b.bbr", "rbbb.bbbr", "rbbb.bbbr", "rbbb.bbbr", "rbbbbbbbr", ".rbbbbbr.", "..rrrrr.."}},
  {UI::rgb(240, 205, 110), UI::rgb(205, 165, 70), UI::rgb(90, 60, 20),   // arcade: a clown's face
   {"..rrrrr..", ".rbbbbbr.", "rbbbbbbbr", "rbbmbmbbr", "rbbbbbbbr", "rbmbbbmbr", "rbbmmmbbr", ".rbbbbbr.", "..rrrrr.."}},
  {UI::rgb(210, 215, 225), UI::rgb(160, 165, 175), UI::rgb(110, 115, 125),   // laundromat: a hole in a ring
   {"..rrrrr..", ".rbbbbbr.", "rbbbbbbbr", "rbbmmmbbr", "rbbm.mbbr", "rbbmmmbbr", "rbbbbbbbr", ".rbbbbbr.", "..rrrrr.."}},
  {UI::rgb(205, 215, 230), UI::rgb(150, 165, 180), UI::rgb(80, 150, 230),   // car wash: a drop
   {"..rrrrr..", ".rbbbbbr.", "rbbbmbbbr", "rbbmmmbbr", "rbmmmmmbr", "rbmmmmmbr", "rbbmmmbbr", ".rbbbbbr.", "..rrrrr.."}},
  {UI::rgb(220, 220, 225), UI::rgb(175, 175, 180), UI::rgb(60, 90, 170),   // parking: a P
   {"..rrrrr..", ".rbbbbbr.", "rbbmmmbbr", "rbbmbbmbr", "rbbmbbmbr", "rbbmmmbbr", "rbbmbbbbr", ".rbmbbbr.", "..rrrrr.."}},
  {UI::rgb(225, 190, 120), UI::rgb(185, 150, 85), UI::rgb(100, 70, 35),   // bus: a bus from the front
   {"..rrrrr..", ".rbbbbbr.", "rbmmmmmbr", "rbmbbbmbr", "rbmmmmmbr", "rbmbmbmbr", "rbmmmmmbr", ".rbmbmbr.", "..rrrrr.."}},
  {UI::rgb(210, 170, 110), UI::rgb(170, 130, 70), UI::rgb(90, 60, 30),   // ferry: a boat
   {"..rrrrr..", ".rbbbbbr.", "rbbbmbbbr", "rbbbmmbbr", "rbbbmbbbr", "rbmmmmmbr", "rbbmmmbbr", ".rbbbbbr.", "..rrrrr.."}},
  {UI::rgb(130, 195, 170), UI::rgb(90, 150, 130), UI::rgb(40, 80, 70),   // nobody knows: square, holed, green
   {".rrrrrrr.", "rbbbbbbbr", "rbmbbbmbr", "rbbbbbbbr", "rbbb.bbbr", "rbbbbbbbr", "rbmbbbmbr", "rbbbbbbbr", ".rrrrrrr."}},
};

// A token, its top left at x,y, `k` px a cell.
inline void drawToken(Canvas& g, int x, int y, int kind, int k) {
  const TokenArt& t = Tokens[kind];
  for (int r = 0; r < 9; r++)
    for (int i = 0; i < 9; i++) {
      const char ch = t.rows[r][i];
      if (ch == '.') continue;
      g.fillRect(x + i * k, y + r * k, k, k, ch == 'r' ? t.rim : ch == 'b' ? t.face : t.mark);
    }
}

// Tonight's tally, started fresh on each Night.
inline void tallyNow() {
  Game::Moments& m = game.moments;
  const Game::Run& r = run();
  if (m.tallyWeek == (uint16_t)r.runNo && m.tallyNight == (uint8_t)r.night) return;
  m.tallyWeek = (uint16_t)r.runNo;
  m.tallyNight = (uint8_t)r.night;
  memset(m.byFaction, 0, sizeof(m.byFaction));
  memset(m.byWard, 0, sizeof(m.byWard));
  memset(m.byEnergy, 0, sizeof(m.byEnergy));
}

// An offering: counted toward tonight's token, and ticked on the ribbon.
inline void noteOffering(int faction, int ward, int energy) {
  ribbonNow.offer = true;
  if (!inRun()) return;
  tallyNow();
  Game::Moments& m = game.moments;
  auto up = [](uint8_t& v) { if (v < 255) v++; };
  up(m.byFaction[faction & 3]);
  up(m.byWard[ward & 3]);
  up(m.byEnergy[energy % 5]);
  momentsDirty = true;
}

// The dawn just came: the token that turned up with its coin. The coin is the
// same as ever; this is only its picture.
inline void dawnToken() {
  Game::Moments& m = game.moments;
  const Game::Run& r = run();
  const int night = r.night - 1;   // endNight has moved on
  const bool counted = m.tallyWeek == (uint16_t)r.runNo && m.tallyNight == (uint8_t)night;
  static const uint8_t none[5] = {0};
  const uint32_t roll = Strays::mix(r.runNo * 0x9E3779B1u + (uint32_t)night * 0x85EBCA77u + Keys::deckTag());
  const int k = counted ? Game::dawnToken(m.byFaction, m.byWard, m.byEnergy, roll) : Game::dawnToken(none, none, none, roll);
  m.lastToken = (uint8_t)(k + 1);
  if (m.tokens[k] < 0xFFFF) m.tokens[k]++;
  momentsDirty = true;
}

// ---- Stretches you might keep ---------------------------------------------------------

constexpr uint32_t StretchWindowMs = 180000;   // wells first heard within three minutes
constexpr int StretchNew = 5;                  // of them never heard before, to keep it
constexpr int StretchAgain = 3;                // of them in one scan, to count as walked again

struct StretchNow {
  bool open;
  uint32_t fromMs;
  Game::Stretch s;
  int n, fresh;
};
inline StretchNow stretchNow{};

inline bool nightOn() { return inRun() && game.phase == Game::Phase::Night; }
inline bool streetsOpen() { return (game.career.pages >> 7) & 1; }   // the list opens with the page about wells

// Keep a stretch just walked: in an empty place, or over the one walked on
// the fewest Nights and, of those, the one first walked longest ago (its first
// Night's code is its age), so a street you keep coming back to is never the
// one pushed out.
inline void keepStretch(const Game::Stretch& s) {
  Game::Moments& m = game.moments;
  int at = 0;
  for (int i = 0; i < Game::Moments::MaxStretches; i++) {
    const Game::Stretch& a = m.stretches[i];
    if (!a.used()) { at = i; break; }
    const Game::Stretch& b = m.stretches[at];
    if (a.nNights < b.nNights || (a.nNights == b.nNights && a.nights[0] < b.nights[0])) at = i;
  }
  m.stretches[at] = s;
  momentsDirty = true;
}

// After each scan: new stretches, and old ones walked again. True with a sound
// to play (`out`): the Deck asks whether this is a street you keep.
inline bool noticeStretches(const Ctx& c, const WellRec* const* list, int n, Tune& out) {
  Game::Moments& m = game.moments;
  const uint32_t scan = c.sky.scans();
  if (!nightOn()) { stretchNow.open = false; return false; }
  const uint16_t tonight = Game::Stretch::nightCode(run().runNo, run().night);
  StretchNow& sn = stretchNow;
  // Wells first heard together.
  if (sn.open && (c.st.nowMs - sn.fromMs > StretchWindowMs || sn.n == Game::Stretch::Max)) {
    if (sn.fresh >= StretchNew) {
      sn.s.nNights = 0;
      sn.s.walk(tonight);
      keepStretch(sn.s);
    }
    sn.open = false;
  }
  for (int i = 0; i < n && scan > 1; i++) {
    const WellRec& w = *list[i];
    if (w.firstScan != scan || !WellRules::fixedish(w)) continue;
    if (!sn.open) sn = StretchNow{true, c.st.nowMs, Game::Stretch{}, 0, 0};
    if (sn.n < Game::Stretch::Max) {
      sn.s.ids[sn.n++] = Game::Stretch::low(w.id);
      sn.fresh += w.neverHeard;
    }
  }
  // Stretches walked again, on another Night.
  bool ask = false;
  for (int s = 0; s < Game::Moments::MaxStretches; s++) {
    Game::Stretch& st = m.stretches[s];
    if (!st.used() || (st.walkedOn(tonight) && (st.asked || st.nNights < 3))) continue;
    int here = 0;
    const WellRec* loud = nullptr;
    for (int i = 0; i < n; i++) {
      const WellRec& w = *list[i];
      if (w.lastScan != scan || !WellRules::fixedish(w) || !st.has(w.id)) continue;
      here++;
      if (w.sawVisible && (!loud || w.sig > loud->sig)) loud = &w;
    }
    if (here < StretchAgain) continue;
    if (st.walk(tonight)) momentsDirty = true;
    if (st.nNights < 3 || st.asked || !loud || !streetsOpen() || m.nStreets >= Game::Moments::MaxStreets || ask) continue;
    st.asked = 1;
    m.keepName = Game::packName(loud->seed, (int)loud->faction);
    holdNote("A STREET YOU KEEP?", UI::Accent);
    teachHeld(Taught::Street, "YOU'VE WALKED IT THREE NIGHTS. NAME IT", "IN GRIMOIRE > STREETS, ADD");
    out = Tunes::of(loud->seed, (int)loud->energy, true);   // its loudest well's tune, come home
    ask = true;
    momentsDirty = true;
  }
  return ask;
}

// ---- Each scan, and each frame ---------------------------------------------------------

// After each scan: what the ribbon and the recap hear.
inline bool walksAfterScan(const Ctx& c, const WellRec* const* list, int n, Tune& out) {
  const uint32_t scan = c.sky.scans();
  if (scan == 1) { newRibbonWalk(c); recap.on = recap.card = false; stretchNow.open = false; }
  int fresh = 0;
  bool hush = false;
  for (int i = 0; i < n; i++) {
    const WellRec& w = *list[i];
    if (w.lastScan != scan || !WellRules::fixedish(w)) continue;
    fresh += w.firstScan == scan && w.neverHeard;
    hush |= w.energy == Energy::Hush && c.st.hushReveal >= 1;
  }
  RibbonNow& rn = ribbonNow;
  rn.newSoFar += fresh;
  if (rn.open) {
    if (rn.wards[(int)c.sky.ward()] < 0xFFFF) rn.wards[(int)c.sky.ward()]++;
    rn.fresh = (uint8_t)(rn.fresh + fresh > 255 ? 255 : rn.fresh + fresh);
    rn.hush |= hush;
  }
  if (recap.on) recap.wards |= (uint8_t)(1u << (int)c.sky.ward());
  return noticeStretches(c, list, n, out);
}

} // namespace App
