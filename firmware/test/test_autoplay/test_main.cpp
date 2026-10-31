// A bot plays whole Bindings on the Deck's own engine, in a made-up world
// shaped like tools/balance_sim.py's, to check that the port holds together:
// the outcome mix should land near the simulator's, and no invariant may
// ever break. pio test -e native -f test_autoplay
#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "game/Game.h"

using namespace Game;
using namespace Cat;

void setUp() {}
void tearDown() {}

static Career car;
static Atlas atlas;
static Run r;
static Shop shop;
static RecentSales mem;
static Rng world;

static int violations = 0;
static void check(bool ok, const char* what) {
  if (!ok && violations++ < 10) printf("invariant broken: %s\n", what);
}

// The simulator's suburb player: where() and make_visit(), with wells that
// stay put so revisits are real.
static const double WardMix[4] = {0.15, 0.40, 0.35, 0.10};
static const int Choices[4] = {2, 2, 3, 3};

struct Place { int ward, n; uint32_t wells[3]; };

static WellView makeWell(uint32_t id, int ward) {
  Rng g;
  g.seed(id, 3);
  WellView w{};
  w.id = id;
  w.seed = id * 2654435761u ^ 0x5bd1e995u;
  static const double synd[4] = {0.03, 0.05, 0.15, 0.30};
  const double fw[4] = {0.15, 0.73 - synd[ward], 0.12, synd[ward]};   // hedge hearth warded syndicate
  w.faction = (uint8_t)g.weighted(fw, 4);
  w.hidden = g.chance(0.10);
  static const int ch[] = {1, 1, 1, 6, 6, 6, 11, 11, 11, 3, 4, 8, 9, 13, 2, 5, 7, 10, 12};
  w.channel = (uint8_t)ch[g.below(sizeof(ch) / sizeof(ch[0]))];
  w.energy = w.hidden ? Hush : w.faction == Hedge ? Static : w.channel <= 4 ? Ember : w.channel <= 9 ? Tide : Gale;
  return w;
}

static Place where() {
  Place p{};
  p.ward = world.weighted(WardMix, 4);
  static const int lo[4] = {1, 4, 11, 26}, hi[4] = {3, 10, 25, 45};
  p.n = lo[p.ward] + (int)world.below(hi[p.ward] - lo[p.ward] + 1);
  // Sometimes somewhere you've been before (your routine). On Homebody, from
  // Night 2, a player would only go to wells they know.
  const bool routine = world.chance(0.12 * (r.night + 1)) || (r.challenge == 6 && r.night >= 1);
  for (int k = 0; k < 3; k++) {
    const uint32_t id = routine ? 1000 + p.ward * 50 + world.below(12) : 100000 + world.below(1u << 30);
    p.wells[k] = id;
  }
  return p;
}

static Here hereAt(const Place& p) {
  Here h{};
  h.n = p.n;
  h.ward = p.ward;
  h.ch11 = (int)(0.25 * (p.n < 40 ? p.n : 40));
  h.still = world.chance(0.7);
  h.noise = world.unit();
  h.battery = world.chance(0.06) ? 15 : 70;
  return h;
}

// What selling energy e at w is worth to this build right now.
static double estimate(const WellView& w, const Here& h, int e) {
  Sale c;
  saleContext(c, r, atlas, w, h, e);
  return score(c, r).resonance;
}

