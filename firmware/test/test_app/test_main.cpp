// The game app on a computer: key presses through a Night, and a save and
// reload at every step, so a Deck switched off anywhere comes back to the same
// place. pio test -e native -f test_app
#include <unity.h>
#include <M5GFX.h>
#include "App.h"

static Sightings sky;
static Motion motion;
static Status st;
static App::Ctx ctx() { return App::Ctx{sky, motion, st, true}; }
static Game::Saved* stash;
static uint8_t buf[Game::SaveFormat::Bytes];

static void street(int along) {
  static Net nets[20];
  for (int i = 0; i < 14; i++) {
    Net& n = nets[i];
    memset(&n, 0, sizeof(n));
    const int w = i + along;
    const uint8_t b[6] = {0x10, (uint8_t)(0x20 + w), 0x33, (uint8_t)(w * 7), (uint8_t)(0x55 + w / 200), 0x60};
    memcpy(n.bssid, b, 6);
    n.rssi = (int8_t)(-46 - i * 3);
    n.channel = (uint8_t)(1 + (i * 5) % 13);
    n.lock = i == 3 ? Lock::Open : Lock::Wpa2;
    n.nameKey = 0x100 + i;
  }
  static uint32_t now = 0;
  now += 2400;
  sky.ingest(nets, 14, now);
  st.nowMs = now;
}

static void frame(Press p = Press()) { App::update(ctx(), p, 0.05f); }
static Press key(char k) {
  Press p;
  p.any = true;
  if (k == 'E') p.enter = true; else if (k == 'B') p.back = true; else if (k == 'D') p.down = true;
  else if (k == 'U') p.up = true; else p.ch = k;
  return p;
}

// Save, wipe, reload and resume, as a reboot would.
static void reboot() {
  Game::SaveFormat::encode(App::game, buf);
  memset(&App::game, 0xA5, sizeof(App::game));
  TEST_ASSERT_EQUAL((int)Game::SaveFormat::Load::Ok, (int)Game::SaveFormat::decode(buf, sizeof(buf), App::game));
  App::resume(st.battery);
}

void setUp() {
  App::newGame();
  App::echoMode = false;   // the random career can leave an ECHO visit open
  for (int s = 0; s < 4; s++) street(0);
  st.battery = 70;
}
void tearDown() {}

static void bindAWeek() {
  App::go(App::Mode::Bind);
  frame(key('D')); frame(key('D'));   // down to BIND
  frame(key('E'));
  // The very first week opens on why you're out here.
  if (App::game.career.runs == 0) {
    TEST_ASSERT_EQUAL((int)App::Mode::Premise, (int)App::mode);
    frame(key('E'));
  }
  TEST_ASSERT_EQUAL((int)App::Mode::NightStart, (int)App::mode);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
}

// Sell at the starred well until the Night closes, rebooting after every sale.
static int playNightWithReboots() {
  int sales = 0, along = 0;
  for (int guard = 0; guard < 120 && App::mode != App::Mode::NightEnd; guard++) {
    for (int k = 0; k < 3; k++) frame();
    const uint32_t id = st.starId;
    if (!id) { street(along += 3); continue; }
    App::wellId = id;
    App::go(App::Mode::Visit);
    App::buildActions(ctx());
    if (App::actions[0].ok && App::actions[0].act == App::Act::DrawOwn) {
      App::sel = 0;
      frame(key('E'));
      for (int k = 0; k < 60; k++) frame();
    }
    App::buildActions(ctx());
    int s = -1;
    for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Sell && App::actions[i].ok) s = i;
    if (s < 0) { App::go(App::Mode::Scanner); street(along += 3); continue; }
    App::sel = s;
    frame(key('E'));
    TEST_ASSERT_EQUAL((int)App::Mode::Show, (int)App::mode);
    sales++;
    reboot();   // switched off during the sale show: comes back to the scanner or the Night's end
    TEST_ASSERT_TRUE(App::mode == App::Mode::Scanner || App::mode == App::Mode::NightEnd ||
                     App::mode == App::Mode::OutOfVisits || App::mode == App::Mode::Boot);
    if (App::game.run.n.met && App::mode != App::Mode::NightEnd) {
      // The save was made before the show ended: finishing the Night is still owed.
      TEST_ASSERT_EQUAL((int)Game::Phase::Night, (int)App::game.phase);
    }
    street(along += 3);
    if (App::game.run.n.met && App::mode == App::Mode::Scanner) break;
  }
  return sales;
}

static void test_a_night_survives_reboots() {
  bindAWeek();
  App::game.run.n.tithe = 150;   // payable on this short made-up street
  playNightWithReboots();
  // However the last reboot fell, the Night ends and is paid out exactly once.
  for (int k = 0; k < 5 && App::mode != App::Mode::NightEnd; k++) frame();
  TEST_ASSERT_EQUAL_MESSAGE((int)App::Mode::NightEnd, (int)App::mode, "the Night closed");
  TEST_ASSERT_EQUAL(1, App::game.run.night);
  TEST_ASSERT_EQUAL(1, (int)App::game.run.stats[(int)Cat::Stat::nights_paid]);
}

static void test_the_shop_survives_a_reboot() {
  bindAWeek();
  App::game.run.n.tithe = 1;   // the first sale pays it
  playNightWithReboots();
  for (int k = 0; k < 3 && App::mode != App::Mode::NightEnd; k++) frame();
  if (App::mode != App::Mode::NightEnd) TEST_FAIL_MESSAGE("never reached the Night's end");
  reboot();
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  frame(key('E'));   // the first ENTER finishes the dawn tally
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  for (int k = 0; k < 5 && App::mode == App::Mode::NightEnd; k++) frame(key('E'));   // the Portents' choices, then the stall
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  const uint8_t first = App::game.shop.relics[0].id;
  frame();
  TEST_ASSERT_TRUE_MESSAGE(Game::getBit(App::game.seen, first), "a relic on the stall counts as seen");
  reboot();
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  TEST_ASSERT_EQUAL(first, App::game.shop.relics[0].id);
  frame(key('B'));   // leave takes two presses: the first only asks
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  frame(key('B'));   // the next Night starts
  TEST_ASSERT_EQUAL((int)App::Mode::NightStart, (int)App::mode);
  TEST_ASSERT_EQUAL(1, App::game.run.night);
  reboot();
  TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
}

static void test_losing_ends_in_the_bind_screen() {
  bindAWeek();
  App::game.run.n.used = App::game.run.n.visits;
  App::game.run.coin = 0;
  frame();
  TEST_ASSERT_EQUAL((int)App::Mode::Lost, (int)App::mode);
  reboot();
  TEST_ASSERT_EQUAL((int)App::Mode::Lost, (int)App::mode);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Reveal, (int)App::mode);
  TEST_ASSERT_EQUAL(0, App::game.run.active);
  TEST_ASSERT_EQUAL(1, (int)App::game.career.runs);
  const uint16_t before = App::pagesBefore;
  reboot();   // switched off on the reveal: it comes back, with the same new pages
  TEST_ASSERT_EQUAL((int)App::Mode::Reveal, (int)App::mode);
  TEST_ASSERT_EQUAL(before, App::pagesBefore);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Bind, (int)App::mode);
  reboot();
  TEST_ASSERT_EQUAL((int)App::Mode::Bind, (int)App::mode);
}

// Below the fourth rung a Portent is one of two: the dawn waits for the choice, even through a reboot.
static void test_the_dawn_offers_one_of_two_portents() {
  bindAWeek();   // Chalk
  App::game.run.n.tithe = 1;   // one sale pays it: 11 to spare, three Portents
  playNightWithReboots();
  for (int k = 0; k < 3 && App::mode != App::Mode::NightEnd; k++) frame();
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  const int portents = App::game.lastEnd.portents;
  TEST_ASSERT_EQUAL(3, portents);
  TEST_ASSERT_EQUAL(7, App::game.moments.portent.pending);
  TEST_ASSERT_EQUAL(0, (int)App::game.run.stats[(int)Cat::Stat::portents]);
  frame(key('E'));   // the tally finishes; the first choice is up
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  reboot();          // switched off mid-choice: it is still waiting
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  TEST_ASSERT_EQUAL(7, App::game.moments.portent.pending);
  frame(key('E'));   // the tally again
  const int second = App::game.moments.portent.alt[0] - 1;
  frame(key('D'));   // the cursor on the other one
  frame(key('E'));
  TEST_ASSERT_EQUAL(6, App::game.moments.portent.pending);
  TEST_ASSERT_EQUAL(second, App::game.lastEnd.got[0].portent);
  TEST_ASSERT_EQUAL(1, (int)App::game.run.stats[(int)Cat::Stat::portents]);
  frame(key('E'));   // keep the first of the second pair
  reboot();
  frame(key('E'));   // tally
  frame(key('E'));   // the third
  TEST_ASSERT_EQUAL(0, App::game.moments.portent.pending);
  TEST_ASSERT_EQUAL(3, (int)App::game.run.stats[(int)Cat::Stat::portents]);
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
}

// A week lost on a Pact offers the Trial that teaches it, under the cursor, when that Trial is open.
static void test_a_lost_week_offers_its_trial() {
  App::game.career.wins = 3;      // Shoestring, One note and Night owl are open
  App::game.career.seals[0] = 0;  // the Wayfarer's Chalk seal opens Lead
  App::bindCalling = 0;
  App::bindPact = 1;
  App::bindChallenge = -1;
  App::go(App::Mode::Bind);
  App::sel = 3;   // BIND
  frame(key('E'));
  if (App::mode == App::Mode::Premise) frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::NightStart, (int)App::mode);
  TEST_ASSERT_EQUAL(1, (int)App::game.run.pact);
  frame(key('E'));
  App::game.run.n.used = App::game.run.n.visits;   // out of visits, no coin: lost
  App::game.run.coin = 0;
  frame();
  TEST_ASSERT_EQUAL((int)App::Mode::Lost, (int)App::mode);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Reveal, (int)App::mode);
  TEST_ASSERT_EQUAL(0, App::bindOffer);        // Shoestring, Lead's lesson
  TEST_ASSERT_EQUAL(0, App::bindChallenge);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Bind, (int)App::mode);
  TEST_ASSERT_EQUAL(2, App::sel);              // the TRIAL row, under the cursor
  static M5Canvas canvas;
  if (!canvas.getBuffer()) { canvas.setColorDepth(16); canvas.createSprite(240, 135); }
  App::draw(canvas, ctx(), 0.1f);
  frame(key('E'));   // on to BIND
  frame(key('E'));   // a Shoestring week
  if (App::mode == App::Mode::Premise) frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::NightStart, (int)App::mode);
  TEST_ASSERT_EQUAL(0, (int)App::game.run.challenge);
  TEST_ASSERT_EQUAL(-1, App::bindOffer);
  // A week lost on Chalk offers nothing; neither does a Pact whose Trial isn't open yet.
  App::game.run.n.used = App::game.run.n.visits;
  App::game.run.coin = 0;
  frame(key('E'));
  frame();
  TEST_ASSERT_EQUAL((int)App::Mode::Lost, (int)App::mode);
  frame(key('E'));
  TEST_ASSERT_EQUAL(-1, App::bindOffer);
  TEST_ASSERT_EQUAL(-1, Game::trialForPact(0));
  App::game.career.wins = 1;   // only Shoestring open: a loss on Iron (Night owl) offers nothing
  App::bindOffer = -1;
  App::game.run.pact = 2;
  App::game.run.challenge = -1;
  App::game.run.won = 0;
  App::game.run.active = 1;
  App::endBinding();
  TEST_ASSERT_EQUAL(-1, App::bindOffer);
}

static void test_screens_survive_keys_at_random() {
  // Mash keys on every screen: nothing may crash or index out of range.
  bindAWeek();
  srand(3);
  const char ks[] = "UDEBELRgsbcmx";
  for (int i = 0; i < 6000; i++) {
    Press p;
    const char k = ks[rand() % (sizeof(ks) - 1)];
    p.any = true;
    switch (k) {
      case 'U': p.up = true; break; case 'D': p.down = true; break; case 'E': p.enter = true; break;
      case 'B': p.back = true; break; case 'L': p.left = true; break; case 'R': p.right = true; break;
      default: p.ch = k; break;
    }
    frame(p);
    if (i % 50 == 0) street(i / 50);
    static M5Canvas canvas;
    if (!canvas.getBuffer()) { canvas.setColorDepth(16); canvas.createSprite(240, 135); }
    App::draw(canvas, ctx(), i * 0.05f);
    if (!App::inRun() && App::mode == App::Mode::Bind && rand() % 4 == 0) { App::sel = 2; frame(key('E')); }
  }
  TEST_ASSERT_TRUE(App::game.run.coin >= 0);
}

// The story screen opens from the menu and goes back to it, any time.
static void test_the_story_opens_from_the_menu() {
  bindAWeek();
  App::go(App::Mode::Menu);
  App::sel = 0;
  while (App::menuItems[App::sel] != App::MStory) frame(key('D'));
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Premise, (int)App::mode);
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Menu, (int)App::mode);
  TEST_ASSERT_TRUE(App::inRun());
}

// On the spell screen DEL erases a typed letter, and with none left it goes back.
static void test_del_erases_then_goes_back() {
  bindAWeek();
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Spells);
  frame(key('g'));
  TEST_ASSERT_EQUAL_STRING("G", App::typed);
  Press del;
  del.del = del.back = del.any = true;
  frame(del);
  TEST_ASSERT_EQUAL((int)App::Mode::Spells, (int)App::mode);
  TEST_ASSERT_EQUAL_STRING("", App::typed);
  frame(del);
  TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
}

