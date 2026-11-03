// A scripted playthrough of the game app with a made-up street, saving every
// screen it passes through. Run by render.cpp after the scanner's screens.
#include <M5GFX.h>
#include <stdio.h>
#include "App.h"

extern M5Canvas canvas;
void saveShot(const char* name);

namespace Play {

static Sightings sky;
static Motion motion;
static Status st;

static int along = 0;   // how far down the street you've walked
static uint32_t now = 0;
static void fakeStreet(int scans) {
  static Net nets[40];
  for (int s = 0; s < scans; s++) {
    int k = 0;
    for (int i = 0; i < 14; i++) {
      Net& n = nets[k++];
      memset(&n, 0, sizeof(n));
      const int w = i + along;   // a different well as you walk
      const uint8_t b[6] = {0x10, (uint8_t)(0x20 + w), 0x33, (uint8_t)(w * 7), (uint8_t)(0x55 + w / 200), 0x60};
      memcpy(n.bssid, b, 6);
      n.rssi = (int8_t)(-46 - i * 3);
      static const uint8_t ch[] = {1, 6, 11, 6, 1, 3, 11, 6, 9, 1, 13, 6, 11, 4};
      n.channel = ch[i];
      n.lock = i == 3 ? Lock::Open : i == 7 ? Lock::Wpa3 : i == 10 ? Lock::Enterprise : Lock::Wpa2;
      n.hidden = i == 5;
      n.nameKey = n.hidden ? 0 : 0x100 + (i == 9 ? 0 : i);   // wells 0 and 9 share a name: a mesh
    }
    now += 2400;
    sky.ingest(nets, k, now);
  }
  st.nowMs = now;
}

static App::Ctx ctx() { return App::Ctx{sky, motion, st, true}; }

static void frame(Press p = Press(), float dt = 0.05f) {
  App::update(ctx(), p, dt);
}
static void wait(float secs) {
  for (float t = 0; t < secs; t += 0.05f) frame();
}
static Press key(char which) {
  Press p;
  p.any = true;
  switch (which) {
    case 'U': p.up = true; break;
    case 'D': p.down = true; break;
    case 'L': p.left = true; break;
    case 'R': p.right = true; break;
    case 'E': p.enter = true; break;
    case 'B': p.back = true; break;
    case 'T': p.tab = true; break;
    default:
      p.ch = which >= 'A' && which <= 'Z' ? (char)(which + 32) : which;
      p.key = which >= 'a' && which <= 'z' ? (char)(which - 32) : which;
      break;
  }
  return p;
}
static void keys(const char* seq) {
  for (const char* k = seq; *k; k++) { frame(key(*k)); frame(); }
}
static void shot(const char* name, float clock = 0.4f) {
  App::draw(canvas, ctx(), clock);
  saveShot(name);
}

// Visit the starred well: draw and sell. Returns false if there's none.
static bool visitStar() {
  const uint32_t id = st.starId;
  if (!id) return false;
  App::wellId = id;
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  // Draw first if it helps, then sell the best.
  for (int i = 0; i < App::nActions; i++) {
    if ((App::actions[i].act == App::Act::DrawOwn || App::actions[i].act == App::Act::DrawFocus) && App::actions[i].ok) {
      App::sel = i;
      frame(key('E'));
      wait(2.0f);
      break;
    }
  }
  App::buildActions(ctx());
  int best = -1;
  for (int i = 0; i < App::nActions; i++)
    if (App::actions[i].act == App::Act::Sell && App::actions[i].ok && (best < 0 || App::actions[i].est > App::actions[best].est)) best = i;
  if (best < 0) { App::go(App::Mode::Scanner); return false; }
  App::sel = best;
  frame(key('E'));
  wait(4);
  frame(key('E'));
  return true;
}

void qa();
void estimates();
void stall();
void dawn();
void silent();
void growth();

void run() {
  fakeStreet(6);
  {   // a Deck held steady in front of you
    const float a[3] = {0, 0.6f, 0.8f}, g[3] = {0, 0, 0};
    for (int i = 0; i < 120; i++) motion.update(a, g, 1 / 30.0f);
  }
  st.battery = 81;
  st.place = WalkFormat::Place::Street;
  st.log = Status::Log::On;
  st.walk = 4;
  App::newGame();
  App::bootInfo = BootScreen::Info{true, true, true};
  App::go(App::Mode::Boot);
  wait(3);   // the boot ends on the Bind screen: no week yet
  shot("game_bind");
  keys("RRR");   // past the open Callings to a locked one
  shot("game_bind_locked");
  keys("LLL");
  keys("DD");
  shot("game_bind_pact");
  {   // the second rung's text, as a Chalk seal opens it
    App::game.career.seals[0] = 0;
    keys("UR");
    shot("game_bind_lead");
    keys("LD");
    App::game.career.seals[0] = -1;
  }
  keys("UU");    // back to CALLING; ENTER walks the rows down to BIND, then binds
  keys("EEE");
  shot("game_premise");
  keys("E");
  shot("game_night1");
  keys("E");
  frame();
  shot("game_scanner_run");
  // Open the starred well and look at the visit menu.
  App::wellId = st.starId;
  App::go(App::Mode::Well);
  shot("game_well");
  keys("E");
  shot("game_visit");
  {   // a well with a favor on offer: its coin in the accent colour, the task under the menu
    const uint32_t was = App::wellId;
    static const WellRec* list[Sightings::MaxWells];
    const int n = sky.listed(list, Sightings::MaxWells);
    for (int i = 0; i < n; i++) {
      if (!sky.inRange(*list[i]) || list[i]->sig < Game::ReachDbm) continue;
      if (!Game::jobOffer(App::game.run, App::viewOf(*list[i], sky, st.nowMs)).kind) continue;
      App::wellId = list[i]->id;
      App::buildActions(ctx());
      for (int k = 0; k < App::nActions; k++) if (App::actions[k].act == App::Act::Job) App::sel = k;
      shot("game_visit_favor");
      break;
    }
    App::wellId = was;
    App::buildActions(ctx());
    App::sel = 0;
  }
  {   // a long menu: four energies held, so it scrolls
    Game::Run& r = App::game.run;
    uint8_t keep[Game::EnergyN];
    for (int e = 0; e < Game::EnergyN; e++) keep[e] = r.hold[e];
    r.hold[0] = 2; r.hold[1] = 2; r.hold[2] = 2; r.hold[4] = 2;
    App::buildActions(ctx());
    App::sel = App::nActions - 1;
    shot("game_visit_long");
    for (int e = 0; e < Game::EnergyN; e++) r.hold[e] = keep[e];
    App::sel = 0;
  }
  {   // a glyph carved here, and one in hand to carve
    Game::Run& r = App::game.run;
    const Game::WellView w = App::currentView(ctx());
    Game::AtlasEntry& a = App::game.atlas.touch(w.id, w.seed, r.runNo, 0);
    a.glyph = Cat::Glyph_lit;
    r.glyphTokens[r.nGlyphTokens++] = Cat::Glyph_beacon;
    App::buildActions(ctx());
    for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Carve) App::sel = i;
    shot("game_visit_glyph");
    App::go(App::Mode::Well);
    frame();
    shot("game_well_glyph");
    App::go(App::Mode::Scanner);
    frame();
    shot("game_scanner_glyph");
    App::go(App::Mode::Visit);
    a.glyph = -1;
    r.nGlyphTokens--;
    App::sel = 0;
  }
  {   // a well with the true name of one you've worked, and a drifting glyph waiting in the glass
    const uint32_t id = App::wellId;
    WellRec* w = sky.edit(id);
    w->namesake = true;
    App::go(App::Mode::Well);
    frame();
    shot("game_well_namesake");
    App::startStray(Cat::Glyph_lit, 1234, 0);
    App::go(App::Mode::Scanner);
    frame();
    shot("game_scanner_found");
    App::toast("NAMESAKE: VESPER KILN", UI::Accent, 2.0f);
    shot("game_toast_namesake");
    App::toastT = 0;
    App::go(App::Mode::Glass);
    App::adrift.drawn = 0;
    st.hushReveal = 0.4f;
    shot("game_glass_adrift", 1.3f);
    st.hushReveal = 1;
    App::adrift.drawn = 0.45f;
    shot("game_glass_drawing", 1.3f);
    App::adrift.drawn = 1;
    shot("game_glass_settled", 1.3f);
    App::adrift.scatter = 0.35f;
    shot("game_glass_scatter", 1.3f);
    for (int k = 0; k < Strays::Shapes; k++) {   // every shape, settled
      App::startStray(k, (uint16_t)(k * 811 + 77), 0);
      App::adrift.drawn = 1;
      char name[32];
      snprintf(name, sizeof(name), "game_glass_shape_%d", k);
      shot(name, 1.3f);
    }
    App::adrift.on = false;
    w->namesake = false;
    App::go(App::Mode::Visit);
    App::sel = 0;
  }
  // The drawing ritual, part way and done.
  App::sel = 0;
  frame(key('E'));
  for (int i = 0; i < 4; i++) frame();
  shot("game_drawing_ready", 0.2f);
  for (int i = 0; i < 8; i++) frame();
  shot("game_drawing", 0.6f);
  wait(1.3f);
  shot("game_drawn");
  wait(1.5f);
  shot("game_visit_after_draw");
  App::go(App::Mode::Well);   // the well page, now that you're carrying its energy
  frame();
  shot("game_well_thirst");
  st.wellFavor = 5;   // as if a favor were on offer there
  st.wellFavorFaction = 0;
  shot("game_well_favor");
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  // Sell: the show, part way and done.
  App::buildActions(ctx());
  for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Sell && App::actions[i].ok) { App::sel = i; break; }
  frame(key('E'));
  frame(); frame(); frame(); frame();
  shot("game_show_mid");
  while (App::showDoneT < 0) frame();
  for (int k = 0; k < 4; k++) frame();
  shot("game_show_ring");
  wait(4);
  shot("game_show_done");
  frame(key('E'));
  // Play out the Night by visiting starred wells (moving down the street refreshes them).
  int guard = 0;
  while (App::mode != App::Mode::NightEnd && App::mode != App::Mode::OutOfVisits && guard++ < 200) {
    visitStar();
    along += 3;   // walk on a little
    fakeStreet(3);
    frame();
  }
  while (App::mode == App::Mode::Show) frame(key('E'));
  printf("night 1 closed after %d tries, mode %d\n", guard, (int)App::mode);
  App::modeT = App::TallyStart + 1.5f * App::TallyRowT;   // part way through the dawn tally
  shot("game_night_end_tally");
  wait(4);
  shot("game_night_end");
  {   // the same dawn with two Portents, to show what each one does
    const Game::NightEnd keep = App::game.lastEnd;
    App::game.lastEnd.portents = 2;
    App::game.lastEnd.got[0].portent = Cat::Portent_windfall;
    App::game.lastEnd.got[0].coin = 10;
    App::game.lastEnd.got[1].portent = Cat::Portent_rare_sight;
    App::modeT += 30;
    shot("game_night_end_portents");
    {   // a Portent still one of two: the dawn waits for the choice
      const Game::PortentChoice keepChoice = App::game.moments.portent;
      App::game.moments.portent = Game::PortentChoice{1, {(uint8_t)(Cat::Portent_doubled_interest + 1), 0, 0}, 3};
      App::sel = 0;
      shot("game_night_end_choice");
      App::sel = 1;
      shot("game_night_end_choice_other");
      App::sel = 0;
      App::game.moments.portent = keepChoice;
    }
    App::modeT -= 30;
    App::game.lastEnd = keep;
    const uint8_t token = App::game.moments.lastToken;   // the rare one nobody recognises
    App::game.moments.lastToken = Game::T_Stranger + 1;
    shot("game_night_end_stranger");
    App::game.moments.lastToken = token;
    // A dawn with favors finished, and a dawn the Syndicate paid for.
    App::game.lastEnd.favors = 9;
    App::modeT += 30;
    shot("game_night_end_favors");
    App::game.lastEnd = keep;
    App::game.run.n.closedBy = 2;
    App::game.lastEnd.trade = 0;
    shot("game_night_end_loan");
    App::game.run.n.closedBy = 1;
    App::modeT -= 30;
    App::game.lastEnd = keep;
  }
  {   // out of visits: the loan's row says what it costs
    const Game::Run keep = App::game.run;
    const App::Mode was = App::mode;
    const Game::Phase phase = App::game.phase;
    App::game.phase = Game::Phase::Night;
    App::game.run.n.met = 0;
    App::game.run.n.used = App::game.run.n.visits;
    App::game.run.n.total = App::game.run.n.tithe * 0.7;
    App::game.run.night = 1;
    App::game.run.loansLeft = 1;
    App::go(App::Mode::OutOfVisits);
    App::sel = 1;
    shot("game_out_of_visits_loan");
    App::game.run = keep;
    App::game.phase = phase;
    App::go(was);
  }
  for (int k = 0; k < 8 && App::mode == App::Mode::NightEnd; k++) { frame(key('E')); frame(); }   // past the tally and any Portent to choose
  shot("game_shop");
  keys("R");
  shot("game_shop_wares");
  keys("R");
  shot("game_shop_yours");
  keys("LL");
  App::game.run.coin = 60;
  keys("E");
  shot("game_shop_bought");
  // Fill the slots, then try to buy: the pick-a-relic-to-sell screen.
  for (int k = Game::slotsUsed(App::game.run); k < Game::relicSlots(App::game.run); k++) Game::addRelic(App::game.run, Cat::R_crowd_feeder + k);
  App::sel = 1;
  keys("E");
  shot("game_shop_drop");
  keys("B");
  keys("R");
  App::sel = 0;
  for (int i = 0; i < App::game.shop.nCons; i++) if (App::game.shop.cons[i].kind == Game::OfferKind::Attune) App::sel = i;
  if (App::game.shop.cons[App::sel].kind == Game::OfferKind::Attune) { keys("E"); shot("game_shop_attune"); keys("B"); }
  {   // a full spell pocket: let one go for the one on the shelf
    Game::Run& r = App::game.run;
    while (r.nSpells < Game::spellSlots(r)) r.spells[r.nSpells++] = Cat::Spell_hex;
    App::game.shop.cons[0].kind = Game::OfferKind::Spell;
    App::game.shop.cons[0].id = Cat::Spell_haste;
    App::game.shop.cons[0].sold = 0;
    App::tab = App::TabWares;
    App::sel = 0;
    r.coin = 60;
    keys("E");
    shot("game_shop_forget");
    keys("B");
    r.nSpells = 0;
  }
  keys("BB");   // leaving the stall takes two presses
  shot("game_night2");
  keys("E");
  // A sale with relics firing, for the show.
  {
    Game::Run& r = App::game.run;
    r.nRelics = 0;
    Game::addRelic(r, Cat::R_static_caller);
    Game::addRelic(r, Cat::R_crowd_feeder);
    Game::addRelic(r, Cat::R_close_call, Cat::Edition_lit);
    Game::addRelic(r, Cat::R_gridlock);
    r.relics[3].tier = 1;
    r.attune[Game::Static] = 2;
    along += 40;
    fakeStreet(3);
    for (int k = 0; k < 25; k++) frame();
    App::wellId = 0;
    const WellRec* list[Sightings::MaxWells];
    const int n = sky.listed(list, Sightings::MaxWells);
    for (int i = 0; i < n; i++) if (list[i]->energy == Energy::Static && sky.inRange(*list[i])) { App::wellId = list[i]->id; break; }
    if (App::wellId) {
      App::go(App::Mode::Visit);
      App::sel = 0;
      frame(key('E'));
      {   // a shaky hand: the cup sloshes and spills
        const float a[3] = {0.25f, 0.55f, 0.8f};
        for (int k = 0; k < 16; k++) {
          const float g[3] = {2.5f * sinf(k * 1.7f), 2.0f * cosf(k * 2.3f), 0.5f};
          motion.update(a, g, 0.05f);
          frame();
        }
        shot("game_drawing_spill");
        const float still[3] = {0, 0, 0}, level[3] = {0, 0.6f, 0.8f};
        for (int k = 0; k < 60; k++) motion.update(level, still, 1 / 30.0f);
      }
      wait(3.0f);   // settle 0.4 s, hold still 1.3 s, then the result shows 1.2 s
      App::buildActions(ctx());
      for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Sell && App::actions[i].ok) App::sel = i;
      frame(key('E'));
      for (int k = 0; k < 18; k++) frame();
      shot("game_show_relics_mid");
      while (App::showDoneT < 0) frame();
      for (int k = 0; k < 5; k++) frame();
      shot("game_show_relics_ring");
      wait(4);
      shot("game_show_relics_done");
      frame(key('U'));   // scrolled back to the first step
      frame(key('U'));
      shot("game_show_relics_scrolled");
      App::showTop = -1;
      {   // the same offering, taken by the Wardens, part way through draining
        const Game::Sold keep = App::lastSale;
        App::lastSale.blocked = true;
        App::lastSale.taken = keep.resonance;
        const float doneAt = App::showDoneT;
        App::showDoneT = App::modeT - 0.55f;
        shot("game_show_wardens");
        App::showDoneT = App::modeT - 2.0f;
        shot("game_show_wardens_done");
        App::lastSale = keep;
        App::showDoneT = doneAt;
      }
      {   // a step just landed: its number pops
        const int keepShown = App::shownSteps;
        const float keepT = App::modeT, keepDone = App::showDoneT;
        App::shownSteps = 3;
        App::modeT = 3 * App::stepTime() + 0.03f;
        App::showDoneT = -1;
        shot("game_show_pop");
        App::shownSteps = keepShown;
        App::modeT = keepT;
        App::showDoneT = keepDone;
      }
      {   // the loudest offering yet: it burns
        const float keepDone = App::showDoneT;
        App::showRecord = App::showBurn = true;
        strcpy(App::beatText, "BEAT 1,050 AT SAINT STOOP");
        App::showDoneT = App::modeT - 0.2f;
        shot("game_show_burn_land");
        App::showDoneT = App::modeT - 1.4f;
        shot("game_show_burn");
        App::showRecord = App::showBurn = false;
        App::showDoneT = keepDone;
      }
      frame(key('E'));
    }
  }
  {   // Heat high enough that the Wardens listen: the eye in the header
    const double was = App::game.run.heat;
    App::game.run.heat = 3.2;
    {   // and one well already drawn and offered tonight, dimmed
      const WellRec* list[Sightings::MaxWells];
      const int n = sky.listed(list, Sightings::MaxWells);
      if (n > 2) {
        Game::AtlasEntry& a = App::game.atlas.touch(list[2]->id, 0, App::game.run.runNo, 0);
        a.drawnNight = a.soldNight = (int16_t)App::game.run.night;
      }
    }
    App::go(App::Mode::Scanner);
    frame();
    shot("game_scanner_heat");
    App::overheardAt = 0;
    App::overhear(ctx(), 2 * 34, true);   // something caught out of the air
    shot("game_overheard");
    App::toastT = 0;
    App::game.run.heat = was;
  }
  {   // taken out after a pocket walk: what you passed
    App::go(App::Mode::Scanner);
    App::recapCount(sky, 0, App::recap);
    App::recap.fresh = 14;
    App::recap.hidden = 2;
    App::recap.wardKinds = 3;
    App::recap.card = true;
    App::recap.loudNear = true;
    frame();
    shot("game_recap");
    App::recap.loudNear = false;   // the loudest well is behind you: no OPEN IT
    shot("game_recap_behind");
    App::recap.loudId = 0;
    shot("game_recap_unnamed");
    App::recap.card = false;
  }
  // Menu, build, atlas, grimoire, a page.
  keys("B");
  {   // two favors owed, one part done, for the build's second tab
    Game::Run& r = App::game.run;
    r.jobs[0] = Game::Job{Game::Jobs::Wander, Game::Warded, 0, 3, 1, 5, 0, 0};
    r.jobs[1] = Game::Job{Game::Jobs::Deliver, Game::Hearth, 1, 6, 0, 4, 0, 0};
    r.n.omen = Cat::Omen_empty_roads;   // a Night under an Omen, for THE REST
    App::game.phase = Game::Phase::Night;
  }
  App::go(App::Mode::Menu);
  App::game.run.hold[Game::Tide] = 5;
  App::game.run.hold[Game::Hush] = 2;
  const uint32_t runs = App::game.career.runs, wins = App::game.career.wins;
  App::game.career.runs = 8; App::game.career.wins = 3;   // a few weeks in, for the shot
  {   // the Bind screen after a week lost on Lead: Shoestring offered as its lesson
    const Game::Saved keepGame = App::game;
    const App::Mode keepMode = App::mode;
    App::game.career.wins = 3;
    App::bindCalling = 0;
    App::bindPact = 1;
    App::bindOffer = App::bindChallenge = Game::trialForPact(1);
    App::go(App::Mode::Bind);
    App::sel = 2;
    shot("game_bind_trial_offer");
    App::bindOffer = -1;
    App::bindChallenge = -1;
    App::game = keepGame;
    App::go(keepMode);
  }
  shot("game_menu");
  App::game.career.runs = runs; App::game.career.wins = wins;
  App::game.run.hold[Game::Tide] = App::game.run.hold[Game::Hush] = 0;
  App::premiseFromMenu = true;
  App::go(App::Mode::Premise);
  shot("game_premise_menu");
  keys("B");
  keys("DE");
  shot("game_build");
  keys("R");
  shot("game_build_tonight");
  keys("R");
  shot("game_build_wares");
  keys("R");
  shot("game_build_you");
  keys("R");
  shot("game_build_week");
  keys("BB");
  App::go(App::Mode::Atlas);
  App::back = App::Mode::Menu;
  shot("game_atlas");
  App::go(App::Mode::Grimoire);
  shot("game_grimoire");
  keys("R");
  keys("DDD");
  shot("game_collection");
  App::sel = Cat::R_last_ember;   // a secret: no hint
  shot("game_collection_secret");
  App::sel = Cat::R_first_light;  // a real goal: progress
  shot("game_collection_goal");
  App::sel = 3;
  keys("R");
  keys("U");
  shot("game_records");
  for (int k : {0, 4, 11, 17, 26, 28, 31, 40, 52, 66, 90, 121}) Game::setBit(App::game.heard.radio, k);
  for (int k : {Lore::CL_first, Lore::CL_coat, Lore::CL_any4, Lore::CL_borderwalker}) Game::setBit(App::game.heard.stall, k);
  keys("R");
  shot("game_heard");
  keys("DDDDDDDDDDDD");
  shot("game_heard_stall");
  keys("R");
  shot("game_found_empty");
  {   // what some weeks of walking turn up
    Game::Moments& m = App::game.moments;
    static const uint8_t shapes[] = {0, 1, 2, 3, 5, 1, 0, 2, 5, 3, 1};
    for (int i = 0; i < 11; i++)
      m.marks[m.nMarks++] = Game::Mark{(uint16_t)((i * 373 + 17) & 4095), 0, (uint16_t)(1 + i / 4), (uint8_t)(i % 7), shapes[i]};
    for (int k : {App::G_Old, App::G_Namesake, App::G_Chorus, App::G_Locked, App::G_Cold, App::G_LeftOn}) {
      m.guideGot |= (uint8_t)(1u << k);
      m.guide[k] = Game::packName(0x3141u * (k + 5), k == App::G_Old ? 0 : k == App::G_Locked ? 2 : k == App::G_Chorus ? 3 : 1);
    }
    for (int i = 0; i < 3; i++) m.namesakes[m.nNamesakes++] = Game::Namesake{100u + i, Game::packName(0x2718u * (i + 2), 1), (uint16_t)(2 + i)};
    for (int i = 0; i < 64; i++) if (i % 3 != 1) Game::setBit(m.words, App::wordBit(0, i));
    for (int i = 0; i < 32; i++) Game::setBit(m.words, App::wordBit(1, i));
    for (int i = 0; i < 32; i += 2) Game::setBit(m.words, App::wordBit(2, i));
    static const uint8_t tokens[Game::TokenCount] = {4, 7, 3, 0, 2, 1, 1, 0};
    for (int k = 0; k < Game::TokenCount; k++) m.tokens[k] = tokens[k];
    m.nRibbons = m.nextRibbon = 0;
    for (int w = 0; w < 6; w++) {   // six walks, short and long, each filling its ribbon
      Game::Ribbon& r = m.ribbons[m.nextRibbon];
      r = Game::Ribbon{};
      const int slivers = 9 + w * 37;
      uint32_t x = 12345u + w * 977u;
      int ward = w % 3;
      for (int i = 0; i < slivers; i++) {   // wards change every few slivers, as streets do
        x = x * 1103515245u + 12345u;
        if ((x >> 16) % 9 == 0) ward = (int)((x >> 20) % 4);
        r.push(Game::Ribbon::sliver(ward, (int)((x >> 8) % 7), (x >> 12) % 17 == 0, (x >> 4) % 29 == 0));
      }
      if (w == 2 || w == 5) r.marks |= Game::Ribbon::MarkFive;
      m.nextRibbon = (uint8_t)((m.nextRibbon + 1) % Game::Moments::MaxRibbons);
      m.nRibbons++;
    }
  }
  shot("game_found");
  keys("E");
  keys("RRD");
  shot("game_found_marks");
  keys("B");
  keys("DE");
  keys("DDDD");
  shot("game_found_wells");
  keys("D");
  shot("game_found_wells_blank");
  keys("B");
  keys("DE");
  shot("game_found_namesakes");
  keys("B");
  keys("DE");
  shot("game_found_words");
  keys("R");
  shot("game_found_words_hedge");
  keys("R");
  shot("game_found_words_hearth");
  keys("B");
  keys("DE");
  keys("RRRRR");
  shot("game_found_tokens");
  keys("B");
  keys("DE");
  shot("game_found_walks");
  keys("B");
  keys("R");
  shot("game_streets_locked");
  {
    const uint16_t was = App::game.career.pages;
    App::game.career.pages |= 1u << 7;
    shot("game_streets");
    // Three Nights on one stretch: ADD under the last entry.
    Game::Moments& m = App::game.moments;
    m.keepName = Game::packName(0x0B5Fu, 1);
    keys("LR");
    shot("game_streets_add");
    keys("E");
    shot("game_streets_initials");
    keys("NP");
    shot("game_streets_suggested");
    for (const char* k = "COAL YARD"; *k; k++) {   // typed at an uneven pace
      Press p;
      p.any = true;
      p.key = *k;
      frame(p);
      for (int i = 0; i < 2 + (*k % 5); i++) frame();
    }
    shot("game_streets_typing");
    keys("E");
    shot("game_streets_own");
    m.streets[m.nStreets] = m.streets[0];   // a second one, crossed out
    memcpy(m.streets[m.nStreets].street, "HALF MOON ST", 12);
    m.streets[m.nStreets].color = UI::rgb(150, 210, 160);
    m.streets[m.nStreets].advance = 5;
    m.streets[m.nStreets].wobble = 0;
    m.streets[m.nStreets].week = 2;
    m.streets[m.nStreets++].crossed = 1;
    m.keepName = Game::packName(0x1234u, 0);
    keys("LR");
    shot("game_streets_more");
    keys("UU");
    keys("E");
    shot("game_streets_cross");
    App::toastT = 0;
    m.nStreets = 0;
    m.keepName = 0;
    App::game.career.pages = was;
  }
  keys("R");
  keys("E");
  shot("game_page");
  // Spells, with a few to choose from.
  App::game.run.spells[App::game.run.nSpells++] = Cat::Spell_haste;
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Spells);
  keys("h");
  shot("game_spells");
  keys("B");
  // Out of visits.
  App::game.run.n.used = App::game.run.n.visits;
  App::game.run.n.total = App::game.run.n.tithe * 0.7;
  App::game.run.coin = 9;
  App::game.phase = Game::Phase::Night;
  App::go(App::Mode::OutOfVisits);   // what the scanner opens on its next tick
  frame();
  shot("game_out_of_visits");
  keys("DDDE");   // down to the last choice: let the Binding break
  shot("game_lost");
  keys("E");
  shot("game_reveal", 3);
  keys("E");
  shot("game_bind_again");
  // A won week.
  App::go(App::Mode::Won);
  shot("game_won", 0.7f);
  printf("playthrough done: %u weeks in the career, %u unlocks waiting\n", (unsigned)App::game.career.runs,
         (unsigned)App::game.career.nQueue);
  qa();
}