// One visit. Returns false if nothing here was worth doing (the player walks on).
static bool tryVisit() {
  const Place p = where();
  const Here h = hereAt(p);
  const int k = p.n < Choices[p.ward] ? p.n : Choices[p.ward];
  // For each candidate well: draw (if it isn't dry) and sell the best energy.
  double bestV = -1;
  WellView bestW{};
  int bestE = -1;
  bool bestDraw = false, bestFocus = false;
  for (int i = 0; i < k; i++) {
    WellView w = makeWell(p.wells[i], p.ward);
    w.sig = world.chance(0.35) ? -48 : -60;
    w.mesh = world.chance(0.1) ? 2 + (int)world.below(3) : 0;
    w.dowsed = world.chance(0.12);
    const bool canD = canDraw(r, atlas, w) == Why::Ok;
    // Try drawing the well's own energy, and your focus energy where it's offered.
    for (int f = 0; f < (canD && givesFocus(r, w) ? 2 : 1); f++) {
      uint8_t saved[EnergyN];
      memcpy(saved, r.hold, sizeof(saved));
      if (canD) r.hold[f ? r.focus : wellEnergy(r, w)] += (uint8_t)(w.sig >= -50 ? 7 : 5);
      for (int e = 0; e < EnergyN; e++) {
        if (canSell(r, atlas, w, e) != Why::Ok) continue;
        const double v = estimate(w, h, e);
        if (v > bestV) { bestV = v; bestW = w; bestE = e; bestDraw = canD; bestFocus = f == 1; }
      }
      memcpy(r.hold, saved, sizeof(saved));
    }
  }
  if (bestE < 0) return false;
  if (bestDraw) draw(r, atlas, bestW, h, h.still ? 1.0 : 0.6, bestFocus);
  if (canSell(r, atlas, bestW, bestE) != Why::Ok) return false;
  // GLAMOUR when the Night looks short.
  const double avg = r.n.used ? r.n.total / r.n.used : bestV;
  const bool danger = r.n.total + avg * (r.n.visits - r.n.used) < r.n.tithe * 1.15;
  for (int i = 0; i < r.nSpells && danger; i++) {
    if (r.spells[i] == Spell_glamour || r.spells[i] == Spell_haste) { castSpell(r, atlas, car, i, &bestW, h); break; }
  }
  sell(r, atlas, bestW, h, bestE, false, &mem);
  int held = 0;
  for (int e = 0; e < EnergyN; e++) held += r.hold[e];
  check(held <= holdCap(r) + 20, "hold overfull");
  return true;
}

static void playVisit() {
  for (int tries = 0; tries < 30; tries++) if (tryVisit()) return;
  r.n.used++;   // nowhere to sell at all: give the visit up so the Night can end
}

static bool tracing = false;
static void trace() {
  if (!tracing) return;
  printf("    shop after N%d, coin %.0f, relics:", r.night, r.coin);
  for (int i = 0; i < r.nRelics; i++) printf(" %s/t%d", Relics[r.relics[i].id].key, r.relics[i].tier);
  printf(" | attune %d %d %d %d %d | offers:", r.attune[0], r.attune[1], r.attune[2], r.attune[3], r.attune[4]);
  for (int i = 0; i < shop.nRelics; i++) printf(" %s(%d%s)", Relics[shop.relics[i].id].key, cost(r, shop.relics[i]), shop.relics[i].sold ? ",sold" : "");
  printf(" star %d\n", shop.star);
}
static void playShop() {
  openShop(shop, r, car);
  if (!shop.open) return;
  trace();
  for (int round = 0; round < 5; round++) {
    for (int buys = 0; buys < 4; buys++) {   // keep buying while something helps
      starBest(shop, r, mem);
      if (shop.star < 0) break;
      const int drop = slotsUsed(r) >= relicSlots(r) ? weakestRelic(r, mem) : -1;
      if (buyRelic(shop, r, shop.star, drop) != Buy::Ok) break;
    }
    // Polish the relic that gains most per coin, keeping 20 coin back.
    for (int tries = 0; tries < 3; tries++) {
      int bestI = -1;
      double bestG = 0.02;
      const double base = buildValue(r, mem);
      for (int i = 0; i < r.nRelics; i++) {
        if (!canPolish(r, i) || r.coin - polishPrice(r, r.relics[i]) < 20) continue;
        Run& t = scratchRun();
        t = r;
        t.relics[i].tier++;
        const double g = (buildValue(t, mem) / (base > 1 ? base : 1) - 1) / polishPrice(r, r.relics[i]) * 10;
        if (g > bestG) { bestG = g; bestI = i; }
      }
      if (bestI < 0) break;
      polish(r, bestI);
    }
    for (int i = 0; i < shop.nCons; i++) {
      const Offer& o = shop.cons[i];
      if (o.sold || r.coin < cost(r, o) + 1) continue;
      if (o.kind == OfferKind::Attune) buyConsumable(shop, r, i, r.focus);
      else if (o.kind == OfferKind::Spell && r.nSpells < spellSlots(r)) buyConsumable(shop, r, i);
    }
    if (shop.mod.kind == OfferKind::Mod && r.coin >= cost(r, shop.mod) + 2 && r.night < 6) buyMod(shop, r, car);
    if (shop.trinket.kind == OfferKind::Trinket && r.coin >= cost(r, shop.trinket) + 2 && r.night < 6) buyTrinket(shop, r);
    if (shop.freeRerolls > 0 || r.coin - shop.rerollCost >= 20) reroll(shop, r, car);
    else break;
  }
  trace();
  closeShop(shop, r);
}

