#pragma once
#include <stdio.h>
#include <string.h>
#include "../Motion.h"
#include "../Press.h"
#include "../Sightings.h"
#include "../Status.h"
#include "../UI.h"
#include "../game/Game.h"
#include "../game/Save.h"

// The game app's shared state: the save, which screen is open, and the
// bridge from what the scanner and motion sensor see to the rules in game/.
// No hardware here: the firmware passes in Hooks, and the screen renderer
// passes in hooks that do nothing, so every screen can be drawn on a computer.

namespace App {

enum class Mode : uint8_t {
  Boot, Scanner, Glass, Well, Dowse, Walk,          // the scanner's screens
  Visit, Drawing, Show, Spells,                     // at a well
  NightStart, NightEnd, OutOfVisits, Shop, ShopDrop, ShopAttune,
  Build, Menu, Bind, Grimoire, Page, Atlas, Won, Lost, Reveal,
  Premise,                                          // once, before the first Night ever
  ShopForget,                                       // full of spells or glyphs: let one go to buy
};

// What the app asks of the hardware.
struct Hooks {
  void (*tone)(int hz, int ms) = nullptr;
  void (*save)(bool now) = nullptr;           // write the game; `now` skips the debounce
  double (*listen)() = nullptr;               // 0..1, how loud it is (microphone)
  void (*focusChannel)(int ch) = nullptr;     // 0 = scan every channel
  void (*hurry)() = nullptr;
  void (*setPlace)(int place) = nullptr;
  void (*toggleRecording)() = nullptr;
  void (*setTheme)(int theme) = nullptr;
  void (*setMuted)(bool muted) = nullptr;
};

// What the app reads each frame.
struct Ctx {
  Sightings& sky;
  const Motion& motion;
  Status& st;
  bool gyro;
};

inline Game::Saved game;    // everything saved
inline Hooks hooks;
inline Mode mode = Mode::Boot;
inline Mode back = Mode::Scanner;   // where ESC returns to from a well or menu
inline Mode listMode = Mode::Scanner;
inline float modeT = 0;
inline uint32_t wellId = 0;         // the well open in Well / Visit / Dowse
inline int sel = 0;                 // the cursor on list screens
inline int top = 0;                 // first row shown on long lists
inline int tab = 0;                 // the tab on tabbed screens

// Toast: a short message over any screen.
inline char toastText[40] = "";
inline float toastT = 0;
inline uint16_t toastColor = 0;

inline void toast(const char* s, uint16_t c = 0, float secs = 1.6f) {
  strncpy(toastText, s, sizeof(toastText) - 1);
  toastText[sizeof(toastText) - 1] = 0;
  toastT = secs;
  toastColor = c ? c : UI::Text;
}

// Toasts that wait for the one on screen to end, two deep (a third drops the oldest waiting).
struct ToastNext { char text[40]; float secs; uint16_t color; };
inline ToastNext toastNext[2] = {};
inline int nToastNext = 0;

inline void toastAfter(const char* s, uint16_t c = 0, float secs = 1.6f) {
  if (toastT <= 0 && nToastNext == 0) { toast(s, c, secs); return; }
  if (nToastNext == 2) { toastNext[0] = toastNext[1]; nToastNext = 1; }
  ToastNext& n = toastNext[nToastNext++];
  strncpy(n.text, s, sizeof(n.text) - 1);
  n.text[sizeof(n.text) - 1] = 0;
  n.secs = secs;
  n.color = c;
}
inline void tickToast(float dt) {
  if (toastT > 0) toastT -= dt;
  if (toastT <= 0 && nToastNext > 0) {
    toast(toastNext[0].text, toastNext[0].color, toastNext[0].secs);
    toastNext[0] = toastNext[1];
    nToastNext--;
  }
}

inline void beep(int hz, int ms) { if (hooks.tone) hooks.tone(hz, ms); }
inline void save(bool now = false) { if (hooks.save) hooks.save(now); }

inline bool atlasEstDirty = true;   // the atlas's estimates want working out again (on opening it, and on a sort)

inline void go(Mode m) {
  mode = m;
  modeT = 0;
  sel = 0;
  top = 0;
  tab = 0;
  if (m == Mode::Atlas) atlasEstDirty = true;
}

inline bool inRun() { return game.run.active != 0; }
inline Game::Run& run() { return game.run; }

// ---- From the scanner to the rules -----------------------------------------------

inline Game::WellView viewOf(const WellRec& w, const Sightings& sky, uint32_t nowMs) {
  Game::WellView v{};
  v.id = w.id;
  v.seed = w.seed;
  v.energy = (uint8_t)w.energy;
  v.faction = (uint8_t)w.faction;
  v.channel = w.channel;
  v.hidden = w.energy == Energy::Hush;
  v.sig = sky.inRange(w) ? w.sig : -120;
  v.mesh = sky.meshOf(w);
  v.dowsed = w.dowsed && nowMs - w.dowsedMs < 600000;   // a dowse counts for 10 minutes
  return v;
}

// A remembered well for ECHO: what the atlas knows, heard from afar.
inline Game::WellView viewOf(const Game::AtlasEntry& a) {
  Game::WellView v{};
  v.id = a.id;
  v.seed = a.seed;
  v.energy = a.energy;
  v.faction = a.faction;
  v.channel = 0;
  v.hidden = a.energy == Game::Hush;
  v.sig = -120;
  return v;
}

// The Night's row in the week's log, and the records it might set.
inline void logNight(uint8_t how) {
  const Game::Run& r = run();
  const double tithe = Game::effectiveTithe(r);
  game.log.night(r.runNo, r.night, r.n.total, tithe, r.n.visits - r.n.used, how);
  if (how != Game::NightPaid) return;
  Game::Record& big = game.log.biggestNight;
  if (r.n.total > big.value) big = Game::Record{r.n.total, 0, 0, (uint16_t)r.runNo, (uint8_t)r.night, 0};
  Game::Record& quick = game.log.quickest;
  if (quick.value == 0 || r.n.used < quick.value) quick = Game::Record{(double)r.n.used, 0, 0, (uint16_t)r.runNo, (uint8_t)r.night, 0};
}

inline Game::Here hereNow(const Ctx& c, double noise = 0.5) {
  Game::Here h{};
  h.n = c.sky.near();
  h.ward = (int)c.sky.ward();
  h.ch11 = c.sky.onChannel(11);
  h.still = c.gyro ? c.motion.steadiness > 0.8f : false;
  h.noise = noise;
  h.battery = c.st.battery;
  h.playMin = game.playMs / 60000;
  h.playSec = game.playMs / 1000.0;
  return h;
}

// A well's name in the atlas, made the way the scanner makes it.
inline void atlasName(const Game::AtlasEntry& a, char out[17]) {
  WellRules::nameOf(a.seed, (Faction)a.faction, out);
}

// ---- Estimates for the hint star and the visit menu --------------------------------

inline Game::Run& scratch() { return Game::scratchRun(); }

// Resonance from selling energy `e` at `w` now (no Wardens, no luck).
inline double estimateSale(const Game::WellView& w, const Game::Here& h, int e) {
  Game::Sale s;   // scoring only reads the run, so no copy is needed
  Game::saleContext(s, game.run, game.atlas, w, h, e);
  return Game::score(s, game.run).resonance * (game.run.n.glamour ? 2 : 1);
}

// The best thing to do at a well: draw its energy (or your focus) and sell,
// or sell something you carry. Returns the estimate, and what to sell.
inline double bestAt(const Game::WellView& w, const Game::Here& h, int* sellE = nullptr, bool* drawFocus = nullptr) {
  using namespace Game;
  Run& r = game.run;
  double best = 0;
  int bestE = -1;
  bool bestF = false;
  const bool canD = canDraw(r, game.atlas, w) == Why::Ok;
  for (int f = 0; f < (canD && givesFocus(r, w) ? 2 : 1); f++) {
    uint8_t keep[EnergyN];
    memcpy(keep, r.hold, sizeof(keep));
    if (canD) {
      const int e = f ? r.focus : wellEnergy(r, w);
      const int units = r.hold[e] + (int)lround(drawUnits(r, w));
      r.hold[e] = (uint8_t)(units > 255 ? 255 : units);
    }
    for (int e = 0; e < EnergyN; e++) {
      if (canSell(r, game.atlas, w, e) != Why::Ok) continue;
      const double v = estimateSale(w, h, e);
      if (v > best) { best = v; bestE = e; bestF = f == 1; }
    }
    memcpy(r.hold, keep, sizeof(keep));
  }
  if (sellE) *sellE = bestE;
  if (drawFocus) *drawFocus = bestF;
  return best;
}

// The well the scanner stars: where your build makes the most right now, and
// what that offering would make (starValue), for the offerings-to-go gauge.
inline double starValue = 0;
inline uint32_t starWell(const Ctx& c) {
  starValue = 0;
  if (!inRun() || run().n.met || run().n.used >= run().n.visits) return 0;
  static const WellRec* list[Sightings::MaxWells];   // static: 1.3 KB off the stack
  const int n = c.sky.listed(list, Sightings::MaxWells);
  const Game::Here h = hereNow(c);
  uint32_t best = 0;
  double bestV = 0;
  for (int i = 0; i < n && i < 40; i++) {
    if (!c.sky.inRange(*list[i]) || list[i]->sig < Game::ReachDbm) continue;
    const double v = bestAt(viewOf(*list[i], c.sky, c.st.nowMs), h);
    if (v > bestV) { bestV = v; best = list[i]->id; }
  }
  starValue = bestV;
  return best;
}

// ---- Words -----------------------------------------------------------------------

inline const char* energyWord(int e) {
  static const char* const k[] = {"EMBER", "TIDE", "GALE", "HUSH", "STATIC"};
  return e >= 0 && e < 5 ? k[e] : "-";
}
inline const char* factionWord(int f) {
  static const char* const k[] = {"HEDGE", "HEARTH", "WARDED", "SYNDICATE"};
  return f >= 0 && f < 4 ? k[f] : "-";
}

inline void upper(const char* in, char* out, int cap) {
  int i = 0;
  for (; in[i] && i < cap - 1; i++) out[i] = (in[i] >= 'a' && in[i] <= 'z') ? (char)(in[i] - 32) : in[i];
  out[i] = 0;
}

} // namespace App