// The states the QA pass changed, each as a shot: guards, overprints, footers, first-time notes.
void qa() {
  Game::Run& r = App::game.run;
  // A fresh week on the fake street.
  App::newGame();
  App::game.career.runs = 1;   // a second week: the TRIAL row and the Bind line about coin
  App::game.career.wins = 1;
  App::go(App::Mode::Bind);
  keys("DD");
  shot("qa_bind_trial");
  keys("D");
  shot("qa_bind_line");
  keys("E");
  keys("E");
  App::go(App::Mode::Scanner);
  frame();
  st.place = WalkFormat::Place::Transit;
  st.log = Status::Log::Failed;
  shot("qa_scanner_footer_transit_err");
  st.log = Status::Log::On;
  shot("qa_scanner_footer_transit");
  st.place = WalkFormat::Place::Street;
  {   // the scanner with no week bound
    const uint8_t active = r.active;
    r.active = 0;
    frame();
    shot("qa_scanner_noweek");
    r.active = active;
  }
  // The menu's order in a week, and the give-up question.
  App::go(App::Mode::Menu);
  shot("qa_menu");
  App::sel = App::nMenu - 1;
  keys("E");
  shot("qa_menu_giveup_ask");
  keys("B");
  shot("qa_scanner_after_giveup_esc");   // no red toast left behind
  // The well page: the < > WELL hint, with and without a gyro, and a lost well.
  App::wellId = st.starId;
  App::go(App::Mode::Well);
  st.gyro = true;
  frame();
  shot("qa_well_footer");
  st.gyro = false;
  {
    WellRec* w = sky.edit(App::wellId);
    const int8_t sig = w->sig;
    w->sig = -120;
    frame();
    shot("qa_well_lost");
    w->sig = sig;
  }
  // The visit: ESC BACK in the footer, the toast above it, a favor's first-time note.
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  frame();
  shot("qa_visit");
  App::toast("TOO FAR: WALK CLOSER", UI::Rec);
  shot("qa_visit_toast");
  App::toastT = 0;
  {
    App::game.moments.taught = 0;
    r.jobs[0] = Game::Job{};
    r.jobs[1] = Game::Job{};
    App::go(App::Mode::Visit);
    App::buildActions(ctx());
    bool favor = false;
    for (int i = 0; i < App::nActions; i++) favor |= App::actions[i].act == App::Act::Job;
    if (favor) frame();
    else App::toast("A FAVOR: A SMALL JOB, +4c WHEN DONE", UI::Text, 2.5f);   // no favor at this well: the line as it would look
    shot("qa_teach_favor");
    App::toastT = 0;
    App::nToastNext = 0;
  }
  // The draw's ESC STOP, and the toast when you do.
  App::sel = 0;
  frame(key('E'));
  for (int i = 0; i < 8; i++) frame();
  shot("qa_drawing");
  frame(key('B'));
  shot("qa_drawing_stopped");
  App::toastT = 0;
  // The show's leave hints, mid and done.
  frame(key('E'));
  wait(3.0f);
  App::buildActions(ctx());
  for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Sell && App::actions[i].ok) App::sel = i;
  frame(key('E'));
  frame(); frame();
  shot("qa_show_mid");
  while (App::showDoneT < 0) frame();
  wait(2);
  shot("qa_show_done");
  frame(key('E'));   // one ENTER leaves
  printf("after ENTER on a finished show: mode %d (scanner is %d)\n", (int)App::mode, (int)App::Mode::Scanner);
  // The stall: the three two-press guards, and a withered relic with a long name.
  App::game.phase = Game::Phase::Night;
  r.n.met = 1;
  r.coin = 40;
  App::closeNight(81);
  App::modeT = 30;
  frame();
  for (int k = 0; k < 8 && App::mode == App::Mode::NightEnd; k++) { frame(key('E')); frame(); }   // past the tally and any Portent to choose
  if (App::mode == App::Mode::Shop) {
    frame(key('B'));
    shot("qa_shop_leave_ask");
    App::toastT = 0;
    r.nRelics = 0;
    Game::addRelic(r, Cat::R_midnight_engine, Cat::Edition_prismatic);
    r.relics[0].tier = 3;
    r.relics[0].withered = 2;
    Game::addRelic(r, Cat::R_seven_league_boots, Cat::Edition_lit);
    r.relics[1].tier = 1;
    Game::addRelic(r, Cat::R_laundromat_token);
    r.relics[2].withered = 1;
    keys("RR");
    shot("qa_shop_yours_withers");
    frame(key('X'));
    shot("qa_shop_yours_sell_ask");
    App::toastT = 0;
    keys("D");
    frame(key('E'));
    shot("qa_shop_yours_polish_ask");
    App::toastT = 0;
    keys("LL");
    App::game.shop.relics[0].sold = 1;
    shot("qa_shop_bought_row");
  } else {
    printf("no stall after the Night: shop shots skipped (mode %d)\n", (int)App::mode);
  }
  // The dawn: the token's kind, the mercy line, the first-time token note.
  App::game.moments.taught = 0;
  App::game.lastEnd.mercy = 1;
  App::game.moments.lastToken = Game::T_Laundromat + 1;
  App::go(App::Mode::NightEnd);
  App::modeT = 30;
  frame();
  shot("qa_night_end_token");
  App::toastT = 0;
  App::nToastNext = 0;
  // A free relic with every slot full: the relic, the list to sell from, leaving it.
  {
    const Game::NightEnd keepEnd = App::game.lastEnd;
    const Game::Run keepRun = r;
    const int fill[] = {Cat::R_ember_caller, Cat::R_tide_caller, Cat::R_gale_caller, Cat::R_porch_light,
                        Cat::R_rush_hour, Cat::R_hearthstone, Cat::R_mirror_glass};
    for (int id : fill) if (Game::slotsUsed(r) < Game::relicSlots(r) && !Game::owns(r, id)) Game::addRelic(r, id);
    r.relics[1].tier = 2;
    App::game.lastEnd = Game::NightEnd{};
    App::game.lastEnd.paid = true;
    App::game.lastEnd.spare = 7;
    App::game.lastEnd.trade = 9;
    App::game.lastEnd.portents = 1;
    App::game.lastEnd.got[0] = Game::PortentResult{Cat::Portent_gift, Cat::R_devils_meter, 0, -1};
    App::game.moments.portent = Game::PortentChoice{};
    App::game.moments.lastToken = 0;
    App::game.moments.swap = Game::GiftSwap{{(uint8_t)(Cat::R_devils_meter + 1), 0, 0}, {-1, -1, -1}, {2, -1, -1}};
    App::go(App::Mode::NightEnd);
    App::modeT = 30;
    frame();
    shot("qa_gift_room");
    frame(key('E'));
    shot("qa_gift_pick");
    frame(key('B'));
    frame(key('B'));
    shot("qa_gift_leave_ask");
    frame(key('B'));
    App::toastT = 0;
    App::nToastNext = 0;
    App::modeT = 30;
    frame();
    shot("qa_gift_left");
    App::game.lastEnd = keepEnd;
    r = keepRun;
    App::game.moments.swap = Game::GiftSwap{};
    App::giftPicking = false;
    App::giftAskLeave = false;
  }
  // Out of visits: the coin in the corner and B.
  r.n.used = r.n.visits;
  r.n.met = 0;
  r.n.total = r.n.tithe * 0.6;
  r.coin = 7;
  App::game.phase = Game::Phase::Night;
  App::go(App::Mode::OutOfVisits);
  frame();
  shot("qa_out_of_visits");
  frame(key('B'));
  shot("qa_out_of_visits_build");
  frame(key('B'));
  printf("ESC from the build opened by OUT OF VISITS: mode %d (out of visits is %d)\n", (int)App::mode, (int)App::Mode::OutOfVisits);
  // The won screen's safe default, and the reveal's line.
  App::go(App::Mode::Won);
  shot("qa_won", 0.7f);
  App::game.career.nQueue = 21;
  App::go(App::Mode::Reveal);
  shot("qa_reveal", 3);
  {   // two unlocks with long texts and four new pages: the pages that don't fit are counted
    Game::Career& car = App::game.career;
    const auto keepCar = car;
    const uint16_t keepBefore = App::pagesBefore;
    car.nRevealed = 2;
    car.revealed[0] = Game::Career::Pending{(uint8_t)Game::Kind::Relic, 0};
    car.revealed[1] = Game::Career::Pending{(uint8_t)Game::Kind::Relic, 1};
    App::pagesBefore = 0;
    car.pages = 0x0F;
    App::go(App::Mode::Reveal);
    App::modeT = 3;   // both unlocks shown
    shot("qa_reveal_more_pages", 3);
    car.nQueue = 0;
    shot("qa_reveal_more_pages_noqueue", 3);
    car = keepCar;
    App::pagesBefore = keepBefore;
  }
  App::game.career.nQueue = 0;
  // Your build: STANDING's labels and a long TONIGHT'S TITHE line.
  r.standing[0] = 12; r.standing[1] = 3; r.standing[2] = -2; r.standing[3] = 0;
  r.pact = 1;
  r.night = 6;
  r.n.omen = Cat::Omen_blood_price;
  r.n.eclipse = Cat::Eclipse_hunger;
  r.owed = 1234;
  Game::addRelic(r, Cat::R_blood_moon);
  App::back = App::Mode::Menu;
  App::go(App::Mode::Build);
  App::tab = App::BuildYou;
  App::sel = 0;
  int rows = App::buildRows();
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "STANDING")) App::sel = i;
  shot("qa_build_standing");
  App::tab = App::BuildTonight;
  App::sel = 0;
  rows = App::buildRows();
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "TONIGHT'S TITHE")) App::sel = i;
  shot("qa_build_tithe_long");
  App::tab = App::BuildWeek;
  shot("qa_build_week_tonight");
  {   // WARES with one of each kind
    const Game::Run keep = r;
    r.trinkets[r.nTrinkets++] = 0;
    r.spells[r.nSpells++] = Cat::Spell_glamour;
    r.glyphTokens[r.nGlyphTokens++] = Cat::Glyph_lit;
    App::tab = App::BuildWares;
    App::sel = 1;
    shot("qa_build_wares");
    r = keep;
  }
  r.night = 0;
  // Spells: TRANSMUTE's two lines, and the footer with none.
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_transmute;
  r.spells[r.nSpells++] = Cat::Spell_echo;
  App::back = App::Mode::Menu;
  App::go(App::Mode::Spells);
  shot("qa_spells_long");
  r.nSpells = 0;
  shot("qa_spells_none");
  // The atlas: a well on six Nights that is settling, and the header's count.
  {
    const WellRec* list[Sightings::MaxWells];
    const int n = sky.listed(list, Sightings::MaxWells);
    for (int i = 0; i < n && i < 6; i++) {
      Game::AtlasEntry& a = App::game.atlas.touch(list[i]->id, list[i]->seed, r.runNo, 0);
      a.visits = i == 0 ? 12 : 0;   // one worked well, five only drawn from
      if (i == 0) { a.nights = 0x3F; a.lastPlay = Game::playMinNow; a.drawnNight = 0; }
    }
    App::game.atlas.unique = 1;
  }
  App::go(App::Mode::Atlas);
  App::atlasSort = 0;
  frame();
  shot("qa_atlas_long");
  // Records on a new career: the dashes.
  App::go(App::Mode::Grimoire);
  keys("RR");
  shot("qa_records");
  keys("R");
  shot("qa_heard");
  // FOUND: the list's lines, the marks' captions, the words' pages, the ribbons' labels.
  keys("R");
  shot("qa_found_list");
  keys("DDD");
  shot("qa_found_list_words");
  {
    Game::Moments& m = App::game.moments;
    m.nMarks = 0;
    m.marks[m.nMarks++] = Game::Mark{1234, 0, 1, 0, 2};
    for (int i = 0; i < 64; i += 2) Game::setBit(m.words, App::wordBit(0, i));
    m.nRibbons = m.nextRibbon = 0;
    for (int w = 0; w < 4; w++) {
      Game::Ribbon& rb = m.ribbons[m.nextRibbon];
      rb = Game::Ribbon{};
      rb.night = (uint8_t)(w == 2 ? 0 : 1 + w);   // one walk outside a week
      if (w != 1) for (int i = 0; i < 20 + w * 9; i++) rb.push(Game::Ribbon::sliver((i / 7) % 4, i % 5, i % 9 == 0, i % 13 == 0));   // the second too short to draw
      m.nextRibbon = (uint8_t)((m.nextRibbon + 1) % Game::Moments::MaxRibbons);
      m.nRibbons++;
    }
    m.tokens[Game::T_Laundromat] = 2;
    m.lastToken = Game::T_Laundromat + 1;
  }
  keys("UUU");
  keys("E");
  shot("qa_found_marks");
  keys("B");
  keys("DDDE");
  shot("qa_found_words_1");
  keys("R");
  shot("qa_found_words_2");
  keys("R");
  shot("qa_found_words_hedge");
  keys("B");
  keys("DE");
  shot("qa_found_tokens_last");
  keys("B");
  keys("DE");
  shot("qa_found_walks");
  keys("B");
  // STREETS: the suggestion as a placeholder, an empty name refused, ESC's toast.
  keys("R");
  App::game.career.pages |= 1u << 7;
  App::game.moments.keepName = Game::packName(0x0B5Fu, 1);
  App::game.moments.nStreets = 0;
  keys("LR");
  keys("E");
  keys("NP");
  shot("qa_streets_placeholder");
  {
    Press del;
    del.any = true;
    del.del = true;
    frame(del);
    frame(key('E'));   // an empty name: refused
    shot("qa_streets_empty_refused");
    frame(key('B'));
    shot("qa_streets_cancel");
    App::toastT = 0;
  }
  // The recap card when the loudest well is behind you.
  r.n.used = 0;
  App::go(App::Mode::Scanner);
  App::recapCount(sky, 0, App::recap);
  App::recap.fresh = 9;
  App::recap.hidden = 1;
  App::recap.wardKinds = 2;
  App::recap.card = true;
  if (WellRec* w = sky.edit(App::recap.loudId)) w->sig = -120;
  frame();
  shot("qa_recap_behind");
  frame(key('G'));   // G is the glass's, and the card stays
  shot("qa_recap_glass");
  frame(key('B'));
  App::toastT = 0;
  App::nToastNext = 0;
  App::recap.card = false;
  // The first-time notes as they look: one held note on the scanner, and one after OFF FOR.
  App::go(App::Mode::Scanner);
  App::toast("A GLYPH DRIFTS BY. PRESS G, HOLD STILL", UI::Text, 2.0f);
  shot("qa_teach_glyph");
  App::toastT = 0;
  App::game.moments.taught = 0;
  App::noteOff(725, 14);
  shot("qa_off_for");
  App::toastT = 0;
  frame();
  shot("qa_off_for_teach");
  App::toastT = 0;
  App::nToastNext = 0;
  // The longest of the lines that say what the Deck did on its own.
  {
    static const char* const lines[] = {
      "OLD SAVE SET ASIDE. NEW CAREER", "SAVE DAMAGED: LOADED THE CARD COPY", "LOADED THE CARD COPY: IT WAS NEWER",
      "THIS SAVE WAS MADE ON ANOTHER DECK", "SAVE FAILED: ONLY THE CARD HAS IT", "CARD BACKUP STOPPED. PRESS R",
      "COULDN'T TELL HOW LONG IT WAS OFF", "STREETS FULL: CROSS ONE OUT FOR ROOM", "ATLAS FULL: LEAST-VISITED WELLS FADE",
      "KEPT. THE OLDEST MARK FADED", "YOU ARE A TITHE-KEEPER NOW", "NO WARD LEFT TONIGHT",
    };
    static const char* const names[] = {
      "qa_toast_set_aside", "qa_toast_card_copy", "qa_toast_card_newer", "qa_toast_other_deck", "qa_toast_save_failed",
      "qa_toast_backup_stopped", "qa_toast_off_unknown", "qa_toast_streets_full", "qa_toast_atlas_full",
      "qa_toast_mark_faded", "qa_toast_rank", "qa_toast_no_ward",
    };
    for (int i = 0; i < 12; i++) {
      App::toast(lines[i], i < 6 ? UI::Rec : UI::Text, 3.0f);
      shot(names[i]);
    }
    App::toastT = 0;
  }
  // The glass with a toast: above the footer.
  App::go(App::Mode::Glass);
  App::toast("...nobody Warded sent them...", UI::EnergyColor[(int)Energy::Hush], 3.5f);
  shot("qa_glass_toast", 1.3f);
  App::toastT = 0;
  estimates();
  stall();
  dawn();
  silent();
  growth();
}