struct Outcome { bool won; int failNight; int relics; double coin; double best; };

// The simulator's "power": a whole Night played with the build as it stands,
// as a multiple of the Tithe (no spells, no stopping at the Tithe).
static double ratios[Nights][2000];
static int nRatios[Nights];
static Run saved;
static Atlas savedAtlas;
static void measurePower() {
  saved = r;
  savedAtlas = atlas;
  const Rng savedWorld = world;
  r.nSpells = 0;
  r.n.tithe = 1e300;
  while (r.n.used < r.n.visits) playVisit();
  const double ratio = r.n.total / saved.n.tithe;
  r = saved;
  atlas = savedAtlas;
  world = savedWorld;
  if (r.night < Nights && nRatios[r.night] < 2000) ratios[r.night][nRatios[r.night]++] = ratio;
}

static bool measuring = false;
static int challenge = -1;
static Outcome playRun(uint32_t seed, int calling, int pact) {
  memset(&mem, 0, sizeof(mem));
  newRun(r, car, calling, pact, challenge, seed, seed);
  world.seed(seed * 7919u + 1, 11);
  Outcome o{false, 0, 0, 0, 0};
  while (r.night < Nights) {
    beginNight(r);
    if (measuring) measurePower();
    check(r.n.visits >= 1, "no visits");
    while (!r.n.met) {
      if (r.n.used >= r.n.visits) {
        if (canLightCandle(r)) { lightCandle(r); continue; }
        if (canTakeLoan(r)) { takeLoan(r); break; }
        break;
      }
      playVisit();
    }
    if (!r.n.met) { o.failNight = r.night + 1; break; }
    const NightEnd e = endNight(r, car, 70);
    check(r.coin >= 0, "negative coin");
    check(slotsUsed(r) <= relicSlots(r), "too many relics");
    check(r.nRelics <= Run::MaxRelics, "relic array overflow");
    if (e.won) { o.won = true; break; }
    playShop();
    check(slotsUsed(r) <= relicSlots(r), "too many relics after shop");
  }
  o.relics = r.nRelics;
  o.coin = r.coin;
  endRun(r, car, atlas);
  return o;
}

static void test_bindings_play_out_like_the_simulator() {
  newCareer(car);
  atlas.clear();
  const int runs = 1500;
  int wins = 0, fails[Nights + 1] = {0};
  double relics = 0;
  for (int i = 0; i < runs; i++) {
    const Outcome o = playRun(1000 + i, Calling_wayfarer, 0);
    wins += o.won;
    fails[o.failNight]++;
    relics += o.relics;
    newCareer(car);   // each run from the starting pool, like the simulator's base check
  }
  printf("autoplay: %d runs, %.1f%% won, relics at the end %.1f\n", runs, 100.0 * wins / runs, relics / runs);
  printf("fails by Night:");
  for (int n = 1; n <= Nights; n++) printf(" N%d %.1f%%", n, 100.0 * fails[n] / runs);
  printf("\n");
  TEST_ASSERT_EQUAL_MESSAGE(0, violations, "an invariant broke");
  TEST_ASSERT_EQUAL_MESSAGE(0, fails[1], "Night 1 should never fail with a sensible player");
  TEST_ASSERT_TRUE_MESSAGE(wins > runs * 0.70 && wins < runs * 0.99, "win rate far from the simulator's ~90%");
}

static void test_a_career_unlocks_things() {
  newCareer(car);
  atlas.clear();
  int revealed = 0;
  for (int i = 0; i < 30; i++) {
    playRun(5000 + i, i % 3, 0);
    revealed += car.nRevealed;
  }
  printf("career: 30 runs, %d unlocks revealed, %u wins, rank %s\n", revealed, (unsigned)car.wins, rankName(car));
  TEST_ASSERT_TRUE(revealed >= 20);
  TEST_ASSERT_EQUAL(0, violations);
}

static void test_harder_pacts_win_less() {
  int wins[3] = {0};
  const int pacts[3] = {0, 4, 7};
  for (int k = 0; k < 3; k++) {
    newCareer(car);
    for (int i = 0; i < 600; i++) {
      wins[k] += playRun(9000 + i, Calling_wayfarer, pacts[k]).won;
      newCareer(car);
    }
  }
  printf("pacts: Chalk %.0f%%, Bone %.0f%%, Gold %.0f%%\n", wins[0] / 6.0, wins[1] / 6.0, wins[2] / 6.0);
  TEST_ASSERT_TRUE(wins[0] > wins[1] && wins[1] > wins[2]);
}