static void test_a_long_random_career() {
  // Weeks of random keys, with a reload every 500 frames and Tithes low enough
  // that random play sometimes wins and goes Overnight.
  srand(11);
  static M5Canvas canvas;
  if (!canvas.getBuffer()) { canvas.setColorDepth(16); canvas.createSprite(240, 135); }
  const char ks[] = "UDEEEEBLRgcbx";
  int wins = 0, weeks = 0, overnight = 0;
  uint32_t lastRuns = App::game.career.runs;
  for (int i = 0; i < 40000; i++) {
    if (App::inRun() && App::game.phase == Game::Phase::Night && App::game.run.night < Game::Nights &&
        App::game.run.n.tithe > 60)
      App::game.run.n.tithe = 60;
    // Overnight is covered once it's reached. Growing relics can carry even random keys a long
    // way out there, so after its first Night the visits are gone and the career moves on.
    if (App::inRun() && App::game.phase == Game::Phase::Night && App::game.run.night >= Game::Nights) {
      const int16_t left = App::game.run.night == Game::Nights ? 1 : 0;
      if (App::game.run.n.used < App::game.run.n.visits - left) App::game.run.n.used = (int16_t)(App::game.run.n.visits - left);
    }
    if (App::mode == App::Mode::Bind) { App::sel = 2 + (App::challengesOpen() ? 1 : 0); frame(key('E')); continue; }
    if (App::mode == App::Mode::Premise) { frame(key('E')); continue; }
    Press p;
    const char k = ks[rand() % (sizeof(ks) - 1)];
    p.any = true;
    switch (k) {
      case 'U': p.up = true; break; case 'D': p.down = true; break; case 'E': p.enter = true; break;
      case 'B': p.back = true; break; case 'L': p.left = true; break; case 'R': p.right = true; break;
      default: p.ch = k; break;
    }
    // Head for the starred well now and then, so sales happen.
    if ((App::mode == App::Mode::Scanner || App::mode == App::Mode::Well) && st.starId && rand() % 3 == 0) { App::wellId = st.starId; App::go(App::Mode::Visit); }
    frame(p);
    if (i % 40 == 0) street(i / 40);
    if (i % 7 == 0) App::draw(canvas, ctx(), i * 0.05f);
    if (i % 500 == 499) reboot();
    if (App::mode == App::Mode::Won) wins++;
    if (App::inRun() && App::game.run.overnight) overnight++;
    if (App::game.career.runs != lastRuns) { weeks++; lastRuns = App::game.career.runs; }
    TEST_ASSERT_TRUE(App::game.run.coin >= 0);
    TEST_ASSERT_TRUE(App::game.run.nRelics <= Game::Run::MaxRelics);
    TEST_ASSERT_TRUE(App::game.run.n.used <= App::game.run.n.visits + 3);
  }
  printf("random career: %d weeks finished, %d frames on the won screen, %d frames Overnight\n", weeks, wins, overnight);
  TEST_ASSERT_TRUE(weeks >= 3);
}

// The Overheard and the Collector's lines: they fit, they fit the HEARD log, and
// the place-bound ones play where they belong.
static void test_heard_lines_fit() {
  TEST_ASSERT_TRUE(Lore::OverheardCount <= (int)sizeof(App::game.heard.radio) * 8);
  TEST_ASSERT_TRUE(Lore::CL_count <= (int)sizeof(App::game.heard.stall) * 8);
  for (int i = 0; i < Lore::OverheardCount; i++) {
    const char* t = Lore::Overheard[i].text;
    TEST_ASSERT_TRUE_MESSAGE(strlen(t) <= 36, t);
    for (const char* c = t; *c; c++) TEST_ASSERT_TRUE_MESSAGE(*c >= 32 && *c < 127, t);
    for (int j = 0; j < i; j++) TEST_ASSERT_TRUE_MESSAGE(strcmp(t, Lore::Overheard[j].text) != 0, t);
  }
  for (int i = 0; i < Lore::CL_count; i++) {
    const char* t = Lore::Collector[i];
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_TRUE_MESSAGE(UI::wrapLines(UI::W - 16, t) <= 2, t);   // two lines at dawn
    for (const char* c = t; *c; c++) TEST_ASSERT_TRUE_MESSAGE(*c >= 32 && *c < 127, t);
  }
}

static void test_place_bound_lines_play_where_they_belong() {
  App::HeardHere h{};
  h.well = true;
  h.ward = (int)WardKind::Fringe;
  h.faction = (int)Faction::Hearth;
  h.energy = (int)Energy::Tide;
  h.visits = 12;   // a well you keep
  int keptOnly = 0;
  for (int i = 0; i < Lore::OverheardCount; i++) {
    const Lore::HeardBind& b = Lore::Overheard[i].b;
    if (b.pool || (b.extra & Lore::HX_OldWell)) continue;   // the old pole radio's line is heard only at one
    TEST_ASSERT_TRUE(b.vmin >= 4);   // the keeping lines only play at wells you keep
    keptOnly += App::heardFits(b, h);
  }
  TEST_ASSERT_TRUE(keptOnly > 0);
  // A plain-pool pick never plays a bound-only line, and never the same line twice running.
  for (uint32_t roll = 1; roll < 3000; roll += 3) {
    const int k = App::pickOverheard(h, roll);
    TEST_ASSERT_TRUE(k >= 0 && k < Lore::OverheardCount);
    if (roll % 3) TEST_ASSERT_TRUE(Lore::Overheard[k].b.pool);
    TEST_ASSERT_TRUE(k != App::overheardLast);
    App::overheardLast = k;
  }
  // A bound pick prefers what hasn't been heard.
  h.visits = 0;
  memset(App::game.heard.radio, 0, sizeof(App::game.heard.radio));
  for (uint32_t roll = 0; roll < 300; roll += 3) {
    const int k = App::pickOverheard(h, roll);
    TEST_ASSERT_TRUE(App::heardFits(Lore::Overheard[k].b, h));
    Game::setBit(App::game.heard.radio, k);
  }
  App::overheardLast = -1;
  memset(App::game.heard.radio, 0, sizeof(App::game.heard.radio));
}

static void test_the_coat_line_can_be_heard() {
  Game::Run& r = App::game.run;
  Game::NightEnd e{};
  e.paid = true;
  e.spare = 4;
  bool coat = false;
  for (uint32_t run = 2; run < 20 && !coat; run++)
    for (int night = 2; night < 5 && !coat; night++) {
      r.runNo = run;
      r.night = night;
      coat = App::collectorLineId(e, r) == Lore::CL_coat;
    }
  TEST_ASSERT_TRUE(coat);
}


// ---- Walking: the Deck's memory of wells, their tunes, and stopping to listen ------

static int tones[64][2], nTones;
static void hearTone(int hz, int ms) {
  if (nTones < 64) { tones[nTones][0] = hz; tones[nTones][1] = ms; }
  nTones++;
}

static Net well(int k, int8_t rssi, uint8_t ch = 6, bool phone = false) {
  Net n{};
  const uint8_t b[6] = {0x10, (uint8_t)(0x70 + (k & 0x0F)), (uint8_t)(k >> 4), 0x77, 0x31, 0x60};
  memcpy(n.bssid, b, 6);
  n.rssi = rssi;
  n.channel = ch;
  n.lock = Lock::Wpa2;
  n.nameKey = 0x7000u + (uint32_t)k;
  n.phone = phone;
  return n;
}
static uint32_t idOf(const Net& n) {
  uint8_t rb[5];
  WellRules::routerBytes(n.bssid, rb);
  return Keys::deviceHash(rb, 5);
}

// A new walk: the Deck switched on again.
static uint32_t walkNow = 0;
static void newWalk() {
  sky.reset(walkNow);
  App::tunedAt = App::chirpedAt = 0;
  App::fivePlayed = false;
  memset(App::fiveAt, 0, sizeof(App::fiveAt));
  App::stopTried = false;
  App::nHeld = 0;
  App::heldThisStop = false;
  App::adrift.on = false;
  App::go(App::Mode::Scanner);
}
// One scan, then a second and a half of frames for its sounds.
static void scanOf(const Net* nets, int n) {
  walkNow += 2400;
  sky.ingest(nets, n, walkNow);
  st.nowMs = walkNow;
  App::afterScan(ctx());
  for (int i = 0; i < 30; i++) frame();
}

static void test_the_deck_remembers_wells_between_walks() {
  newWalk();
  const Net a = well(1, -70), b = well(2, -75);
  Net first[2] = {a, b};
  scanOf(first, 2);
  TEST_ASSERT_TRUE(sky.byId(idOf(a))->neverHeard);
  TEST_ASSERT_TRUE(sky.byId(idOf(b))->neverHeard);
  newWalk();
  const Net c = well(3, -72);
  Net second[2] = {a, c};
  scanOf(second, 2);
  TEST_ASSERT_FALSE(sky.byId(idOf(a))->neverHeard);
  TEST_ASSERT_TRUE(sky.byId(idOf(c))->neverHeard);
}

// A well that comes into reach plays its tune once. Wells already there when
// the Deck wakes stay quiet, and a phone's hotspot never plays.
static void test_a_well_coming_into_reach_plays_its_tune() {
  App::hooks.tone = hearTone;
  newWalk();
  const Net home = well(4, -50, 1), passing = well(5, -60, 6), phone = well(6, -52, 6, true);
  scanOf(&home, 1);
  nTones = 0;
  for (int i = 0; i < 3; i++) scanOf(&home, 1);
  TEST_ASSERT_EQUAL(0, nTones);
  Net two[2] = {home, phone};
  for (int i = 0; i < 4; i++) scanOf(two, 2);
  TEST_ASSERT_EQUAL(0, nTones);
  Net three[3] = {home, phone, passing};
  scanOf(three, 3);
  const WellRec* w = sky.byId(idOf(passing));
  const Tune t = Tunes::of(w->seed, (int)w->energy, false);
  TEST_ASSERT_EQUAL(t.count, nTones);
  for (int i = 0; i < t.count; i++) TEST_ASSERT_EQUAL(t.n[i].hz, tones[i][0]);
  TEST_ASSERT_EQUAL(Tunes::Root[(int)Energy::Tide], tones[0][0]);
  for (int i = 0; i < 6; i++) scanOf(three, 3);
  TEST_ASSERT_EQUAL(t.count, nTones);   // once a walk
  App::hooks.tone = nullptr;
}

// A companion's hotspot, with an ordinary name, walks with you: the Deck
// remembers it, and on the next walk it's set aside from the start.
static void test_a_companion_is_set_aside_on_the_next_walk() {
  App::hooks.tone = hearTone;
  newWalk();
  const Net friendPhone = well(9, -48, 11);
  Net nets[8];
  for (int s = 0; s < 270; s++) {
    int k = 0;
    nets[k++] = friendPhone;
    for (int p = s / 3; p < s / 3 + 5; p++) nets[k++] = well(200 + p, -78);
    walkNow += 2400;
    sky.ingest(nets, k, walkNow);
    st.nowMs = walkNow;
    App::afterScan(ctx());
  }
  TEST_ASSERT_TRUE(App::game.moments.travelled(idOf(friendPhone)));
  // Saved and loaded with the game.
  reboot();
  TEST_ASSERT_TRUE(App::game.moments.travelled(idOf(friendPhone)));
  newWalk();
  const Net home = well(4, -50, 1);
  scanOf(&home, 1);
  nTones = 0;
  Net two[2] = {home, friendPhone};
  for (int i = 0; i < 4; i++) scanOf(two, 2);
  TEST_ASSERT_FALSE(WellRules::fixedish(*sky.byId(idOf(friendPhone))));
  TEST_ASSERT_EQUAL(0, nTones);
  App::hooks.tone = nullptr;
}

// Footsteps, two a second, the Deck held up in front of you.
static void walkFor(float secs) {
  for (float t = 0; t < secs; t += 1 / 30.0f) {
    const float b = 0.25f * sinf(motion.t * 6.2832f * 2);
    const float a[3] = {0, 0.6f * (1 + b), 0.8f * (1 + b)}, g[3] = {0.1f * sinf(motion.t * 9), 0, 0};
    motion.update(a, g, 1 / 30.0f);
  }
}
static void holdStill(float secs) {
  const float a[3] = {0, 0.6f, 0.8f}, g[3] = {0, 0, 0};
  for (float t = 0; t < secs; t += 1 / 30.0f) motion.update(a, g, 1 / 30.0f);
}

// Stopping after a walk always catches a fragment, with a soft settle; just
// standing still on the scanner doesn't.
static void test_stopping_after_a_walk_catches_a_fragment() {
  App::hooks.tone = hearTone;
  newWalk();
  const Net home = well(4, -50, 1);
  scanOf(&home, 1);
  memset(App::game.heard.radio, 0, sizeof(App::game.heard.radio));
  App::overheardAt = 0;
  App::toastT = 0;
  motion = Motion();
  holdStill(4);
  for (int i = 0; i < 8; i++) {   // standing still while a crowd's phones and a bus come and go
    Net crowd[3] = {home, well(300 + 2 * i, -70), well(301 + 2 * i, -74)};
    scanOf(crowd, 3);
  }
  holdStill(1);
  frame();
  TEST_ASSERT_TRUE(App::toastT <= 0);   // no walk: nothing
  walkFor(200);
  holdStill(3.5f);
  nTones = 0;
  frame();
  TEST_ASSERT_TRUE(App::toastT > 0);
  int heard = 0;
  for (size_t i = 0; i < sizeof(App::game.heard.radio); i++) heard += __builtin_popcount(App::game.heard.radio[i]);
  TEST_ASSERT_EQUAL(1, heard);
  for (int i = 0; i < 10; i++) frame();
  TEST_ASSERT_TRUE(nTones >= 2);
  TEST_ASSERT_EQUAL(784, tones[0][0]);
  TEST_ASSERT_EQUAL(587, tones[1][0]);
  // Once a stop, and only after walking again.
  App::toastT = 0;
  App::overheardAt = 0;
  holdStill(2);
  frame();
  TEST_ASSERT_TRUE(App::toastT <= 0);
  App::hooks.tone = nullptr;
}

// After half a day off, the first stop catches one too, walk or no walk.
static void test_the_first_stop_after_half_a_day_off_catches_one() {
  newWalk();
  const Net home = well(4, -50, 1);
  scanOf(&home, 1);
  App::overheardAt = 0;
  App::toastT = 0;
  App::game.moments.taught |= (uint16_t)(1u << (int)App::Taught::OffFor);   // its first-time note was said already
  motion = Motion();
  App::noteOff(600, 0);   // ten hours: not enough
  App::toastT = 0;        // (its OFF FOR toast)
  holdStill(4);
  frame();
  TEST_ASSERT_TRUE(App::toastT <= 0);
  motion = Motion();
  App::noteOff(800, 0);
  App::toastT = 0;
  holdStill(4);
  frame();
  TEST_ASSERT_TRUE(App::toastT > 0);
  TEST_ASSERT_FALSE(App::settleDue);
}

// A sure catch from the plain pool goes for a line not heard yet.
static void test_a_sure_catch_prefers_a_new_line() {
  App::HeardHere h{};
  int last = -1;
  for (int i = 0; i < Lore::OverheardCount; i++) Game::setBit(App::game.heard.radio, i);
  for (int i = Lore::OverheardCount - 1; i >= 0; i--)
    if (Lore::Overheard[i].b.pool && App::heardAllowed(Lore::Overheard[i].b, h)) { last = i; break; }
  App::game.heard.radio[last / 8] &= (uint8_t)~(1u << (last % 8));
  App::overheardLast = -1;
  for (uint32_t roll = 1; roll < 300; roll += 3) TEST_ASSERT_EQUAL(last, App::pickOverheard(h, roll, true));
  memset(App::game.heard.radio, 0, sizeof(App::game.heard.radio));
}