// A fresh week bound straight from the Bind screen's choices, standing on the
// scanner at Night 1.
static void bindFresh(int calling, int pact) {
  App::newGame();
  App::game.career.runs = 3;
  App::bindCalling = calling;
  App::bindPact = pact;
  App::bindChallenge = -1;
  App::startRun();
  keys("E");
  App::go(App::Mode::Scanner);
  frame();
}

// Close the Night as paid and walk into the stall. False if the stall didn't open.
static bool intoStall() {
  Game::Run& r = App::game.run;
  App::game.phase = Game::Phase::Night;
  r.n.met = 1;
  r.coin = 40;
  App::closeNight(81);
  App::modeT = 30;
  frame();
  for (int k = 0; k < 8 && App::mode == App::Mode::NightEnd; k++) { frame(key('E')); frame(); }   // past the tally and any Portent to choose
  if (App::mode != App::Mode::Shop) printf("no stall after the Night: estimate shots skipped (mode %d)\n", (int)App::mode);
  App::toastT = 0;   // the dawn's first-time note, not the stall's
  App::nToastNext = 0;
  return App::mode == App::Mode::Shop;
}

// The Night-sized numbers: the offerings-to-go gauge and the Portent notches
// on the Tithe bar, ATTUNED as the show's first step, the atlas's estimates,
// the stall's Omen and its star priced in offerings (on the shelf, and on
// YOURS when a Polish wins), the Diviner's two Nights ahead; then a quiet Pact.
void estimates() {
  Game::Run& r = App::game.run;
  bindFresh(Cat::Calling_wayfarer, 0);
  for (int e = 0; e < Game::EnergyN; e++) r.attune[e] = 2;   // every offering shows ATTUNED +2
  // The show, a step in: ATTUNED first.
  App::wellId = st.starId;
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  App::sel = 0;
  frame(key('E'));
  wait(3.5f);   // the draw, and the drawn screen after it
  App::buildActions(ctx());
  for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Sell && App::actions[i].ok) { App::sel = i; break; }
  frame(key('E'));
  wait(0.45f);
  shot("qa_show_attuned");
  wait(4);
  frame(key('E'));
  // The scanner after the first offering: the gauge and three notches.
  App::go(App::Mode::Scanner);
  frame();
  shot("qa_scanner_gauge");
  // ...behind: more offerings needed than visits left. The Tithe stays up, so the Night stays open below.
  r.n.tithe = 2000;
  frame();
  shot("qa_scanner_gauge_behind");
  // Two more offerings, then the atlas: each known well's pay for the hold, sorted by it.
  for (int k = 0; k < 2; k++) { visitStar(); along += 3; fakeStreet(3); frame(); }
  while (App::mode == App::Mode::Show) frame(key('E'));
  {
    const WellRec* list[Sightings::MaxWells];
    const int n = sky.listed(list, Sightings::MaxWells);
    if (n > 0) {   // a glyph on one row, to see the columns sit together
      Game::AtlasEntry* a = App::game.atlas.find(list[0]->id);
      if (a) a->glyph = Cat::Glyph_echoing;
    }
  }
  r.hold[Game::Tide] = 6;
  App::back = App::Mode::Menu;
  App::go(App::Mode::Atlas);
  App::atlasSort = 4;
  frame();
  shot("qa_atlas_pays");
  App::go(App::Mode::Scanner);
  frame();
  shot("qa_scanner_gauge_later");
  // The stall: the Omen ahead (the longest name), and the star priced in offerings.
  r.omens[1] = (uint8_t)(Cat::Omen_tangled + 1);
  if (intoStall()) {
    Game::Shop& s = App::game.shop;
    if (s.star >= 0) App::sel = s.star;
    shot("qa_shop_omen_star");
    // A Polish that beats the shelf: the star moves to YOURS.
    r.nRelics = 0;
    Game::addRelic(r, Cat::R_leyline);
    s.nRelics = 1;
    s.relics[0] = Game::relicOffer(Cat::R_hermits_lamp, -1, -1);
    Game::starBest(s, r, App::game.recent);
    printf("polish star: star %d starPolish %d\n", s.star, (int)s.starPolish);
    App::sel = 0;
    shot("qa_shop_star_on_yours");
    keys("RR");
    shot("qa_shop_polish_star");
    keys("LL");
  }
  // The Diviner: two Nights ahead at the Night's start, and in THE REST.
  bindFresh(Cat::Calling_diviner, 0);
  r.omens[1] = (uint8_t)(Cat::Omen_tangled + 1);
  r.omens[2] = (uint8_t)(Cat::Omen_silent_bell + 1);
  App::go(App::Mode::NightStart);
  shot("qa_night_diviner");
  keys("E");
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Build);
  keys("R");
  shot("qa_build_omens_ahead");
  // A quiet Pact: no gauge, no notches, no Omen or star at the stall.
  bindFresh(Cat::Calling_wayfarer, Game::rules.quietRung);
  visitStar();
  while (App::mode == App::Mode::Show) frame(key('E'));
  App::go(App::Mode::Scanner);
  frame();
  shot("qa_scanner_quiet");
  {   // the widest Tithe line: a five-digit Tithe still to go, 12 visits, 999 coin
    const Game::Run keep = r;
    r.n.tithe = 98765;
    r.n.total = 0;
    r.n.used = 0;
    r.coin = 999;
    frame();
    shot("qa_scanner_widest_tithe");
    st.unsaved = true;   // nothing keeps the game: the footer says so
    frame();
    shot("qa_scanner_not_saving");
    st.unsaved = false;
    r = keep;
    frame();
  }
  if (intoStall()) {
    r.nRelics = 0;
    for (int k = 0; k < Game::relicSlots(r); k++) Game::addRelic(r, Cat::R_crowd_feeder + k);
    r.coin = 40;
    App::sel = 0;
    shot("qa_shop_quiet");
  }
}