static int cmp(const void* a, const void* b) {
  const double x = *(const double*)a, y = *(const double*)b;
  return x < y ? -1 : x > y;
}

[[maybe_unused]] static void test_trace_a_few_shops() {
  tracing = true;
  for (int i = 0; i < 3; i++) {
    newCareer(car);
    atlas.clear();
    printf("  run %d\n", i);
    playRun(31000 + i, Calling_wayfarer, 0);
  }
  tracing = false;
}

static void test_power_against_the_tithe() {
  measuring = true;
  memset(nRatios, 0, sizeof(nRatios));
  for (int i = 0; i < 800; i++) {
    newCareer(car);
    atlas.clear();
    playRun(20000 + i, Calling_wayfarer, 0);
  }
  measuring = false;
  printf("power / Tithe   median  top 10%%  (simulator: N1 1.27/1.37, N2 2.65/5.29, N4 2.70/4.91, N7 2.63/5.53)\n");
  for (int n = 0; n < Nights; n++) {
    if (!nRatios[n]) continue;
    qsort(ratios[n], nRatios[n], sizeof(double), cmp);
    printf("  N%d  %6.2f  %6.2f   (%d runs reached it)\n", n + 1, ratios[n][nRatios[n] / 2],
           ratios[n][nRatios[n] * 9 / 10], nRatios[n]);
  }
  TEST_ASSERT_TRUE(ratios[0][nRatios[0] / 2] > 1.0 && ratios[0][nRatios[0] / 2] < 1.6);
}

static void test_every_calling_and_challenge_holds_together() {
  // Everything unlocked, so Glitch's rares and Summon's legendaries can appear.
  for (int ch = -1; ch < ChallengeCount; ch++) {
    challenge = ch;
    for (int calling = 0; calling < CallingCount; calling++) {
      int wins = 0;
      for (int i = 0; i < 40; i++) {
        newCareer(car);
        for (int k = 0; k < RelicCount; k++) unlock(car, Kind::Relic, k);
        for (int k = 0; k < SpellCount; k++) unlock(car, Kind::Spell, k);
        for (int k = 0; k < TrinketCount; k++) unlock(car, Kind::Trinket, k);
        atlas.clear();
        wins += playRun(70000 + ch * 1000 + calling * 50 + i, calling, 0).won;
      }
      if (ch < 0) printf("  %-12s %3d%%\n", Callings[calling].key, wins * 100 / 40);
      else if (calling == 0) printf("  challenge %-13s %3d%%\n", Challenges[ch].name, wins * 100 / 40);
    }
  }
  challenge = -1;
  TEST_ASSERT_EQUAL_MESSAGE(0, violations, "an invariant broke");
}

// Win rate of one Challenge on the Wayfarer, everything unlocked.
static double challengeRate(int ch, int runs) {
  int wins = 0;
  challenge = ch;
  for (int i = 0; i < runs; i++) {
    newCareer(car);
    for (int k = 0; k < RelicCount; k++) unlock(car, Kind::Relic, k);
    for (int k = 0; k < SpellCount; k++) unlock(car, Kind::Spell, k);
    for (int k = 0; k < TrinketCount; k++) unlock(car, Kind::Trinket, k);
    atlas.clear();
    wins += playRun(90000 + ch * 1000 + i, Calling_wayfarer, 0).won;
  }
  challenge = -1;
  return 100.0 * wins / runs;
}

[[maybe_unused]] static void test_challenge_sweep() {
#ifdef SWEEP
  const int list[] = {SWEEP};
  for (int ch : list) printf("  sweep %-13s %.0f%%\n", Challenges[ch].name, challengeRate(ch, 300));
#endif
}

int main() {
  rules.quietRung = RungOff;   // the bot values relics by the star's arithmetic; a quiet stall would blind it, not a player
  UNITY_BEGIN();
  RUN_TEST(test_bindings_play_out_like_the_simulator);
  RUN_TEST(test_a_career_unlocks_things);
  RUN_TEST(test_harder_pacts_win_less);
#ifdef SWEEP
  RUN_TEST(test_challenge_sweep);
  return UNITY_END();
#endif
  RUN_TEST(test_every_calling_and_challenge_holds_together);
  RUN_TEST(test_power_against_the_tithe);
  return UNITY_END();
}
