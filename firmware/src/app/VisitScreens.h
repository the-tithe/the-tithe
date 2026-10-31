#pragma once
#include "AppState.h"
#include "Listen.h"
#include "../Effects.h"

// At a well: the visit menu (draw, sell, cast, carve, take a job), the
// drawing ritual (hold still), the sale show (every relic firing in turn),
// and the spell list.

namespace App {

// ---- The visit menu ------------------------------------------------------------

enum class Act : uint8_t { DrawOwn, DrawFocus, Sell, Cast, Carve, Job, Echo, Leave };

struct Action {
  Act act;
  int arg;              // energy for Sell, glyph token for Carve
  bool ok;
  Game::Why why;
  double est;
};

inline Action actions[16];
inline int nActions = 0;
inline bool echoMode = false;       // selling to a remembered well through ECHO

inline const WellRec* openWell(const Ctx& c) { return c.sky.byId(wellId); }

inline Game::WellView currentView(const Ctx& c) {
  if (echoMode) {
    const Game::AtlasEntry* a = game.atlas.find(wellId);
    if (a) return viewOf(*a);
  }
  const WellRec* w = openWell(c);
  if (w) return viewOf(*w, c.sky, c.st.nowMs);
  Game::WellView v{};
  v.id = wellId;
  v.sig = -120;
  return v;
}

inline void buildActions(const Ctx& c) {
  using namespace Game;
  Run& r = run();
  const WellView w = currentView(c);
  const Here h = hereNow(c);
  nActions = 0;
  auto add = [&](Act a, int arg, Why why, double est) {
    if (nActions < 16) actions[nActions++] = Action{a, arg, why == Why::Ok, why, est};
  };
  if (!echoMode) {
    const Why dw = canDraw(r, game.atlas, w);
    add(Act::DrawOwn, wellEnergy(r, w), dw, drawUnits(r, w));
    if (givesFocus(r, w) && r.focus != wellEnergy(r, w)) add(Act::DrawFocus, r.focus, dw, drawUnits(r, w));
  }
  for (int e = 0; e < EnergyN; e++) {
    if (!r.hold[e]) continue;
    const Why sw = canSell(r, game.atlas, w, e, echoMode);
    add(Act::Sell, e, sw, sw == Why::Ok ? estimateSale(w, h, e) : 0);
  }
  if (!echoMode) {
    if (r.nSpells) add(Act::Cast, 0, Why::Ok, 0);
    for (int i = 0; i < r.nGlyphTokens; i++)
      add(Act::Carve, i, w.sig >= ReachDbm ? Why::Ok : Why::TooFar, 0);
    const Job j = jobOffer(r, w);
    if (j.kind) add(Act::Job, j.faction, Why::Ok, 0);
    if (r.n.echo) add(Act::Echo, 0, Why::Ok, 0);
  }
  if (sel >= nActions) sel = nActions > 0 ? nActions - 1 : 0;   // leaving is ESC, not a row
}

// The line under a row you can't do: the same words wherever it's said.
inline const char* whyLine(const Action& a, char* b, int cap) {
  using namespace Game;
  const AtlasEntry* here = game.atlas.find(wellId);
  const int left = (a.why == Why::Dry || a.why == Why::AlreadySold) ? settleLeft(here) : 0;
  if (left > 0) { snprintf(b, cap, "SETTLING: BACK IN %d MIN", left); return b; }
  if (a.why == Why::Dry && here && here->drawnNight >= 2000) return "OFFER HERE TO DRAW AGAIN";
  return whyText(a.why);
}

inline void actionLabel(const Action& a, char* b, int cap) {
  switch (a.act) {
    case Act::DrawOwn: snprintf(b, cap, "DRAW %s", energyWord(a.arg)); break;
    case Act::DrawFocus: snprintf(b, cap, "DRAW %s (YOUR FOCUS)", energyWord(a.arg)); break;
    case Act::Sell: snprintf(b, cap, "OFFER %d %s", run().hold[a.arg], energyWord(a.arg)); break;
    case Act::Cast: snprintf(b, cap, "CAST A SPELL"); break;
    case Act::Carve: {
      char g[16];
      upper(Cat::Glyphs[run().glyphTokens[a.arg]].name, g, sizeof(g));
      snprintf(b, cap, "CARVE %s GLYPH", g);
      break;
    }
    case Act::Job: snprintf(b, cap, "TAKE THE %s FAVOR", factionWord(a.arg)); break;
    case Act::Echo: snprintf(b, cap, "OFFER THROUGH ECHO"); break;
    case Act::Leave: snprintf(b, cap, "LEAVE"); break;
  }
}

// ---- The drawing ritual ---------------------------------------------------------

inline bool drawFocusPending = false;
// The draw as a cup filling to the brim: the surface sloshes with your hand,
// and when it's not steady, drops spill over the low rim and patter below.
struct Drop { float x, y, vx, vy; bool landed; float life; };
inline Drop drops[16];
inline float slosh = 0, sloshPhase = 0, spillDue = 0, plipWait = 0;
inline uint32_t dropSeed = 1;
constexpr int CupRimY = 34, CupBotY = 86, CupRim = 30, CupBot = 21, CupFloorY = 98;

inline void resetCup() {
  memset(drops, 0, sizeof(drops));
  slosh = sloshPhase = spillDue = plipWait = 0;
}

// The surface's lean on screen: level with the world, so it tips against the Deck.
inline float cupSlope(const Ctx& c) {
  if (!c.gyro) return 0;
  const float up1 = c.motion.up[1] < 0.3f ? 0.3f : c.motion.up[1];
  const float s = c.motion.up[0] / up1;
  return s > 0.6f ? 0.6f : s < -0.6f ? -0.6f : s;
}

inline void stepCup(const Ctx& c, float dt) {
  const float shake = c.gyro ? 1 - c.motion.steadiness : 0;
  slosh += (shake * 7 - slosh) * (dt * 4 > 1 ? 1 : dt * 4);
  sloshPhase += dt * (5 + shake * 12);
  // Spilling runs with unsteadiness, like the spill itself (up to a quarter of the draw).
  spillDue += dt * (shake > 0.15f ? (shake - 0.15f) * 14 : 0);
  const float slope = cupSlope(c);
  while (spillDue >= 1) {
    spillDue -= 1;
    dropSeed = dropSeed * 1664525u + 1013904223u;
    const float rnd = (dropSeed >> 8) / 16777216.0f;
    const int side = slope < -0.08f ? 1 : slope > 0.08f ? -1 : (rnd < 0.5f ? -1 : 1);   // over the low rim
    for (Drop& d : drops) {
      if (d.life > 0) continue;
      d = Drop{(float)(UI::W / 2 + side * CupRim), (float)CupRimY, side * (25 + rnd * 45), -20 - rnd * 30, false, 1.2f};
      break;
    }
  }
  if (plipWait > 0) plipWait -= dt;
  for (Drop& d : drops) {
    if (d.life <= 0) continue;
    d.life -= dt;
    if (d.landed) continue;
    d.vy += 320 * dt;
    d.x += d.vx * dt;
    d.y += d.vy * dt;
    if (d.y >= CupFloorY) {   // it hits the ground: a little plip
      d.y = CupFloorY;
      d.landed = true;
      d.life = 0.5f;
      if (plipWait <= 0) { beep(170 + (int)(d.x) % 60, 18); plipWait = 0.12f; }
    }
  }
}

inline void drawCup(Canvas& g, const Ctx& c, float fill, uint16_t ec) {
  const int cx = UI::W / 2;
  const float slope = cupSlope(c);
  const float level = CupBotY - fill * (CupBotY - CupRimY - 3);   // brim-full when done
  for (int x = cx - CupRim + 1; x <= cx + CupRim - 1; x++) {
    // The cup's inside at this column: below the slanted side, above the bottom.
    const int dx = x > cx ? x - cx : cx - x;
    const float sideLimit = dx <= CupBot ? CupBotY : CupRimY + (float)(CupRim - dx) / (CupRim - CupBot) * (CupBotY - CupRimY);
    float surf = level + slope * (x - cx) + slosh * fill * sinf(sloshPhase + (x - cx) * 0.18f);
    if (surf < CupRimY + 1) surf = CupRimY + 1;
    const int top = (int)surf, bot = (int)sideLimit - 1;
    if (fill <= 0 || top > bot) continue;
    g.drawFastVLine(x, top, bot - top + 1, UI::dimmed(ec));
    g.drawPixel(x, top, ec);   // the surface catches the light
  }
  // The cup itself.
  g.drawLine(cx - CupRim, CupRimY, cx - CupBot, CupBotY, UI::Dim);
  g.drawLine(cx + CupRim, CupRimY, cx + CupBot, CupBotY, UI::Dim);
  g.drawFastHLine(cx - CupBot, CupBotY, 2 * CupBot + 1, UI::Dim);
  g.drawFastHLine(cx - CupRim - 2, CupRimY, 4, UI::Dim);
  g.drawFastHLine(cx + CupRim - 1, CupRimY, 4, UI::Dim);
  for (const Drop& d : drops) {
    if (d.life <= 0) continue;
    if (d.landed) g.drawFastHLine((int)d.x - 1, CupFloorY, 3, UI::dimmed(ec));
    else g.fillRect((int)d.x, (int)d.y, 2, 2, ec);
  }
}

inline double steadySum = 0;
inline int steadyN = 0;
inline Game::Drawn lastDraw{};
inline bool drawDone = false;
constexpr float DrawHold = 1.3f;   // seconds to hold still (gyro); shorter without one

// ---- The sale show ---------------------------------------------------------------

inline Game::Sold lastSale{};
inline Game::WellView lastSaleWell{};   // for the ring's chord
inline Game::Trace lastTrace{};
inline char lastWellName[17] = "";
inline int shownSteps = 0;

inline float stepTime() {
  const int n = lastTrace.n;
  if (n <= 0) return 0.3f;
  const float t = 2.4f / n;
  return t < 0.12f ? 0.12f : t > 0.35f ? 0.35f : t;
}

inline const char* sourceName(uint8_t src, int ed, char* b, int cap) {
  if (src == Game::Trace::FromAttune) { snprintf(b, cap, "ATTUNED"); return b; }   // the base every multiplier multiplies
  if (src == Game::Trace::FromGlyph) {   // which glyph: the one carved at the well you offered to
    const int gl = Game::glyphAt(run(), game.atlas, wellId);
    char gn[16] = "";
    if (gl >= 0) upper(Cat::Glyphs[gl].name, gn, sizeof(gn));
    snprintf(b, cap, gl >= 0 ? "%s GLYPH" : "GLYPH", gn);
    return b;
  }
  char n[24], e[12];
  upper(Cat::Relics[src].name, n, sizeof(n));
  if (ed >= 0) { upper(Cat::Editions[ed].name, e, sizeof(e)); snprintf(b, cap, "%s %s", e, n); }
  else snprintf(b, cap, "%s", n);
  return b;
}

inline int visitTop = 0;          // the first visit-menu row on screen

inline char favorNote[40] = "";   // a favor that moved along, said once the show ends
inline float showDoneT = -1;       // when the show's last step landed (modeT)
constexpr int ShowRows = 4;        // step lines on screen at once
inline int showTop = -1;           // the first step line on screen once you scroll, -1 to follow the newest
inline bool showRecord = false;    // this offering beat the loudest ever
inline bool showBurn = false;      // ...or paid the whole Tithe on its own: it lands in gold
inline char beatText[48] = "";     // the record it beat, and where
constexpr uint16_t Gold = UI::rgb(255, 185, 40);

// "BEAT 1,050 AT SAINT STOOP": the old record and the well that held it.
inline void noteBeat(const Game::Record& old) {
  char n[16], name[17];
  WellRules::nameOf(old.wellSeed, (Faction)old.faction, name);
  snprintf(beatText, sizeof(beatText), "BEAT %s AT %s", UI::num(old.value, n, sizeof(n)), name);
}

// Do the sale at the open well, with the trace for the show.
inline void doSell(const Ctx& c, int e) {
  using namespace Game;
  const WellView w = currentView(c);
  const double noise = owns(run(), Cat::R_din) && hooks.listen ? hooks.listen() : 0.5;
  const Here h = hereNow(c, noise);
  // Score once more with the trace on: the same numbers sell() gets.
  {
    Sale s;
    saleContext(s, run(), game.atlas, w, h, e);
    score(s, run(), &lastTrace);
  }
  uint8_t gotBefore[2] = {run().jobs[0].got, run().jobs[1].got};
  lastSale = sell(run(), game.atlas, w, h, e, echoMode, &game.recent);
  if (lastSale.why == Why::Ok) noteOffering(wellFaction(run(), w), h.ward, e);
  lastSaleWell = w;
  favorNote[0] = 0;
  for (int i = 0; i < 2; i++) {
    const Job& j = run().jobs[i];
    if (j.kind && !j.done && j.got > gotBefore[i])
      snprintf(favorNote, sizeof(favorNote), "%s FAVOR %d/%d", factionWord(j.faction), j.got, j.need);
  }
  if (echoMode) {
    const AtlasEntry* a = game.atlas.find(wellId);
    if (a) atlasName(*a, lastWellName);
  } else if (const WellRec* wr = openWell(c)) {
    strncpy(lastWellName, wr->name, sizeof(lastWellName));
  }
  // A record is only a record with a few offerings behind it: the first week's
  // second offering beating its first is no fanfare (the log still keeps it).
  constexpr uint32_t RecordAfter = 6;   // offerings in the career, this one included
  showRecord = !lastSale.blocked && game.log.loudest.value > 0 && lastSale.resonance > game.log.loudest.value &&
               careerStat(game.career, Cat::Stat::sales_total) >= RecordAfter;
  showBurn = !lastSale.blocked && (showRecord || lastSale.resonance >= effectiveTithe(run()));
  if (!lastSale.blocked) game.wells.note(w.id, lastSale.resonance, run().runNo, run().night);
  if (showRecord) noteBeat(game.log.loudest);
  if (!lastSale.blocked && lastSale.resonance > game.log.loudest.value)   // a record
    game.log.loudest = Game::Record{lastSale.resonance, w.id, w.seed, (uint16_t)run().runNo, (uint8_t)run().night, w.faction};
  echoMode = false;
  shownSteps = 0;
  showDoneT = -1;
  showTop = -1;
  save(true);
  go(Mode::Show);
}

// A paid Night closes: the payout, then the Night's end screen. Safe to
// call more than once; only the first call does anything.
inline void closeNight(int battery) {
  Game::Run& r = run();
  if (!r.active || !r.n.met || game.phase != Game::Phase::Night) return;
  logNight(Game::NightPaid);
  game.lastEnd = Game::endNight(r, game.career, battery, &game.moments.portent);
  dawnToken();
  noteCollectorLine();
  game.phase = Game::Phase::NightEnd;
  save(true);
  go(Mode::NightEnd);
  beep(784, 90);
  beep(1047, 160);
}

inline void afterShow(const Ctx& c) {
  Game::Run& r = run();
  if (r.n.met) {
    closeNight(c.st.battery);
  } else if (r.n.used >= r.n.visits) {
    go(Mode::OutOfVisits);
  } else {
    go(listMode);
  }
  if (favorNote[0] && !r.n.met) toast(favorNote, UI::Accent);
  favorNote[0] = 0;
}

// An offering that rings plays the well's tune as a chord, with the energy you brought.
inline void ringChord() {
  const Game::WellView& w = lastSaleWell;
  playTune(Tunes::chord(w.seed, w.energy, keptWell(w.id), lastSale.energy));
}

// ---- Updates ------------------------------------------------------------------

inline uint32_t echoFrom = 0;   // the well you were at when ECHO opened the atlas

inline void updateVisit(const Ctx& c, const Press& p) {
  using namespace Game;
  buildActions(c);
  // The first favor on offer: what it is.
  for (int i = 0; i < nActions && !taught(Taught::Favor); i++) {
    if (actions[i].act != Act::Job) continue;
    char b[40];
    snprintf(b, sizeof(b), "A FAVOR: A SMALL JOB, +%dc WHEN DONE", (int)jobOffer(run(), currentView(c)).reward);
    teachToast(Taught::Favor, b, "WORTH THE COIN OF 2 TO 4 OFFERINGS");
  }
  // ESC leaves, to the list you came from; an echo visit goes back to the atlas.
  if (p.back) {
    if (echoMode) { echoMode = false; go(Mode::Atlas); }
    else go(listMode);
    return;
  }
  if (p.down && sel < nActions - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (!p.enter || nActions == 0) return;
  const Action a = actions[sel];
  if (!a.ok) {
    char b[40];
    beep(180, 80);
    toast(whyLine(a, b, sizeof(b)), UI::Rec);
    return;
  }
  switch (a.act) {
    case Act::DrawOwn:
    case Act::DrawFocus:
      drawFocusPending = a.act == Act::DrawFocus;
      steadySum = 0;
      steadyN = 0;
      drawDone = false;
      resetCup();
      go(Mode::Drawing);
      break;
    case Act::Sell: doSell(c, a.arg); break;
    case Act::Cast: back = Mode::Visit; go(Mode::Spells); break;
    case Act::Carve: {
      const int g = run().glyphTokens[a.arg];
      if (carve(run(), game.atlas, currentView(c), hereNow(c), a.arg)) {
        char b[40], n[16];
        upper(Cat::Glyphs[g].name, n, sizeof(n));
        snprintf(b, sizeof(b), "%s GLYPH CARVED", n);
        toast(b, UI::Accent);
        beep(988, 80);
        save();
      }
      break;
    }
    case Act::Job:
      if (takeJob(run(), jobOffer(run(), currentView(c)))) { toast("FAVOR TAKEN", UI::Accent); save(); }
      else {
        int owed = 0;
        for (int i = 0; i < 2; i++) owed += run().jobs[i].kind && !run().jobs[i].done;
        toast(owed >= 2 ? "YOU OWE TWO FAVORS ALREADY" : "YOU OWE THEM A FAVOR ALREADY", UI::Rec);
      }
      break;
    case Act::Echo: back = Mode::Visit; echoFrom = wellId; go(Mode::Atlas); break;
    case Act::Leave: echoMode = false; go(listMode); break;
  }
}

inline void updateDrawing(const Ctx& c, const Press& p, float dt) {
  if (!drawDone) {
    steadySum += c.gyro ? c.motion.steadiness : 1.0;
    steadyN++;
    if (modeT >= (c.gyro ? DrawHold : 0.5f)) {
      const double s = steadyN ? steadySum / steadyN : 1;
      lastDraw = Game::draw(run(), game.atlas, currentView(c), hereNow(c), s, drawFocusPending);
      drawDone = true;
      modeT = 0;
      save();
      beep(lastDraw.spilled ? 330 : 660, 70);
      if (lastDraw.units) beep(880, 90);
    }
    stepCup(c, dt);
    if (p.back) { go(Mode::Visit); toast("DRAW STOPPED", UI::Dim); }
    return;
  }
  if (modeT > 1.2f || p.enter || p.back) {
    go(Mode::Visit);
    // Put the cursor on selling what was just drawn: the obvious next step.
    buildActions(c);
    for (int i = 0; i < nActions; i++)
      if (actions[i].act == Act::Sell && actions[i].arg == lastDraw.energy) { sel = i; break; }
  }
}

inline void updateShow(const Ctx& c, const Press& p) {
  const int total = lastTrace.n;
  // ESC leaves at once, played out or not. ENTER mid-show skips to the end:
  // the last step lands this same frame, so the total starts counting at once.
  if (p.back) { afterShow(c); return; }
  if (p.enter && shownSteps < total - 1) { shownSteps = total - 1; modeT = total * stepTime(); }
  const int due = (int)(modeT / stepTime());
  while (shownSteps < total && shownSteps < due) {
    shownSteps++;
    // Each kind of step has its own voice, and every step climbs higher.
    const int up = 40 * shownSteps;
    switch (lastTrace.s[shownSteps - 1].field) {
      case Game::Trace::Units: beep(330 + up, 25); break;
      case Game::Trace::Price: beep(440 + up, 25); break;
      case Game::Trace::Mult: beep(587 + up, 25); break;
      case Game::Trace::X: beep(784 + up, 40); beep(1175 + up, 40); break;   // the ka-ching
      default: beep(660 + up, 30); break;
    }
    if (shownSteps == total) {
      if (lastSale.blocked) { beep(220, 90); beep(165, 220); }   // taken: falling, low
      else { beep(1047, 60); beep(1319, 120); if (showBurn) beep(98, 260); if (showRecord) beep(1568, 180); ringChord(); }
    }
  }
  if (total == 0 && shownSteps == 0 && modeT > 0.2f) {
    shownSteps = 1;
    beep(lastSale.blocked ? 196 : 1047, 120);
    if (showBurn) beep(98, 260);
    if (!lastSale.blocked) ringChord();
  }
  if (showDoneT < 0 && (total > 0 ? shownSteps >= total : shownSteps == 1)) showDoneT = modeT;
  // Once it's done, up and down scroll back through every step.
  if (showDoneT >= 0 && (p.up || p.down) && total > ShowRows) {
    const int last = total - ShowRows;
    int t = showTop < 0 ? last : showTop;
    t += p.down ? 1 : -1;
    showTop = t < 0 ? 0 : t > last ? last : t;
    return;
  }
  if (p.enter && showDoneT >= 0 && modeT - showDoneT >= 0.1f) afterShow(c);   // one ENTER on a finished show leaves
}

// ---- Drawing ---------------------------------------------------------------------

inline void drawHoldLine(Canvas& g, int x, int y, int gap = 6) {
  const Game::Run& r = run();
  int held = 0;
  for (int e = 0; e < Game::EnergyN; e++) {
    if (!r.hold[e]) continue;
    held += r.hold[e];
    UI::energyGlyph(g, x, y, (Energy)e, UI::energyColor(e));
    char b[6];
    snprintf(b, sizeof(b), "%d", r.hold[e]);
    UI::text(g, x + 9, y, b, UI::Text);
    x += 9 + UI::textW(b) + gap;
  }
  char b[12];
  snprintf(b, sizeof(b), "%d/%d", held, Game::holdCap(r));
  UI::text(g, x, y, held ? b : "HOLD EMPTY", UI::Dim);
}

// ESC LEAVE, what you're holding, and the Night's visits and coin.
inline void drawRunFooter(Canvas& g) {
  UI::footerLine(g);
  const int x = UI::keyHint(g, 2, "ESC", "LEAVE", UI::Text);
  drawHoldLine(g, x, UI::FooterY, 3);
  const Game::Run& r = run();
  char b[32];
  snprintf(b, sizeof(b), "%d LEFT %dc", r.n.visits - r.n.used, (int)r.coin);
  UI::textRight(g, UI::W - 2, UI::FooterY, b, UI::Dim);
}

inline void drawVisit(Canvas& g, const Ctx& c) {
  using namespace Game;
  g.fillSprite(UI::Bg);
  const WellView w = currentView(c);
  char name[17] = "";
  if (echoMode) {
    const AtlasEntry* a = game.atlas.find(wellId);
    if (a) atlasName(*a, name);
  } else if (const WellRec* wr = openWell(c)) {
    strncpy(name, wr->name, sizeof(name));
  }
  const int own = wellEnergy(run(), w);
  UI::energyGlyph(g, 3, 3, (Energy)own, UI::energyColor(own));
  UI::text(g, 14, 3, name, UI::SelText);
  UI::textRight(g, UI::W - 3, 3, factionWord(wellFaction(run(), w)), UI::factionColor((Faction)wellFaction(run(), w)));
  char b[48];
  if (echoMode) snprintf(b, sizeof(b), "THROUGH ECHO, FROM AFAR");
  else snprintf(b, sizeof(b), "%s WELL, %s", energyWord(own), UI::reachName(UI::reachOf(w.sig)));
  UI::text(g, 3, 14, b, UI::Dim);
  const int carved = glyphAt(run(), game.atlas, w.id);
  if (carved >= 0) {   // the glyph you carved here this week
    char gn[16];
    upper(Cat::Glyphs[carved].name, gn, sizeof(gn));
    snprintf(b, sizeof(b), "%s GLYPH", gn);
    UI::textRight(g, UI::W - 3, 14, b, UI::Accent);
  }
  g.drawFastHLine(0, 24, UI::W, UI::Sep);
  buildActions(c);
  if (nActions == 0) UI::textCenter(g, 60, "NOTHING TO OFFER HERE", UI::Dim);
  const int y0 = 28, rowH = 11, show = 7;
  // More choices than fit (two draws, several energies held, a favor): scroll with the cursor.
  if (sel < visitTop) visitTop = sel;
  if (sel >= visitTop + show) visitTop = sel - show + 1;
  if (visitTop > 0 && visitTop + show > nActions) visitTop = nActions > show ? nActions - show : 0;
  if (visitTop > 0) g.fillTriangle(UI::W - 2, y0 - 2, UI::W - 4, y0, UI::W, y0, UI::Dim);
  if (visitTop + show < nActions) {
    const int yb = y0 + show * rowH - 1;
    g.fillTriangle(UI::W - 2, yb + 2, UI::W - 4, yb, UI::W, yb, UI::Dim);
  }
  for (int i = visitTop; i < nActions && i < visitTop + show; i++) {
    const Action& a = actions[i];
    const int y = y0 + (i - visitTop) * rowH;
    if (i == sel) g.fillRect(0, y - 1, UI::W, rowH, UI::SelBg);
    actionLabel(a, b, sizeof(b));
    uint16_t col = a.ok ? (i == sel ? UI::SelText : UI::Text) : UI::Dim;
    if (a.act == Act::Sell || a.act == Act::DrawOwn || a.act == Act::DrawFocus) {
      UI::energyGlyph(g, 4, y + 1, (Energy)a.arg, a.ok ? UI::energyColor(a.arg) : UI::dimmed(UI::energyColor(a.arg)));
      UI::text(g, 15, y + 1, b, col);
    } else {
      UI::text(g, 15, y + 1, b, col);
    }
    if (a.act == Act::Sell && a.ok) {
      char n[16], v[20];
      snprintf(v, sizeof(v), "~%s", UI::num(a.est, n, sizeof(n)));
      UI::textRight(g, UI::W - 4, y + 1, v, UI::Accent);
    } else if ((a.act == Act::DrawOwn || a.act == Act::DrawFocus) && a.ok) {
      snprintf(b, sizeof(b), "+%d", (int)lround(a.est));
      UI::textRight(g, UI::W - 4, y + 1, b, UI::Dim);
    } else if (a.act == Act::Job) {
      snprintf(b, sizeof(b), "+%dc", (int)jobOffer(run(), w).reward);
      UI::textRight(g, UI::W - 4, y + 1, b, UI::Accent);   // real coin, like a sale's estimate
    }
  }
  // The line under the menu: why not, or what the job asks.
  if (sel < nActions) {
    const Action& a = actions[sel];
    if (!a.ok) {
      const char* why = whyLine(a, b, sizeof(b));
      UI::text(g, 3, 108, why, why == b ? UI::Dim : UI::Rec);   // settling is a wait, not a refusal
    } else if (a.act == Act::Job) {
      UI::text(g, 3, 108, jobText(jobOffer(run(), w), b, sizeof(b)), UI::Accent);
    } else if (a.act == Act::Sell && a.arg == Static && wardensCanCome(run()) && heatAfterStatic(run()) >= 3) {
      snprintf(b, sizeof(b), "HEAT %.1f: THE WARDENS WILL LISTEN", heatAfterStatic(run()));
      UI::text(g, 3, 108, b, UI::Rec);
    } else if (a.act == Act::Sell) {
      char n[16];
      snprintf(b, sizeof(b), "TITHE %s TO GO", UI::num(effectiveTithe(run()) - run().n.total, n, sizeof(n)));
      UI::text(g, 3, 108, b, UI::Dim);
    } else if (a.act == Act::DrawOwn || a.act == Act::DrawFocus) {
      UI::text(g, 3, 108, c.gyro ? "HOLD THE DECK STILL TO DRAW CLEAN" : "", UI::Dim);
    } else if (a.act == Act::Carve) {   // what it would do (it replaces any glyph named above)
      UI::text(g, 3, 108, Cat::Glyphs[run().glyphTokens[a.arg]].text, UI::Accent);
    } else if (carved >= 0) {   // what the glyph here does
      UI::text(g, 3, 108, Cat::Glyphs[carved].text, UI::Accent);
    }
  }
  drawRunFooter(g);
}

inline void drawDrawing(Canvas& g, const Ctx& c, float t) {
  g.fillSprite(UI::Bg);
  const int cx = UI::W / 2, cy = 58;
  const int e = drawDone ? lastDraw.energy : (drawFocusPending ? run().focus : Game::wellEnergy(run(), currentView(c)));
  const uint16_t ec = UI::energyColor(e < 0 ? 0 : e);
  if (!drawDone) {
    const float hold = c.gyro ? DrawHold : 0.5f;
    const float f = modeT / hold > 1 ? 1 : modeT / hold;
    drawCup(g, c, f, ec);
    UI::energyGlyph(g, cx - 3, 18, (Energy)(e < 0 ? 0 : e), ec);
    UI::textCenter(g, 102, c.gyro ? "HOLD STILL" : "DRAWING", UI::Text);
    if (c.gyro && c.motion.steadiness < 0.5f) UI::textCenter(g, 112, "IT'S SPILLING", UI::Rec);
    UI::footerLine(g);
    UI::keyHint(g, 2, "ESC", "STOP", UI::Dim);
    (void)cy;
    (void)t;
    return;
  }
  char b[40];
  if (lastDraw.why != Game::Why::Ok) {
    UI::textCenter(g, 55, Game::whyText(lastDraw.why), UI::Rec);
    return;
  }
  snprintf(b, sizeof(b), "+%d %s", lastDraw.units, energyWord(lastDraw.energy));
  UI::textCenter(g, 48, b, ec, 2);
  if (lastDraw.spilled) {
    snprintf(b, sizeof(b), "%d SPILLED", lastDraw.spilled);
    UI::textCenter(g, 72, b, UI::Rec);
  }
  drawHoldLine(g, 70, 96);
}

// 6 rather than 6.0, but 4.5 stays 4.5.
inline const char* tidy(double v, char* buf, int cap) {
  if (fabs(v - lround(v)) < 0.05) snprintf(buf, cap, "%ld", lround(v));
  else snprintf(buf, cap, "%.1f", v);
  return buf;
}

constexpr float ShowCountT = 0.45f;   // how long the total takes to count up
constexpr float BigCountT = 1.1f;     // ...for a record, or one that pays the whole Tithe
constexpr float ShowPopT = 0.22f;     // how long a changed number stays popped

// Flames licking up off a burning total: a row of flickering tongues, gold
// at the root, orange, then red at the tips, with a spark now and then.
inline void drawShow(Canvas& g, const Ctx& c) {
  using namespace Game;
  g.fillSprite(UI::Bg);
  char b[48], n1[16], n2[16];
  const int e = lastSale.energy;
  UI::energyGlyph(g, 3, 3, (Energy)e, UI::energyColor(e));
  snprintf(b, sizeof(b), "%s AT %s", energyWord(e), lastWellName);
  UI::text(g, 14, 3, b, UI::Dim);
  // Replay the trace up to the step being shown.
  double cur[5] = {lastTrace.units0, lastTrace.price0, lastTrace.mult0, 1, 0};
  const int shown = shownSteps < lastTrace.n ? shownSteps : lastTrace.n;
  // Once the last step lands, the well rings: the total counts up and rings
  // spread from it, more of them when this offering pays the Tithe.
  const float since = showDoneT < 0 ? 0 : modeT - showDoneT;
  const float countT = showBurn ? BigCountT : ShowCountT;
  const float k = since >= countT ? 1 : 1 - powf(1 - since / countT, 3);
  // A big offering shakes as it lands.
  const int shakeX = showBurn && showDoneT >= 0 && since < 0.5f ? (int)(sinf(since * 70) * 3 * (1 - since / 0.5f)) : 0;
  if (showDoneT >= 0 && !lastSale.blocked) {
    if (showBurn) {   // wide, slow gold rings
      for (int i = 0; i < 5; i++) {
        const float t = since - i * 0.3f;
        if (t <= 0 || t >= 2.0f) continue;
        g.drawCircle(UI::W / 2 + shakeX, 101, 10 + (int)(t * 50), UI::fade(Gold, 1 - t / 2.0f));
      }
    } else {
      const int rings = lastSale.closed ? 5 : 3;
      for (int i = 0; i < rings; i++) {
        const float t = since - i * 0.12f;
        if (t <= 0 || t >= 0.9f) continue;
        g.drawCircle(UI::W / 2 + shakeX, 101, 10 + (int)(t * 120), UI::fade(UI::energyColor(e), 1 - t / 0.9f));
      }
    }
  }
  struct Line { uint8_t src, field; int8_t ed; double delta, factor; } lines[Trace::Max];
  int nl = 0;
  for (int k = 0; k < shown; k++) {
    const Trace::Step& s = lastTrace.s[k];
    lines[nl++] = Line{s.source, s.field, s.ed, s.after - cur[s.field], cur[s.field] != 0 ? s.after / cur[s.field] : 1};
    cur[s.field] = s.after;
  }
  // The window of step lines: the newest while it plays, or wherever you've scrolled.
  // Room for COUNTS 2X, or the record it beat.
  const int rows = ShowRows - ((showRecord && showDoneT >= 0) || cur[4] > 0 || lastSale.glamour || lastSale.witching ? 1 : 0);
  int top = nl > rows ? nl - rows : 0;
  if (showTop >= 0 && nl > rows) top = showTop > nl - rows ? nl - rows : showTop;
  if (top > 0) g.fillTriangle(UI::W - 3, 48, UI::W - 5, 50, UI::W - 1, 50, UI::Dim);
  if (top + rows < nl) g.fillTriangle(UI::W - 3, 52 + rows * 10, UI::W - 5, 50 + rows * 10, UI::W - 1, 50 + rows * 10, UI::Dim);
  // Units x Thirst x Amp, with one decimal each, each in its color. The one
  // the newest step changed pops: it jumps and flashes white.
  const double mult = cur[2] * cur[3];
  char f[3][12];
  tidy(cur[0], f[0], sizeof(f[0])); tidy(cur[1], f[1], sizeof(f[1])); tidy(mult, f[2], sizeof(f[2]));
  {
    static const uint16_t col[3] = {UI::UnitsColor, UI::ThirstColor, UI::AmpColor};
    static const char* const lab[3] = {"UNITS", "THIRST", "AMP"};
    int popF = -1;
    float pop = 0;
    if (shown > 0) {
      const uint8_t fld = lastTrace.s[shown - 1].field;
      popF = fld == Trace::Units ? 0 : fld == Trace::Price ? 1 : (fld == Trace::Mult || fld == Trace::X) ? 2 : -1;
      const float age = modeT - shown * stepTime();
      pop = age < 0 ? 1 : age < ShowPopT ? 1 - age / ShowPopT : 0;
    }
    const int cw = UI::CharW * 2;
    const int l[3] = {(int)strlen(f[0]), (int)strlen(f[1]), (int)strlen(f[2])};
    const int x0 = (UI::W - (l[0] + l[1] + l[2] + 6) * cw) / 2;
    const int xs[3] = {x0, x0 + (l[0] + 3) * cw, x0 + (l[0] + l[1] + 6) * cw};
    for (int i = 0; i < 3; i++) {
      const bool p = i == popF && pop > 0;
      UI::text(g, xs[i], 20 - (p ? (int)(3 * pop + 0.5f) : 0), f[i], p && pop > 0.5f ? UI::Text : col[i], 2);
      if (i < 2) UI::text(g, xs[i] + (l[i] + 1) * cw, 20, "x", UI::Dim, 2);
      const int mid = xs[i] + l[i] * cw / 2;
      UI::text(g, mid - UI::textW(lab[i]) / 2, 38, lab[i], UI::dimmed(col[i]));
    }
  }
  for (int i = top; i < nl && i < top + rows; i++) {
    const Line& l = lines[i];
    const int y = 52 + (i - top) * 10;
    char who[32];
    sourceName(l.src, l.ed, who, sizeof(who));
    UI::text(g, 8, y, who, i == nl - 1 ? UI::SelText : UI::Dim);
    static const char* const field[] = {"UNITS", "THIRST", "AMP", "AMP", "REPEATS"};
    static const uint16_t fcol[] = {UI::UnitsColor, UI::ThirstColor, UI::AmpColor, UI::AmpColor, 0};
    if (l.field == Trace::X) snprintf(b, sizeof(b), "x%.3g %s", l.factor, field[l.field]);
    else if (l.field == Trace::Repeats) snprintf(b, sizeof(b), "COUNTS AGAIN");
    else snprintf(b, sizeof(b), "%+.3g %s", l.delta, field[l.field]);
    const uint16_t fc = l.field == Trace::Repeats ? UI::Accent : fcol[l.field];
    UI::textRight(g, UI::W - 8, y, b, i == nl - 1 ? fc : UI::dimmed(fc));
  }
  // The total shows once the last step has landed (a frame or two after it's due).
  const bool done = shown >= lastTrace.n && showDoneT >= 0;
  if (done) {
    if (lastSale.blocked) {
      // It lands, then drains away.
      if (since < 0.85f && lastSale.taken > 0) {
        const double left = since < 0.3f ? 1 : 1 - (since - 0.3f) / 0.55f;
        snprintf(b, sizeof(b), "= %s", UI::num(lastSale.taken * (left < 0 ? 0 : left), n1, sizeof(n1)));
        UI::textCenter(g, 94, b, since < 0.3f ? UI::SelText : UI::Rec, 2);
      } else {
        UI::textCenter(g, 94, "WARDENS TOOK IT", UI::Rec, 2);
        g.fillRect(0, 82, UI::W, 10, UI::Bg);   // over any step line underneath
        UI::textCenter(g, 83, "HEAT 3+: THE WARDENS WERE LISTENING", UI::Dim);
      }
    } else {
      snprintf(b, sizeof(b), "= %s", UI::num(lastSale.resonance * k, n1, sizeof(n1)));
      const int w2 = UI::textW(b, 2) + 12;
      if (since < 2.4f)   // the rings pass behind the number, not through it
        g.fillRect((UI::W - w2) / 2 + shakeX, 92, w2, 19, UI::Bg);
      // A big one counts up white and lands in gold.
      const uint16_t col = !showBurn ? UI::SelText : since < countT ? UI::Text : Gold;
      UI::text(g, (UI::W - UI::textW(b, 2)) / 2 + shakeX, 94, b, col, 2);
      char extra[48] = "";
      if (cur[4] > 0) snprintf(extra + strlen(extra), sizeof(extra) - strlen(extra), "COUNTS %dX ", (int)cur[4] + 1);
      if (lastSale.glamour) snprintf(extra + strlen(extra), sizeof(extra) - strlen(extra), "GLAMOUR X2 ");
      if (lastSale.witching) snprintf(extra + strlen(extra), sizeof(extra) - strlen(extra), "WITCHING X3");
      // What explains the number comes first (COUNTS 2X, GLAMOUR X2), then the record it beat.
      const bool beat = showRecord && beatText[0] && since >= countT && (!extra[0] || since >= countT + 2.4f);
      const char* line = beat ? beatText : extra[0] ? extra : nullptr;
      if (line) {
        const int lw = UI::textW(line) + 8;
        g.fillRect((UI::W - lw) / 2, 83, lw, 9, UI::Bg);   // the rings pass behind it
        UI::textCenter(g, 84, line, line == beatText ? Gold : UI::Accent);
      }
    }
  }
  // The Night's progress: where it stood before this sale, until the sale lands.
  const Run& r = run();
  const double total = r.n.total - lastSale.resonance * (done ? 1 - k : 1);
  const double tithe = effectiveTithe(r);
  const bool flash = lastSale.closed && since >= countT && since < countT + 0.15f;
  UI::bar(g, 4, 113, UI::W - 8, 6, total / tithe, flash ? UI::Text : UI::Accent);
  UI::footerLine(g);
  snprintf(b, sizeof(b), "%s / %s", UI::num(total, n1, sizeof(n1)), UI::num(tithe, n2, sizeof(n2)));
  UI::text(g, 2, UI::FooterY, b, done && r.n.met ? UI::SelText : UI::Text);
  int right = UI::W - 2;
  if (done && !lastSale.blocked) {
    if (lastSale.jobCoin) snprintf(b, sizeof(b), "FAVOR +%dc", lastSale.jobCoin);
    else snprintf(b, sizeof(b), "+%dc AT DAWN", (int)lastSale.coin);
    UI::textRight(g, right, UI::FooterY, b, UI::Dim);
    right -= UI::textW(b) + 12;
  }
  // The key that leaves: ESC while it plays, ENTER once it has landed.
  const char* key = done ? "ENTER" : "ESC";
  UI::textRight(g, right, UI::FooterY, "LEAVE", UI::Dim);
  UI::textRight(g, right - UI::textW("LEAVE") - UI::CharW, UI::FooterY, key, UI::Faint);
  (void)c;
}

// ---- Spells ---------------------------------------------------------------------

inline char typed[12] = "";

inline void updateSpells(const Ctx& c, const Press& p) {
  using namespace Game;
  Run& r = run();
  if (p.del && typed[0]) {   // DEL erases the last letter typed; with none left, it's back
    typed[strlen(typed) - 1] = 0;
    for (int i = 0; i < r.nSpells && typed[0]; i++) {
      if (strncmp(Cat::Spells[r.spells[i]].name, typed, strlen(typed)) == 0) { sel = i; break; }
    }
    return;
  }
  if (p.back) { typed[0] = 0; go(back); return; }
  if (p.down && sel < r.nSpells - 1) sel++;
  if (p.up && sel > 0) sel--;
  // Typing a spell's name picks it, the way the Deck was meant to be used.
  if (p.ch) {
    const size_t len = strlen(typed);
    if (len < sizeof(typed) - 1) { typed[len] = (char)(p.ch - 32); typed[len + 1] = 0; }
    for (int i = 0; i < r.nSpells; i++) {
      if (strncmp(Cat::Spells[r.spells[i]].name, typed, strlen(typed)) == 0) { sel = i; break; }
    }
  }
  if (!p.enter || r.nSpells == 0) return;
  typed[0] = 0;
  const int sp = r.spells[sel];
  const WellRec* wr = c.sky.byId(wellId);
  Game::WellView w = wr ? viewOf(*wr, c.sky, c.st.nowMs) : Game::WellView{};
  const CastResult res = castSpell(r, game.atlas, game.career, sel, wr ? &w : nullptr, hereNow(c));
  char b[40], n[16];
  switch (res.how) {
    case Cast::Ok:
      upper(Cat::Spells[sp].name, n, sizeof(n));
      if (res.glyph >= 0) { char gn[16]; upper(Cat::Glyphs[res.glyph].name, gn, sizeof(gn)); snprintf(b, sizeof(b), "%s GLYPH CARVED", gn); }
      else if (res.relic >= 0 && sp == Cat::Spell_summon) { upper(Cat::Relics[res.relic].name, b, sizeof(b)); }
      else if (sp == Cat::Spell_transmute && r.n.transmuteLeft > 0)   // cast with an empty hold, nothing shows: say what it set up
        snprintf(b, sizeof(b), "TRANSMUTE: NEXT %d OFFERINGS AS %s", r.n.transmuteLeft, energyWord(r.n.transmuteE));
      else snprintf(b, sizeof(b), "%s CAST", n);
      toast(b, UI::Accent);
      beep(1175, 60);
      beep(1568, 120);
      save();
      if (sp == Cat::Spell_echo) { back = Mode::Scanner; go(Mode::Atlas); return; }
      go(back);
      break;
    case Cast::NeedWell: toast("STAND AT A WELL TO CAST IT", UI::Rec); beep(180, 80); break;
    case Cast::NoSlot: toast("NO FREE RELIC SLOT", UI::Rec); beep(180, 80); break;
    default: toast("NOTHING FOR IT TO DO", UI::Rec); beep(180, 80); break;
  }
}

inline void drawSpells(Canvas& g, const Ctx& c) {
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  UI::text(g, 3, 3, "SPELLS", UI::SelText);
  if (typed[0]) UI::textRight(g, UI::W - 3, 3, typed, UI::Accent);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  if (r.nSpells == 0) UI::textCenter(g, 60, "NO SPELLS. THE COLLECTOR SELLS THEM.", UI::Dim);
  for (int i = 0; i < r.nSpells; i++) {
    const Cat::SpellDef& s = Cat::Spells[r.spells[i]];
    const int y = 18 + i * 27;
    if (i == sel) g.fillRect(0, y - 2, UI::W, 26, UI::SelBg);
    UI::text(g, 6, y, s.name, i == sel ? UI::SelText : UI::Text);
    UI::wrap(g, 6, y + 9, UI::W - 12, s.text, UI::Dim, 2, 8, 0, true);   // a long one takes two lines
  }
  const int flavorY = 18 + r.nSpells * 27 > 100 ? 18 + r.nSpells * 27 : 100;
  if (r.nSpells && sel < r.nSpells && flavorY <= 112)
    UI::wrap(g, 6, flavorY, UI::W - 12, Cat::Spells[r.spells[sel]].flavor, UI::Faint, 1);
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  if (r.nSpells) x = UI::keyHint(g, x, "ENTER", "CAST", UI::Text);
  if (r.nSpells > 1) UI::keyHint(g, x, "TYPE", "A NAME", UI::Text);
  (void)c;
}

} // namespace App
