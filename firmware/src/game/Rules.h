#pragma once
#include <math.h>
#include "Run.h"

// One sale: Units x Price x Mult, with every relic firing in turn. A line-by-
// line port of score() and fire() in tools/balance_sim.py, and of each
// relic's effect in tools/catalog.py. test/test_parity checks the two agree
// on thousands of random sales, so change both together.

namespace Game {

// What a sale knows about the world and the hold. The names follow the
// simulator's sale context.
struct Sale {
  int e;                 // energy sold
  double units, price, mult, x, repeats, coin;
  int n;                 // networks in range (Fog halves what relics see)
  int ward;
  int faction;
  bool hidden;           // a well with no name
  int channel;
  int ch11;              // wells on channel 11 in range
  bool strong;           // standing right next to it
  int mesh;              // routers in this well's mesh group, 0 if none
  bool revisit;          // visited on an earlier Night of this run
  int nightsSeen;        // Nights this run you've visited it, tonight included
  bool isNew;            // never visited before
  bool wardChanged;
  int wardsTonight;
  int glyph;             // Cat::Glyph carved here, -1 for none
  double noise;          // 0..1, how loud it is
  bool still;            // the Deck held still while selling
  bool batteryLow;
  bool dowsed;
  bool oneKind;          // the hold carries one kind of energy
  int holdKinds;
  int lvl;               // Attunement level of the energy sold
  int vi;                // visits already used tonight
  int lastE;             // energy of the last sale, -1 for none
  bool ghost;            // Ghost key at a hidden well: counts as every energy
};

inline void clearSale(Sale& c) {
  memset(&c, 0, sizeof(c));
  c.mult = 1;
  c.x = 1;
  c.glyph = -1;
  c.lastE = -1;
}

// The last few sales as they stood before scoring, so the shop can try a relic
// against how you actually play (the hint star). Saved with the run.
struct RecentSales {
  static constexpr int Max = 10;
  Sale s[Max];
  uint8_t n, head, pad[2];
  void add(const Sale& c) {
    s[head] = c;
    head = (uint8_t)((head + 1) % Max);
    if (n < Max) n++;
  }
};

inline int rarityRank(int relic) { return Cat::Relics[relic].rarity + 1; }

// ---- Each relic's effect (tools/catalog.py) ---------------------------------

inline void effect(int id, Sale& c, const Run& r) {
  using namespace Cat;
  const double n40 = c.n < 40 ? c.n : 40;
  switch (id) {
    case R_crowd_feeder: c.mult += 0.1 * n40; break;
    case R_rush_hour: if (c.ward == Nexus) c.mult += 3; break;
    case R_choir_of_eleven: c.mult += 0.5 * c.ch11; break;
    case R_packed_house: if (c.n >= 30) c.x *= 1.5; break;
    case R_gridlock: c.x *= 1 + 0.05 * n40; break;
    case R_hollow_saint: if (c.ward == Hollow) c.mult += 4; break;
    case R_hermits_lamp: if (c.n <= 5) c.price += 3; break;
    case R_quiet_hours: if (c.ward == Hollow) c.x *= 2; break;
    case R_lantern_bearer: c.mult += r.n.hollows; break;
    case R_lost_signal: if (c.n <= 2) c.x *= 3; break;
    case R_borderwalker: if (c.n >= 4 && c.n <= 15) c.mult += 2.5; break;
    case R_porch_light: if (c.faction == Hearth) c.price += 4; break;
    case R_threshold: if (c.wardChanged) c.x *= 1.5; break;
    case R_picket_fence: c.mult += r.state[R_picket_fence]; break;
    case R_long_walk: c.mult += 0.25 * c.wardsTonight; break;
    case R_first_light: if (c.isNew) c.mult += 4; break;
    case R_cartographer: c.price += (int)(r.newWells / 6); break;
    case R_wanderlust: if (c.isNew) c.x *= 1.5; break;
    case R_pilgrims_staff: c.x *= 1 + r.state[R_pilgrims_staff]; break;
    case R_hearthstone: if (c.revisit) c.units += 3; break;
    case R_commuters_charm: if (c.revisit && c.nightsSeen >= 3) c.x *= r.night >= 4 ? 2 : 1.5; break;
    case R_well_worn_path: if (c.revisit) c.mult += c.nightsSeen; break;
    case R_ember_caller: if (c.e == Ember || c.ghost) c.price += 4; break;
    case R_tide_caller: if (c.e == Tide || c.ghost) c.price += 4; break;
    case R_gale_caller: if (c.e == Gale || c.ghost) c.price += 4; break;
    case R_hush_caller: if (c.e == Hush || c.ghost) c.price += 4; break;
    case R_static_caller: if (c.e == Static || c.ghost) c.price += 4; break;
    case R_resonant_fork: c.mult += c.lvl; break;
    case R_ascetic: if (c.oneKind) c.x *= 2; break;
    case R_mirror_glass: if (c.e == c.lastE) c.x *= 1.5; break;
    case R_purist: c.x *= 1 + 0.2 * c.lvl; break;
    case R_mixed_blessing: if (c.holdKinds >= 3) c.price += 3; break;
    case R_prism_wheel: c.mult += 1.5 * bits(r.n.kinds | (1u << c.e)); break;
    case R_spectrum: c.x *= 1 + 0.3 * (bits(r.n.kinds | (1u << c.e)) - 1); break;
    case R_static_eater: if (c.e == Static || c.ghost) c.price += 5; break;
    case R_smugglers_lining: if (c.e == Static) c.units += 5; break;
    case R_devils_meter: c.x *= 1 + r.heat; break;
    case R_black_ledger: c.x *= 1 + r.state[R_black_ledger]; break;
    case R_wanted_poster: if (r.heat >= 3) c.x *= 3; break;
    case R_hushed_step: if (c.hidden || c.e == Hush) c.mult += 3; break;
    case R_ghost_key: if (c.hidden) c.mult += 5; break;
    case R_hush_lantern: if (c.e == Hush || c.ghost) c.x *= 2; break;
    case R_unseen_hand: if (c.hidden) c.x *= 2.5; break;
    case R_close_call: if (c.strong) c.mult += 3; break;
    case R_leyline: c.units *= 1.2; c.price *= 1.2; break;
    case R_channel_surfer: if (c.channel != 1 && c.channel != 6 && c.channel != 11) c.x *= 1.75; break;
    case R_overclock: c.x *= 1.5; break;
    case R_twin_wells: if (c.mesh) c.repeats += c.mesh - 1; break;
    case R_first_bell: if (c.vi == 0) c.mult += 8; break;
    case R_night_shift: c.mult += 0.5 * c.vi; break;
    case R_patience: c.mult += r.state[R_patience]; break;
    case R_tally_stick: c.mult += r.state[R_tally_stick]; break;
    case R_ritual_candle: c.x *= 1 + r.state[R_ritual_candle]; break;
    case R_hoard: { const double h = 1 + 0.02 * r.coin; c.x *= h < 2.0 ? h : 2.0; break; }
    case R_doppel: if (c.vi == 0) c.repeats += 1; break;
    case R_echo_chamber: if (r.sales % 3 == 2) c.repeats += 1; break;
    case R_din: c.mult += 4 * c.noise; break;
    case R_steady_hand: if (c.still) c.mult += 3; break;
    case R_dowsers_knot: if (c.dowsed) c.x *= 1.5; break;
    case R_last_ember: if (c.batteryLow) c.x *= 2.5; break;
    case R_glass_crown: c.x *= c.e == r.n.crownBan ? 0 : 3; break;
    case R_hungry_coin: c.mult += 6; c.coin -= 2; break;
    case R_blood_moon: c.x *= 3; break;
    case R_grid_saint: c.x *= 1 + 0.1 * n40; break;
    case R_the_unheard: c.x *= c.e == Hush ? 2 : 1; break;
    case R_seven_league_boots: c.x *= 1 + 0.1 * r.newWells; break;
    case R_old_keeper: if (c.revisit) c.x *= 3; break;
    case R_midnight_engine: c.repeats += 1; break;
    // The second batch.
    case R_laundromat_token: { const int t = (int)(r.coin / 5); c.mult += t < 4 ? t : 4; break; }
    case R_bus_fare: if (r.coin <= 4) c.mult += 5; break;
    case R_piggy_bank: c.mult += r.state[R_piggy_bank]; break;
    case R_long_receipt: c.x *= 1 + r.state[R_long_receipt]; break;
    case R_odd_socks: if (c.lastE >= 0 && c.e != c.lastE) c.mult += 4; break;
    case R_tasting_spoon: if (!((r.n.kinds >> c.e) & 1u)) c.x *= 2; break;
    case R_charm_bracelet: if (c.holdKinds > 1) c.x *= 1 + 0.4 * (c.holdKinds - 1); break;
    case R_one_way_ticket: c.x *= c.revisit ? 0 : 2.5; break;
    case R_empty_frame: { const int e = relicSlots(r) - slotsUsed(r); if (e > 0) c.x *= 1 + 0.5 * e; break; }
    case R_spare_key: if (r.standing[Hearth] > 0) c.mult += (int)(r.standing[Hearth] / 10); break;
    case R_garden_gate: if (c.ward == Fringe) c.x *= c.wardChanged ? 2 : 1.5; break;
    case R_chalk_stub: if (c.glyph >= 0) c.x *= 1.5; break;
    case R_bus_pass: if (c.revisit && r.night >= 1 && c.nightsSeen >= r.night + 1) c.repeats += 1; break;
    case R_key_ring: if (c.revisit) c.x *= 1 + 0.25 * c.nightsSeen; break;
    case R_thermos: if (c.e == Hush || c.ghost) c.units += 4; break;
    case R_locked_diary: c.x *= 1 + r.state[R_locked_diary]; break;
    case R_tote_bag: if (c.units >= 12) c.repeats += 1; break;
    case R_seashell: if (c.wardChanged) c.repeats += 1; break;
    case R_earplugs: c.mult += 4 * (1 - c.noise); break;
    case R_spirit_level: if (c.still && c.strong) c.x *= 1.5; break;
    case R_lanyard: if (c.faction == Syndicate) c.price += 5; break;
    case R_picnic_blanket: if (c.ward == Hollow) c.units += 3; break;
    case R_postcard: c.mult += r.n.newTonight; break;
    case R_film_canister: if ((c.e == Static || c.ghost) && r.heat < 1) c.x *= 2; break;
    default: break;   // keystone, witching hour, copycat and overtone work elsewhere
  }
}

// A record of a sale as it plays out, step by step, for the show on screen.
struct Trace {
  enum Field : uint8_t { Units, Price, Mult, X, Repeats };
  enum Source : uint8_t { FromAttune = 0xFD, FromGlyph = 0xFE };
  struct Step { uint8_t source; uint8_t field; int8_t ed; double after; };   // ed: an edition's shine, -1 none
  static constexpr int Max = 40;
  Step s[Max];
  int n;
  double units0, price0, mult0;
  void note(uint8_t source, const Sale& before, const Sale& after, int ed = -1) {
    const double b[5] = {before.units, before.price, before.mult, before.x, before.repeats};
    const double a[5] = {after.units, after.price, after.mult, after.x, after.repeats};
    for (int f = 0; f < 5 && n < Max; f++) if (a[f] != b[f]) s[n++] = Step{source, (uint8_t)f, (int8_t)ed, a[f]};
  }
};

// One relic firing, scaled up by its Polish tier: each tier makes what it
// added 50% bigger.
inline void fire(int id, Sale& c, const Run& r, int tier) {
  if (!tier) { effect(id, c, r); return; }
  const double bu = c.units, bp = c.price, bm = c.mult, bc = c.coin, br = c.repeats, bx = c.x;
  effect(id, c, r);
  const double k = 1 + PolishStep * tier;
  c.units = bu + (c.units - bu) * k;
  c.price = bp + (c.price - bp) * k;
  c.mult = bm + (c.mult - bm) * k;
  c.coin = bc + (c.coin - bc) * k;
  c.repeats = br + (c.repeats - br) * k;
  if (bx != 0 && c.x != bx) {
    const double ratio = c.x / bx;
    if (ratio > 1) {
      const double f = 1 + (ratio - 1) * k;
      c.x = bx * (f > 0 ? f : 0.0);
    }
  }
}

struct SaleResult {
  double resonance;
  double coin;
};

// Resonance and coin for one sale, before spells. The sale context is changed
// as the relics fire, so the screen can show each step afterwards.
inline SaleResult score(Sale& c, const Run& r, Trace* trace = nullptr) {
  using namespace Cat;
  Sale before;
  auto note = [&](uint8_t src, int ed = -1) { if (trace) { trace->note(src, before, c, ed); } };
  auto mark = [&]() { if (trace) before = c; };
  const int om = r.n.omen, ecl = r.n.eclipse;
  if (om == Omen_fade && c.e == r.n.fadeE) c.price *= 0.5;
  if (om == Omen_silent_bell && c.vi < 2) {
    if (trace) { trace->n = 0; trace->units0 = c.units; trace->price0 = c.price; trace->mult0 = c.mult; }
    return {0, 0};
  }
  if (r.n.veil && c.e == Static) c.price += 3;
  const double glyphK = (owns(r, R_keystone) ? 2 : 1) * (isCalling(r, Calling_cartomancer) ? 1.5 : 1);
  if (c.glyph == Glyph_deep) c.units += 5 * glyphK;
  if (trace) {
    trace->n = 0;
    trace->units0 = c.units;
    trace->price0 = c.price;
    // Attunement is the base every later multiplier multiplies, so the show
    // lists it as the first Amp step (ATTUNED +N) rather than folding it in.
    trace->mult0 = c.lvl > 0 ? 1 : c.mult;
    if (c.lvl > 0 && c.mult != 1) trace->s[trace->n++] = Trace::Step{Trace::FromAttune, Trace::Mult, -1, c.mult};
  }

  // Copycat sigil copies the best other relic (rarest, then latest phase).
  int copyOf = -1;
  if (owns(r, R_copycat_sigil)) {
    int bestRank = -1, bestPhase = -1;
    for (int i = 0; i < r.nRelics; i++) {
      const int id = r.relics[i].id;
      if (!Relics[id].hasFn) continue;
      const int rk = rarityRank(id), ph = Relics[id].phase;
      if (rk > bestRank || (rk == bestRank && ph > bestPhase)) { copyOf = id; bestRank = rk; bestPhase = ph; }
    }
  }
  auto tierOf = [&](const OwnedRelic& x) { return r.n.noPolish ? x.tier / 2 : (int)x.tier; };

  const double priceBefore = c.price;
  for (int i = 0; i < r.nRelics; i++) {
    const OwnedRelic& x = r.relics[i];
    if (Relics[x.id].phase == 0 && Relics[x.id].hasFn) {
      mark();
      fire(x.id, c, r, tierOf(x));
      if (x.id == copyOf) fire(x.id, c, r, tierOf(x));
      note(x.id);
    }
  }
  if (om == Omen_iron_price && c.price > priceBefore) c.price = priceBefore + (c.price - priceBefore) / 2;
  const double baseMult = c.mult;
  mark();
  if (c.glyph == Glyph_lit) c.mult += 4 * glyphK;
  if (c.glyph == Glyph_bound) c.x *= 1 + 0.5 * glyphK;
  if (c.glyph == Glyph_echoing) c.repeats += (int)glyphK;
  note(Trace::FromGlyph);
  const int times = (owns(r, R_overtone) && c.units >= 10) ? 2 : 1;
  const bool dim = ecl == Eclipse_dim;
  for (int phase = 1; phase <= 3; phase++) {
    for (int i = 0; i < r.nRelics; i++) {
      const OwnedRelic& x = r.relics[i];
      mark();
      if (phase == 1 && x.ed == Edition_lit) c.mult += 3;
      if (phase == 2 && x.ed == Edition_prismatic) c.x *= dim ? 1.375 : 1.5;
      note(x.id, x.ed);
      if (Relics[x.id].phase != phase || !Relics[x.id].hasFn) continue;
      const int reps = times * (x.id == copyOf ? 2 : 1);
      mark();
      for (int k = 0; k < reps; k++) {
        const double xb = c.x;
        fire(x.id, c, r, tierOf(x));
        if (dim && xb != 0 && c.x > xb) c.x = xb * (1 + (c.x / xb - 1) * 0.75);
      }
      note(x.id);
    }
  }
  double res;
  if (om == Omen_tangled) {
    const double mult = baseMult * c.x + (c.mult - baseMult);
    res = c.units * c.price * mult * (1 + c.repeats);
  } else {
    res = c.units * c.price * c.mult * c.x * (1 + c.repeats);
  }
  double coin = c.units * c.price * CoinPerBase + c.coin + (hasTrinket(r, Trinket_pickpocket) ? 1 : 0);
  if (c.glyph == Glyph_gilded) coin += 3 * glyphK;
  return {res > 0 ? res : 0.0, coin};
}

} // namespace Game