// Wells of all five energies in reach on one walk: their chord, once.
static void test_all_five_energies_play_their_chord_once() {
  App::hooks.tone = hearTone;
  newWalk();
  const Net home = well(4, -80, 1);
  scanOf(&home, 1);
  Net five[6] = {home, well(20, -60, 1), well(21, -60, 6), well(22, -60, 11), well(23, -60, 3), well(24, -60, 6)};
  five[4].hidden = true;   // Hush
  five[4].nameKey = 0;
  five[5].lock = Lock::Open;   // Static
  App::game.moments.guideGot = 0xFF;   // every kind already met, so no find's chime comes first
  motion = Motion();
  walkFor(5);   // walking, the Hush well stays silent, so four energies aren't the five
  scanOf(five, 6);
  TEST_ASSERT_FALSE(App::fivePlayed);
  TEST_ASSERT_EQUAL(0, App::fiveAt[(int)Energy::Hush]);
  holdStill(2);   // held still, the Hush well sings too
  nTones = 0;
  scanOf(five, 6);
  TEST_ASSERT_EQUAL(5, nTones);
  const Tune t = Tunes::five();
  for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL(t.n[i].hz, tones[i][0]);
  TEST_ASSERT_TRUE(App::fivePlayed);
  for (int i = 0; i < 8; i++) scanOf(five, 6);
  for (int i = 5; i < nTones && i < 64; i++) TEST_ASSERT_TRUE(tones[i][0] != t.n[4].hz || tones[i][1] != t.n[4].ms);
  App::hooks.tone = nullptr;
}

// ---- What walks turn up: words, namesakes, the field guide, drifting glyphs ---------

// A well with a 24-bit number, for tests that need many different ones.
static Net wellAt(uint32_t k, int8_t rssi, Lock lock = Lock::Wpa2, uint8_t ch = 6) {
  Net n{};
  const uint8_t b[6] = {0x12, (uint8_t)k, (uint8_t)(k >> 8), (uint8_t)(k >> 16), 0x31, 0x60};
  memcpy(n.bssid, b, 6);
  n.rssi = rssi;
  n.channel = ch;
  n.lock = lock;
  n.nameKey = 0x9000000u + k;
  return n;
}

// Time passing between scans, on the scanner's clock and the sequencer's.
static void later(uint32_t ms) { walkNow += ms; App::soundMs += ms; }

// Wells coming into reach one after another, 9 s apart (past the tune gap).
static void passWells(uint32_t from, int n, int8_t rssi = -60) {
  const Net home = wellAt(3000, -90);
  for (int i = 0; i < n; i++) {
    later(9000);
    Net two[2] = {home, wellAt(from + i, rssi)};
    scanOf(two, 2);
  }
}

// A walk's first ten tunes are free; after that only a well never heard by this
// Deck, or one you've worked, sings. The chirp is for never-heard wells only.
static void test_a_walk_has_a_tune_budget() {
  App::hooks.tone = hearTone;
  App::game.moments.guideGot = 0xFF;
  // Hear a known street first (no tunes needed: they were in reach on waking).
  newWalk();
  Net known[12];
  for (int i = 0; i < 12; i++) known[i] = wellAt(3100 + i, -60);
  scanOf(known, 12);
  // A new walk: ten never-heard wells sing, then a known well doesn't.
  newWalk();
  const Net home = wellAt(3000, -90);
  scanOf(&home, 1);
  nTones = 0;
  passWells(3200, 10);
  TEST_ASSERT_EQUAL(10, App::tunesThisWalk);
  TEST_ASSERT_TRUE(nTones >= 30);
  nTones = 0;
  passWells(3100, 3);   // known, never worked: silent
  TEST_ASSERT_EQUAL(0, nTones);
  TEST_ASSERT_EQUAL(10, App::tunesThisWalk);
  passWells(3300, 1);   // never heard: sings
  TEST_ASSERT_TRUE(nTones >= 3);
  TEST_ASSERT_EQUAL(11, App::tunesThisWalk);
  // A well you've worked sings too (3101 is in reach, unsung, and now worked).
  const Net worked = wellAt(3101, -60);
  Game::AtlasEntry& a = App::game.atlas.touch(idOf(worked), 0x1234567u, 1, 0);
  a.visits = 1;
  nTones = 0;
  passWells(3101, 1);
  TEST_ASSERT_TRUE(nTones >= 3);
  TEST_ASSERT_EQUAL(12, App::tunesThisWalk);
  // A chirp is a never-heard well first heard out of reach; a known one is quiet.
  nTones = 0;
  later(20000);
  Net heardFar[2] = {home, wellAt(3400, -80)};
  scanOf(heardFar, 2);
  TEST_ASSERT_EQUAL(1, nTones);
  nTones = 0;
  later(20000);
  Net knownFar[2] = {home, wellAt(3110, -80)};
  scanOf(knownFar, 2);
  TEST_ASSERT_EQUAL(0, nTones);
  App::hooks.tone = nullptr;
}

// The walk's sounds belong to the scanner's screens: a well coming into reach
// over the stall, the show or the dawn says nothing, and doesn't sing later
// either. A find's chime still plays on a quiet screen like the menu.
static void test_walk_sounds_stay_on_the_scanner_screens() {
  App::hooks.tone = hearTone;
  App::game.moments.guideGot = 0xFF;
  newWalk();
  const Net home = wellAt(3000, -90);
  scanOf(&home, 1);
  App::go(App::Mode::Shop);
  nTones = 0;
  passWells(3500, 2);
  TEST_ASSERT_EQUAL(0, nTones);
  App::go(App::Mode::Scanner);
  for (int i = 0; i < 3; i++) { later(9000); Net two[2] = {home, wellAt(3501, -60)}; scanOf(two, 2); }
  TEST_ASSERT_EQUAL(0, nTones);   // it was already in reach when you came back
  passWells(3600, 1);
  TEST_ASSERT_TRUE(nTones >= 3);   // a well that comes into reach now sings
  // On the menu, no tune, but a find's chime (a never-met kind) still plays.
  App::go(App::Mode::Menu);
  nTones = 0;
  passWells(3700, 1);
  TEST_ASSERT_EQUAL(0, nTones);
  App::game.moments.guideGot = (uint8_t)~(1u << App::G_LeftOn);   // one kind left to meet: an open well on a quiet street
  Net open[2] = {home, wellAt(3800, -60, Lock::Open)};
  later(9000);
  scanOf(open, 2);
  TEST_ASSERT_TRUE(App::guideMet(App::G_LeftOn));
  TEST_ASSERT_EQUAL(Tunes::guide().count, nTones);
  TEST_ASSERT_EQUAL(Tunes::guide().n[0].hz, tones[0][0]);
  // At the stall, even a find keeps quiet (its words still wait for a stop).
  App::go(App::Mode::Shop);
  App::game.moments.guideGot = (uint8_t)~(1u << App::G_LeftOn);
  nTones = 0;
  Net open2[2] = {home, wellAt(3801, -60, Lock::Open)};
  later(9000);
  scanOf(open2, 2);
  TEST_ASSERT_TRUE(App::guideMet(App::G_LeftOn));
  TEST_ASSERT_EQUAL(0, nTones);
  App::go(App::Mode::Scanner);
  App::game.moments.guideGot = 0xFF;
  App::nHeld = 0;
  App::hooks.tone = nullptr;
}
static uint32_t seedOf(const Net& n) {
  uint8_t rb[5];
  WellRules::routerBytes(n.bssid, rb);
  return Keys::worldHash(rb, 5);
}
static void walking() { motion = Motion(); walkFor(5); }

// Walking past wells fills in their two naming words; a phone's hotspot doesn't,
// and a Hush well's only once it has surfaced.
static void test_naming_words_fill_as_you_pass() {
  newWalk();
  const Net a = wellAt(100, -75);
  uint32_t k = 101;
  while ((seedOf(wellAt(k, -70)) & 63) == (seedOf(a) & 63)) k++;
  Net phone = wellAt(k, -70);
  phone.phone = true;
  Net hidden = wellAt(k + 1, -72);
  hidden.hidden = true;
  hidden.nameKey = 0;
  st.hushReveal = 0;
  Net nets[3] = {a, phone, hidden};
  scanOf(nets, 3);
  const uint32_t sa = seedOf(a), sp = seedOf(phone), sh = seedOf(hidden);
  TEST_ASSERT_TRUE(App::wordHeard(0, (int)(sa & 63)));
  TEST_ASSERT_TRUE(App::wordHeard(1 + (int)Faction::Hearth, (int)((sa >> 6) & 31)));
  TEST_ASSERT_FALSE(App::wordHeard(0, (int)(sp & 63)));
  const bool shared = (sh & 63) == (sa & 63);
  if (!shared) TEST_ASSERT_FALSE(App::wordHeard(0, (int)(sh & 63)));
  st.hushReveal = 1;   // held still: it says its name
  scanOf(nets, 3);
  TEST_ASSERT_TRUE(App::wordHeard(0, (int)(sh & 63)));
  TEST_ASSERT_EQUAL(App::wordsHeard(0) + App::wordsHeard(1 + (int)Faction::Hearth), App::wordsCount());
  // Saved with the game.
  reboot();
  TEST_ASSERT_TRUE(App::wordHeard(0, (int)(sa & 63)));
}

// Passing a well with the true name of one you've worked: noted once, with its
// two-note sound at once and its words only when you stop and look.
static void test_a_namesake_is_noted_once_when_you_stop() {
  App::hooks.tone = hearTone;
  const Net worked = wellAt(500, -80);
  const uint16_t name = Game::packName(seedOf(worked), (int)Faction::Hearth);
  Game::AtlasEntry& e = App::game.atlas.touch(idOf(worked), seedOf(worked), 1, 0);
  e.visits = 2;
  e.faction = (uint8_t)Faction::Hearth;
  uint32_t k = 501;
  while (Game::packName(seedOf(wellAt(k, -60)), (int)Faction::Hearth) != name) k++;
  Net phoneTwin = wellAt(k, -60);   // a hotspot with that name is never one
  phoneTwin.phone = true;
  uint32_t k2 = k + 1;
  while (Game::packName(seedOf(wellAt(k2, -60)), (int)Faction::Hearth) != name) k2++;
  const Net twin = wellAt(k2, -60);
  newWalk();
  walking();
  scanOf(&phoneTwin, 1);
  TEST_ASSERT_EQUAL(0, App::game.moments.nNamesakes);
  nTones = 0;
  Net both[2] = {phoneTwin, twin};
  scanOf(both, 2);
  TEST_ASSERT_EQUAL(1, App::game.moments.nNamesakes);
  TEST_ASSERT_EQUAL_HEX16(name, App::game.moments.namesakes[0].name);
  TEST_ASSERT_TRUE(sky.byId(idOf(twin))->namesake);
  TEST_ASSERT_TRUE(App::guideMet(App::G_Namesake));
  TEST_ASSERT_EQUAL(2, nTones);
  TEST_ASSERT_EQUAL(tones[0][0], tones[1][0]);
  TEST_ASSERT_TRUE(App::toastT <= 0);   // walking: nothing to read yet
  holdStill(1);
  frame();
  TEST_ASSERT_TRUE(App::toastT > 0);
  char want[40];
  snprintf(want, sizeof(want), "NAMESAKE: %s", sky.byId(idOf(twin))->name);
  TEST_ASSERT_EQUAL_STRING(want, App::toastText);
  // Seen again on another walk: never noted again.
  newWalk();
  scanOf(&twin, 1);
  for (int i = 0; i < 3; i++) scanOf(&twin, 1);
  TEST_ASSERT_EQUAL(1, App::game.moments.nNamesakes);
  TEST_ASSERT_FALSE(sky.byId(idOf(twin))->namesake);
  TEST_ASSERT_EQUAL(0, App::nHeld);
  App::hooks.tone = nullptr;
}

// Each odd kind of well is met once, with the true name of the first; phones
// singing one name aren't a chorus, and the quiet kinds need a quiet street.
static void test_the_field_guide_meets_each_kind_once() {
  using namespace App;
  newWalk();
  walking();   // so what's found waits to be read
  Net phones[3];
  for (int i = 0; i < 3; i++) { phones[i] = wellAt(700 + i, -70); phones[i].nameKey = 0x5150; phones[i].phone = true; }
  scanOf(phones, 3);
  TEST_ASSERT_FALSE(guideMet(G_Chorus));
  Net chorus[3];
  for (int i = 0; i < 3; i++) { chorus[i] = wellAt(710 + i, -70); chorus[i].nameKey = 0x5151; }
  scanOf(chorus, 3);
  TEST_ASSERT_TRUE(guideMet(G_Chorus));
  TEST_ASSERT_EQUAL_STRING("FOUND: CHORUS", heldText[0]);
  const uint16_t first = game.moments.guide[G_Chorus];
  Net chorus2[3];
  for (int i = 0; i < 3; i++) { chorus2[i] = wellAt(720 + i, -70); chorus2[i].nameKey = 0x5152; }
  scanOf(chorus2, 3);
  TEST_ASSERT_EQUAL_HEX16(first, game.moments.guide[G_Chorus]);   // the first one stays
  TEST_ASSERT_EQUAL(2, nHeld);   // the find, and the first-time note that follows it
  TEST_ASSERT_EQUAL_STRING("YOUR FINDS: SEE GRIMOIRE > FOUND", heldText[1]);

  // An open well: left on in a quiet street, not in a busy one.
  Net busy[12];
  for (int i = 0; i < 12; i++) busy[i] = wellAt(800 + i, -75);
  busy[0] = wellAt(830, -70, Lock::Open);
  newWalk();
  scanOf(busy, 12);
  TEST_ASSERT_FALSE(guideMet(G_LeftOn));
  newWalk();
  scanOf(busy, 3);
  TEST_ASSERT_TRUE(App::guideMet(App::G_LeftOn));

  // A Hush well that surfaces on a quiet street.
  Net cold = wellAt(840, -70);
  cold.hidden = true;
  cold.nameKey = 0;
  newWalk();
  st.hushReveal = 0;
  scanOf(&cold, 1);
  TEST_ASSERT_FALSE(guideMet(G_Cold));
  st.hushReveal = 1;
  scanOf(&cold, 1);
  TEST_ASSERT_TRUE(guideMet(G_Cold));

  // A locked well among the free ones.
  Net market[12];
  for (int i = 0; i < 12; i++) market[i] = wellAt(850 + i, -75, i < 7 ? Lock::Open : Lock::Wpa2);
  market[11] = wellAt(870, -70, Lock::Wpa3);
  newWalk();
  scanOf(market, 12);
  TEST_ASSERT_TRUE(guideMet(G_Locked));

  // Sixty wells at once.
  static Net crowd[64];
  for (int i = 0; i < 64; i++) crowd[i] = wellAt(900 + i, -80);
  newWalk();
  scanOf(crowd, 59);
  TEST_ASSERT_FALSE(guideMet(G_Crowded));
  scanOf(crowd, 64);
  TEST_ASSERT_TRUE(guideMet(G_Crowded));

  // Old radios on the lampposts: one, then a row of three with one name.
  Net old[3];
  uint32_t k = 1000;
  for (int i = 0; i < 3; i++) {
    while (((seedOf(wellAt(k, -70, Lock::Open, 11)) >> 20) & 1) != 0) k++;
    old[i] = wellAt(k++, -70, Lock::Open, 11);
    old[i].nameKey = 0x0D0D;
  }
  newWalk();
  scanOf(old, 1);
  TEST_ASSERT_TRUE(guideMet(G_Old));
  TEST_ASSERT_FALSE(guideMet(G_Row));
  scanOf(old, 3);
  TEST_ASSERT_TRUE(guideMet(G_Row));
  TEST_ASSERT_EQUAL_HEX16(Game::packName(seedOf(old[0]), (int)Faction::Hedge), game.moments.guide[G_Old]);
}