// The stall saying what it does: the shine, rerolls that take something, the
// header's labels, what can't be bought, sold or polished, and focus-only weeks.
static void clearToasts() {
  App::toastT = 0;
  App::nToastNext = 0;
}
static void stallShot(const char* name) {
  shot(name);
  clearToasts();
}

void stall() {
  Game::Run& r = App::game.run;
  Game::Shop& s = App::game.shop;
  bindFresh(Cat::Calling_wayfarer, 0);
  if (!intoStall()) return;
  // The shine's relic says whose shine it is (the dawn's own Portents cleared, so only these count).
  for (int i = 0; i < s.nRelics; i++) { s.relics[i].marks = 0; s.relics[i].half = 0; }
  s.relics[1].ed = Cat::Edition_prismatic;
  s.relics[1].marks |= Game::Mark_Shine;
  s.relics[0].half = 1;
  App::tab = App::TabRelics;
  App::sel = 1;
  App::top = 0;
  stallShot("stall_shine_details");
  // R asks first when it would take something away.
  frame(key('r'));
  stallShot("stall_reroll_ask_two");
  App::shopAsk = App::AskNone;
  s.relics[0].half = 0;
  s.relics[0].marks = 0;
  frame(key('r'));
  stallShot("stall_reroll_ask_shine");
  frame(key('r'));
  stallShot("stall_reroll_new_relics");
  // Free rerolls: the count in the footer, and the leave question.
  s.freeRerolls = 2;
  stallShot("stall_free_rerolls_footer");
  frame(key('B'));
  stallShot("stall_leave_free_rerolls");
  App::shopAsk = App::AskNone;
  s.freeRerolls = 0;
  // The header with everything on it: MERCY, BARGAIN, a big Tithe, lots of coin,
  // and a Charm ward that may cancel the Omen (and the Tithe it raises).
  r.night = 5;
  r.omens[5] = (uint8_t)(Cat::Omen_blood_price + 1);
  r.mods |= 1u << Cat::Mod_charm_ward;
  r.coin = 140;
  r.bargain = 1;
  s.mercy = 1;
  stallShot("stall_header_full");
  r.bargain = 0;
  s.mercy = 0;
  r.coin = 40;
  stallShot("stall_header_omen_maybe");
  r.mods &= ~(1u << Cat::Mod_charm_ward);
  r.night = 1;
  // Trinket slots full.
  r.nTrinkets = 0;
  r.trinkets[r.nTrinkets++] = Cat::Trinket_haggler;
  r.trinkets[r.nTrinkets++] = Cat::Trinket_pawn_ticket;
  s.trinket.kind = Game::OfferKind::Trinket;
  s.trinket.id = Cat::Trinket_lucky_coin;
  s.trinket.sold = 0;
  App::tab = App::TabWares;
  App::sel = s.nCons;
  frame(key('E'));
  stallShot("stall_trinket_full");
  r.nTrinkets = 0;
  // YOURS: nothing to polish, already at III, Polish lost on a sale, Copycat's target.
  r.nRelics = 0;
  Game::addRelic(r, Cat::R_keystone);
  Game::addRelic(r, Cat::R_seven_league_boots, Cat::Edition_lit);
  r.relics[1].tier = 3;
  Game::addRelic(r, Cat::R_copycat_sigil);
  App::tab = App::TabYours;
  App::sel = 0;
  frame(key('E'));
  stallShot("stall_polish_nothing");
  App::sel = 1;
  frame(key('E'));
  stallShot("stall_polish_max");
  frame(key('x'));
  stallShot("stall_sell_polish_lost");
  App::shopAsk = App::AskNone;
  App::sel = 2;
  stallShot("stall_copycat_yours");
  // Shoestring at the cap: the sale says what it really adds.
  r.challenge = 0;
  r.coin = 13;
  App::sel = 1;
  frame(key('x'));
  stallShot("stall_sell_cap_polish");
  App::shopAsk = App::AskNone;
  App::sel = 0;
  frame(key('x'));
  stallShot("stall_sell_cap");
  App::shopAsk = App::AskNone;
  r.challenge = -1;
  r.coin = 40;
  // Glass house: the crown isn't for sale.
  r.challenge = 3;
  r.nRelics = 0;
  Game::addRelic(r, Cat::R_glass_crown);
  Game::addRelic(r, Cat::R_crowd_feeder);
  App::sel = 0;
  frame(key('x'));
  stallShot("stall_crown_cant_sell");
  // Sell one to buy: the crown and a Phantom dimmed, withering marked, the details in full.
  Game::addRelic(r, Cat::R_seven_league_boots, Cat::Edition_phantom);
  Game::addRelic(r, Cat::R_laundromat_token);
  r.relics[3].withered = 2;
  Game::addRelic(r, Cat::R_midnight_engine, Cat::Edition_gilt);
  r.relics[4].tier = 2;
  for (int k = Game::slotsUsed(r); k < Game::relicSlots(r); k++) Game::addRelic(r, Cat::R_crowd_feeder + 1 + k);
  App::tab = App::TabRelics;
  App::sel = 0;
  s.relics[0].sold = 0;
  s.relics[0].ed = -1;
  frame(key('E'));
  App::sel = 4;
  stallShot("stall_drop_details");
  App::sel = 2;
  frame(key('E'));
  stallShot("stall_drop_phantom");
  App::sel = 0;
  frame(key('E'));
  stallShot("stall_drop_crown");
  App::sel = 1;
  r.coin = 60;
  frame(key('E'));
  stallShot("stall_drop_bought");
  r.challenge = -1;
  // 8 relics carried: Phantoms count.
  r.nRelics = 0;
  for (int i = 0; i < 3; i++) Game::addRelic(r, Cat::R_crowd_feeder + i, Cat::Edition_phantom);
  for (int i = 0; i < 5; i++) Game::addRelic(r, Cat::R_crowd_feeder + 3 + i);
  r.mods |= 1u << Cat::Mod_sixth_socket;
  for (int i = 0; i < s.nRelics; i++) s.relics[i].sold = 0;
  App::sel = 1;
  frame(key('E'));
  stallShot("stall_carry_limit");
  r.mods &= ~(1u << Cat::Mod_sixth_socket);
  // Attuner's fork: when it gives two.
  r.mods |= 1u << Cat::Mod_attuners_fork;
  App::tab = App::TabWares;
  for (int tries = 0; tries < 40 && strstr(App::toastText, "TWICE") == nullptr; tries++) {
    clearToasts();
    s.cons[0].kind = Game::OfferKind::Attune;
    s.cons[0].sold = 0;
    r.coin = 40;
    App::go(App::Mode::Shop);
    App::tab = App::TabWares;
    App::sel = 0;
    frame(key('E'));
    frame(key('E'));
  }
  shot("stall_attuned_twice");
  clearToasts();
  // Rare sight with no rare left to show.
  Game::stallNotes = Game::Note_NoRare;
  App::tab = App::TabRelics;
  App::sel = 0;
  frame();
  stallShot("stall_rare_sight_none");
  // One note: only the Caller, no R, and a mercy that brings only the Attunement.
  bindFresh(Cat::Calling_wayfarer, 0);
  r.challenge = 1;
  if (!intoStall()) return;
  r.shopFlags |= Game::SF_Mercy;
  App::openStall();
  frame();
  stallShot("stall_one_note_mercy");
  frame(key('r'));
  stallShot("stall_one_note_reroll");
  App::tab = App::TabWares;
  App::sel = 0;
  stallShot("stall_one_note_attune");
  // YOUR BUILD before a stall: the shine, BARGAIN and MERCY say what they really do.
  bindFresh(Cat::Calling_wayfarer, 0);
  r.shopFlags |= Game::SF_Shine | Game::SF_Mercy;
  r.bargain = 1;
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Build);
  App::tab = App::BuildTonight;
  const int rows = App::buildRows();
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "SHINE")) App::sel = i;
  stallShot("stall_build_shine");
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "BARGAIN")) App::sel = i;
  stallShot("stall_build_bargain");
}

