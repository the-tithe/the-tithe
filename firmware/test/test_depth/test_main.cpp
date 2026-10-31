// Plateau finder: the autoplay bot with switchable playstyles, played on every
// Pact and on into Overnight, to see which reasonable habits cap a player and
// where, and the same bot under other rule sets (Game::rules), to measure a
// rule change before it ships. Thousands of weeks a cell, so it lives in its
// own env and the plain test run skips it:
//   pio test -e depth --without-testing
//   .pio/build/depth/program [-r runs] [-a] [scenario ...]   (-a: everything unlocked)
// With no arguments (pio test -e depth) it plays a few weeks as a smoke test.
#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
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

// ---- Policy switches --------------------------------------------------------------
enum Pick : uint8_t { PickStar, PickNearest, PickOwnBack };
enum ShopStyle : uint8_t { ShopStar, ShopFirstAffordable, ShopRaresOnly, ShopUncommonUp, ShopSpendAll, ShopRerollHappy, ShopInterest25, ShopMultFirst, ShopAddsFirst };
enum Loan : uint8_t { LoanLastResort, LoanNever, LoanEarly };
enum EnergyUse : uint8_t { EnergyAny, EnergyFocusOnly, EnergySpread };
enum Wards : uint8_t { WardsAnywhere, WardsNexus, WardsHollow, WardsAdaptive };
enum SpellUse : uint8_t { SpellsPanic, SpellsNone, SpellsSmart };

struct Policy {
  const char* name;
  Pick pick = PickStar;
  ShopStyle shop = ShopStar;
  Loan loan = LoanLastResort;
  EnergyUse energy = EnergyAny;
  Wards wards = WardsAnywhere;
  SpellUse spells = SpellsPanic;
  bool favors = false;      // take whatever the well you sell at offers, never detour
  bool polish = true;
  bool attune = true;
  bool glyphs = false;      // buy glyph tokens and carve them into routine wells
  bool candles = true;
  bool attuneSpread = false;   // attune the energy you've been offering with the lowest level, not always your focus
};
static Policy P;

// The simulator's suburb player: where() and make_visit(), with wells that
// stay put so revisits are real.
static const double WardMixSuburb[4] = {0.15, 0.40, 0.35, 0.10};
static const double WardMixNexus[4] = {0.05, 0.15, 0.35, 0.45};
static const double WardMixHollow[4] = {0.50, 0.30, 0.15, 0.05};
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

static bool isRoutine(uint32_t id) { return id < 100000; }