// The old pole radio's line belongs to the old radios, and only there.
static void test_the_old_radios_line_plays_only_at_one() {
  int k = -1;
  for (int i = 0; i < Lore::OverheardCount; i++) if (strstr(Lore::Overheard[i].text, "pole radio")) k = i;
  TEST_ASSERT_TRUE(k >= 0);
  const Lore::HeardBind& b = Lore::Overheard[k].b;
  TEST_ASSERT_FALSE(b.pool);
  App::HeardHere h{};
  h.well = true;
  TEST_ASSERT_FALSE(App::heardFits(b, h));
  h.old = true;
  TEST_ASSERT_TRUE(App::heardFits(b, h));
  // The three lines about two wells with one name float in the plain pool.
  int floating = 0;
  for (int i = 0; i < Lore::OverheardCount; i++)
    if (strstr(Lore::Overheard[i].text, "name") && Lore::Overheard[i].b.pool) {
      const Lore::HeardBind& f = Lore::Overheard[i].b;
      floating += !f.wards && !f.factions && !f.energies && !f.how && !f.extra && !f.vmin && f.vmax == 0xFFFF;
    }
  TEST_ASSERT_EQUAL(2, floating);   // "...same name, ..." and "...two wells, one name..."
}

// A well where a drifting glyph turns up tonight, on this Deck.
static Net strayWell(uint32_t from) {
  for (uint32_t k = from;; k++) {
    const Net n = wellAt(k, -60);
    if (App::strayRoll(seedOf(n), App::run().runNo, App::run().night) % App::StrayOdds == 0) return n;
  }
}
static void quietStreet(const Net& w, Net* out) {   // the well, and four far ones: Fringe
  out[0] = w;
  for (int i = 1; i < 5; i++) out[i] = wellAt(60000 + i, -88);
}

// Out on a quiet street, past a few new wells, a drifting glyph turns up by a
// well you've never worked, with a soft chime. In the glass, held still, it
// draws itself and plays its rhythm; ENTER keeps it. One a Night.
static void test_a_drifting_glyph_turns_up_and_is_kept() {
  App::hooks.tone = hearTone;
  bindAWeek();
  Net street5[5];
  const Net w = strayWell(3000);
  // Not on a busy street, nor before six new wells, nor by a well you've worked.
  Net busy[12];
  busy[0] = w;
  for (int i = 1; i < 12; i++) busy[i] = wellAt(61000 + i, -70);   // heard, not faint: a Quarter
  newWalk();
  App::game.moments.newSinceStray = 6;
  scanOf(busy, 12);
  TEST_ASSERT_FALSE(App::adrift.on);
  newWalk();
  App::game.moments.newSinceStray = 0;
  quietStreet(w, street5);
  scanOf(street5, 1);
  TEST_ASSERT_FALSE(App::adrift.on);
  // Now: a quiet street, six new wells passed.
  newWalk();
  App::game.moments.newSinceStray = 6;
  nTones = 0;
  scanOf(street5, 5);
  TEST_ASSERT_TRUE(App::adrift.on);
  TEST_ASSERT_EQUAL(Tunes::adrift().n[0].hz, tones[0][0]);
  const uint32_t h = App::strayRoll(seedOf(w), App::run().runNo, App::run().night);
  TEST_ASSERT_EQUAL((int)((h >> 8) % Strays::Shapes), App::adrift.shape);
  TEST_ASSERT_TRUE(st.adrift);
  TEST_ASSERT_EQUAL(1, App::nHeld);   // no words for it but the first-time note, and the sound
  TEST_ASSERT_EQUAL_STRING("A GLYPH DRIFTS BY. PRESS G, HOLD STILL", App::heldText[0]);
  App::nHeld = 0;
  // Switched off while it waits: it's still there tonight.
  App::adrift = App::Adrift{};
  reboot();
  TEST_ASSERT_TRUE(App::adrift.on);
  TEST_ASSERT_EQUAL((int)((h >> 8) % Strays::Shapes), App::adrift.shape);
  // In the glass, held still: it draws itself, then plays its rhythm.
  App::go(App::Mode::Glass);
  holdStill(1);
  nTones = 0;
  for (int i = 0; i < 20; i++) frame();
  TEST_ASSERT_FALSE(App::straySettled());
  frame(key('E'));   // not drawn yet: ENTER opens a well as always
  TEST_ASSERT_EQUAL(0, App::game.moments.nMarks);
  App::go(App::Mode::Glass);
  for (int i = 0; i < 80; i++) frame();
  TEST_ASSERT_TRUE(App::straySettled());
  const Tune r = Tunes::glyph(App::adrift.shape);
  bool rang = false;
  for (int i = 0; i < nTones && i < 64; i++) rang |= tones[i][0] == r.n[0].hz;
  TEST_ASSERT_TRUE(rang);
  frame(key('E'));
  TEST_ASSERT_FALSE(App::adrift.on);
  TEST_ASSERT_EQUAL((int)App::Mode::Glass, (int)App::mode);
  const Game::Moments& m = App::game.moments;
  TEST_ASSERT_EQUAL(1, m.nMarks);
  TEST_ASSERT_EQUAL(0, m.adrift);   // kept: nothing waits any more
  TEST_ASSERT_EQUAL((int)((h >> 8) % Strays::Shapes), m.marks[0].shape);
  TEST_ASSERT_EQUAL_HEX16(0, m.marks[0].name);   // nothing of where it turned up
  TEST_ASSERT_EQUAL(App::run().runNo, m.marks[0].week);
  // One a Night: another chance by another well comes to nothing.
  newWalk();
  App::game.moments.newSinceStray = 20;
  quietStreet(strayWell(w.bssid[1] + (w.bssid[2] << 8) + 1), street5);
  scanOf(street5, 5);
  TEST_ASSERT_FALSE(App::adrift.on);
  // Kept across a reboot.
  reboot();
  TEST_ASSERT_EQUAL(1, App::game.moments.nMarks);
  App::hooks.tone = nullptr;
}

// The same odds on an emptier street: a Hollow brings one as a Fringe street does.
static void test_a_drifting_glyph_turns_up_in_a_hollow_too() {
  bindAWeek();
  const Net w = strayWell(3000);
  Net hollow[3] = {w, wellAt(62001, -88), wellAt(62002, -88)};
  newWalk();
  App::game.moments.newSinceStray = 6;
  scanOf(hollow, 3);
  TEST_ASSERT_EQUAL((int)WardKind::Hollow, (int)sky.ward());
  TEST_ASSERT_TRUE(App::adrift.on);
}

// A flick of the wrist breaks it up: its rhythm, once, and it's gone, unkept.
static void test_a_flick_scatters_a_drifting_glyph() {
  App::hooks.tone = hearTone;
  bindAWeek();
  newWalk();
  App::startStray(4, 321, 0x8123);
  App::go(App::Mode::Glass);
  walking();
  nTones = 0;
  motion.flick = true;
  frame();
  motion.flick = false;
  TEST_ASSERT_TRUE(App::adrift.scatter >= 0);
  for (int i = 0; i < 10; i++) frame();
  TEST_ASSERT_EQUAL(Tunes::glyph(4).n[0].hz, tones[0][0]);
  frame(key('E'));
  for (int i = 0; i < 30; i++) frame();
  TEST_ASSERT_FALSE(App::adrift.on);
  TEST_ASSERT_EQUAL(0, App::game.moments.nMarks);
  App::hooks.tone = nullptr;
}

// Away from the glass it waits: a few minutes and fifteen more new wells, both,
// before it drifts off. Its chime never means "look now".
static void test_a_drifting_glyph_waits_for_you() {
  bindAWeek();
  newWalk();
  App::startStray(1, 99, 0x8123);
  App::game.moments.newSinceStray = 15;
  App::game.playMs += 60000;
  frame();
  TEST_ASSERT_TRUE(App::adrift.on);
  App::game.moments.newSinceStray = 3;
  App::game.playMs += 600000;
  frame();
  TEST_ASSERT_TRUE(App::adrift.on);
  App::game.moments.newSinceStray = 15;
  frame();
  TEST_ASSERT_FALSE(App::adrift.on);
}

static Press arrow(bool right) {
  Press p;
  p.any = true;
  (right ? p.right : p.left) = true;
  return p;
}

// The FOUND tab: a list of sections, ENTER opens one, ESC comes back, and
// left and right turn the columns of WORDS.
static void test_the_found_tab_opens_and_closes() {
  App::go(App::Mode::Grimoire);
  for (int i = 0; i < 4; i++) frame(arrow(true));
  TEST_ASSERT_EQUAL(4, App::tab);
  frame(key('E'));
  TEST_ASSERT_EQUAL(App::FS_Marks, App::foundOpen);
  frame(arrow(true));   // inside a section the arrows stay in it
  TEST_ASSERT_EQUAL(4, App::tab);
  frame(key('B'));
  TEST_ASSERT_EQUAL(-1, App::foundOpen);
  TEST_ASSERT_EQUAL((int)App::Mode::Grimoire, (int)App::mode);
  frame(key('D')); frame(key('D')); frame(key('D'));
  frame(key('E'));
  TEST_ASSERT_EQUAL(App::FS_Words, App::foundOpen);
  frame(arrow(true));
  TEST_ASSERT_EQUAL(1, App::foundSel);
  frame(arrow(false)); frame(arrow(false));
  TEST_ASSERT_EQUAL(App::WordPages - 1, App::foundSel);
  frame(key('B'));
  TEST_ASSERT_EQUAL(App::FS_Words, App::sel);
  frame(arrow(true));
  TEST_ASSERT_EQUAL(5, App::tab);
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Menu, (int)App::mode);
}

// ---- After the walk: the recap, the ribbon, the dawn token, your own streets -------

// Footsteps with the Deck upright in a pocket, screen facing your leg.
static void walkPocketed(float secs) {
  for (float t = 0; t < secs; t += 1 / 30.0f) {
    const float b = 0.25f * sinf(motion.t * 6.2832f * 2);
    const float a[3] = {0, 1 + b, -0.05f}, g[3] = {0.1f * sinf(motion.t * 9), 0, 0};
    motion.update(a, g, 1 / 30.0f);
  }
}
// A pocket walk past `n` new wells a scan; a phone's hotspot passes loudest,
// and a companion who walked along last time is louder still.
static void pocketWalk(int scans, const Net& phone, const Net& companion, uint32_t from) {
  for (int s = 0; s < scans; s++) {
    walkPocketed(2.4f);
    Net nets[5] = {phone, companion, wellAt(from + 3 * s, -66), wellAt(from + 3 * s + 1, -80), wellAt(from + 3 * s + 2, -84)};
    nets[4].hidden = true;
    nets[4].nameKey = 0;
    scanOf(nets, 5);
  }
}

// Taking the Deck out and holding it still, a frame at a time, the way the
// device does it. Returns how many seconds passed before the card came up (or
// -1 if it never did), and keeps going to `secs` so the card has to survive.
static float stillFrames(float secs) {
  const float a[3] = {0, 0.6f, 0.8f}, g[3] = {0, 0, 0};
  float cameUp = -1;
  for (float t = 0; t < secs; t += 1 / 30.0f) {
    motion.update(a, g, 1 / 30.0f);
    frame();
    if (App::recap.card && cameUp < 0) cameUp = t;
  }
  return cameUp;
}