// One dawn, drawn after its tally and with no first-time note over it.
static void dawnAt(const Game::NightEnd& e, const char* name) {
  App::game.lastEnd = e;
  App::go(App::Mode::NightEnd);
  App::modeT = 30;
  frame();
  App::toastT = 0;
  App::nToastNext = 0;
  shot(name);
}

// The dawn's lines (MERCY and a crumble always have room, every Portent says what it
// did), its waiting relic and spell, the Night start's notes, and the Lost screen's Tithe.
void dawn() {
  Game::Run& r = App::game.run;
  Game::Moments& m = App::game.moments;
  bindFresh(Cat::Calling_wayfarer, 0);
  App::game.phase = Game::Phase::NightEnd;
  r.night = 2;
  r.coin = 23;
  m.portent = Game::PortentChoice{};
  m.swap = Game::GiftSwap{};
  m.lastToken = Game::T_Laundromat + 1;
  m.dawn = Game::DawnNotes{};
  m.dawn.crumbled = Cat::R_lantern_bearer + 1;
  m.dawn.nCrumbled = 1;
  Game::NightEnd e{};
  e.paid = true;
  e.spare = 1;
  e.trade = 6;
  e.spareCoin = 1;
  e.reward = 5;
  e.mercy = true;
  e.withered = true;
  e.repaid = true;
  dawnAt(e, "qa_dawn_mercy_crumble");
  e.favors = 4;   // a long tally: the token line and the Collector make way
  e.interest = 2;
  e.gilt = 2;
  e.thread = 4;
  m.dawn.nCrumbled = 2;
  dawnAt(e, "qa_dawn_mercy_crumble_long");
  // Three Portents: Doubled interest's long name, a Gift named, a Chalk mark with 4 held.
  m.dawn = Game::DawnNotes{};
  e = Game::NightEnd{};
  e.paid = true;
  e.spare = 11;
  e.trade = 9;
  e.spareCoin = 11;
  e.reward = 5;
  e.interest = 4;
  e.portents = 3;
  e.got[0] = Game::PortentResult{Cat::Portent_doubled_interest, -1, 2, -1};
  e.got[1] = Game::PortentResult{Cat::Portent_gift, Cat::R_lantern_bearer, 0, -1};
  e.got[2] = Game::PortentResult{Cat::Portent_chalk_mark, -1, 0, -2};
  m.portent.interest = 2;
  dawnAt(e, "qa_dawn_three_portents");
  e.interest = 2;
  e.got[0] = Game::PortentResult{Cat::Portent_windfall, -1, 10, -1};
  e.got[1] = Game::PortentResult{Cat::Portent_conjunction, Cat::R_midnight_engine, 0, -1};
  e.got[2] = Game::PortentResult{Cat::Portent_scroll, -1, 0, Cat::Spell_haste};
  dawnAt(e, "qa_dawn_named");
  // One of two, with Doubled interest worth nothing on a Night 1 dawn.
  e = Game::NightEnd{};
  e.paid = true;
  e.spare = 6;
  e.trade = 3;
  e.spareCoin = 6;
  e.reward = 5;
  e.portents = 1;
  e.got[0] = Game::PortentResult{Cat::Portent_windfall, -1, 0, -1};
  m.portent = Game::PortentChoice{1, {(uint8_t)(Cat::Portent_doubled_interest + 1), 0, 0}, 0};
  App::sel = 1;
  dawnAt(e, "qa_dawn_choice_interest");
  App::sel = 0;
  m.portent = Game::PortentChoice{};
  // Shoestring: 3 coin before the dawn, 28 paid, 15 kept; the Windfall came at the cap.
  r.challenge = 0;
  r.coin = 15;
  m.lastToken = 0;
  m.dawn.capped = 16;
  e.spare = 7;
  e.trade = 6;
  e.spareCoin = 7;
  e.got[0] = Game::PortentResult{Cat::Portent_windfall, -1, 0, -1};
  dawnAt(e, "qa_dawn_shoestring");
  m.dawn = Game::DawnNotes{};
  // Empty pockets: the Night's relic with a long name, its Attunement, and ENTER's way on.
  r.challenge = 7;
  r.night = 3;
  e = Game::NightEnd{};
  e.paid = true;
  e.spare = 2;
  e.trade = 7;
  e.spareCoin = 2;
  e.reward = 5;
  e.mercy = true;   // no stall to go easy: no MERCY line
  e.gift = Cat::R_laundromat_token;
  m.lastToken = Game::T_Bus + 1;
  dawnAt(e, "qa_dawn_pockets");
  {   // a crowded one: three Portents and a long name, so the column cuts names to keep 4 lines
    Game::NightEnd c = e;
    c.mercy = false;
    c.spare = 11;
    c.spareCoin = 11;
    c.portents = 3;
    c.gift = Cat::R_smugglers_lining;
    c.got[0] = Game::PortentResult{Cat::Portent_doubled_interest, -1, 0, -1};
    c.got[1] = Game::PortentResult{Cat::Portent_tuning, -1, 0, -1};
    c.got[2] = Game::PortentResult{Cat::Portent_scroll, -1, 0, Cat::Spell_echo};
    dawnAt(c, "qa_dawn_pockets_crowded");
  }
  {   // every slot full: it waits on the free-relic screen
    const Game::Run keep = r;
    const int fill[] = {Cat::R_ember_caller, Cat::R_tide_caller, Cat::R_gale_caller, Cat::R_porch_light,
                        Cat::R_rush_hour, Cat::R_hearthstone, Cat::R_mirror_glass};
    for (int id : fill) if (Game::slotsUsed(r) < Game::relicSlots(r) && !Game::owns(r, id)) Game::addRelic(r, id);
    m.dawn.gift = Cat::R_laundromat_token + 1;
    m.dawn.giftEd = -1;
    dawnAt(e, "qa_dawn_pockets_swap");
    frame(key('B'));
    frame(key('B'));
    App::toastT = 0;
    App::nToastNext = 0;
    App::modeT = 30;
    frame();
    shot("qa_dawn_pockets_left");
    r = keep;
    m.dawn = Game::DawnNotes{};
  }
  r.challenge = -1;
  // The seventh dawn: a stall Portent only counts if you go Overnight.
  r.night = 7;
  e = Game::NightEnd{};
  e.paid = true;
  e.won = true;
  e.spare = 6;
  e.trade = 40;
  e.spareCoin = 6;
  e.reward = 5;
  e.interest = 5;
  e.portents = 1;
  e.got[0] = Game::PortentResult{Cat::Portent_rare_sight, -1, 0, -1};
  m.lastToken = Game::T_Ferry + 1;
  dawnAt(e, "qa_dawn_night7");
  r.won = 0;
  r.night = 2;
  // A Scroll with both spell slots full: the spell, the list to forget from, then the dawn.
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_glamour;
  r.spells[r.nSpells++] = Cat::Spell_hex;
  e = Game::NightEnd{};
  e.paid = true;
  e.spare = 6;
  e.trade = 8;
  e.spareCoin = 6;
  e.reward = 5;
  e.portents = 1;
  e.got[0] = Game::PortentResult{Cat::Portent_scroll, -1, 0, Cat::Spell_haste};
  m.lastToken = 0;
  m.swap = Game::GiftSwap{{(uint8_t)(Game::SwapSpell | (Cat::Spell_haste + 1)), 0, 0}, {-1, -1, -1}, {-1, -1, -1}};
  dawnAt(e, "qa_scroll_swap");
  frame(key('E'));
  shot("qa_scroll_pick");
  frame(key('E'));   // forget GLAMOUR
  shot("qa_scroll_forgotten_toast");
  App::toastT = 0;
  App::nToastNext = 0;
  App::modeT = 30;
  frame();
  shot("qa_scroll_taken");
  m.swap = Game::GiftSwap{};
  // The Night start: a ward that turned the Omen away, and the crown's refusal.
  bindFresh(Cat::Calling_wayfarer, 0);
  r.night = 3;
  Game::addRelic(r, Cat::R_glass_crown);
  r.n.omen = -1;
  r.n.crownBan = Game::Tide;
  m.dawn.turned = Cat::Omen_tangled + 1;
  App::go(App::Mode::NightStart);
  shot("qa_night_start_ward");
  // The seventh Night, the Eclipse held off.
  r.night = 6;
  r.n.eclipse = -1;
  m.dawn.turned = (uint8_t)((Cat::Eclipse_dim + 1) << 4);
  shot("qa_night_start_eclipse_ward");
  m.dawn.turned = 0;
  // Glitch: tonight's focus and shuffle, under an Omen with two lines of text.
  bindFresh(Cat::Calling_glitch, 0);
  r.night = 2;
  r.n.omen = Cat::Omen_iron_price;
  r.focus = Game::Gale;
  r.n.perm[0] = Game::Tide;
  r.n.perm[1] = Game::Gale;
  r.n.perm[2] = Game::Ember;
  App::go(App::Mode::NightStart);
  shot("qa_night_start_glitch");
  r.n.omen = -1;
  r.n.perm[0] = Game::Ember;
  r.n.perm[1] = Game::Gale;
  r.n.perm[2] = Game::Tide;
  shot("qa_night_start_glitch_calm");
  // Grand tour, lost: the doubled Tithe it really was.
  r.challenge = 8;
  r.n.tithe = 2350;
  r.n.total = 4100;
  r.n.wardKinds = 3;
  App::go(App::Mode::Lost);
  shot("qa_lost_grand_tour");
  r.challenge = -1;
  // The Contractor's Night 2: the loan in the Tithe's breakdown.
  bindFresh(Cat::Calling_contractor, 0);
  r.night = 1;
  Game::beginNight(r);
  App::back = App::Mode::Menu;
  App::go(App::Mode::Build);
  App::tab = App::BuildTonight;
  const int rows = App::buildRows();
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "TONIGHT'S TITHE")) App::sel = i;
  shot("qa_build_contractor");
}

