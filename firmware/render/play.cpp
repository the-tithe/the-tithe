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
  for (int i = 0; i < 12; i++) frame();
  shot("game_drawing", 0.6f);
  wait(1.0f);
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
      wait(3.0f);   // hold still 1.3 s, then the result shows 1.2 s
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
  shot("game_build_rest");
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
  // The scanner's footer at every place and log state must fit the screen.
  for (int p = 0; p < (int)WalkFormat::Place::Count; p++)
    for (int l = 0; l < 4; l++) {
      const int w = ScannerScreen::footerWidth((WalkFormat::Place)p, (Status::Log)l, true);
      const int w2 = ScannerScreen::footerWidth((WalkFormat::Place)p, (Status::Log)l, false);
      if (w2 > UI::W) printf("SCANNER FOOTER TOO WIDE: place %d log %d: %d px\n", p, l, w2);
      if (w > UI::W) printf("scanner footer drops G GLASS: place %d log %d (%d px)\n", p, l, w);
    }
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
  // The visit: ESC LEAVE in the footer, the toast above it, a favor's first-time note.
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
  App::tab = 1;
  const int rows = App::buildRows();
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "STANDING")) App::sel = i;
  shot("qa_build_standing");
  for (int i = 0; i < rows; i++) if (!strcmp(App::restRows[i].title, "TONIGHT'S TITHE")) App::sel = i;
  shot("qa_build_tithe_long");
  App::tab = 2;
  shot("qa_build_week_tonight");
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
  App::toast("OFF FOR 12H 05M", 0, 3.0f);
  App::game.moments.taught = 0;
  App::noteOff(725);
  shot("qa_off_for");
  App::toastT = 0;
  frame();
  shot("qa_off_for_teach");
  App::toastT = 0;
  App::nToastNext = 0;
  // The glass with a toast: above the footer.
  App::go(App::Mode::Glass);
  App::toast("...nobody Warded sent them...", UI::EnergyColor[(int)Energy::Hush], 3.5f);
  shot("qa_glass_toast", 1.3f);
  App::toastT = 0;
  estimates();
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
  // A quiet Pact: the bar takes the room, no gauge, no notches, no Omen or star at the stall.
  bindFresh(Cat::Calling_wayfarer, Game::rules.quietRung);
  visitStar();
  while (App::mode == App::Mode::Show) frame(key('E'));
  App::go(App::Mode::Scanner);
  frame();
  shot("qa_scanner_quiet");
  if (intoStall()) {
    r.nRelics = 0;
    for (int k = 0; k < Game::relicSlots(r); k++) Game::addRelic(r, Cat::R_crowd_feeder + k);
    r.coin = 40;
    App::sel = 0;
    shot("qa_shop_quiet");
  }
}

} // namespace Play