// Walk with the Deck put away, take it out and hold it still: a card says what
// you passed, and stays up. A clean stop is the hard case: the step counter
// runs on for a second after the last footstep, and the card used to come up
// and vanish in that second. A hotspot that passed you, or one that walked
// along, is never counted or named. ENTER opens the loudest well if it's still
// in range; if it's behind you, ENTER and ESC just close the card.
static void test_a_pocket_walk_ends_in_a_recap() {
  App::hooks.tone = hearTone;
  Net phone = wellAt(4000, -40);
  phone.phone = true;
  const Net companion = wellAt(4001, -38);
  App::game.moments.noteTraveller(idOf(companion));
  // Too short a walk: nothing.
  newWalk();
  motion = Motion();
  pocketWalk(20, phone, companion, 4100);
  TEST_ASSERT_EQUAL_FLOAT(-1, stillFrames(3));
  TEST_ASSERT_FALSE(App::recap.card);
  // A real one, ending by a loud well that's still in range when you stop.
  newWalk();
  motion = Motion();
  pocketWalk(90, phone, companion, 4400);
  const Net nearBy = wellAt(4999, -50);
  Net last[2] = {phone, nearBy};
  walkPocketed(2.4f);
  scanOf(last, 2);
  TEST_ASSERT_FALSE(App::recap.card);   // still walking, still put away
  nTones = 0;
  const float cameUp = stillFrames(4);
  TEST_ASSERT_TRUE(cameUp > 0.7f && cameUp < 2.0f);   // after the stop, within two seconds
  TEST_ASSERT_TRUE(App::recap.card);                   // and still up two seconds later
  TEST_ASSERT_TRUE(App::recap.hidden >= 85);   // a hidden well and two named ones a scan, once put away
  TEST_ASSERT_EQUAL(3 * App::recap.hidden + 1, App::recap.fresh);   // NEW means never heard by this Deck
  TEST_ASSERT_EQUAL(App::recap.fresh, App::recap.passed);
  TEST_ASSERT_EQUAL(1, App::recap.wardKinds);
  const WellRec* loud = sky.byId(App::recap.loudId);
  TEST_ASSERT_NOT_NULL(loud);
  TEST_ASSERT_TRUE(WellRules::fixedish(*loud));
  TEST_ASSERT_EQUAL(idOf(nearBy), App::recap.loudId);
  TEST_ASSERT_TRUE(App::recap.loudNear);
  TEST_ASSERT_TRUE(nTones > 0);   // its tune
  TEST_ASSERT_EQUAL(Tunes::Root[(int)loud->energy], tones[0][0]);
  frame(key('m'));   // not the card's key: it stays
  TEST_ASSERT_TRUE(App::recap.card);
  // Two scans without the loudest well: it's behind you now, so there's nothing to open; G is the
  // glass's key and the card stays; ESC puts it away.
  Net onward[1] = {phone};
  scanOf(onward, 1);
  scanOf(onward, 1);
  TEST_ASSERT_TRUE(App::recap.card);
  TEST_ASSERT_FALSE(App::recapCanOpen(App::Ctx{sky, motion, st, true}));
  frame(key('g'));
  TEST_ASSERT_TRUE(App::recap.card);
  TEST_ASSERT_EQUAL((int)App::Mode::Glass, (int)App::mode);
  frame(key('B'));
  TEST_ASSERT_FALSE(App::recap.card);
  TEST_ASSERT_EQUAL((int)App::Mode::Glass, (int)App::mode);
  TEST_ASSERT_EQUAL_STRING("THE DECK KEPT LISTENING IN YOUR POCKET", App::toastText);   // said once
  App::go(App::Mode::Scanner);
  // Heard again, so in range: ENTER opens it.
  scanOf(last, 2);
  App::recap.card = true;
  App::recap.loudId = idOf(nearBy);
  TEST_ASSERT_TRUE(App::recapCanOpen(App::Ctx{sky, motion, st, true}));
  frame(key('E'));
  TEST_ASSERT_FALSE(App::recap.card);
  TEST_ASSERT_EQUAL((int)App::Mode::Well, (int)App::mode);
  TEST_ASSERT_EQUAL(App::recap.loudId, App::wellId);
  // The usual loop: nothing never-heard, still a card (0 NEW is a fact).
  newWalk();
  motion = Motion();
  pocketWalk(90, phone, companion, 4400);
  TEST_ASSERT_TRUE(stillFrames(3) > 0);
  TEST_ASSERT_EQUAL(0, App::recap.fresh);
  TEST_ASSERT_TRUE(App::recap.passed > 0);
  // The loudest well is behind you: no OPEN IT, ENTER only closes the card.
  TEST_ASSERT_FALSE(App::recap.loudNear);
  frame(key('E'));
  TEST_ASSERT_FALSE(App::recap.card);
  TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
  // Glancing at it while still walking isn't stopping: no card until you stop.
  newWalk();
  motion = Motion();
  pocketWalk(90, phone, companion, 5000);
  walkFor(1.5f);
  for (int i = 0; i < 20; i++) frame();
  TEST_ASSERT_FALSE(App::recap.card);
  App::recap = App::Recap{};
  App::hooks.tone = nullptr;
}

// Each walk draws a ribbon once you've walked a minute: a sliver every fifteen
// seconds out, with a tick where you offered. Twenty minutes without a step
// ends it, and the next walk starts another; seven are kept.
static void test_each_walk_draws_a_ribbon() {
  newWalk();
  motion = Motion();
  const Net w = wellAt(6000, -70);
  scanOf(&w, 1);
  walkFor(30);
  for (int i = 0; i < 40; i++) frame();
  TEST_ASSERT_EQUAL(0, App::game.moments.nRibbons);
  walkFor(40);
  frame();
  TEST_ASSERT_EQUAL(1, App::game.moments.nRibbons);
  for (int i = 0; i < 20 * 15; i++) frame();   // fifteen seconds
  App::noteOffering(1, 2, 1);
  for (int i = 0; i < 20 * 31; i++) frame();
  const Game::Ribbon& r = App::ribbonOpen();
  TEST_ASSERT_EQUAL(3, r.len);
  TEST_ASSERT_TRUE(r.s[1] & Game::Ribbon::Offer);
  TEST_ASSERT_FALSE(r.s[0] & Game::Ribbon::Offer);
  // Twenty minutes without a step: the walk is over.
  holdStill(2);
  App::ribbonNow.idle = App::RibbonIdle - 0.01f;
  frame();
  TEST_ASSERT_FALSE(App::ribbonNow.open);
  for (int k = 0; k < 9; k++) {   // more walks; the oldest give way
    walkFor(65);
    frame();
    frame();
    TEST_ASSERT_TRUE(App::ribbonNow.open);
    holdStill(2);
    App::ribbonNow.idle = App::RibbonIdle;
    frame();
    TEST_ASSERT_FALSE(App::ribbonNow.open);
  }
  TEST_ASSERT_EQUAL(Game::Moments::MaxRibbons, App::game.moments.nRibbons);
  App::ribbonNow = App::RibbonNow{};
}

// Each dawn's coin comes with a token, its kind from where you offered. The coin
// itself is the same as ever.
static void test_each_dawn_brings_a_token() {
  bindAWeek();
  App::game.run.n.tithe = 150;
  playNightWithReboots();
  for (int k = 0; k < 5 && App::mode != App::Mode::NightEnd; k++) frame();
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  const Game::Moments& m = App::game.moments;
  TEST_ASSERT_TRUE(m.lastToken >= 1 && m.lastToken <= Game::TokenCount);
  int kept = 0;
  for (int k = 0; k < Game::TokenCount; k++) kept += m.tokens[k];
  TEST_ASSERT_EQUAL(1, kept);
  TEST_ASSERT_EQUAL(m.lastToken - 1, Game::dawnToken(m.byFaction, m.byWard, m.byEnergy,
                                                      Strays::mix(App::run().runNo * 0x9E3779B1u + 0u * 0x85EBCA77u + Keys::deckTag())));
  int offered = 0;
  for (int f = 0; f < 4; f++) offered += m.byFaction[f];
  TEST_ASSERT_TRUE(offered > 0);
  const double coin = App::run().coin;
  App::dawnToken();   // a picture only
  TEST_ASSERT_EQUAL_DOUBLE(coin, App::run().coin);
  reboot();
  TEST_ASSERT_EQUAL(m.lastToken, App::game.moments.lastToken);
}

// A street of wells for the stretch tests: eight fixed, and a phone loudest of all.
static void stretchStreet(Net* out, bool phoneToo) {
  for (int i = 0; i < 8; i++) out[i] = wellAt(7000 + i, (int8_t)(-60 - i * 2));
  out[8] = wellAt(7100, -35);
  out[8].phone = true;
  (void)phoneToo;
}

// With all sixteen stretch slots used, a new stretch takes the place of the one
// walked on the fewest Nights and, among those, the one first walked longest
// ago. Never slot 0 over and over, and never a street walked twice over one
// walked once.
static void test_a_new_stretch_replaces_the_oldest_forgotten_one() {
  Game::Moments& m = App::game.moments;
  for (int i = 0; i < Game::Moments::MaxStretches; i++) {
    Game::Stretch s{};
    s.ids[0] = (uint16_t)(100 + i);
    s.nights[0] = (uint16_t)(1000 + i);   // slot i was first walked on Night i
    s.nNights = 1;
    m.stretches[i] = s;
  }
  m.stretches[0].nights[1] = 1200; m.stretches[0].nNights = 2;   // slot 0: the oldest, but walked twice
  m.stretches[5].nights[1] = 1201; m.stretches[5].nNights = 2;
  Game::Stretch fresh{};
  fresh.ids[0] = 900;
  fresh.nights[0] = 1300;
  fresh.nNights = 1;
  App::keepStretch(fresh);
  TEST_ASSERT_EQUAL(900, m.stretches[1].ids[0]);   // slot 1: walked once, the oldest of those
  TEST_ASSERT_EQUAL(100, m.stretches[0].ids[0]);   // slot 0 untouched
  fresh.ids[0] = 901;
  fresh.nights[0] = 1301;
  App::keepStretch(fresh);
  TEST_ASSERT_EQUAL(901, m.stretches[2].ids[0]);   // then slot 2, not slot 1 again
  TEST_ASSERT_EQUAL(900, m.stretches[1].ids[0]);
  // Every slot walked twice: the one first walked longest ago goes first.
  for (int i = 0; i < Game::Moments::MaxStretches; i++) { m.stretches[i].nNights = 2; m.stretches[i].nights[1] = 1500; }
  fresh.ids[0] = 950;
  fresh.nights[0] = 1400;
  App::keepStretch(fresh);
  TEST_ASSERT_EQUAL(950, m.stretches[0].ids[0]);   // slot 0, first walked on Night 1000
  fresh.ids[0] = 951;
  fresh.nights[0] = 1401;
  App::keepStretch(fresh);
  TEST_ASSERT_EQUAL(951, m.stretches[0].ids[0]);   // the one just kept is the only one walked once: it goes first
  m.stretches[0].nNights = 2;
  fresh.ids[0] = 952;
  fresh.nights[0] = 1402;
  App::keepStretch(fresh);
  TEST_ASSERT_EQUAL(952, m.stretches[3].ids[0]);   // then slot 3 (Night 1003); 1 and 2 are newer now
  TEST_ASSERT_EQUAL(900, m.stretches[1].ids[0]);
  memset(m.stretches, 0, sizeof(m.stretches));
}

// Pass the same stretch of wells on three Nights and the Deck asks whether it's
// a street you keep. Phones' hotspots never belong to a stretch, and never
// make one count as walked.
static void test_a_stretch_walked_on_three_nights_asks() {
  App::hooks.tone = hearTone;
  bindAWeek();
  App::game.career.pages |= 1u << 7;
  Net street9[9];
  stretchStreet(street9, true);
  newWalk();
  const Net start = wellAt(6999, -90);
  scanOf(&start, 1);
  for (int i = 0; i < 8; i++) { Net two[3] = {start, street9[i], street9[8]}; scanOf(two, 3); }
  walkNow += 200000;   // three minutes on
  scanOf(&start, 1);
  const Game::Moments& m = App::game.moments;
  int at = -1;
  for (int i = 0; i < Game::Moments::MaxStretches; i++) if (m.stretches[i].used()) at = i;
  TEST_ASSERT_TRUE(at >= 0);
  const Game::Stretch& st = m.stretches[at];
  TEST_ASSERT_FALSE(st.has(idOf(street9[8])));
  for (int i = 0; i < 8; i++) TEST_ASSERT_TRUE(st.has(idOf(street9[i])));
  TEST_ASSERT_EQUAL(1, st.nNights);
  // The next Night: two of its wells and the phone aren't enough; three are.
  App::run().night++;
  newWalk();
  Net few[3] = {street9[0], street9[1], street9[8]};
  scanOf(few, 3);
  scanOf(few, 3);
  TEST_ASSERT_EQUAL(1, st.nNights);
  scanOf(street9, 9);
  TEST_ASSERT_EQUAL(2, st.nNights);
  scanOf(street9, 9);   // the same Night again counts once
  TEST_ASSERT_EQUAL(2, st.nNights);
  TEST_ASSERT_EQUAL(0, App::nHeld);
  // A third Night: the question, with its loudest well's tune come home.
  App::run().night++;
  newWalk();
  walking();
  nTones = 0;
  scanOf(street9, 9);
  TEST_ASSERT_EQUAL(3, st.nNights);
  TEST_ASSERT_EQUAL(1, st.asked);
  TEST_ASSERT_EQUAL_STRING("A STREET YOU KEEP?", App::heldText[0]);
  const WellRec* loud = sky.byId(idOf(street9[0]));
  TEST_ASSERT_EQUAL_HEX16(Game::packName(loud->seed, (int)loud->faction), m.keepName);
  const Tune t = Tunes::of(loud->seed, (int)loud->energy, true);
  TEST_ASSERT_EQUAL(t.n[0].hz, tones[0][0]);
  // Asked once.
  App::nHeld = 0;
  App::run().night++;
  newWalk();
  scanOf(street9, 9);
  TEST_ASSERT_EQUAL(0, App::nHeld);
  App::hooks.tone = nullptr;
}

static Press typeKey(char k) {
  Press p;
  p.any = true;
  p.key = k;
  if (k >= 'A' && k <= 'Z') p.ch = (char)(k + 32);
  return p;
}
static Press delKey() {
  Press p;
  p.any = p.del = p.back = true;
  return p;
}