// ---- What the rules did, said on screen ---------------------------------------
// A full hold, a relic turning a draw, forced draws, a zero offering's reason,
// the Wardens listening, the Grand tour, every step of the show, waiting
// spells, YOUR BUILD's relic lines, and tonight's energy on the scanner.

// The first well in reach (and not the one to skip), or 0.
static uint32_t wellInReach(uint32_t skip = 0) {
  static const WellRec* list[Sightings::MaxWells];
  const int n = sky.listed(list, Sightings::MaxWells);
  for (int i = 0; i < n; i++)
    if (sky.inRange(*list[i]) && list[i]->sig >= Game::ReachDbm && list[i]->id != skip) return list[i]->id;
  return 0;
}

// Open a visit at a well in reach, with the cursor on the first row of `act` (or energy `e`).
static void visitAt(uint32_t id, App::Act act, int e = -1) {
  if (Game::AtlasEntry* a = App::game.atlas.find(id)) a->soldNight = -1;   // settled
  App::wellId = id;
  App::echoMode = false;
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  for (int i = 0; i < App::nActions; i++)
    if (App::actions[i].act == act && (e < 0 || App::actions[i].arg == e)) { App::sel = i; break; }
}

// Offer energy `e` at the open well and play the show out. The well is taken
// as settled, so one well can show several offerings.
static void offer(int e, float secs = 4) {
  App::toastT = 0;
  App::nToastNext = 0;
  if (Game::AtlasEntry* a = App::game.atlas.find(App::wellId)) a->soldNight = -1;
  App::buildActions(ctx());
  for (int i = 0; i < App::nActions; i++)
    if (App::actions[i].act == App::Act::Sell && App::actions[i].arg == e && !App::actions[i].ok)
      printf("silent(): offering %d refused: %s\n", e, Game::whyText(App::actions[i].why));
  App::doSell(ctx(), e);
  wait(secs);
}