static Place where() {
  Place p{};
  const double* mix = P.wards == WardsNexus ? WardMixNexus : P.wards == WardsHollow ? WardMixHollow : WardMixSuburb;
  p.ward = world.weighted(mix, 4);
  static const int lo[4] = {1, 4, 11, 26}, hi[4] = {3, 10, 25, 45};
  p.n = lo[p.ward] + (int)world.below(hi[p.ward] - lo[p.ward] + 1);
  const int night = r.night < Nights ? r.night : Nights - 1;
  const bool routine = world.chance(0.12 * (night + 1));
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

static double estimate(const WellView& w, const Here& h, int e) {
  Sale c;
  saleContext(c, r, atlas, w, h, e);
  return score(c, r).resonance;
}

struct Tally {
  long sales, nights, candles, loans, favorsTaken, favorsDone, glyphsCarved, glamours, detours, choices, attunes;
  double trade, spare, jobCoin, candleCoin, attuneCoin;
};
static Tally tally;

// What a Portent is worth to this build right now, for keeping one of two.
static int portentValue(int kind, const PortentChoice& ch) {
  switch (kind) {
    case Portent_conjunction: {
      for (int i = 0; i < RelicCount; i++)
        if (Relics[i].legendary && unlocked(car, Kind::Relic, i) && !owns(r, i)) return slotsUsed(r) < relicSlots(r) ? 100 : 20;
      return 0;
    }
    case Portent_gift: return slotsUsed(r) < relicSlots(r) ? 60 : 20;
    case Portent_rare_sight: return r.coin >= 10 ? 40 : 15;
    case Portent_windfall: return 35;
    case Portent_tuning: return 30;
    case Portent_shine: return 25;
    case Portent_free_rerolls: return 20;
    case Portent_doubled_interest: return 3 * ch.interest;
    case Portent_scroll: return r.nSpells < spellSlots(r) && P.spells != SpellsNone ? 10 : 0;
    case Portent_chalk_mark: return P.glyphs ? 15 : 3;
    default: return 0;
  }
}

struct Choice {
  double v = -1;
  WellView w{};
  Here h{};
  int e = -1, i = -1;
  bool draw = false, focus = false;
  Job offer{};
  bool any = false;
};

// Look at a place and decide the best thing to do there under the policy.
static Choice lookAt(const Place& p) {
  Choice c;
  c.h = hereAt(p);
  const int k = p.n < Choices[p.ward] ? p.n : Choices[p.ward];
  WellView wells[3];
  double sig[3];
  for (int i = 0; i < k; i++) {
    wells[i] = makeWell(p.wells[i], p.ward);
    wells[i].sig = world.chance(0.35) ? -48 : -60;
    sig[i] = wells[i].sig;
    wells[i].mesh = world.chance(0.1) ? 2 + (int)world.below(3) : 0;
    wells[i].dowsed = world.chance(0.12);
  }
  // Nearest: only the loudest well is considered.
  int only = -1;
  if (P.pick == PickNearest) {
    only = 0;
    for (int i = 1; i < k; i++) if (sig[i] > sig[only]) only = i;
  }
  bool anyFocusSale = false;
  for (int pass = 0; pass < 2; pass++) {
    // pass 0 under FocusOnly looks for focus sales; pass 1 relaxes if none exist.
    if (pass == 1 && (P.energy != EnergyFocusOnly || anyFocusSale)) break;
    for (int i = 0; i < k; i++) {
      if (only >= 0 && i != only) continue;
      WellView& w = wells[i];
      const bool canD = canDraw(r, atlas, w) == Why::Ok;
      const int fMax = canD && givesFocus(r, w) ? 2 : 1;
      for (int f = 0; f < fMax; f++) {
        if (P.energy == EnergySpread && f == 1) continue;          // never draw focus
        if (P.energy == EnergyFocusOnly && pass == 0 && canD && fMax == 2 && f == 0) continue;   // draw focus where offered
        uint8_t saved[EnergyN];
        memcpy(saved, r.hold, sizeof(saved));
        int drawnE = -1;
        if (canD) {
          drawnE = f ? r.focus : wellEnergy(r, w);
          int held = 0;
          for (int e = 0; e < EnergyN; e++) held += r.hold[e];
          int add = w.sig >= -50 ? 7 : 5;
          if (held + add > holdCap(r)) add = holdCap(r) - held;
          r.hold[drawnE] = (uint8_t)(r.hold[drawnE] + (add > 0 ? add : 0));
        }
        for (int e = 0; e < EnergyN; e++) {
          if (canSell(r, atlas, w, e) != Why::Ok) continue;
          if (P.energy == EnergyFocusOnly && pass == 0 && e != r.focus) continue;
          if (P.pick == PickOwnBack && canD && e != drawnE) continue;   // give the well back what it gave
          if (e == r.focus) anyFocusSale = true;
          const double v = estimate(w, c.h, e);
          if (v > c.v) { c.v = v; c.w = w; c.e = e; c.i = i; c.draw = canD; c.focus = f == 1; c.any = true; }
        }
        memcpy(r.hold, saved, sizeof(saved));
      }
    }
  }
  if (c.any) c.offer = jobOffer(r, c.w);
  return c;
}

static bool danger(double fallback) {
  const double avg = r.n.used ? r.n.total / r.n.used : fallback;
  return r.n.total + avg * (r.n.visits - r.n.used) < effectiveTithe(r) * 1.15;
}

// One visit. Returns false if nothing here was worth doing (the player walks on).
static bool tryVisit() {
  Choice c = lookAt(where());
  if (P.wards == WardsAdaptive) {   // look at a second place and go where the build pays more
    const Choice d = lookAt(where());
    if (d.v > c.v) c = d;
  }
  if (!c.any) return false;
  WellView& w = c.w;
  const Here& h = c.h;
  if (P.favors && c.offer.kind && takeJob(r, c.offer)) tally.favorsTaken++;
  if (c.draw) draw(r, atlas, w, h, h.still ? 1.0 : 0.6, c.focus);
  if (canSell(r, atlas, w, c.e) != Why::Ok) return false;
  // Glyphs: carve a token into a routine well that has none this run.
  if (P.glyphs && r.nGlyphTokens > 0 && isRoutine(w.id) && glyphAt(r, atlas, w.id) < 0) {
    if (carve(r, atlas, w, h, 0)) tally.glyphsCarved++;
  }
  // Spells.
  if (P.spells == SpellsPanic) {
    const bool dng = danger(c.v);
    for (int i = 0; i < r.nSpells && dng; i++) {
      if (r.spells[i] == Spell_glamour || r.spells[i] == Spell_haste) { castSpell(r, atlas, car, i, &w, h); tally.glamours++; break; }
    }
  } else if (P.spells == SpellsSmart) {
    // GLAMOUR when doubling this sale closes the Night and the sale alone wouldn't,
    // or when the Night is in danger. HASTE only when in danger.
    const double est = estimate(w, h, c.e);
    const double need = effectiveTithe(r) - r.n.total;
    const bool closes = est < need && 2 * est >= need;
    const bool dng = danger(c.v);
    for (int i = 0; i < r.nSpells; i++) {
      if (r.spells[i] == Spell_glamour && (closes || dng)) { castSpell(r, atlas, car, i, &w, h); tally.glamours++; break; }
      if (r.spells[i] == Spell_haste && dng && r.n.used >= r.n.visits - 1) { castSpell(r, atlas, car, i, &w, h); break; }
    }
  }
  const Sold s = sell(r, atlas, w, h, c.e, false, &mem);
  if (s.why == Why::Ok && r.night < Nights) { tally.sales++; tally.jobCoin += s.jobCoin; if (s.jobCoin) tally.favorsDone++; }
  int held = 0;
  for (int e = 0; e < EnergyN; e++) held += r.hold[e];
  check(held <= holdCap(r) + 20, "hold overfull");
  return true;
}

static void playVisit() {
  for (int tries = 0; tries < 30; tries++) if (tryVisit()) return;
  r.n.used++;   // nowhere to sell at all: give the visit up so the Night can end
}

// ---- The shop ------------------------------------------------------------------------

static void polishBest(double reserve) {
  if (!P.polish) return;
  for (int tries = 0; tries < 3; tries++) {
    int bestI = -1;
    double bestG = 0.02;
    const double base = buildValue(r, mem);
    for (int i = 0; i < r.nRelics; i++) {
      if (!canPolish(r, i) || r.coin - polishPrice(r, r.relics[i]) < reserve) continue;
      Run& t = scratchRun();
      t = r;
      t.relics[i].tier++;
      const double g = (buildValue(t, mem) / (base > 1 ? base : 1) - 1) / polishPrice(r, r.relics[i]) * 10;
      if (g > bestG) { bestG = g; bestI = i; }
    }
    if (bestI < 0) break;
    polish(r, bestI);
  }
}

static void buyWares(double reserve) {
  for (int i = 0; i < shop.nCons; i++) {
    const Offer& o = shop.cons[i];
    if (o.sold || r.coin - cost(r, o) < reserve + 1) continue;
    if (o.kind == OfferKind::Attune && P.attune) {
      int e = r.focus;
      if (P.attuneSpread && mem.n > 0) {   // of the energies you've offered lately, the least attuned (ties: offered most)
        int sold[EnergyN] = {0};
        for (int k = 0; k < mem.n; k++) sold[mem.s[k].e]++;
        for (int k = 0; k < EnergyN; k++)
          if (sold[k] && (r.attune[k] < r.attune[e] || (r.attune[k] == r.attune[e] && sold[k] > sold[e]))) e = k;
      }
      buyConsumable(shop, r, i, e);
    }
    else if (o.kind == OfferKind::Spell && P.spells == SpellsSmart && o.id == Spell_hex && r.coin >= cost(r, o)) {
      // HEX: 3 coin for 8. Buy it and cast it on the spot, even with full pockets.
      const int drop = r.nSpells >= spellSlots(r) ? 0 : -1;
      if (buyConsumable(shop, r, i, -1, drop) == Buy::Ok) {
        Here none{};
        for (int k = 0; k < r.nSpells; k++) if (r.spells[k] == Spell_hex) { castSpell(r, atlas, car, k, nullptr, none); break; }
      }
    }
    else if (o.kind == OfferKind::Spell && P.spells != SpellsNone && r.nSpells < spellSlots(r)) buyConsumable(shop, r, i);
    else if (o.kind == OfferKind::Glyph && P.glyphs && r.nGlyphTokens < GlyphTokens) buyConsumable(shop, r, i);
  }
}

static void playShop() {
  openShop(shop, r, car);
  if (!shop.open) return;
  const int night = r.night;   // the Night about to start (1-based count of shops so far)
  switch (P.shop) {
    case ShopFirstAffordable: {
      // Walk the shelf top to bottom and buy the first relic you can pay for. Full? Polish.
      bool bought = false;
      for (int k = 0; k < shop.nRelics && !bought; k++) {
        if (shop.relics[k].sold || r.coin < cost(r, shop.relics[k])) continue;
        if (slotsUsed(r) >= relicSlots(r)) break;
        bought = buyRelic(shop, r, k, -1) == Buy::Ok;
      }
      if (!bought && slotsUsed(r) >= relicSlots(r)) polishBest(0);
      buyWares(0);
      break;
    }
    case ShopRaresOnly:
    case ShopUncommonUp: {
      // Save for rares: buy only rare (or uncommon+) relics, and the first Caller, plus Attunements. Reroll to find them.
      const int minRarity = P.shop == ShopRaresOnly ? Rare : Uncommon;
      for (int round = 0; round < 6; round++) {
        bool bought = false;
        for (int k = 0; k < shop.nRelics; k++) {
          const Offer& o = shop.relics[k];
          if (o.sold) continue;
          const bool caller = shop.night == 0 && o.id == R_ember_caller + r.focus;
          if (!caller && Relics[o.id].rarity < minRarity) continue;
          if (r.coin < cost(r, o)) continue;
          const int drop = slotsUsed(r) >= relicSlots(r) ? weakestRelic(r, mem) : -1;
          if (buyRelic(shop, r, k, drop) == Buy::Ok) bought = true;
        }
        if (!bought && (shop.freeRerolls > 0 || r.coin - shop.rerollCost >= 10)) reroll(shop, r, car);
        else if (!bought) break;
      }
      buyWares(10);
      break;
    }
    case ShopInterest25: {
      // Hoard: never let coin drop under 25 (max interest), except the first Caller.
      const double floor_ = shop.night == 0 ? 0 : 25;
      for (int buys = 0; buys < 4; buys++) {
        starBest(shop, r, mem);
        if (shop.star < 0) break;
        if (r.coin - cost(r, shop.relics[shop.star]) < floor_) break;
        const int drop = slotsUsed(r) >= relicSlots(r) ? weakestRelic(r, mem) : -1;
        if (buyRelic(shop, r, shop.star, drop) != Buy::Ok) break;
      }
      polishBest(floor_);
      buyWares(floor_);
      break;
    }
    case ShopSpendAll:
    case ShopRerollHappy:
    case ShopMultFirst:
    case ShopAddsFirst:
    case ShopStar:
    default: {
      const double reserve = P.shop == ShopSpendAll ? 0 : 20;
      const int rerollAt = P.shop == ShopRerollHappy ? 7 : 20;
      for (int round = 0; round < 5; round++) {
        for (int buys = 0; buys < 4; buys++) {
          starBest(shop, r, mem);
          int pickK = shop.star;
          if (P.shop == ShopMultFirst || P.shop == ShopAddsFirst) {
            // Mio's rule: after the Caller, buy anything that multiplies (or, for the novice mirror, anything that adds).
            const bool wantMult = P.shop == ShopMultFirst;
            const int drop = slotsUsed(r) >= relicSlots(r) ? weakestRelic(r, mem) : -1;
            double bestG = 0.0;
            int bestK = -1;
            for (int k = 0; k < shop.nRelics; k++) {
              const Offer& o = shop.relics[k];
              if (o.sold || r.coin < cost(r, o)) continue;
              const bool mult = Relics[o.id].phase == 2 || Relics[o.id].phase == 3;
              if (mult != wantMult) continue;
              const double g = relicGain(r, mem, o.id, o.ed, drop);
              if (g > bestG) { bestG = g; bestK = k; }
            }
            if (bestK >= 0) pickK = bestK;
            else if (shop.night == 0 && pickK >= 0) {}            // the Caller
            else pickK = -1;                                       // nothing of the kind: hold the coin
          }
          if (pickK < 0) break;
          const int drop = slotsUsed(r) >= relicSlots(r) ? weakestRelic(r, mem) : -1;
          if (buyRelic(shop, r, pickK, drop) != Buy::Ok) break;
        }
        polishBest(reserve);
        buyWares(0);
        if (shop.mod.kind == OfferKind::Mod && !shop.mod.sold && r.coin >= cost(r, shop.mod) + 2 && night < 6) buyMod(shop, r, car);
        if (shop.trinket.kind == OfferKind::Trinket && !shop.trinket.sold && r.coin >= cost(r, shop.trinket) + 2 && night < 6) buyTrinket(shop, r);
        if (shop.freeRerolls > 0 || r.coin - shop.rerollCost >= rerollAt) reroll(shop, r, car);
        else break;
      }
      break;
    }
  }
  closeShop(shop, r);
}

// ---- A run ----------------------------------------------------------------------------

struct Outcome { bool won; int failNight; int overnight; double coinAtWin; int relics, bought, attunes; };

static const int OvernightCap = 10;

static Outcome playRun(uint32_t seed, int calling, int pact) {
  memset(&mem, 0, sizeof(mem));
  newRun(r, car, calling, pact, -1, seed, seed);
  if (P.spells == SpellsNone) r.nSpells = 0;
  world.seed(seed * 7919u + 1, 11);
  Outcome o{false, 0, 0, 0, 0, 0};
  while (r.night < Nights + OvernightCap) {
    beginNight(r);
    check(r.n.visits >= 1, "no visits");
    bool loaned = false;
    while (!r.n.met) {
      if (P.loan == LoanEarly && r.night >= 1 && canTakeLoan(r)) { takeLoan(r); if (r.night < Nights) tally.loans++; loaned = true; break; }
      if (r.n.used >= r.n.visits) {
        if (P.candles && canLightCandle(r)) { if (r.night < Nights) { tally.candleCoin += candlePrice(r); tally.candles++; } lightCandle(r); continue; }
        if (P.loan != LoanNever && canTakeLoan(r)) { takeLoan(r); if (r.night < Nights) tally.loans++; loaned = true; break; }
        break;
      }
      playVisit();
    }
    (void)loaned;
    if (!r.n.met) { o.failNight = r.night + 1; break; }
    PortentChoice ch;
    NightEnd e = endNight(r, car, 70, &ch);
    for (int k = 0; k < e.portents; k++) {   // one of two: keep the one worth more to this build
      if (!((ch.pending >> k) & 1)) continue;
      const int a = e.got[k].portent, b = ch.alt[k] - 1;
      choosePortent(r, car, e, ch, k, portentValue(b, ch) > portentValue(a, ch));
      tally.choices++;
    }
    if (r.night <= Nights) { tally.nights++; tally.trade += e.trade; tally.spare += e.spareCoin; }
    check(r.coin >= 0, "negative coin");
    check(slotsUsed(r) <= relicSlots(r), "too many relics");
    check(r.nRelics <= Run::MaxRelics, "relic array overflow");
    if (e.won) { o.won = true; o.coinAtWin = r.coin; o.relics = r.nRelics; o.bought = (int)r.stats[(int)Stat::bought_total]; o.attunes = (int)r.stats[(int)Stat::attunes_run]; }
    if (r.night > Nights) o.overnight = r.night - Nights;
    playShop();
    check(slotsUsed(r) <= relicSlots(r), "too many relics after shop");
  }
  if (!o.won) { o.relics = r.nRelics; o.bought = (int)r.stats[(int)Stat::bought_total]; o.attunes = (int)r.stats[(int)Stat::attunes_run]; }
  endRun(r, car, atlas);
  return o;
}

static int cmpi(const void* a, const void* b) { return *(const int*)a - *(const int*)b; }

struct CellResult { double win; double fails[Nights + 1]; double onMedian, onMean, onTop10, coin, salesPerNight, candlesPerNight, loansPerRun, bought, favorCoinPerNight, attunes, choices; };
static int modalFail(const CellResult& c) { int m = 1; for (int n = 2; n <= Nights; n++) if (c.fails[n] > c.fails[m]) m = n; return c.fails[m] > 0 ? m : 0; }

static CellResult runCell(const Policy& pol, int pact, int runs, bool everything) {
  P = pol;
  memset(&tally, 0, sizeof(tally));
  int wins = 0, fails[Nights + 1] = {0};
  static int depths[20000];
  int nd = 0;
  double coin = 0, bought = 0, onSum = 0, attunes = 0;
  for (int i = 0; i < runs; i++) {
    newCareer(car);
    if (everything) {
      for (int k = 0; k < RelicCount; k++) unlock(car, Kind::Relic, k);
      for (int k = 0; k < SpellCount; k++) unlock(car, Kind::Spell, k);
      for (int k = 0; k < TrinketCount; k++) unlock(car, Kind::Trinket, k);
    }
    atlas.clear();
    const Outcome o = playRun(1000 + i, Calling_wayfarer, pact);
    wins += o.won;
    bought += o.bought;
    attunes += o.attunes;
    if (o.won) { coin += o.coinAtWin; depths[nd++] = o.overnight; onSum += o.overnight; }
    else fails[o.failNight]++;
  }
  CellResult c{};
  c.win = 100.0 * wins / runs;
  for (int n = 0; n <= Nights; n++) c.fails[n] = 100.0 * fails[n] / runs;
  if (nd) { qsort(depths, nd, sizeof(int), cmpi); c.onMedian = depths[nd / 2]; c.onTop10 = depths[nd * 9 / 10]; c.onMean = onSum / nd; }
  c.coin = wins ? coin / wins : 0;
  const double N = tally.nights > 0 ? tally.nights : 1;
  c.salesPerNight = tally.sales / N;
  c.candlesPerNight = tally.candles / N;
  c.loansPerRun = (double)tally.loans / runs;
  c.bought = bought / runs;
  c.favorCoinPerNight = tally.jobCoin / N;
  c.attunes = attunes / runs;
  c.choices = (double)tally.choices / runs;
  return c;
}

// ---- The policies ---------------------------------------------------------------------

static Policy policies[32];
static int nPolicies = 0;
static Policy& add(const char* name) { policies[nPolicies] = Policy{}; policies[nPolicies].name = name; return policies[nPolicies++]; }

static void definePolicies() {
  nPolicies = 0;
  add("baseline (star, no favors)");
  { Policy& p = add("planner"); p.favors = true; p.spells = SpellsSmart; p.wards = WardsAdaptive; p.glyphs = true; }
  { Policy& p = add("nearest well"); p.pick = PickNearest; }
  { Policy& p = add("give it back (own energy)"); p.pick = PickOwnBack; }
  { Policy& p = add("first affordable relic"); p.shop = ShopFirstAffordable; }
  { Policy& p = add("rares only / hoard"); p.shop = ShopRaresOnly; }
  { Policy& p = add("uncommon+ only"); p.shop = ShopUncommonUp; }
  { Policy& p = add("multipliers first"); p.shop = ShopMultFirst; }
  { Policy& p = add("adders first"); p.shop = ShopAddsFirst; }
  { Policy& p = add("spend everything (polish)"); p.shop = ShopSpendAll; }
  { Policy& p = add("reroll happy"); p.shop = ShopRerollHappy; }
  { Policy& p = add("keep 25 coin (interest)"); p.shop = ShopInterest25; }
  { Policy& p = add("never polish"); p.polish = false; }
  { Policy& p = add("never attune"); p.attune = false; }
  { Policy& p = add("no spells"); p.spells = SpellsNone; }
  { Policy& p = add("glamour to close"); p.spells = SpellsSmart; }
  { Policy& p = add("carve glyphs"); p.glyphs = true; }
  { Policy& p = add("take favors"); p.favors = true; }
  { Policy& p = add("never loan"); p.loan = LoanNever; }
  { Policy& p = add("loan at 60%"); p.loan = LoanEarly; }
  { Policy& p = add("no candles"); p.candles = false; }
  { Policy& p = add("focus only"); p.energy = EnergyFocusOnly; }
  { Policy& p = add("spread (never draw focus)"); p.energy = EnergySpread; }
  { Policy& p = add("seek Nexus"); p.wards = WardsNexus; }
  { Policy& p = add("seek Hollow"); p.wards = WardsHollow; }
  { Policy& p = add("walk to where it pays"); p.wards = WardsAdaptive; }
  { Policy& p = add("attune spread"); p.attuneSpread = true; }
  { Policy& p = add("planner, attune spread"); p.favors = true; p.spells = SpellsSmart; p.wards = WardsAdaptive; p.glyphs = true; p.attuneSpread = true; }
}

// ---- Rule sets to measure ----------------------------------------------------------------

struct Scenario { const char* name; Rules rules; };
static Scenario scenarios[40];
static int nScenarios = 0;
// The bot values relics by the same arithmetic the stall's star uses, so the
// quiet rung (no star shown) would blind it, not the player it stands for: off.
static Rules& scenario(const char* name) {
  scenarios[nScenarios].name = name;
  scenarios[nScenarios].rules = Rules{};
  scenarios[nScenarios].rules.quietRung = RungOff;
  return scenarios[nScenarios++].rules;
}
static Rules& allOff(Rules& x) { for (int i = 0; i < PactRuleCount; i++) x.rung[i] = RungOff; x.saltRung = RungOff; return x; }

static void defineScenarios() {
  nScenarios = 0;
  { Rules& x = scenario("before: Salt at 1, no Portent choice"); x.rung[PR_lead] = RungOff; x.saltRung = 1; x.portentChoiceBelow = 0; }
  scenario("ship: Lead at 1, Portent choice below 3");
  { Rules& x = scenario("Lead at 1, no Portent choice"); x.portentChoiceBelow = 0; }
  { Rules& x = scenario("Lead at 1, Portent choice everywhere"); x.portentChoiceBelow = 8; }
  { Rules& x = scenario("Salt at 1, Portent choice below 3"); x.rung[PR_lead] = RungOff; x.saltRung = 1; }
  // Each rule alone at rung 1, to see what it costs by itself.
  { Rules& x = allOff(scenario("solo: lead")); x.rung[PR_lead] = 1; }
  { Rules& x = allOff(scenario("solo: salt")); x.saltRung = 1; }
  { Rules& x = allOff(scenario("solo: iron")); x.rung[PR_iron] = 1; }
  { Rules& x = allOff(scenario("solo: ash")); x.rung[PR_ash] = 1; }
  { Rules& x = allOff(scenario("solo: bone")); x.rung[PR_bone] = 1; }
  { Rules& x = allOff(scenario("solo: silver")); x.rung[PR_silver] = 1; }
  { Rules& x = allOff(scenario("solo: obsidian")); x.rung[PR_obsidian] = 1; }
  { Rules& x = allOff(scenario("solo: gold")); x.rung[PR_gold] = 1; }
  // Other places for the Lead rung.
  { Rules& x = scenario("iron 1, lead 2"); x.rung[PR_iron] = 1; x.rung[PR_lead] = 2; }
  { Rules& x = scenario("iron 1 ash 2 bone 3, lead 4"); x.rung[PR_iron] = 1; x.rung[PR_ash] = 2; x.rung[PR_bone] = 3; x.rung[PR_lead] = 4; }
}

// One table: every chosen policy on every Pact under one rule set.
static void table(const Scenario& sc, int runs, bool everything, const int* which, int nWhich) {
  rules = sc.rules;
  static const int pacts[] = {0, 1, 2, 3, 4, 5, 6, 7};
  const int np = (int)(sizeof(pacts) / sizeof(pacts[0]));
  printf("\n== %s: %d runs per cell, Wayfarer, %s unlocked\n", sc.name, runs, everything ? "everything" : "starting pool");
  printf("   rungs: lead %d iron %d ash %d bone %d silver %d obsidian %d gold %d | salt %d | portent choice below %d\n",
         rules.rung[PR_lead], rules.rung[PR_iron], rules.rung[PR_ash], rules.rung[PR_bone], rules.rung[PR_silver], rules.rung[PR_obsidian], rules.rung[PR_gold],
         rules.saltRung, rules.portentChoiceBelow);
  printf("   cell: win%% | Overnight depth median/top10 among winners | coin at the win | fN: the Night most losses fall on\n");
  printf("%-28s", "policy");
  for (int k = 0; k < np; k++) printf(" | %-16s", Pacts[pacts[k]].name);
  printf(" | Chalk: sales/N bought attunes favor/N choices/run\n");
  for (int w = 0; w < nWhich; w++) {
    const Policy& pol = policies[which[w]];
    printf("%-28s", pol.name);
    CellResult first{};
    for (int k = 0; k < np; k++) {
      const CellResult c = runCell(pol, pacts[k], runs, everything);
      printf(" | %3.0f%% %1.0f/%-2.0f c%-3.0f f%d", c.win, c.onMedian, c.onTop10, c.coin, modalFail(c));
      if (k == 0) first = c;
      fflush(stdout);
    }
    printf(" | %.1f %.1f %.1f %.1f %.1f\n", first.salesPerNight, first.bought, first.attunes, first.favorCoinPerNight, first.choices);
    fflush(stdout);
  }
}

static int policyIndex(const char* name) {
  for (int i = 0; i < nPolicies; i++) if (!strcmp(policies[i].name, name)) return i;
  return -1;
}

// The smoke test pio runs: a few weeks under today's rules and under the full set, nothing may break.
static void test_bot_holds_together() {
  definePolicies();
  defineScenarios();
  const int which[2] = {policyIndex("baseline (star, no favors)"), policyIndex("planner")};
  table(scenarios[0], 12, false, which, 2);
  table(scenarios[1], 12, false, which, 2);
  rules = Rules{};
  TEST_ASSERT_EQUAL_MESSAGE(0, violations, "an invariant broke");
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    UNITY_BEGIN();
    RUN_TEST(test_bot_holds_together);
    return UNITY_END();
  }
  definePolicies();
  defineScenarios();
  int runs = 1000;
  bool everything = false;
  // The policies that tell the story; -P adds one by name, -A plays them all.
  static const char* const story[] = {"baseline (star, no favors)", "planner", "take favors", "focus only",
                                      "spread (never draw focus)", "never attune", "attune spread", "planner, attune spread"};
  int which[32], nWhich = 0;
  for (const char* nm : story) which[nWhich++] = policyIndex(nm);
  bool any = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-r") && i + 1 < argc) { runs = atoi(argv[++i]); continue; }
    if (!strcmp(argv[i], "-a")) { everything = true; continue; }
    if (!strcmp(argv[i], "-A")) { nWhich = 0; for (int k = 0; k < nPolicies; k++) which[nWhich++] = k; continue; }
    if (!strcmp(argv[i], "-P") && i + 1 < argc) { const int k = policyIndex(argv[++i]); if (k >= 0 && nWhich < 32) which[nWhich++] = k; continue; }
    if (!strcmp(argv[i], "-l")) { for (int k = 0; k < nScenarios; k++) printf("%s\n", scenarios[k].name); for (int k = 0; k < nPolicies; k++) printf("  policy: %s\n", policies[k].name); return 0; }
    // A scenario, by the start of its name.
    bool found = false;
    for (int k = 0; k < nScenarios; k++) {
      if (strncmp(scenarios[k].name, argv[i], strlen(argv[i]))) continue;
      table(scenarios[k], runs, everything, which, nWhich);
      found = any = true;
      break;
    }
    if (!found) { fprintf(stderr, "no scenario starts with %s (-l lists them)\n", argv[i]); return 2; }
  }
  if (!any) for (int k = 0; k < nScenarios; k++) table(scenarios[k], runs, everything, which, nWhich);
  if (violations) { printf("%d invariants broke\n", violations); return 1; }
  return 0;
}