// ADD on the STREETS tab: two initials, then a name over the suggested one,
// written in under the others in a hand from how you typed, dated by the week.
// Four at most; ENTER twice crosses one of yours out.
static void test_you_write_your_own_street_in() {
  using namespace App;
  Game::Moments& m = game.moments;
  game.career.pages |= 1u << 7;
  m.keepName = Game::packName(0x0123u, 1);
  go(Mode::Grimoire);
  frame(arrow(false));   // left from PAGES: STREETS
  TEST_ASSERT_EQUAL(5, tab);
  TEST_ASSERT_EQUAL(StreetCount, sel);   // on ADD
  frame(key('E'));
  TEST_ASSERT_TRUE(edit.on);
  char want[13];
  suggestedStreet(want);
  TEST_ASSERT_EQUAL_STRING(want, edit.name);
  TEST_ASSERT_TRUE(strncmp(want, "BY ", 3) == 0 && strlen(want) <= 12);
  frame(typeKey('7'));   // initials are letters
  frame(typeKey('N'));
  frame(); frame();
  frame(typeKey('P'));
  TEST_ASSERT_EQUAL(1, edit.field);
  frame(typeKey('E')); frame(); frame(); frame();
  frame(typeKey('L')); frame();
  frame(typeKey('X'));
  frame(delKey());     // erases the X, stays
  TEST_ASSERT_TRUE(edit.on);
  frame(typeKey('M'));
  const char* rest = " ROW 9 - LONGER THAN TWELVE";
  for (const char* k = rest; *k; k++) { frame(typeKey(*k)); frame(); }
  TEST_ASSERT_EQUAL(12, (int)strlen(edit.name));
  frame(key('E'));
  TEST_ASSERT_FALSE(edit.on);
  TEST_ASSERT_EQUAL(1, m.nStreets);
  TEST_ASSERT_EQUAL(0, m.keepName);
  const Game::OwnStreet& o = m.streets[0];
  TEST_ASSERT_EQUAL('N', o.who[0]);
  TEST_ASSERT_EQUAL('P', o.who[1]);
  TEST_ASSERT_EQUAL_MEMORY("ELM ROW 9 -", o.street, 11);   // the space at the end let go
  TEST_ASSERT_EQUAL(0, o.street[11]);
  TEST_ASSERT_EQUAL(weekNow(), o.week);
  TEST_ASSERT_TRUE(o.advance >= 5 && o.advance <= 7);
  for (int i = 0; i < 11; i++) {
    const char c = o.street[i];
    TEST_ASSERT_TRUE((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'');
  }
  // Kept in the Deck's memory, across a reboot.
  reboot();
  TEST_ASSERT_EQUAL(1, game.moments.nStreets);
  // Only you cross it out: ENTER, then ENTER again.
  go(Mode::Grimoire);
  frame(arrow(false));
  TEST_ASSERT_EQUAL(StreetCount, sel);   // nothing to add: on your street
  frame(key('E'));
  TEST_ASSERT_FALSE(m.streets[0].crossed);
  frame(key('E'));
  TEST_ASSERT_TRUE(m.streets[0].crossed);
  // Keeping the suggested name: ENTER after the initials.
  m.keepName = Game::packName(0x0456u, 2);
  streetsEnter();
  frame(key('E'));
  frame(typeKey('A')); frame(typeKey('B'));
  frame(key('E'));
  suggestedStreet(want);
  TEST_ASSERT_EQUAL(0, m.keepName);   // (suggestedStreet reads it, so compare with what was written)
  char got[13] = {0};
  memcpy(got, m.streets[1].street, 12);
  TEST_ASSERT_TRUE(strncmp(got, "BY ", 3) == 0);
  // Four at most: a crossed-out one makes room, and the new one takes its place...
  m.nStreets = Game::Moments::MaxStreets;
  m.keepName = Game::packName(0x0789u, 0);
  TEST_ASSERT_TRUE(m.streets[0].crossed);
  TEST_ASSERT_TRUE(canAddStreet());
  const Game::OwnStreet second = m.streets[1];
  streetsEnter();
  TEST_ASSERT_EQUAL(StreetCount + Game::Moments::MaxStreets, sel);   // on ADD
  frame(key('E'));
  frame(typeKey('A')); frame(typeKey('B'));
  frame(key('E'));
  TEST_ASSERT_EQUAL(Game::Moments::MaxStreets, m.nStreets);
  TEST_ASSERT_EQUAL_MEMORY(&second, &m.streets[0], sizeof(second));   // the crossed-out one gave way
  TEST_ASSERT_EQUAL('A', m.streets[3].who[0]);
  TEST_ASSERT_FALSE(m.streets[3].crossed);
  // ...but four not crossed out: no more ADD.
  for (int i = 0; i < m.nStreets; i++) m.streets[i].crossed = 0;
  m.keepName = Game::packName(0x0789u, 0);
  TEST_ASSERT_FALSE(canAddStreet());
  streetsEnter();
  TEST_ASSERT_EQUAL(StreetCount + Game::Moments::MaxStreets - 1, sel);
  // ESC while writing puts it away unwritten.
  m.nStreets = 2;
  streetsEnter();
  frame(key('E'));
  TEST_ASSERT_TRUE(edit.on);
  frame(key('B'));
  TEST_ASSERT_FALSE(edit.on);
  TEST_ASSERT_EQUAL(2, m.nStreets);
  TEST_ASSERT_EQUAL((int)Mode::Grimoire, (int)mode);
  m.nStreets = 0;
  m.keepName = 0;
}

// ---- Guards, footers and first-time notes (the QA pass) -------------------------------

// On BIND A NEW WEEK, ENTER on a choice moves the cursor down to BIND; ENTER there binds.
static void test_enter_walks_the_bind_rows_down() {
  App::go(App::Mode::Bind);
  frame(key('E'));
  TEST_ASSERT_EQUAL(1, App::sel);
  frame(key('E'));
  TEST_ASSERT_EQUAL(2, App::sel);
  frame(key('E'));
  TEST_ASSERT_TRUE(App::inRun());
}

// ESC on the visit goes one step back, to the well's page, and ESC there to the list.
static void test_esc_leaves_the_visit() {
  bindAWeek();
  for (int k = 0; k < 3; k++) frame();
  const uint32_t id = st.starId;
  App::wellId = id;
  App::listMode = App::Mode::Glass;
  App::go(App::Mode::Visit);
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Well, (int)App::mode);
  TEST_ASSERT_EQUAL_UINT32(id, App::wellId);
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Glass, (int)App::mode);
  App::listMode = App::Mode::Scanner;
}

// A Gift with no room waits at the dawn: ENTER, then a relic of yours to sell for it; ESC twice leaves it.
static void test_a_gift_waits_for_room_at_dawn() {
  bindAWeek();
  Game::Run& r = App::game.run;
  const int ids[5] = {Cat::R_ember_caller, Cat::R_tide_caller, Cat::R_gale_caller, Cat::R_hush_caller, Cat::R_static_caller};
  for (int id : ids) Game::addRelic(r, id);
  App::game.lastEnd = Game::NightEnd{};
  App::game.lastEnd.paid = true;
  App::game.lastEnd.portents = 2;
  App::game.lastEnd.got[0] = Game::PortentResult{Cat::Portent_gift, Cat::R_porch_light, 0, -1};
  App::game.lastEnd.got[1] = Game::PortentResult{Cat::Portent_gift, Cat::R_rush_hour, 0, -1};
  App::game.moments.portent = Game::PortentChoice{};
  App::game.moments.swap = Game::GiftSwap{{(uint8_t)(Cat::R_porch_light + 1), (uint8_t)(Cat::R_rush_hour + 1), 0}, {-1, -1, -1}, {-1, -1, -1}};
  App::go(App::Mode::NightEnd);
  frame(key('E'));   // skip the tally
  TEST_ASSERT_EQUAL(0, App::giftToPlace(App::game.lastEnd));
  frame(key('E'));   // make room: your relics
  TEST_ASSERT_TRUE(App::giftPicking);
  App::sel = 1;      // sell the Tide caller
  frame(key('E'));
  TEST_ASSERT_TRUE(Game::owns(r, Cat::R_porch_light));
  TEST_ASSERT_FALSE(Game::owns(r, Cat::R_tide_caller));
  TEST_ASSERT_EQUAL(5, r.nRelics);
  TEST_ASSERT_EQUAL((int)App::Mode::NightEnd, (int)App::mode);
  // The second: ESC once asks, a second ESC leaves it.
  TEST_ASSERT_EQUAL(1, App::giftToPlace(App::game.lastEnd));
  frame(key('B'));
  TEST_ASSERT_EQUAL_STRING("ESC AGAIN TO LEAVE IT", App::toastText);
  TEST_ASSERT_EQUAL(1, App::giftToPlace(App::game.lastEnd));
  frame(key('B'));
  TEST_ASSERT_EQUAL(-1, App::giftToPlace(App::game.lastEnd));
  TEST_ASSERT_FALSE(Game::owns(r, Cat::R_rush_hour));
  TEST_ASSERT_EQUAL(-2, App::game.lastEnd.got[1].relic);
  frame(key('E'));   // on to the stall
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
}

// The dawn's lines that matter (MERCY, what crumbled) always find room; the token
// line and then the Collector's remark make way for them.
static bool dawnSays(const char* text) {
  App::DawnLine l[App::DawnLineMax];
  int n = App::dawnLines(App::game.lastEnd, App::game.run, l);
  App::fitDawnLines(l, n, App::dawnTop(App::game.lastEnd));
  for (int i = 0; i < n; i++) if (!l[i].say && !strcmp(l[i].text, text)) return true;
  return false;
}
static void test_the_dawn_always_says_mercy_and_what_crumbled() {
  bindAWeek();
  Game::NightEnd& e = App::game.lastEnd;
  e = Game::NightEnd{};
  e.paid = true;
  e.spare = 1;
  e.trade = 6;
  e.spareCoin = 1;
  e.reward = 5;
  e.mercy = true;
  e.withered = true;
  App::game.run.night = 2;
  App::game.moments.portent = Game::PortentChoice{};
  App::game.moments.lastToken = Game::T_Laundromat + 1;
  App::game.moments.dawn = Game::DawnNotes{};
  App::game.moments.dawn.crumbled = Cat::R_porch_light + 1;
  App::game.moments.dawn.nCrumbled = 1;
  TEST_ASSERT_TRUE(dawnSays("MERCY: THE NEXT STALL GOES EASY ON YOU"));
  TEST_ASSERT_TRUE(dawnSays("PORCH LIGHT CRUMBLED"));
  TEST_ASSERT_TRUE(App::collectorShown(e, App::game.run));   // a short tally leaves room for all of it
  // A long tally: the token line and the Collector go, MERCY and the crumble stay.
  e.favors = 4;
  e.interest = 2;
  e.gilt = 2;
  e.thread = 4;
  App::game.moments.dawn.nCrumbled = 2;
  TEST_ASSERT_TRUE(dawnSays("MERCY: THE NEXT STALL GOES EASY ON YOU"));
  TEST_ASSERT_TRUE(dawnSays("2 WITHERED RELICS CRUMBLED"));
  TEST_ASSERT_FALSE(dawnSays("A LAUNDROMAT TOKEN TURNED UP"));
  TEST_ASSERT_FALSE(App::collectorShown(e, App::game.run));
  // Three Portents and the token: every Portent's line has room.
  e = Game::NightEnd{};
  e.paid = true;
  e.spare = 11;
  e.portents = 3;
  e.got[0] = Game::PortentResult{Cat::Portent_windfall, -1, 10, -1};
  e.got[1] = Game::PortentResult{Cat::Portent_tuning, -1, 0, -1};
  e.got[2] = Game::PortentResult{Cat::Portent_chalk_mark, -1, 0, -2};
  TEST_ASSERT_TRUE(dawnSays("WINDFALL: +10 COIN"));
  TEST_ASSERT_TRUE(dawnSays("CHALK MARK: YOU HOLD 4 ALREADY"));
}

// A Scroll with no spell slot waits at the dawn: ENTER, then a spell of yours to forget; ESC twice leaves it.
static void test_a_scroll_waits_for_a_slot_at_dawn() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_glamour;
  r.spells[r.nSpells++] = Cat::Spell_hex;
  App::game.lastEnd = Game::NightEnd{};
  App::game.lastEnd.paid = true;
  App::game.lastEnd.portents = 2;
  App::game.lastEnd.got[0] = Game::PortentResult{Cat::Portent_scroll, -1, 0, Cat::Spell_haste};
  App::game.lastEnd.got[1] = Game::PortentResult{Cat::Portent_scroll, -1, 0, Cat::Spell_echo};
  App::game.moments.portent = Game::PortentChoice{};
  App::game.moments.dawn = Game::DawnNotes{};
  App::game.moments.swap = Game::GiftSwap{{(uint8_t)(Game::SwapSpell | (Cat::Spell_haste + 1)),
                                            (uint8_t)(Game::SwapSpell | (Cat::Spell_echo + 1)), 0},
                                           {-1, -1, -1}, {-1, -1, -1}};
  App::go(App::Mode::NightEnd);
  frame(key('E'));   // skip the tally
  TEST_ASSERT_EQUAL(-1, App::giftToPlace(App::game.lastEnd));
  TEST_ASSERT_EQUAL(0, App::spellToPlace(App::game.lastEnd));
  frame(key('E'));   // make room: your spells
  TEST_ASSERT_TRUE(App::spellPicking);
  App::sel = 1;      // forget HEX
  frame(key('E'));
  TEST_ASSERT_EQUAL(Cat::Spell_glamour, r.spells[0]);
  TEST_ASSERT_EQUAL(Cat::Spell_haste, r.spells[1]);
  TEST_ASSERT_EQUAL(2, r.nSpells);
  TEST_ASSERT_EQUAL_STRING("HEX FORGOTTEN", App::toastText);
  // The second: ESC once asks, a second ESC leaves it.
  TEST_ASSERT_EQUAL(1, App::spellToPlace(App::game.lastEnd));
  frame(key('B'));
  TEST_ASSERT_EQUAL_STRING("ESC AGAIN TO LEAVE IT", App::toastText);
  frame(key('B'));
  TEST_ASSERT_EQUAL(-1, App::spellToPlace(App::game.lastEnd));
  TEST_ASSERT_EQUAL(-2, App::game.lastEnd.got[1].spell);
  TEST_ASSERT_TRUE(dawnSays("SCROLL: HASTE"));
  TEST_ASSERT_TRUE(dawnSays("NO SLOT, SO YOU LEFT THE FREE SPELL"));
  frame(key('E'));   // on to the stall
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
}

// Empty pockets: the Night's relic with no room waits on the same screen as a Gift.
static void test_the_empty_pockets_relic_waits_at_dawn() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.challenge = 7;
  const int ids[5] = {Cat::R_ember_caller, Cat::R_tide_caller, Cat::R_gale_caller, Cat::R_hush_caller, Cat::R_static_caller};
  for (int id : ids) Game::addRelic(r, id);
  App::game.lastEnd = Game::NightEnd{};
  App::game.lastEnd.paid = true;
  App::game.lastEnd.gift = Cat::R_porch_light;
  App::game.moments.portent = Game::PortentChoice{};
  App::game.moments.swap = Game::GiftSwap{};
  App::game.moments.dawn = Game::DawnNotes{};
  App::game.moments.dawn.gift = Cat::R_porch_light + 1;
  App::game.moments.dawn.giftEd = -1;
  App::go(App::Mode::NightEnd);
  frame(key('E'));   // skip the tally
  TEST_ASSERT_EQUAL(3, App::giftToPlace(App::game.lastEnd));
  frame(key('B'));
  frame(key('B'));   // leave it
  TEST_ASSERT_EQUAL(-1, App::giftToPlace(App::game.lastEnd));
  TEST_ASSERT_FALSE(Game::owns(r, Cat::R_porch_light));
  TEST_ASSERT_EQUAL(Cat::R_porch_light, Game::nightRelic(App::game.lastEnd));
  char b[40];
  snprintf(b, sizeof(b), "LEFT THE RELIC. +1 ATTUNEMENT: %s", App::energyWord(r.focus));
  TEST_ASSERT_TRUE(dawnSays(b));
  TEST_ASSERT_FALSE(dawnSays("MERCY: THE NEXT STALL GOES EASY ON YOU"));
}