static void freshNight(int calling, int challenge = -1) {
  App::newGame();
  App::game.career.runs = 3;
  App::bindCalling = calling;
  App::bindPact = 0;
  App::bindChallenge = challenge;
  App::startRun();
  keys("E");
  App::go(App::Mode::Scanner);
  frame();
  App::toastT = 0;
  App::nToastNext = 0;
}

void silent() {
  Game::Run& r = App::game.run;
  freshNight(Cat::Calling_hedge_witch);   // starts with Static eater
  const uint32_t a = wellInReach(), b = wellInReach(a);
  // The draw row priced at what fits, and the draw that fills the hold.
  r.hold[Game::Gale] = 5;
  visitAt(a, App::Act::DrawOwn);
  shot("silent_visit_hold_room");
  {   // a draw that a relic turned, a shaky hand, and a full hold, all at once
    App::lastDraw = Game::Drawn{Game::Why::Ok, Game::Static, 3, 1, 4, Cat::R_static_eater};
    App::drawDone = true;
    App::go(App::Mode::Drawing);
    App::drawDone = true;
    shot("silent_drawn_turned");
    App::lastDraw = Game::Drawn{Game::Why::Ok, Game::Hush, 6, 0, 0, Cat::R_hush_lantern};
    shot("silent_drawn_hush");
  }
  r.hold[Game::Gale] = 0;

  // Pilgrim and TRANSMUTE: one draw row, in the energy it gives, with the reason under it.
  freshNight(Cat::Calling_pilgrim);
  visitAt(wellInReach(), App::Act::DrawOwn);
  shot("silent_visit_pilgrim");
  r.n.transmuteLeft = 3;
  r.n.transmuteE = Game::Gale;
  visitAt(wellInReach(), App::Act::DrawOwn);
  shot("silent_visit_transmute");
  App::drawDone = false;
  App::go(App::Mode::Drawing);
  for (int i = 0; i < 12; i++) frame();
  shot("silent_drawing_transmute", 0.6f);
  App::go(App::Mode::Scanner);

  // The long way: an old well's row at ~0 with why, and the show saying why.
  freshNight(Cat::Calling_wayfarer, 5);
  {
    const uint32_t id = wellInReach();
    r.hold[Game::Tide] = 6;
    visitAt(id, App::Act::Sell, Game::Tide);
    offer(Game::Tide);
    App::go(App::Mode::Scanner);
    r.night = 1;
    r.hold[Game::Tide] = 6;
    visitAt(id, App::Act::Sell, Game::Tide);
    shot("silent_visit_longway");
    offer(Game::Tide);
    shot("silent_show_longway");
    App::go(App::Mode::Scanner);
  }

  // Silent bell and Glass crown.
  freshNight(Cat::Calling_wayfarer);
  r.n.omen = Cat::Omen_silent_bell;
  r.hold[Game::Ember] = 6;
  visitAt(wellInReach(), App::Act::Sell, Game::Ember);
  shot("silent_visit_silent_bell");
  offer(Game::Ember);
  shot("silent_show_silent_bell");
  App::go(App::Mode::Scanner);
  r.n.omen = -1;
  Game::addRelic(r, Cat::R_glass_crown);
  r.n.crownBan = Game::Tide;
  r.hold[Game::Tide] = 6;
  r.n.glamour = 1;
  visitAt(wellInReach(), App::Act::Sell, Game::Tide);
  shot("silent_visit_crown");
  offer(Game::Tide, 3);
  shot("silent_show_crown");
  App::afterShow(ctx());
  frame();
  shot("silent_glamour_kept");
  App::go(App::Mode::Scanner);
  r.nRelics = 0;
  r.n.glamour = 0;

  // Heat 3 or more: any offering, warned.
  r.heat = 3.2;
  r.hold[Game::Ember] = 6;
  visitAt(wellInReach(), App::Act::Sell, Game::Ember);
  App::toastT = 0;
  App::nToastNext = 0;
  shot("silent_visit_heat");
  r.heat = 0;

  // Grand tour: what's still to go, and the offering that finishes it.
  freshNight(Cat::Calling_wayfarer, 8);
  r.n.wardKinds = 0x5;   // Hollow and Quarter done
  r.hold[Game::Ember] = 6;
  visitAt(wellInReach(), App::Act::Sell, Game::Ember);
  shot("silent_visit_tour");
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Build);
  keys("R");
  shot("silent_build_tour");
  r.n.wardKinds = (uint8_t)(0xF & ~(1u << (int)sky.ward()));   // this ward is the last one
  visitAt(wellInReach(), App::Act::Sell, Game::Ember);
  offer(Game::Ember);
  shot("silent_show_tour_done");
  App::go(App::Mode::Scanner);

  // The show's steps: the ward and Omen on the units, Still water, the Fade, Tangled lines, Copycat, an x0.
  freshNight(Cat::Calling_wayfarer);
  r.night = 2;
  r.n.omen = Cat::Omen_iron_price;
  Game::addRelic(r, Cat::R_tide_caller);
  Game::addRelic(r, Cat::R_porch_light);
  r.hold[Game::Tide] = 7;
  visitAt(wellInReach(), App::Act::Sell, Game::Tide);
  offer(Game::Tide);
  shot("silent_show_still_water");
  App::go(App::Mode::Scanner);
  r.n.omen = Cat::Omen_fade;
  r.n.fadeE = Game::Tide;
  r.hold[Game::Tide] = 7;
  App::go(App::Mode::Well);
  App::wellId = wellInReach(App::wellId);
  frame();
  shot("silent_well_fade");
  visitAt(App::wellId, App::Act::Sell, Game::Tide);
  offer(Game::Tide);
  App::showTop = 0;
  shot("silent_show_fade_top");
  App::go(App::Mode::Scanner);
  r.n.omen = Cat::Omen_tangled;
  r.nRelics = 0;
  Game::addRelic(r, Cat::R_hushed_step);
  Game::addRelic(r, Cat::R_crowd_feeder);
  Game::addRelic(r, Cat::R_overclock);
  Game::addRelic(r, Cat::R_copycat_sigil);
  r.hold[Game::Ember] = 6;
  visitAt(wellInReach(), App::Act::Sell, Game::Ember);
  offer(Game::Ember);
  shot("silent_show_tangled_copied");
  App::showTop = 0;
  shot("silent_show_tangled_top");
  App::go(App::Mode::Scanner);
  r.n.omen = -1;
  r.nRelics = 0;
  Game::addRelic(r, Cat::R_glass_crown);
  Game::addRelic(r, Cat::R_overclock);
  Game::addRelic(r, Cat::R_hungry_coin);
  r.n.crownBan = Game::Gale;
  r.hold[Game::Gale] = 6;
  visitAt(wellInReach(), App::Act::Sell, Game::Gale);
  offer(Game::Gale);
  shot("silent_show_stays_zero");
  App::go(App::Mode::Scanner);
  r.nRelics = 0;
  Game::addRelic(r, Cat::R_hungry_coin);
  r.hold[Game::Ember] = 2;
  visitAt(wellInReach(), App::Act::Sell, Game::Ember);
  offer(Game::Ember);
  shot("silent_show_hungry_coin");
  App::go(App::Mode::Scanner);

  // A favor finished by this offering: its coin and standing on the show, the offering's own coin in the footer.
  r.nRelics = 0;
  {
    const uint32_t id = wellInReach();
    const WellRec* w = sky.byId(id);
    r.jobs[0] = Game::Job{Game::Jobs::Wander, (uint8_t)Game::wellFaction(r, App::viewOf(*w, sky, st.nowMs)), 0, 1, 0, 5, 0, 0};
    r.hold[Game::Ember] = 5;
    visitAt(id, App::Act::Sell, Game::Ember);
    offer(Game::Ember);
    shot("silent_show_favor_done");
    App::go(App::Mode::Scanner);
  }

  // Spells: what's waiting, a second GLAMOUR refused, KINDLE and RECALL saying what they did, CHALK asking.
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_glamour;
  r.spells[r.nSpells++] = Cat::Spell_kindle;
  r.n.glamour = 1;
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Spells);
  frame(key('E'));
  shot("silent_spells_ready");
  App::toastT = 0;
  Game::addRelic(r, Cat::R_porch_light);
  keys("D");
  frame(key('E'));
  shot("silent_toast_kindle");
  App::toastT = 0;
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_recall;
  r.lastSpell = Cat::Spell_haste;
  App::go(App::Mode::Spells);
  frame(key('E'));
  shot("silent_toast_recall");
  App::toastT = 0;
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_augur;
  r.n.omen = -1;
  App::go(App::Mode::Spells);
  frame(key('E'));
  shot("silent_toast_no_omen");
  App::toastT = 0;
  r.n.omen = Cat::Omen_short_night;
  App::go(App::Mode::Spells);
  frame(key('E'));
  shot("silent_toast_augur");
  App::toastT = 0;
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_chalk;
  {
    App::wellId = wellInReach();
    const WellRec* w = sky.byId(App::wellId);
    Game::AtlasEntry& e = App::game.atlas.touch(w->id, w->seed, r.runNo, 0);
    e.glyph = Cat::Glyph_echoing;
    App::back = App::Mode::Visit;
    App::go(App::Mode::Spells);
    frame(key('E'));
    shot("silent_chalk_ask");
    App::toastT = 0;
    e.glyph = -1;
  }

  // YOUR BUILD's relics: the Sleeper's halved Polish, what Copycat copies, growth as x and +, a withering.
  r.nRelics = 0;
  Game::addRelic(r, Cat::R_porch_light);
  r.relics[0].tier = 3;
  Game::addRelic(r, Cat::R_copycat_sigil);
  Game::addRelic(r, Cat::R_black_ledger);
  r.state[Cat::R_black_ledger] = 1.2;
  Game::addRelic(r, Cat::R_patience, -1, 2);
  r.state[Cat::R_patience] = 140;
  Game::addRelic(r, Cat::R_hush_lantern);
  r.n.noPolish = 1;
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Build);
  shot("silent_build_relics");
  App::sel = 0;
  shot("silent_build_sleeper");
  App::sel = 1;
  shot("silent_build_copycat");
  r.n.noPolish = 0;
  keys("R");
  for (int i = 0; i < 30; i++) keys("D");
  keys("UU");
  shot("silent_build_heat");

  // Tonight's energy on the scanner, the glass and a well's page (a Static storm).
  freshNight(Cat::Calling_wayfarer, 4);
  App::go(App::Mode::Scanner);
  frame();
  shot("silent_scanner_storm");
  App::wellId = wellInReach();
  App::go(App::Mode::Well);
  frame();
  shot("silent_well_storm");
  App::go(App::Mode::Glass);
  frame();
  shot("silent_glass_storm", 1.3f);
  App::go(App::Mode::Scanner);
  // The hold's leak.
  r.hold[Game::Ember] = 4;
  frame();
  App::toastT = 0;
  App::nToastNext = 0;
  App::game.playMs = r.holdLeakMs + 15 * 60000;
  frame();
  shot("silent_toast_leak");
  App::toastT = 0;

  // ECHO: the picker priced the way the offering will be, and the visit through it.
  freshNight(Cat::Calling_wayfarer);
  for (int k = 0; k < 2; k++) {
    r.hold[Game::Tide] = 5;
    visitAt(wellInReach(k ? wellInReach() : 0), App::Act::Sell, Game::Tide);
    offer(Game::Tide);
    App::go(App::Mode::Scanner);
  }
  for (int i = 0; i < App::game.atlas.count; i++) App::game.atlas.e[i].soldNight = -1;   // settled
  r.hold[Game::Tide] = 6;
  r.n.echo = 1;
  App::back = App::Mode::Visit;
  App::echoFrom = 0;
  App::go(App::Mode::Atlas);
  frame();
  shot("silent_atlas_echo");
  frame(key('E'));
  shot("silent_visit_echo");
  App::echoMode = false;
  r.n.echo = 0;
  App::go(App::Mode::Scanner);

  // Out of visits: RECALL on the HASTE row, and no loans Overnight.
  freshNight(Cat::Calling_wayfarer);
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_recall;
  r.lastSpell = Cat::Spell_haste;
  r.n.used = r.n.visits;
  r.n.total = r.n.tithe * 0.4;
  App::go(App::Mode::OutOfVisits);
  App::sel = 2;
  shot("silent_out_recall");
  r.night = 8;
  App::sel = 1;
  shot("silent_out_overnight_loan");
  App::go(App::Mode::Scanner);
}

