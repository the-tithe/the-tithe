#pragma once
#include "BootScreen.h"
#include "DowseScreen.h"
#include "Effects.h"
#include "GlassScreen.h"
#include "ScannerScreen.h"
#include "WalkScreen.h"
#include "WellScreen.h"
#include "app/AppState.h"
#include "app/BookScreens.h"
#include "app/Listen.h"
#include "app/RunScreens.h"
#include "app/ShopScreens.h"
#include "app/VisitScreens.h"

// The game app: which screen is open, what keys do there, and drawing it.
// main.cpp owns the hardware and calls update() and draw() each frame; the
// screen renderer (render/render.cpp) does the same with a made-up world.

namespace App {

inline BootScreen::Info bootInfo{};

// Where to go once the boot animation ends: back to whatever the save was in
// the middle of.
inline void resume() {
  UI::setTheme(game.career.theme);
  switch (game.phase) {
    case Game::Phase::NightEnd: go(Mode::NightEnd); break;
    case Game::Phase::Shop:
      if (game.shop.open) go(Mode::Shop);
      else beginNightNow();   // a shop with nothing in it: on to the next Night
      break;
    case Game::Phase::Lost: go(Mode::Lost); break;
    case Game::Phase::Night:
      go(Mode::Scanner);
      closeNight(-1);   // switched off after paying, before the Night closed
      break;
    default: go(inRun() ? Mode::Scanner : Mode::Bind); break;
  }
}

// A fresh game: a new career and no run.
inline void newGame() {
  memset(&game, 0, sizeof(game));
  Game::newCareer(game.career);
  game.atlas.clear();
  game.phase = Game::Phase::NoRun;
}

inline bool dowseCounted = false;   // this dowse has counted toward the dowsing unlocks

inline void enterDowse(const Ctx& c) {
  const WellRec* w = c.sky.byId(wellId);
  if (!w || !c.gyro) return;
  DowseScreen::begin(wellId);
  dowseCounted = false;
  if (hooks.focusChannel) hooks.focusChannel(w->channel);
  go(Mode::Dowse);
}

inline void leaveDowse() {
  if (hooks.focusChannel) hooks.focusChannel(0);
  go(Mode::Well);
}

// A dowsing reading from a quick one-channel scan.
inline void dowseSample(Sightings& sky, float heading, int rssi, uint32_t nowMs) {
  if (mode != Mode::Dowse) return;
  DowseScreen::sample(heading, rssi);
  DowseScreen::heardAt = modeT;
  if (DowseScreen::dowse.ready() && DowseScreen::dowse.contrast() >= 2.5f) {
    sky.setBearing(DowseScreen::target, DowseScreen::dowse.bearing(), nowMs);
    if (!dowseCounted && inRun()) {
      Game::noteDowse(run());
      dowseCounted = true;
      save();
    }
  }
}

// A twist of the wrist.
inline void twist(const Ctx& c) {
  if (inRun() && Game::castWard(run(), hereNow(c))) {
    toast("HIDDEN: THE WARDENS CAN'T HEAR YOU");
    save();
  }
}

// Overheard: a pulse, or holding the Deck still on the glass, sometimes catches a
// fragment of the city's messages. At most one every 40 seconds, about half the time.
// About one time in three it's a line that belongs where you are (Lore::HeardBind),
// one you haven't heard yet if there is one; otherwise any line in the plain pool.
// Stopping after a walk always catches one (a sure catch, below), usually new.
inline uint32_t overheardAt = 0;
inline int overheardLast = -1;

// Where the Deck is, for the Overheard: the loudest well in range and what the atlas
// knows about it.
struct HeardHere {
  bool well;            // a well in range
  int ward, faction, energy, visits;
  bool home;            // it's the well you've sold at most
  bool old;             // it's one of the old radios on the lampposts
  bool keeper;          // you hold The Old Keeper, or you've kept this well three weeks running
  bool pulse;
};

inline HeardHere heardHere(const Ctx& c, bool pulse) {
  HeardHere h{};
  h.pulse = pulse;
  h.ward = (int)c.sky.ward();
  static const WellRec* list[Sightings::MaxWells];
  const int n = c.sky.listed(list, Sightings::MaxWells);
  if (n > 0 && c.sky.inRange(*list[0])) {
    const WellRec& w = *list[0];
    h.well = true;
    h.faction = (int)w.faction;
    h.energy = (int)w.energy;
    h.old = WellRules::fixedish(w) && WellRules::isOldWell(w);
    const Game::AtlasEntry* a = game.atlas.find(w.id);
    h.visits = a ? a->visits : 0;
    int most = 0;
    uint32_t mostId = 0;
    for (int i = 0; i < game.atlas.count; i++) if (game.atlas.e[i].visits > most) { most = game.atlas.e[i].visits; mostId = game.atlas.e[i].id; }
    h.home = a && mostId == w.id;
    const Game::WellMemory* m = game.wells.find(w.id);
    h.keeper = m && h.visits >= 3 && inRun() && run().runNo >= m->firstRun + 2u;
  }
  if (inRun() && Game::owns(run(), Cat::R_old_keeper)) h.keeper = true;
  return h;
}

// Lines held back everywhere until they make sense: never at the well you've sold at most,
// and none about the Tithe page until it's open.
inline bool heardAllowed(const Lore::HeardBind& b, const HeardHere& h) {
  if ((b.extra & Lore::HX_NotHome) && h.home) return false;
  if ((b.extra & Lore::HX_TithePage) && !((game.career.pages >> 1) & 1)) return false;
  return true;
}

// A line's binding holds here. A line with no binding never "belongs" anywhere.
inline bool heardFits(const Lore::HeardBind& b, const HeardHere& h) {
  const bool needsWell = b.factions || b.energies || b.vmin || b.vmax != 0xFFFF || (b.extra & (Lore::HX_NotHome | Lore::HX_OldWell));
  if (!b.wards && !needsWell && !b.how && !b.extra) return false;
  if (needsWell && !h.well) return false;
  if (b.wards && !((b.wards >> h.ward) & 1)) return false;
  if (b.factions && !((b.factions >> h.faction) & 1)) return false;
  if (b.energies && !((b.energies >> h.energy) & 1)) return false;
  if (h.well && (h.visits < b.vmin || h.visits > b.vmax)) return false;
  if (b.how == Lore::HowPulse && !h.pulse) return false;
  if (b.how == Lore::HowGlass && h.pulse) return false;
  const bool run_ = inRun();
  if ((b.extra & Lore::HX_Loan) && !(run_ && run().owed > 0)) return false;
  if ((b.extra & Lore::HX_LastVisit) && !(run_ && !run().n.met && run().n.visits - run().n.used <= 1)) return false;
  if ((b.extra & Lore::HX_Dawn) && !(mode == Mode::NightEnd || mode == Mode::Shop)) return false;
  if ((b.extra & Lore::HX_Keeper) && !h.keeper) return false;
  if ((b.extra & Lore::HX_NotHome) && h.home) return false;
  if ((b.extra & Lore::HX_OldWell) && !h.old) return false;
  if (!heardAllowed(b, h)) return false;
  return true;
}

inline int pickOverheard(const HeardHere& h, uint32_t roll, bool sure = false) {
  using namespace Lore;
  static uint8_t picks[OverheardCount];
  if (roll % 3 == 0) {   // one that belongs here, unheard first
    for (int pass = 0; pass < 2; pass++) {
      int n = 0;
      for (int i = 0; i < OverheardCount; i++) {
        if (i == overheardLast || !heardFits(Overheard[i].b, h)) continue;
        if (pass == 0 && Game::getBit(game.heard.radio, i)) continue;
        picks[n++] = (uint8_t)i;
      }
      if (n) return picks[(roll / 3) % n];
    }
  }
  // The plain pool. A sure catch tries the lines not heard yet first.
  int n = 0;
  for (int pass = sure ? 0 : 1; pass < 2 && n == 0; pass++)
    for (int i = 0; i < OverheardCount; i++)
      if (Overheard[i].b.pool && i != overheardLast && heardAllowed(Overheard[i].b, h) &&
          !(pass == 0 && Game::getBit(game.heard.radio, i)))
        picks[n++] = (uint8_t)i;
  return picks[(roll / 3) % n];
}

// Returns true if something was caught.
inline bool overhear(const Ctx& c, uint32_t roll, bool pulse, bool sure = false) {
  if (overheardAt && game.playMs - overheardAt < 40000) return false;
  if (!sure && (roll & 1)) return false;
  const int k = pickOverheard(heardHere(c, pulse), roll >> 1, sure);
  overheardLast = k;
  overheardAt = game.playMs;
  Game::setBit(game.heard.radio, k);
  toast(Lore::Overheard[k].text, UI::EnergyColor[(int)Energy::Hush], 3.5f);
  return true;
}

// Stopping after a walk: holding the Deck still for a few seconds on the
// scanner, the glass or a well plays a soft settle and always catches a
// fragment (the 40-second gap still holds). "A walk" is three minutes of
// footsteps in the last five, and at least one more since the last sure
// catch. Wells coming and going don't count: a crowd passing a Deck that
// stands still would. The first stop after the Deck was off for half a day
// catches one too: the talk around it has drifted meanwhile. Off without a
// motion sensor, which can't tell a stop.
constexpr float StopStill = 3;                  // seconds held still
constexpr uint32_t OffForSettle = 720;          // minutes
inline bool settleDue = false;                  // off long enough: the next stop catches one
inline float walkAtSettle = 0;                  // Motion::walkTotal at the last sure catch
inline bool stopTried = false;                  // this stop has had its chance

inline void noteOff(uint32_t minutes) {
  if (minutes >= OffForSettle) settleDue = true;
  teachToast(Taught::OffFor, "WELLS KEPT SETTLING WHILE IT WAS OFF");   // after the OFF FOR toast
}

inline bool walkedRecently(const Ctx& c) {
  return c.motion.walkedRecently() >= 180 && c.motion.walkTotal - walkAtSettle >= 60;
}

inline void stillHands(const Ctx& c) {
  if (c.motion.stillFor < 0.5f) stopTried = false;
  const bool listening = mode == Mode::Scanner || mode == Mode::Glass || mode == Mode::Well;
  if (!listening || !c.gyro || stopTried || c.motion.stillFor <= StopStill || !c.motion.viewing()) return;
  const bool sure = settleDue || walkedRecently(c);
  if (!sure && mode != Mode::Glass) return;
  const bool caught = overhear(c, randomSeed ? randomSeed() : game.playMs * 2654435761u, false, sure);
  if (caught && sure) {
    playTune(Tunes::settle());
    settleDue = false;
    walkAtSettle = c.motion.walkTotal;
  }
  stopTried = caught || !sure;   // a sure catch waits out the gap while you're still
}

// A well you've drawn from and offered to tonight: the scanner dims it.
inline bool spentTonight(uint32_t id) {
  if (!inRun()) return false;
  const Game::Run& r = run();
  const Game::AtlasEntry* a = game.atlas.find(id);
  if (!a || a->runNo != r.runNo || a->soldNight != r.night || a->drawnNight % 1000 != r.night) return false;
  const int times = a->drawnNight >= 1000 ? 2 : 1;
  return times >= Game::drawsAllowed(r, a) && !Game::homebodyOpen(r, a) && !Game::settled(a);
}

inline int glyphHere(uint32_t id) { return inRun() ? Game::glyphAt(run(), game.atlas, id) : -1; }

// Fill in the run's numbers for the scanner's header.
inline void statusFromRun(const Ctx& c) {
  Status& st = c.st;
  st.inRun = inRun();
  for (int e = 0; e < Game::EnergyN; e++) st.wellThirst[e] = 0;
  st.wellFocus = -1;
  st.wellFavor = 0;
  st.wellGlyph = nullptr;
  st.glyph = glyphHere;
  st.adrift = adrift.on && adrift.scatter < 0;
  if (!st.inRun) { st.starId = 0; return; }
  const Game::Run& r = run();
  st.night = r.night;
  st.total = r.n.total;
  st.tithe = Game::effectiveTithe(r);
  st.visitsLeft = r.n.visits - r.n.used;
  st.coin = (int)r.coin;
  // The gauge on the Tithe bar: offerings to go, and the Portent notches.
  st.quiet = Game::deckQuiet(r);
  st.more = Game::moreTonight(r, starValue);
  st.notches = st.quiet || r.n.met ? 0 : Game::portentNotches(r, st.notch);
  st.heat = r.heat;
  st.wardens = Game::wardensCanCome(r);
  st.spent = spentTonight;
  // The well page shows its Thirst for what you're carrying.
  if (mode == Mode::Well) {
    const Game::WellView w = currentView(c);
    const Game::Here h = hereNow(c);
    // Your focus there tonight, and a favor you could take.
    if (Game::givesFocus(r, w) && Game::wellEnergy(r, w) != r.focus) st.wellFocus = r.focus;
    const Game::Job j = Game::jobOffer(r, w);
    bool owed = false;
    for (int i = 0; i < 2; i++) owed |= r.jobs[i].kind && !r.jobs[i].done && r.jobs[i].faction == j.faction;
    int open = 0;
    for (int i = 0; i < 2; i++) open += r.jobs[i].kind && !r.jobs[i].done;
    if (j.kind && !owed && open < 2) { st.wellFavor = j.reward; st.wellFavorFaction = j.faction; }
    const int gl = Game::glyphAt(r, game.atlas, w.id);
    st.wellGlyph = gl >= 0 ? Cat::Glyphs[gl].name : nullptr;
    for (int e = 0; e < Game::EnergyN; e++) {
      if (!r.hold[e]) continue;
      if (Game::canSell(r, game.atlas, w, e) == Game::Why::Banned) { st.wellThirst[e] = -1; continue; }
      Game::Sale s;
      Game::saleContext(s, r, game.atlas, w, h, e);
      st.wellThirst[e] = (float)s.price;
    }
  }
  st.wardLeft = r.n.wardUntil - game.playMs / 1000.0;
}

inline uint32_t starCacheScan = 0;
inline uint32_t starCacheId = 0;
inline uint32_t starCacheUsed = 0;
inline float starCacheAge = 0;
inline uint32_t starCacheRun = 0;

// The collection: every relic held or on a stall counts as seen. Saved with
// the next save, which buying or opening the stall always makes.
inline void noteSeen() {
  // A save from before the HEARD log heard the first dawn's line if it ever paid a Night (this week counts).
  if (Game::careerStat(game.career, Cat::Stat::nights_paid) > 0) Game::setBit(game.heard.stall, Lore::CL_first);
  const Game::Run& r = run();
  if (r.active) for (int i = 0; i < r.nRelics; i++) Game::setBit(game.seen, r.relics[i].id);
  if (game.shop.open) for (int i = 0; i < game.shop.nRelics; i++) Game::setBit(game.seen, game.shop.relics[i].id);
}

inline void update(const Ctx& c, const Press& p, float dt) {
  modeT += dt;
  noteSeen();
  tickToast(dt);
  game.playMs += (uint32_t)(dt * 1000);
  Game::playMinNow = game.playMs / 60000;
  Game::runNow = &game.run;   // career counters read this week's Nights too
  Game::wellsNow = game.atlas.unique;
  tickTunes(dt);
  Tune rhythm{};
  if (tickStray(c, dt, rhythm)) playTune(rhythm);
  tickRibbon(c, dt);
  if (tickRecap(c, dt, rhythm)) playTune(rhythm);
  stillHands(c);
  if (!recap.card) showHeld(c);
  if (inRun() && Game::leak(run(), game.playMs)) save();
  // The hint star: after each scan or sale, and at least once a second (the
  // build or the hold may have changed).
  starCacheAge += dt;
  const uint32_t runKey = run().runNo * 1000u + (uint32_t)run().night * 10u + run().active;
  if (c.sky.scans() != starCacheScan || (uint32_t)run().n.used != starCacheUsed || runKey != starCacheRun ||
      starCacheAge > 1.0f) {
    starCacheRun = runKey;
    starCacheScan = c.sky.scans();
    starCacheUsed = run().n.used;
    starCacheAge = 0;
    starCacheId = starWell(c);
  }
  statusFromRun(c);
  c.st.starId = starCacheId;

  // Keys that work on the scanner's screens.
  const bool scannerish = mode == Mode::Scanner || mode == Mode::Glass || mode == Mode::Well || mode == Mode::Walk;
  if (scannerish) {
    if (p.tab && hooks.setPlace) {
      const int place = ((int)c.st.place + 1) % (int)WalkFormat::Place::Count;
      hooks.setPlace(place);
      if (mode == Mode::Glass || mode == Mode::Well) {   // the scanner and the walk log show the place; these don't
        char b[24];
        snprintf(b, sizeof(b), "PLACE: %s", WalkFormat::placeLabel((WalkFormat::Place)place));
        toast(b, UI::Accent);
      }
    }
    if (p.ch == 'r' && hooks.toggleRecording) hooks.toggleRecording();
    if (p.ch == 'm') {
      c.st.muted = !c.st.muted;
      if (hooks.setMuted) hooks.setMuted(c.st.muted);
      toast(c.st.muted ? "SOUND OFF" : "SOUND ON");
    }
  }
  if (!recapPress(c, p)) switch (mode) {
    case Mode::Boot:
      if (modeT > BootScreen::Length || p.any) resume();
      break;
    case Mode::Scanner: {
      listMode = Mode::Scanner;
      const uint32_t open = ScannerScreen::update(p, c.sky);
      if (open) { wellId = open; go(Mode::Well); }
      else if (p.back) go(Mode::Menu);
      else if (p.ch == 'g') go(Mode::Glass);
      else if (p.ch == 's') { back = Mode::Scanner; go(Mode::Walk); }
      else if (p.ch == 'b' && inRun()) { back = Mode::Scanner; go(Mode::Build); }
      else if (p.ch == 'c' && inRun()) { back = Mode::Scanner; wellId = ScannerScreen::selectedId; go(Mode::Spells); }
      break;
    }
    case Mode::Glass: {
      listMode = Mode::Glass;
      if (p.enter && straySettled()) { keepStray(); break; }
      const uint32_t open = GlassScreen::update(p, c.sky, c.st);
      if (open) { wellId = open; go(Mode::Well); }
      else if (p.back || p.ch == 'g') go(Mode::Scanner);
      else if (p.ch == 's') { back = Mode::Glass; go(Mode::Walk); }
      else if (p.ch == 'b' && inRun()) { back = Mode::Glass; go(Mode::Build); }
      else if (p.ch == 'c' && inRun()) { back = Mode::Glass; wellId = GlassScreen::selectedId; go(Mode::Spells); }
      break;
    }
    case Mode::Well:
      if (p.left || p.right) {   // the next or previous well, in the scanner's order
        static const WellRec* list[Sightings::MaxWells];
        const int n = c.sky.listed(list, Sightings::MaxWells);
        int i = 0;
        while (i < n && list[i]->id != wellId) i++;
        const int to = i + (p.right ? 1 : -1);
        if (i < n && to >= 0 && to < n) {   // a well that's gone from the list stays where it is
          wellId = list[to]->id;
          ScannerScreen::selectedId = wellId;   // and the scanner comes back on it
          ScannerScreen::selIndex = to;
          modeT = 0;
        }
      } else if (WellScreen::update(p)) go(listMode);
      else if (WellScreen::wantsDowse(p)) enterDowse(c);
      else if (p.enter && inRun()) { echoMode = false; go(Mode::Visit); }
      else if (p.ch == 'b' && inRun()) { back = Mode::Well; go(Mode::Build); }
      else if (p.ch == 'c' && inRun()) { back = Mode::Well; go(Mode::Spells); }
      break;
    case Mode::Dowse:
      if (DowseScreen::update(p)) leaveDowse();
      break;
    case Mode::Walk:
      if (WalkScreen::update(p)) go(back);
      break;
    case Mode::Visit: updateVisit(c, p); break;
    case Mode::Drawing: updateDrawing(c, p, dt); break;
    case Mode::Show: updateShow(c, p); break;
    case Mode::Spells: updateSpells(c, p); break;
    case Mode::NightStart: updateNightStart(c, p); break;
    case Mode::NightEnd: updateNightEnd(c, p); break;
    case Mode::OutOfVisits: updateOutOfVisits(c, p); break;
    case Mode::Shop: updateShop(c, p); break;
    case Mode::ShopDrop: updateShopDrop(c, p); break;
    case Mode::ShopAttune: updateShopAttune(c, p); break;
    case Mode::ShopForget: updateShopForget(c, p); break;
    case Mode::Build: updateBuild(c, p); break;
    case Mode::Menu: updateMenu(c, p); break;
    case Mode::Bind: updateBind(c, p); break;
    case Mode::Grimoire: updateGrimoire(c, p); break;
    case Mode::Page: updatePage(c, p); break;
    case Mode::Atlas: updateAtlas(c, p); break;
    case Mode::Won: updateWon(c, p); break;
    case Mode::Lost: updateLost(c, p); break;
    case Mode::Reveal: updateReveal(c, p); break;
    case Mode::Premise: updatePremise(c, p); break;
  }
  // A paid Night always closes, however it was paid (a sale, a loan, a reload).
  if (inRun() && run().n.met && game.phase == Game::Phase::Night && mode != Mode::Show) closeNight(c.st.battery);
  // Out of visits and short of the Tithe: the safety nets, or the end.
  if (inRun() && !run().n.met && run().n.used >= run().n.visits && game.phase == Game::Phase::Night &&
      (mode == Mode::Scanner || mode == Mode::Glass || mode == Mode::Well || mode == Mode::Visit)) {
    if (Game::stuck(run())) lose();
    else go(Mode::OutOfVisits);
  }
}

inline void drawToast(Canvas& g) {
  if (toastT <= 0) return;
  // Over the footer's key hints, so it never hides what the screen is saying.
  // On the visit and the glass the footer is what you need (ESC LEAVE and the
  // hold; ESC LIST ENTER OPEN), so there it takes the line above instead.
  const bool above = mode == Mode::Visit || mode == Mode::Glass;
  const int y = above ? UI::FooterSepY - 17 : UI::FooterSepY + 1;
  g.fillRect(0, y, UI::W, UI::H - UI::FooterSepY - 1, UI::SelBg);
  UI::textCenter(g, y + 2, toastText, toastColor);
}

inline void draw(Canvas& g, const Ctx& c, float clock) {
  switch (mode) {
    case Mode::Boot: BootScreen::draw(g, modeT, bootInfo); break;
    case Mode::Scanner: ScannerScreen::draw(g, c.sky, c.st, clock); break;
    case Mode::Glass:
      GlassScreen::drift = GlassScreen::Drift{adrift.on, adrift.shape, adrift.hand, adrift.drawn, adrift.scatter};
      GlassScreen::draw(g, c.sky, c.motion, c.st, clock);
      break;
    case Mode::Well: WellScreen::draw(g, c.sky, wellId, c.st, modeT); break;
    case Mode::Dowse: DowseScreen::draw(g, c.sky, c.motion, c.st, modeT); break;
    case Mode::Walk: WalkScreen::draw(g, c.sky, c.st, clock); break;
    case Mode::Visit: drawVisit(g, c); break;
    case Mode::Drawing: drawDrawing(g, c, clock); break;
    case Mode::Show: drawShow(g, c); break;
    case Mode::Spells: drawSpells(g, c); break;
    case Mode::NightStart: drawNightStart(g, c, clock); break;
    case Mode::NightEnd: drawNightEnd(g, c); break;
    case Mode::OutOfVisits: drawOutOfVisits(g, c); break;
    case Mode::Shop: drawShop(g, c); break;
    case Mode::ShopDrop: drawShopDrop(g, c); break;
    case Mode::ShopAttune: drawShopAttune(g, c); break;
    case Mode::ShopForget: drawShopForget(g, c); break;
    case Mode::Build: drawBuild(g, c); break;
    case Mode::Menu: drawMenu(g, c); break;
    case Mode::Bind: drawBind(g, c); break;
    case Mode::Grimoire: drawGrimoire(g, c); break;
    case Mode::Page: drawPage(g, c); break;
    case Mode::Atlas: drawAtlas(g, c); break;
    case Mode::Won: drawWon(g, c, clock); break;
    case Mode::Lost: drawLost(g, c); break;
    case Mode::Reveal: drawReveal(g, c, modeT); break;
    case Mode::Premise: drawPremise(g, c); break;
  }
  if (mode != Mode::Boot) drawRecap(g, c);
  if (mode != Mode::Boot) drawToast(g);
}

} // namespace App