// The stall asks twice before leaving, selling or polishing; a cursor move withdraws the question.
static void test_the_stall_asks_twice() {
  bindAWeek();
  Game::Run& r = App::game.run;
  Game::addRelic(r, Cat::R_tide_caller);
  r.coin = 50;
  Game::openShop(App::game.shop, r, App::game.career);
  App::game.phase = Game::Phase::Shop;
  App::openStall();
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  TEST_ASSERT_EQUAL_STRING("ESC AGAIN TO LEAVE THE STALL", App::toastText);
  frame(arrow(true));   // a move withdraws it: the next ESC asks again
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  // YOURS: X once asks, twice sells; ENTER once asks, twice polishes.
  App::tab = App::TabYours;
  App::sel = 0;
  const double coin = r.coin;
  frame(key('E'));
  TEST_ASSERT_EQUAL(1, r.nRelics);
  TEST_ASSERT_EQUAL(0, r.relics[0].tier);
  TEST_ASSERT_EQUAL_STRING("ENTER AGAIN TO POLISH FOR 6c", App::toastText);
  frame(key('E'));
  TEST_ASSERT_EQUAL(1, r.relics[0].tier);
  TEST_ASSERT_TRUE(r.coin < coin);
  frame(key('x'));
  TEST_ASSERT_EQUAL(1, r.nRelics);
  char ask[40];
  snprintf(ask, sizeof(ask), "X AGAIN TO SELL FOR %dc (POLISH LOST)", Game::sellBack(r, r.relics[0]));   // polished just above
  TEST_ASSERT_EQUAL_STRING(ask, App::toastText);
  frame(key('x'));
  TEST_ASSERT_EQUAL(0, r.nRelics);
  // And leaving, asked and answered.
  frame(key('B'));
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::NightStart, (int)App::mode);
}

// The show: ESC leaves at once, ENTER mid-show skips to the end, and one ENTER on a finished show leaves.
static void test_the_show_leaves_on_one_key() {
  bindAWeek();
  for (int k = 0; k < 3; k++) frame();
  for (int pass = 0; pass < 2; pass++) {
    App::wellId = st.starId;
    App::go(App::Mode::Visit);
    App::buildActions(ctx());
    App::sel = 0;
    frame(key('E'));   // draw
    for (int k = 0; k < 60; k++) frame();
    App::buildActions(ctx());
    for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Sell && App::actions[i].ok) App::sel = i;
    frame(key('E'));
    TEST_ASSERT_EQUAL((int)App::Mode::Show, (int)App::mode);
    if (pass == 0) {
      frame(key('B'));   // ESC: out at once
      TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
      street(3);
      continue;
    }
    frame(key('E'));   // ENTER mid-show skips to the end (with no relics the end comes by itself) and doesn't leave
    TEST_ASSERT_EQUAL((int)App::Mode::Show, (int)App::mode);
    for (int k = 0; k < 10 && App::showDoneT < 0; k++) frame();
    TEST_ASSERT_TRUE(App::showDoneT >= 0);
    for (int k = 0; k < 10; k++) frame();
    frame(key('E'));   // one ENTER on a finished show leaves
    TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
  }
}

// A first-time note says its line once, in order after whatever is on screen, and never again,
// across a reboot.
static void test_first_time_notes_say_their_line_once() {
  App::toast("OFF FOR 1H 00M", 0, 3.0f);
  App::teachToast(App::Taught::OffFor, "WELLS KEPT SETTLING WHILE IT WAS OFF");
  TEST_ASSERT_EQUAL_STRING("OFF FOR 1H 00M", App::toastText);   // it waits its turn
  for (int k = 0; k < 70; k++) frame();
  TEST_ASSERT_EQUAL_STRING("WELLS KEPT SETTLING WHILE IT WAS OFF", App::toastText);
  TEST_ASSERT_TRUE(App::taught(App::Taught::OffFor));
  reboot();
  TEST_ASSERT_TRUE(App::taught(App::Taught::OffFor));
  App::toastT = 0;
  App::teachToast(App::Taught::OffFor, "WELLS KEPT SETTLING WHILE IT WAS OFF");
  TEST_ASSERT_TRUE(App::toastT <= 0);   // said once
  // Two lines on a screen you're reading come one after the other.
  App::teachToast(App::Taught::Mercy, "MERCY: A CHEAP RELIC AND A FREE REROLL", "PLUS ONE ATTUNEMENT ON YOUR FOCUS");
  TEST_ASSERT_EQUAL_STRING("MERCY: A CHEAP RELIC AND A FREE REROLL", App::toastText);
  for (int k = 0; k < 60; k++) frame();
  TEST_ASSERT_EQUAL_STRING("PLUS ONE ATTUNEMENT ON YOUR FOCUS", App::toastText);
  App::toastT = 0;
}

// A held note that follows a find comes in the same stop, right after it; another find waits for
// the next stop, as before.
static void test_held_notes_follow_their_find() {
  App::nHeld = 0;
  App::toastT = 0;
  App::go(App::Mode::Scanner);
  App::holdNote("FOUND: LEFT ON", UI::Accent);
  App::teachHeld(App::Taught::Find, "YOUR FINDS: SEE GRIMOIRE > FOUND");
  App::holdNote("FOUND: COLD ONE", UI::Accent);
  TEST_ASSERT_EQUAL(3, App::nHeld);
  motion = Motion();
  holdStill(1.2f);
  frame();
  TEST_ASSERT_EQUAL_STRING("FOUND: LEFT ON", App::toastText);
  for (int k = 0; k < 45; k++) { holdStill(0.05f); frame(); }
  TEST_ASSERT_EQUAL_STRING("YOUR FINDS: SEE GRIMOIRE > FOUND", App::toastText);
  for (int k = 0; k < 60; k++) { holdStill(0.05f); frame(); }
  TEST_ASSERT_TRUE(App::toastT <= 0);   // the second find waits for the next stop
  TEST_ASSERT_EQUAL(1, App::nHeld);
  TEST_ASSERT_TRUE(App::taught(App::Taught::Find));
  App::nHeld = 0;
}

// The GIVE UP warning goes with the menu: back out, and the scanner is clean.
static void test_the_give_up_warning_goes_with_the_menu() {
  bindAWeek();
  App::go(App::Mode::Menu);
  App::buildMenu();
  TEST_ASSERT_EQUAL(App::MAbandon, App::menuItems[App::nMenu - 1]);   // last, below SOUND
  App::sel = App::nMenu - 1;
  frame(key('E'));
  TEST_ASSERT_TRUE(App::inRun());
  TEST_ASSERT_TRUE(App::toastT > 0);
  frame(key('B'));
  TEST_ASSERT_EQUAL((int)App::Mode::Scanner, (int)App::mode);
  TEST_ASSERT_TRUE(App::toastT <= 0);
  TEST_ASSERT_TRUE(App::inRun());
}

// Writing a street in: the suggestion stays until you type over it, an empty name is refused,
// and a deleted suggestion never comes back.
static void test_an_empty_street_name_is_refused() {
  Game::Moments& m = App::game.moments;
  App::game.career.pages |= 1u << 7;
  m.keepName = Game::packName(0x0B5Fu, 1);
  m.nStreets = 0;
  App::go(App::Mode::Grimoire);
  App::tab = 5;
  App::streetsEnter();
  frame(key('E'));   // ADD
  TEST_ASSERT_TRUE(App::edit.on);
  Press n; n.any = true; n.key = 'N';
  Press p; p.any = true; p.key = 'P';
  frame(n); frame(p);
  TEST_ASSERT_EQUAL(1, App::edit.field);
  TEST_ASSERT_FALSE(App::edit.typed);
  Press del; del.any = true; del.del = true;
  frame(del);   // clears the suggestion
  TEST_ASSERT_TRUE(App::edit.typed);
  TEST_ASSERT_EQUAL(0, (int)strlen(App::edit.name));
  frame(key('E'));   // nothing to write: refused
  TEST_ASSERT_EQUAL(0, m.nStreets);
  TEST_ASSERT_TRUE(App::edit.on);
  Press a; a.any = true; a.key = 'A';
  frame(a);
  frame(key('E'));
  TEST_ASSERT_EQUAL(1, m.nStreets);
  TEST_ASSERT_EQUAL('A', m.streets[0].street[0]);
  TEST_ASSERT_EQUAL(0, m.streets[0].street[1]);
  m.nStreets = 0;
  m.keepName = 0;
  App::game.career.pages &= ~(1u << 7);
}

// GIVE UP asks twice, and the question goes when its warning does: an ENTER
// minutes later asks again instead of ending the week.
static void test_give_up_is_only_armed_while_it_says_so() {
  bindAWeek();
  App::go(App::Mode::Menu);
  for (int i = 0; i < 12; i++) frame(key('D'));   // the last row
  frame(key('E'));
  TEST_ASSERT_TRUE(App::confirmAbandon);
  TEST_ASSERT_TRUE(App::toastSays(App::AbandonAsk));
  for (int i = 0; i < 70; i++) frame();   // the warning ends
  TEST_ASSERT_FALSE(App::confirmAbandon);
  frame(key('E'));
  TEST_ASSERT_TRUE(App::inRun());
  TEST_ASSERT_TRUE(App::confirmAbandon);
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Lost, (int)App::mode);
  App::toastT = 0;
}

// One ward a Night: a second twist casts nothing and says so; with no week bound, a twist is nothing.
static void test_a_second_twist_says_theres_no_ward_left() {
  App::toastT = 0;
  TEST_ASSERT_FALSE(App::twist(ctx()));
  TEST_ASSERT_TRUE(App::toastT <= 0);
  bindAWeek();
  TEST_ASSERT_TRUE(App::twist(ctx()));
  TEST_ASSERT_EQUAL_STRING("HIDDEN: THE WARDENS CAN'T HEAR YOU", App::toastText);
  TEST_ASSERT_FALSE(App::twist(ctx()));
  TEST_ASSERT_EQUAL_STRING("NO WARD LEFT TONIGHT", App::toastText);
}

// On a dimmed or face-down screen, notes, toasts and the Overheard wait for it to light up.
static void test_words_wait_for_a_lit_screen() {
  App::go(App::Mode::Scanner);
  App::toast("ONE");
  App::toastAfter("TWO");
  st.awake = false;
  for (int i = 0; i < 100; i++) frame();
  TEST_ASSERT_EQUAL_STRING("ONE", App::toastText);
  TEST_ASSERT_TRUE(App::toastT > 1.5f);
  st.awake = true;
  for (int i = 0; i < 40; i++) frame();
  TEST_ASSERT_EQUAL_STRING("TWO", App::toastText);
  for (int i = 0; i < 40; i++) frame();
  App::nHeld = 0;
  App::holdNote("A NOTE", UI::Text);
  motion = Motion();
  holdStill(2);
  st.awake = false;
  for (int i = 0; i < 40; i++) frame();
  TEST_ASSERT_EQUAL(1, App::nHeld);
  st.awake = true;
  frame();
  TEST_ASSERT_EQUAL(0, App::nHeld);
  TEST_ASSERT_EQUAL_STRING("A NOTE", App::toastText);
  App::toastT = 0;
}

// Rank rises with Tithes paid, and says so when it does.
static void test_a_new_rank_is_said() {
  App::go(App::Mode::Scanner);
  App::rankSeen = -1;
  frame();
  TEST_ASSERT_EQUAL(0, App::rankSeen);
  App::game.career.stats[(int)Cat::Stat::nights_paid] = 3;
  frame();
  TEST_ASSERT_EQUAL_STRING("YOU ARE A DOWSER NOW", App::toastText);
  App::toastT = 0;
  frame();
  TEST_ASSERT_TRUE(App::toastT <= 0);   // once
  App::game.career.stats[(int)Cat::Stat::nights_paid] = 200;
  frame();
  TEST_ASSERT_EQUAL_STRING("YOU ARE AN OLD ONE NOW", App::toastText);
  App::toastT = 0;
  App::rankSeen = -1;
}

// Back on after a while: how long, and how many wells settled meanwhile. The
// first-time note only comes when some did.
static void test_the_off_time_says_what_settled() {
  App::go(App::Mode::Scanner);
  App::noteOff(30, 0);
  TEST_ASSERT_EQUAL_STRING("OFF FOR 30 MIN", App::toastText);
  TEST_ASSERT_FALSE(App::taught(App::Taught::OffFor));
  App::toastT = 0;
  App::noteOff(182, 4);
  TEST_ASSERT_EQUAL_STRING("OFF FOR 3H 02M. 4 WELLS SETTLED", App::toastText);
  TEST_ASSERT_TRUE(App::taught(App::Taught::OffFor));
  App::toastT = 0;
  App::nToastNext = 0;
  App::noteOffUnknown(0);
  TEST_ASSERT_TRUE(App::toastT <= 0);
  App::noteOffUnknown(2);
  TEST_ASSERT_EQUAL_STRING("COULDN'T TELL HOW LONG IT WAS OFF", App::toastText);
  App::toastT = 0;
  App::settleDue = false;
}

// A toast set while the boot animation plays waits for it to end.
static void test_a_toast_waits_out_the_boot() {
  App::go(App::Mode::Boot);
  App::toastAfter("OLD SAVE SET ASIDE. NEW CAREER", UI::Rec, 3.0f);
  for (int i = 0; i < 20; i++) frame();
  TEST_ASSERT_EQUAL((int)App::Mode::Boot, (int)App::mode);
  TEST_ASSERT_TRUE(App::toastT >= 3.0f);
  for (int i = 0; i < 40; i++) frame();   // the boot ends
  TEST_ASSERT_TRUE(App::mode != App::Mode::Boot);
  TEST_ASSERT_TRUE(App::toastT > 2.0f);
  App::toastT = 0;
}

// A reroll that would throw away Mercy's half-price relic asks first, and leaving with free rerolls says so.
static void test_the_stall_says_what_a_reroll_and_leaving_lose() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.coin = 50;
  r.night = 1;   // the stall after Night 1: the Caller and the half-price relic both on the shelf
  r.shopFlags |= Game::SF_Mercy;
  App::openStall();
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  bool half = false;
  for (int i = 0; i < App::game.shop.nRelics; i++) half |= App::game.shop.relics[i].half;
  TEST_ASSERT_TRUE(half);
  frame(key('B'));
  TEST_ASSERT_EQUAL_STRING("ESC AGAIN: 1 FREE REROLL UNUSED", App::toastText);
  frame(arrow(true));
  frame(key('r'));
  TEST_ASSERT_EQUAL(1, App::game.shop.freeRerolls);
  TEST_ASSERT_TRUE(strstr(App::toastText, "R AGAIN") == App::toastText);
  frame(key('r'));
  TEST_ASSERT_EQUAL(0, App::game.shop.freeRerolls);
  TEST_ASSERT_EQUAL_STRING("NEW RELICS", App::toastText);
}