// Growing relics, Iron and Ash (Nov 2026): the Pacts' texts, Ash's warning and step, a shelf of
// growing relics and how far each has come in YOUR BUILD.
void growth() {
  Game::Run& r = App::game.run;
  App::newGame();
  App::game.career.seals[Cat::Calling_wayfarer] = 6;
  App::bindCalling = Cat::Calling_wayfarer;
  App::go(App::Mode::Bind);
  App::sel = 1;
  App::bindPact = 2;
  frame();
  shot("growth_bind_iron");
  App::bindPact = 3;
  frame();
  shot("growth_bind_ash");

  // Ash: Tide twice in a row.
  freshNight(Cat::Calling_wayfarer);
  r.pact = 3;
  const uint32_t a = wellInReach(), b = wellInReach(a);
  r.hold[Game::Tide] = 6;
  visitAt(a, App::Act::Sell, Game::Tide);
  offer(Game::Tide);
  App::go(App::Mode::Scanner);
  r.hold[Game::Tide] = 6;
  r.hold[Game::Ember] = 4;
  visitAt(b, App::Act::Sell, Game::Tide);
  App::toastT = 0;
  App::nToastNext = 0;
  shot("growth_visit_ash");
  offer(Game::Tide);
  shot("growth_show_ash");
  App::go(App::Mode::Scanner);

  // A shelf of growing relics, and one's details.
  bindFresh(Cat::Calling_wayfarer, 0);
  if (intoStall()) {
    Game::Shop& s = App::game.shop;
    const int ids[] = {Cat::R_pedometer, Cat::R_mixtape, Cat::R_rain_gauge, Cat::R_prism_jar};
    for (int k = 0; k < 4 && k < s.nRelics; k++) { s.relics[k].id = (uint8_t)ids[k]; s.relics[k].ed = -1; s.relics[k].marks = 0; s.relics[k].half = 0; s.relics[k].withered = -1; }
    App::tab = App::TabRelics;
    App::top = 0;
    App::sel = 1;
    Game::starBest(s, r, App::game.recent);   // a growing relic adds nothing yet, so no star
    stallShot("growth_stall_mixtape");
    App::sel = 3;
    stallShot("growth_stall_prism_jar");
    const int held[] = {Cat::R_pedometer, Cat::R_mixtape, Cat::R_prism_jar};
    const double grown[] = {7.2, 15, 1.2};
    for (int k = 0; k < 3; k++) { Game::addRelic(r, held[k]); r.state[held[k]] = grown[k]; }
    r.relics[r.nRelics - 1].tier = 1;
    App::tab = App::TabYours;
    App::sel = 0;
    stallShot("growth_stall_yours");
    App::go(App::Mode::Scanner);
  }

  // YOUR BUILD: how far each has come.
  freshNight(Cat::Calling_wayfarer);
  const int held[] = {Cat::R_pedometer, Cat::R_mixtape, Cat::R_rain_gauge, Cat::R_prism_jar, Cat::R_seed_vault};
  const double grown[] = {12.6, 21, 9, 1.5, 2.1};
  for (int k = 0; k < 5; k++) { Game::addRelic(r, held[k]); r.state[held[k]] = grown[k]; }
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Build);
  App::tab = 0;
  App::sel = 3;
  frame();
  shot("growth_build");
}

} // namespace Play