// The Glass house crown isn't offered for sale, and a full pair of trinket slots says FULL.
static void test_the_stall_says_what_cant_be_had() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.challenge = 3;
  Game::addRelic(r, Cat::R_glass_crown);
  r.coin = 50;
  r.trinkets[r.nTrinkets++] = Cat::Trinket_haggler;
  r.trinkets[r.nTrinkets++] = Cat::Trinket_pawn_ticket;
  App::openStall();
  App::tab = App::TabYours;
  App::sel = 0;
  frame(key('x'));
  TEST_ASSERT_EQUAL_STRING("THE GLASS CROWN CAN'T BE SOLD", App::toastText);
  frame(key('x'));
  TEST_ASSERT_EQUAL(1, r.nRelics);
  App::game.shop.trinket.kind = Game::OfferKind::Trinket;
  App::game.shop.trinket.id = Cat::Trinket_lucky_coin;
  App::game.shop.trinket.sold = 0;
  App::tab = App::TabWares;
  App::sel = App::game.shop.nCons;
  frame(key('E'));
  TEST_ASSERT_EQUAL_STRING("TRINKET SLOTS FULL (2 OF 2)", App::toastText);
  TEST_ASSERT_EQUAL(2, r.nTrinkets);
}

// Selling one to buy sells the one you picked, and forgetting a spell forgets the one you picked.
static void test_the_stall_lets_go_of_the_one_you_pick() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.coin = 60;
  r.night = 2;
  App::openStall();
  while (Game::slotsUsed(r) < Game::relicSlots(r)) Game::addRelic(r, Cat::R_crowd_feeder + r.nRelics);
  const int keep0 = r.relics[0].id, gone = r.relics[2].id;
  App::game.shop.relics[0] = Game::relicOffer(Cat::R_long_receipt, -1, -1);
  App::tab = App::TabRelics;
  App::sel = 0;
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::ShopDrop, (int)App::mode);
  App::sel = 2;
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::Shop, (int)App::mode);
  TEST_ASSERT_TRUE(Game::owns(r, keep0));
  TEST_ASSERT_FALSE(Game::owns(r, gone));
  TEST_ASSERT_TRUE(Game::owns(r, Cat::R_long_receipt));
  TEST_ASSERT_EQUAL_STRING("BOUGHT LONG RECEIPT", App::toastText);
  // Spells: the second one goes.
  r.nSpells = 0;
  r.spells[r.nSpells++] = Cat::Spell_hex;
  r.spells[r.nSpells++] = Cat::Spell_glamour;
  App::game.shop.cons[0].kind = Game::OfferKind::Spell;
  App::game.shop.cons[0].id = Cat::Spell_haste;
  App::game.shop.cons[0].sold = 0;
  App::tab = App::TabWares;
  App::sel = 0;
  frame(key('E'));
  TEST_ASSERT_EQUAL((int)App::Mode::ShopForget, (int)App::mode);
  App::sel = 1;
  frame(key('E'));
  TEST_ASSERT_EQUAL(Cat::Spell_hex, r.spells[0]);
  TEST_ASSERT_EQUAL(Cat::Spell_haste, r.spells[1]);
}

// ---- What the screen says matches what the rules do --------------------------

// The long way: the visit's estimate (and so the star) prices an old well at 0, as the offering pays.
static void test_the_long_way_estimate_is_zero() {
  bindAWeek();
  for (int k = 0; k < 3; k++) frame();
  Game::Run& r = App::game.run;
  r.challenge = 5;
  const WellRec* w = sky.byId(st.starId);
  TEST_ASSERT_NOT_NULL(w);
  const Game::WellView v = App::viewOf(*w, sky, st.nowMs);
  r.hold[Game::Tide] = 6;
  Game::sell(r, App::game.atlas, v, App::hereNow(ctx()), Game::Tide);
  r.night = 1;
  r.hold[Game::Tide] = 6;
  TEST_ASSERT_EQUAL_DOUBLE(0, App::estimateSale(v, App::hereNow(ctx()), Game::Tide));
  const Game::Sold s = Game::sell(r, App::game.atlas, v, App::hereNow(ctx()), Game::Tide);
  TEST_ASSERT_EQUAL_DOUBLE(0, s.resonance);
}

// Walking past, a well you opened in reach stays in reach until it fades
// past -75. One you never opened is out of reach below -70.
static void test_an_open_well_is_held_while_it_fades() {
  bindAWeek();
  const Net passing = wellAt(9100, -60), other = wellAt(9101, -73);
  const uint32_t id = idOf(passing);
  auto drawWhy = [&](uint32_t at) {
    App::wellId = at;
    return (int)Game::canDraw(App::run(), App::game.atlas, App::currentView(ctx()));
  };
  for (int k = 0; k < 3; k++) { Net two[2] = {passing, other}; scanOf(two, 2); }
  App::go(App::Mode::Visit);
  TEST_ASSERT_EQUAL((int)Game::Why::Ok, drawWhy(id));
  for (int k = 0; k < 6; k++) { Net two[2] = {wellAt(9100, -73), other}; scanOf(two, 2); }
  TEST_ASSERT_TRUE(sky.byId(id)->sig < Game::ReachDbm);
  TEST_ASSERT_EQUAL((int)Game::Why::Ok, drawWhy(id));
  TEST_ASSERT_EQUAL((int)Game::Why::TooFar, drawWhy(idOf(other)));   // never opened: too far
  for (int k = 0; k < 6; k++) { Net two[2] = {wellAt(9100, -80), other}; scanOf(two, 2); }
  TEST_ASSERT_EQUAL((int)Game::Why::TooFar, drawWhy(id));
  // Back at -73 after fading past -75: not held any more.
  for (int k = 0; k < 6; k++) { Net two[2] = {wellAt(9100, -73), other}; scanOf(two, 2); }
  TEST_ASSERT_EQUAL((int)Game::Why::TooFar, drawWhy(id));
}

// A Pilgrim sees one draw row, naming the energy every draw gives.
static void test_a_pilgrim_sees_one_draw_row() {
  bindAWeek();
  for (int k = 0; k < 3; k++) frame();
  Game::Run& r = App::game.run;
  r.calling = Cat::Calling_pilgrim;
  App::wellId = st.starId;
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  int draws = 0;
  for (int i = 0; i < App::nActions; i++) {
    if (App::actions[i].act != App::Act::DrawOwn && App::actions[i].act != App::Act::DrawFocus) continue;
    draws++;
    TEST_ASSERT_EQUAL(r.focus, App::actions[i].arg);
  }
  TEST_ASSERT_EQUAL(1, draws);
}

// A GLAMOUR already waiting: the second is refused with the reason, and kept.
static void test_a_waiting_spell_is_refused_by_name() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.spells[r.nSpells++] = Cat::Spell_glamour;
  r.n.glamour = 1;
  App::back = App::Mode::Scanner;
  App::go(App::Mode::Spells);
  frame(key('E'));
  TEST_ASSERT_EQUAL_STRING("GLAMOUR IS ALREADY READY", App::toastText);
  TEST_ASSERT_EQUAL(2, r.nSpells);
}

// Carving over a glyph you carved asks for a second ENTER.
static void test_carving_over_a_glyph_asks_twice() {
  bindAWeek();
  for (int k = 0; k < 3; k++) frame();
  Game::Run& r = App::game.run;
  App::wellId = st.starId;
  const WellRec* w = sky.byId(App::wellId);
  Game::AtlasEntry& a = App::game.atlas.touch(w->id, w->seed, r.runNo, 0);
  a.glyph = Cat::Glyph_deep;
  r.glyphTokens[r.nGlyphTokens++] = Cat::Glyph_lit;
  App::go(App::Mode::Visit);
  App::buildActions(ctx());
  for (int i = 0; i < App::nActions; i++) if (App::actions[i].act == App::Act::Carve) App::sel = i;
  frame(key('E'));
  TEST_ASSERT_EQUAL_STRING("ENTER AGAIN: REPLACES DEEP GLYPH", App::toastText);
  TEST_ASSERT_EQUAL(Cat::Glyph_deep, App::game.atlas.find(w->id)->glyph);
  frame(key('E'));
  TEST_ASSERT_EQUAL(Cat::Glyph_lit, App::game.atlas.find(w->id)->glyph);
  TEST_ASSERT_EQUAL(0, r.nGlyphTokens);
}

// ECHO: the picker's estimate is what the offering through it would make.
static void test_the_echo_picker_prices_the_offering() {
  bindAWeek();
  for (int k = 0; k < 3; k++) frame();
  Game::Run& r = App::game.run;
  const WellRec* w = sky.byId(st.starId);
  r.hold[Game::Tide] = 5;
  Game::sell(r, App::game.atlas, App::viewOf(*w, sky, st.nowMs), App::hereNow(ctx()), Game::Tide);
  App::game.atlas.find(w->id)->soldNight = -1;   // as if it had settled
  r.hold[Game::Tide] = 6;
  r.n.echo = 1;
  App::back = App::Mode::Visit;
  App::go(App::Mode::Atlas);
  frame();
  const Game::AtlasEntry* a = App::game.atlas.find(w->id);
  TEST_ASSERT_NOT_NULL(a);
  const float shown = App::atlasEst[a - App::game.atlas.e];
  TEST_ASSERT_TRUE(shown > 0);
  App::wellId = w->id;
  App::echoMode = true;
  App::buildActions(ctx());
  bool found = false;
  for (int i = 0; i < App::nActions; i++)
    if (App::actions[i].act == App::Act::Sell && App::actions[i].arg == Game::Tide) {
      TEST_ASSERT_FLOAT_WITHIN(0.01f * (1 + shown), shown, (float)App::actions[i].est);
      found = true;
    }
  TEST_ASSERT_TRUE(found);
  App::echoMode = false;
}

// The hold's leak says what went.
static void test_the_leak_is_said() {
  bindAWeek();
  Game::Run& r = App::game.run;
  r.hold[Game::Ember] = 4;
  frame();
  App::toastT = 0;
  App::nToastNext = 0;
  App::game.playMs = r.holdLeakMs + 15 * 60000;
  frame();
  TEST_ASSERT_EQUAL(3, r.hold[Game::Ember]);
  TEST_ASSERT_EQUAL_STRING("1 EMBER LEAKED FROM YOUR HOLD", App::toastText);
}

int main() {
  stash = new Game::Saved;
  UNITY_BEGIN();
  RUN_TEST(test_enter_walks_the_bind_rows_down);
  RUN_TEST(test_esc_leaves_the_visit);
  RUN_TEST(test_a_gift_waits_for_room_at_dawn);
  RUN_TEST(test_the_dawn_always_says_mercy_and_what_crumbled);
  RUN_TEST(test_a_scroll_waits_for_a_slot_at_dawn);
  RUN_TEST(test_the_empty_pockets_relic_waits_at_dawn);
  RUN_TEST(test_the_stall_asks_twice);
  RUN_TEST(test_the_show_leaves_on_one_key);
  RUN_TEST(test_first_time_notes_say_their_line_once);
  RUN_TEST(test_held_notes_follow_their_find);
  RUN_TEST(test_the_give_up_warning_goes_with_the_menu);
  RUN_TEST(test_an_empty_street_name_is_refused);
  RUN_TEST(test_a_night_survives_reboots);
  RUN_TEST(test_the_shop_survives_a_reboot);
  RUN_TEST(test_losing_ends_in_the_bind_screen);
  RUN_TEST(test_the_dawn_offers_one_of_two_portents);
  RUN_TEST(test_a_lost_week_offers_its_trial);
  RUN_TEST(test_screens_survive_keys_at_random);
  RUN_TEST(test_the_story_opens_from_the_menu);
  RUN_TEST(test_del_erases_then_goes_back);
  RUN_TEST(test_a_long_random_career);
  RUN_TEST(test_heard_lines_fit);
  RUN_TEST(test_place_bound_lines_play_where_they_belong);
  RUN_TEST(test_the_coat_line_can_be_heard);
  RUN_TEST(test_the_deck_remembers_wells_between_walks);
  RUN_TEST(test_a_well_coming_into_reach_plays_its_tune);
  RUN_TEST(test_a_companion_is_set_aside_on_the_next_walk);
  RUN_TEST(test_stopping_after_a_walk_catches_a_fragment);
  RUN_TEST(test_the_first_stop_after_half_a_day_off_catches_one);
  RUN_TEST(test_a_sure_catch_prefers_a_new_line);
  RUN_TEST(test_all_five_energies_play_their_chord_once);
  RUN_TEST(test_a_walk_has_a_tune_budget);
  RUN_TEST(test_walk_sounds_stay_on_the_scanner_screens);
  RUN_TEST(test_naming_words_fill_as_you_pass);
  RUN_TEST(test_a_namesake_is_noted_once_when_you_stop);
  RUN_TEST(test_the_field_guide_meets_each_kind_once);
  RUN_TEST(test_the_old_radios_line_plays_only_at_one);
  RUN_TEST(test_a_drifting_glyph_turns_up_and_is_kept);
  RUN_TEST(test_a_drifting_glyph_turns_up_in_a_hollow_too);
  RUN_TEST(test_a_flick_scatters_a_drifting_glyph);
  RUN_TEST(test_a_drifting_glyph_waits_for_you);
  RUN_TEST(test_the_found_tab_opens_and_closes);
  RUN_TEST(test_a_pocket_walk_ends_in_a_recap);
  RUN_TEST(test_each_walk_draws_a_ribbon);
  RUN_TEST(test_each_dawn_brings_a_token);
  RUN_TEST(test_a_stretch_walked_on_three_nights_asks);
  RUN_TEST(test_a_new_stretch_replaces_the_oldest_forgotten_one);
  RUN_TEST(test_you_write_your_own_street_in);
  RUN_TEST(test_give_up_is_only_armed_while_it_says_so);
  RUN_TEST(test_a_second_twist_says_theres_no_ward_left);
  RUN_TEST(test_words_wait_for_a_lit_screen);
  RUN_TEST(test_a_new_rank_is_said);
  RUN_TEST(test_the_off_time_says_what_settled);
  RUN_TEST(test_a_toast_waits_out_the_boot);
  RUN_TEST(test_the_stall_says_what_a_reroll_and_leaving_lose);
  RUN_TEST(test_the_stall_says_what_cant_be_had);
  RUN_TEST(test_the_stall_lets_go_of_the_one_you_pick);
  RUN_TEST(test_the_long_way_estimate_is_zero);
  RUN_TEST(test_an_open_well_is_held_while_it_fades);
  RUN_TEST(test_a_pilgrim_sees_one_draw_row);
  RUN_TEST(test_a_waiting_spell_is_refused_by_name);
  RUN_TEST(test_carving_over_a_glyph_asks_twice);
  RUN_TEST(test_the_leak_is_said);
  RUN_TEST(test_the_echo_picker_prices_the_offering);
  return UNITY_END();
}
