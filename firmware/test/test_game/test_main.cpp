// The run's rules, one at a time: pio test -e native -f test_game
#include <unity.h>
#include <string.h>
#include "game/Game.h"
#include "game/RealClock.h"

using namespace Game;
using namespace Cat;

static Career car;
static Run r;
static Atlas atlas;
static Shop shop;
static RecentSales mem;

static WellView well(uint32_t id, int energy = Tide, int faction = Hearth, float sig = -50, int channel = 6) {
  WellView w{};
  w.id = id;
  w.seed = id * 2654435761u;
  w.energy = (uint8_t)energy;
  w.faction = (uint8_t)faction;
  w.channel = (uint8_t)channel;
  w.sig = sig;
  return w;
}
static Here here(int n = 8, int ward = Fringe) {
  Here h{};
  h.n = n;
  h.ward = ward;
  h.battery = 80;
  return h;
}

void setUp() {
  playMinNow = 0;
  newCareer(car);
  atlas.clear();
  memset(&mem, 0, sizeof(mem));
  newRun(r, car, Calling_wayfarer, 0, -1, 1234, 1);
  beginNight(r);
}
void tearDown() {}

static void test_a_new_run() {
  TEST_ASSERT_EQUAL(4, (int)r.coin);
  TEST_ASSERT_EQUAL(1, r.nSpells);
  TEST_ASSERT_EQUAL(Spell_glamour, r.spells[0]);
  TEST_ASSERT_TRUE(r.focus <= Gale);
  TEST_ASSERT_EQUAL(0, r.omens[0]);
  for (int i = 1; i < Nights - 1; i++) TEST_ASSERT_TRUE(r.omens[i] >= 1 && r.omens[i] <= OmenCount);
  TEST_ASSERT_EQUAL_DOUBLE(410, r.n.tithe);
  TEST_ASSERT_EQUAL(12, r.n.visits);
  TEST_ASSERT_EQUAL(-1, r.n.omen);
}

static void test_callings_and_pacts_change_the_start() {
  newRun(r, car, Calling_hedge_witch, 0, -1, 5, 1);
  TEST_ASSERT_TRUE(owns(r, R_static_eater) && owns(r, R_static_caller));
  newRun(r, car, Calling_courier, 2, -1, 5, 1);
  beginNight(r);
  TEST_ASSERT_EQUAL(9, r.n.visits);   // 10, and the Iron Pact takes one
  newRun(r, car, Calling_contractor, 1, -1, 5, 1);
  TEST_ASSERT_EQUAL(20, (int)r.coin);
  r.night = 1;
  beginNight(r);
  const double base = Tithes[1];   // Lead: the Tithe curve is the plain one
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, base + 0.2 * Tithes[1], r.n.tithe - (r.n.omen == Omen_blood_price ? base * 0.2 : 0));
}

static void test_drawing_takes_more_closer() {
  WellView nearW = well(1, Tide, Hearth, -45), edge = well(2, Gale, Hearth, -67), far = well(3, Ember, Hearth, -75);
  Drawn a = draw(r, atlas, nearW, here(), 1.0);
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)a.why);
  TEST_ASSERT_EQUAL(8, a.units);
  r.hold[Tide] = 0;
  Drawn b = draw(r, atlas, edge, here(), 1.0);
  TEST_ASSERT_EQUAL(4, b.units);
  TEST_ASSERT_EQUAL((int)Why::TooFar, (int)draw(r, atlas, far, here(), 1.0).why);
  TEST_ASSERT_EQUAL((int)Why::Dry, (int)draw(r, atlas, edge, here(), 1.0).why);
}

static void test_a_shaky_hand_spills() {
  Drawn d = draw(r, atlas, well(1, Tide, Hearth, -45), here(), 0.0);
  TEST_ASSERT_EQUAL(6, d.units);
  TEST_ASSERT_EQUAL(2, d.spilled);
}

static void test_the_hold_has_a_limit() {
  draw(r, atlas, well(1, Tide, Hearth, -45), here(), 1);
  TEST_ASSERT_EQUAL((int)Why::HoldFull, (int)canDraw(r, atlas, well(2, Tide, Hearth, -45)));
  r.mods |= 1u << Mod_satchel;
  Drawn d = draw(r, atlas, well(2, Ember, Hearth, -45), here(), 1);
  TEST_ASSERT_EQUAL(2, d.units);   // only room for 2 more
}

// A used well settles after SettleMin minutes: it can be drawn from and
// offered to again that Night, for a little less Thirst.
static void test_a_well_settles() {
  playMinNow = 0;
  WellView w = well(3, Tide, Hearth, -50);
  draw(r, atlas, w, here(30, Nexus), 1);
  const Sold first = sell(r, atlas, w, here(30, Nexus), Tide, false, &mem);
  TEST_ASSERT_EQUAL((int)Why::Dry, (int)canDraw(r, atlas, w));
  playMinNow = SettleMin - 1;
  TEST_ASSERT_EQUAL((int)Why::Dry, (int)canDraw(r, atlas, w));
  TEST_ASSERT_EQUAL(1, settleLeft(atlas.find(w.id)));
  playMinNow = SettleMin;
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canDraw(r, atlas, w));
  Here later = here(30, Nexus);
  later.playMin = SettleMin;
  draw(r, atlas, w, later, 1);
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canSell(r, atlas, w, Tide));
  const Sold again = sell(r, atlas, w, later, Tide, false, &mem);
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)again.why);
  // About three quarters of the first Thirst (standing from the first offering nudges it up a little).
  TEST_ASSERT_DOUBLE_WITHIN(0.03 * first.c.price, first.c.price * ResettledThirst, again.c.price);
  TEST_ASSERT_EQUAL((int)Why::AlreadySold, (int)canSell(r, atlas, w, Tide));   // and settles again from now
  playMinNow = 0;
}

// Settling reopens one draw, not endless ones: you can't keep drawing from a
// settled well without offering to it.
static void test_a_settled_well_gives_one_draw() {
  playMinNow = 0;
  WellView w = well(4, Tide, Hearth, -50);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide, false, &mem);
  playMinNow = SettleMin + 5;
  Here later = here();
  later.playMin = SettleMin + 5;
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canDraw(r, atlas, w));
  draw(r, atlas, w, later, 1);
  r.hold[Tide] = 0;   // offered somewhere else
  TEST_ASSERT_EQUAL((int)Why::Dry, (int)canDraw(r, atlas, w));
  r.hold[Tide] = 4;
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canSell(r, atlas, w, Tide));   // the offering is still open
  sell(r, atlas, w, later, Tide, false, &mem);
  TEST_ASSERT_EQUAL((int)Why::Dry, (int)canDraw(r, atlas, w));
  playMinNow = 2 * SettleMin + 5;
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canDraw(r, atlas, w));      // and the next settling reopens it
  playMinNow = 0;
}

// Heat stays in whole tenths: five Static offerings make exactly 2.0, so the
// Wardens' eye and the "at 3" checks see what the screen shows.
static void test_heat_stays_in_tenths() {
  for (int k = 0; k < 5; k++) {
    WellView w = well(100 + k, Static, Hearth, -45);
    r.hold[Static] = 3;
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, tenths(r.heat + 0.4), heatAfterStatic(r));
    sell(r, atlas, w, here(), Static, false, &mem);
  }
  TEST_ASSERT_TRUE(r.heat == 2.0);
}

static void test_selling_pays_and_uses_a_visit() {
  WellView w = well(1, Tide, Hearth, -50);
  draw(r, atlas, w, here(), 1);
  const int units = r.hold[Tide];
  Sold s = sell(r, atlas, w, here(), Tide, false, &mem);
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)s.why);
  TEST_ASSERT_EQUAL(0, r.hold[Tide]);
  TEST_ASSERT_EQUAL(1, r.n.used);
  // Units x Price x Mult, with Fringe's 1.1 on price and no relics.
  const double price = 5 * taste(w.seed, Tide) * 1.1;
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, units * price, s.resonance);
  TEST_ASSERT_TRUE(s.isNew);
  TEST_ASSERT_EQUAL((int)Why::AlreadySold, (int)canSell(r, atlas, w, Tide));
  TEST_ASSERT_EQUAL(1, mem.n);
  TEST_ASSERT_EQUAL(1, (int)atlas.unique);
}

static void test_the_warded_refuse_static_unless_fenced() {
  WellView w = well(1, Static, Warded, -50);
  r.hold[Static] = 5;
  TEST_ASSERT_EQUAL((int)Why::Banned, (int)canSell(r, atlas, w, Static));
  r.trinkets[r.nTrinkets++] = Trinket_fence;
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canSell(r, atlas, w, Static));
}

static void test_paying_the_tithe_closes_the_night() {
  r.n.tithe = 30;
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  Sold s = sell(r, atlas, w, here(), Tide);
  TEST_ASSERT_TRUE(s.closed);
  TEST_ASSERT_EQUAL(1, r.n.met);
  r.hold[Ember] = 3;
  TEST_ASSERT_EQUAL((int)Why::NightClosed, (int)canSell(r, atlas, well(2), Ember));
}

static void test_night_end_pays_out() {
  r.n.tithe = 10;
  r.coin = 12;
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  NightEnd e = endNight(r, car, 80);
  TEST_ASSERT_TRUE(e.paid);
  TEST_ASSERT_EQUAL(11, e.spare);
  bool doubled = false;
  for (int i = 0; i < e.portents; i++) doubled |= e.got[i].portent == Portent_doubled_interest;
  TEST_ASSERT_EQUAL(doubled ? 4 : 2, e.interest);   // 12 coin: 2 interest (twice with Doubled interest)
  TEST_ASSERT_EQUAL(NightReward, e.reward);
  TEST_ASSERT_EQUAL(3, e.portents);   // 11 to spare: three Portents
  TEST_ASSERT_EQUAL(1, r.night);
  TEST_ASSERT_TRUE((int)r.coin >= 12 + 11 + 5 + 2);
}

static void test_candles_then_a_loan() {
  r.n.used = r.n.visits;
  r.coin = 30;
  TEST_ASSERT_EQUAL(4, candlePrice(r));
  lightCandle(r);
  TEST_ASSERT_EQUAL(6, candlePrice(r));
  TEST_ASSERT_EQUAL(26, (int)r.coin);
  TEST_ASSERT_EQUAL(13, r.n.visits);
  r.n.used = r.n.visits;
  r.n.total = r.n.tithe * 0.59;
  TEST_ASSERT_FALSE(canTakeLoan(r));
  r.n.total = r.n.tithe * 0.6;
  TEST_ASSERT_TRUE(canTakeLoan(r));
  takeLoan(r);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, r.n.tithe * 0.4 * 1.25, r.owed);
  TEST_ASSERT_EQUAL(1, r.n.met);
  NightEnd e = endNight(r, car, 80);
  TEST_ASSERT_TRUE(e.mercy);
  beginNight(r);
  TEST_ASSERT_TRUE(r.n.tithe > Tithes[1]);   // the loan is on the next Tithe
}

static void test_stuck_means_the_binding_is_lost() {
  r.n.used = r.n.visits;
  r.coin = 0;
  r.n.total = 0;
  TEST_ASSERT_TRUE(stuck(r));
}

static void test_revisits_and_nights_seen() {
  WellView w = well(7, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  r.n.met = 1;
  endNight(r, car, 80);
  beginNight(r);
  Sale c;
  r.hold[Tide] = 5;
  saleContext(c, r, atlas, w, here(), Tide);
  TEST_ASSERT_TRUE(c.revisit);
  TEST_ASSERT_EQUAL(2, c.nightsSeen);
  TEST_ASSERT_FALSE(c.isNew);
}

static void test_wardens_walk_starts_the_night_at_heat_3() {
  r.night = 1;
  r.heat = 0;
  r.omens[1] = (uint8_t)(Omen_wardens_walk + 1);
  beginNight(r);
  TEST_ASSERT_EQUAL(Omen_wardens_walk, r.n.omen);
  TEST_ASSERT_EQUAL_DOUBLE(3, r.heat);
  r.night = 2;
  r.heat = 3.5;   // already hotter: stays where it is
  r.omens[2] = (uint8_t)(Omen_wardens_walk + 1);
  beginNight(r);
  TEST_ASSERT_EQUAL_DOUBLE(3.5, r.heat);
}

// Doubled interest pays on the Night it's rolled, not the one after.
static void test_doubled_interest_pays_tonight() {
  for (uint32_t seed = 1; seed < 400; seed++) {
    newRun(r, car, Calling_wayfarer, 0, -1, seed, 1);
    beginNight(r);
    atlas.clear();
    r.n.tithe = 10;
    r.coin = 12;
    WellView w = well(1, Tide, Hearth, -45);
    draw(r, atlas, w, here(), 1);
    sell(r, atlas, w, here(), Tide);
    NightEnd e = endNight(r, car, 80);
    bool doubled = false;
    for (int i = 0; i < e.portents; i++) doubled |= e.got[i].portent == Portent_doubled_interest;
    if (!doubled) {
      TEST_ASSERT_EQUAL(2, e.interest);
      continue;
    }
    TEST_ASSERT_EQUAL(4, e.interest);   // 12 coin: 2 interest, paid twice
    TEST_ASSERT_EQUAL(0, (int)(r.shopFlags & SF_DoubleInterest));
    return;
  }
  TEST_FAIL_MESSAGE("no seed rolled a Doubled interest Portent");
}

static void test_static_raises_heat_and_a_ward_holds_the_wardens() {
  for (int i = 0; i < 8; i++) {
    WellView w = well(100 + i, Static, Hedge, -45);
    r.hold[Static] = 4;
    sell(r, atlas, w, here(), Static);
  }
  TEST_ASSERT_TRUE(r.heat >= 3);
  Here h = here();
  h.playSec = 100;
  TEST_ASSERT_TRUE(castWard(r, h));
  TEST_ASSERT_FALSE(castWard(r, h));   // once a Night
  int blocked = 0;
  for (int i = 0; i < 4 && r.n.used < r.n.visits; i++) {
    r.hold[Tide] = 4;
    blocked += sell(r, atlas, well(200 + i), h, Tide).blocked;
  }
  TEST_ASSERT_EQUAL(0, blocked);
}

static void test_the_first_shop_has_your_caller() {
  r.night = 1;
  openShop(shop, r, car);
  TEST_ASSERT_TRUE(shop.open);
  TEST_ASSERT_EQUAL(R_ember_caller + r.focus, shop.relics[0].id);
  TEST_ASSERT_EQUAL(3, shop.nCons);
}

static void test_overstock_grows_the_stall_youre_at() {
  r.night = 2;
  r.coin = 200;
  openShop(shop, r, car);
  const int before = shop.nRelics;
  for (int i = 0; i < before; i++)
    for (int k = 0; k < i; k++) TEST_ASSERT_NOT_EQUAL(shop.relics[k].id, shop.relics[i].id);   // no repeats
  shop.mod.kind = OfferKind::Mod;
  shop.mod.id = Mod_overstock;
  shop.mod.sold = 0;
  TEST_ASSERT_EQUAL((int)Buy::Ok, (int)buyMod(shop, r, car));
  TEST_ASSERT_EQUAL(before + 1, shop.nRelics);
  int full = 0;   // a reroll keeps the extra relic
  for (int t = 0; t < 20; t++) { shop.freeRerolls = 1; reroll(shop, r, car); full += shop.nRelics >= 4; }
  TEST_ASSERT_TRUE(full >= 18);
}

static void test_a_full_spell_pocket_can_swap() {
  r.night = 1;
  r.coin = 100;
  openShop(shop, r, car);
  while (r.nSpells < spellSlots(r)) r.spells[r.nSpells++] = Spell_hex;
  shop.cons[0].kind = OfferKind::Spell;
  shop.cons[0].id = Spell_haste;
  shop.cons[0].sold = 0;
  TEST_ASSERT_EQUAL((int)Buy::NoSlot, (int)buyConsumable(shop, r, 0));
  TEST_ASSERT_EQUAL((int)Buy::Ok, (int)buyConsumable(shop, r, 0, -1, 0));
  TEST_ASSERT_EQUAL(spellSlots(r), r.nSpells);
  TEST_ASSERT_EQUAL(Spell_haste, r.spells[r.nSpells - 1]);
}

static void test_buying_with_full_slots_needs_a_sale() {
  r.night = 1;
  for (int i = 0; i < 5; i++) addRelic(r, R_crowd_feeder + i);
  r.coin = 50;
  openShop(shop, r, car);
  TEST_ASSERT_EQUAL((int)Buy::NoSlot, (int)buyRelic(shop, r, 0));
  const double before = r.coin;
  const int back = sellBack(r, r.relics[0]);
  TEST_ASSERT_EQUAL((int)Buy::Ok, (int)buyRelic(shop, r, 0, 0));
  TEST_ASSERT_EQUAL(5, slotsUsed(r));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, before + back - cost(r, shop.relics[0]), r.coin);
}

static void test_polish_tiers_and_prices() {
  addRelic(r, R_crowd_feeder);
  r.coin = 100;
  TEST_ASSERT_EQUAL(6, polishPrice(r, r.relics[0]));
  polish(r, 0); polish(r, 0); polish(r, 0);
  TEST_ASSERT_EQUAL(3, r.relics[0].tier);
  TEST_ASSERT_EQUAL(100 - 6 - 12 - 20, (int)r.coin);
  TEST_ASSERT_EQUAL((int)Buy::Cant, (int)polish(r, 0));
  r.pact = 5;   // Silver doubles it
  addRelic(r, R_gridlock);
  TEST_ASSERT_EQUAL(12, polishPrice(r, r.relics[1]));
}

static void test_rerolls_get_dearer() {
  r.night = 1;
  r.coin = 30;
  openShop(shop, r, car);
  TEST_ASSERT_EQUAL(3, shop.rerollCost);
  reroll(shop, r, car);
  TEST_ASSERT_EQUAL(27, (int)r.coin);
  TEST_ASSERT_EQUAL(5, shop.rerollCost);
}

static void test_mercy_helps_the_next_shop() {
  r.night = 1;
  r.shopFlags |= SF_Mercy;
  const int lvl = r.attune[r.focus];
  openShop(shop, r, car);
  TEST_ASSERT_EQUAL(1, shop.mercy);
  TEST_ASSERT_EQUAL(1, shop.freeRerolls);
  TEST_ASSERT_EQUAL(lvl + 1, r.attune[r.focus]);
  bool half = false;
  for (int i = 0; i < shop.nRelics; i++) half |= shop.relics[i].half;
  TEST_ASSERT_TRUE(half);
}

static void test_spells() {
  WellView w = well(1, Tide, Hearth, -45);
  Here h = here();
  // GLAMOUR doubles the next sale.
  draw(r, atlas, w, h, 1);
  Sale c;
  saleContext(c, r, atlas, w, h, Tide);
  const double plain = score(c, r).resonance;
  TEST_ASSERT_EQUAL((int)Cast::Ok, (int)castSpell(r, atlas, car, 0, &w, h).how);
  Sold s = sell(r, atlas, w, h, Tide);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, plain * 2, s.resonance);
  // CHALK needs a well in reach.
  r.spells[r.nSpells++] = Spell_chalk;
  WellView far = well(2, Tide, Hearth, -80);
  TEST_ASSERT_EQUAL((int)Cast::NeedWell, (int)castSpell(r, atlas, car, 0, &far, h).how);
  TEST_ASSERT_EQUAL(1, r.nSpells);
  TEST_ASSERT_EQUAL((int)Cast::Ok, (int)castSpell(r, atlas, car, 0, &w, h).how);
  TEST_ASSERT_TRUE(atlas.find(1)->glyph >= 0);
  // HASTE adds two visits.
  r.spells[r.nSpells++] = Spell_haste;
  const int v = r.n.visits;
  castSpell(r, atlas, car, 0, nullptr, h);
  TEST_ASSERT_EQUAL(v + 2, r.n.visits);
}

static void test_jobs_pay_and_raise_standing() {
  // Find a Hearth well that offers a job.
  Job j{};
  WellView w;
  for (uint32_t id = 1; id < 200 && !j.kind; id++) { w = well(id, Tide, Hearth, -45); j = jobOffer(r, w); }
  TEST_ASSERT_EQUAL(Jobs::Deliver, j.kind);
  TEST_ASSERT_TRUE(takeJob(r, j));
  TEST_ASSERT_FALSE(takeJob(r, j));   // one per faction
  const double coin = r.coin;
  r.hold[j.energy] = j.need;
  Sold s = sell(r, atlas, w, here(), j.energy);
  TEST_ASSERT_EQUAL(j.reward, s.jobCoin);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, coin + j.reward, r.coin);
  TEST_ASSERT_TRUE(r.standing[Hearth] >= 9);
}

// A delivery adds up across the Night's offerings, and the dawn tally shows
// what favors paid. The tally's byte sits in NightEnd's last padding byte.
static void test_deliveries_add_up_and_show_at_dawn() {
  Job j{};
  WellView w;
  for (uint32_t id = 1; id < 200 && !j.kind; id++) { w = well(id, Tide, Hearth, -45); j = jobOffer(r, w); }
  TEST_ASSERT_EQUAL(Jobs::Deliver, j.kind);
  char text[48];
  jobText(j, text, sizeof(text));
  TEST_ASSERT_NOT_NULL(strstr(text, "AT HEARTH WELLS"));
  TEST_ASSERT_TRUE(takeJob(r, j));
  r.n.tithe = 1e9;   // the Night stays open
  // Two units short, at one Hearth well: not done, but counted.
  r.hold[j.energy] = (uint8_t)(j.need - 2);
  Sold s = sell(r, atlas, w, here(), j.energy);
  TEST_ASSERT_EQUAL(0, s.jobCoin);
  TEST_ASSERT_EQUAL(j.need - 2, r.jobs[0].got);
  TEST_ASSERT_FALSE(r.jobs[0].done);
  // The rest at another Hearth well finishes it; got never passes need.
  WellView w2 = well(500, Tide, Hearth, -45);
  r.hold[j.energy] = 5;
  s = sell(r, atlas, w2, here(), j.energy);
  TEST_ASSERT_EQUAL(j.reward, s.jobCoin);
  TEST_ASSERT_EQUAL(j.need, r.jobs[0].got);
  TEST_ASSERT_TRUE(r.jobs[0].done);
  // The wrong energy or faction adds nothing.
  Job k = Job{Jobs::Deliver, Hearth, (uint8_t)((j.energy + 1) % 3), 7, 0, 4, 0, 0};
  r.jobs[1] = k;
  r.n.jobsDone = 0;
  r.hold[k.energy] = 3;
  sell(r, atlas, well(501, k.energy, Hedge, -45), here(), k.energy);
  TEST_ASSERT_EQUAL(0, r.jobs[1].got);
  // At dawn: the favor coin in its own row.
  r.n.met = 1;
  const NightEnd e = endNight(r, car, 80);
  TEST_ASSERT_EQUAL(j.reward, e.favors);
  TEST_ASSERT_EQUAL(96, (int)sizeof(NightEnd));
}

// Switched off for a night, every settling well settles, but only by what it
// needed: the wells you worked last are still the most recent in the atlas.
static void test_settling_off_keeps_the_atlas_order() {
  atlas.touch(1, 1, 1, 44);    // long ago
  atlas.touch(2, 2, 1, 140);   // settling when the Deck went off
  atlas.touch(3, 3, 1, 159);   // the last well worked, play-minute 159
  TEST_ASSERT_EQUAL_INT(2, settleFor(atlas, 863, 159));   // off 14 h
  playMinNow = 159;
  TEST_ASSERT_TRUE(settled(atlas.find(2)) && settled(atlas.find(3)));
  TEST_ASSERT_EQUAL_UINT32(44, atlas.find(1)->lastPlay);
  TEST_ASSERT_EQUAL_UINT32(139, atlas.find(3)->lastPlay);   // settled just now, not 14 hours ago
  TEST_ASSERT_EQUAL_UINT32(139, atlas.find(2)->lastPlay);   // so did this one: both newer than everything older
  TEST_ASSERT_TRUE(atlas.find(2)->lastPlay > atlas.find(1)->lastPlay);
  // A short nap still only takes off what passed.
  atlas.touch(4, 4, 1, 159);
  TEST_ASSERT_EQUAL_INT(0, settleFor(atlas, 5, 159));
  TEST_ASSERT_EQUAL_UINT32(154, atlas.find(4)->lastPlay);
}

// Mid-week, the career's counters (rank, "PAY 10 TITHES", "FIND 100 DIFFERENT
// WELLS") include the week under way; at the week's end nothing is counted twice.
static void test_counters_read_the_live_week() {
  car.stats[(int)Stat::nights_paid] = 2;
  car.stats[(int)Stat::unique_wells] = 10;
  car.stats[(int)Stat::max_coin] = 30;
  r.stats[(int)Stat::nights_paid] = 6;
  r.stats[(int)Stat::max_coin] = 25;
  atlas.unique = 16;
  TEST_ASSERT_EQUAL_UINT32(2, careerStat(car, Stat::nights_paid));   // no live run set: as saved
  TEST_ASSERT_EQUAL_STRING("LISTENER", rankName(car));
  runNow = &r;
  wellsNow = atlas.unique;
  TEST_ASSERT_EQUAL_UINT32(8, careerStat(car, Stat::nights_paid));
  TEST_ASSERT_EQUAL_UINT32(16, careerStat(car, Stat::unique_wells));
  TEST_ASSERT_EQUAL_UINT32(30, careerStat(car, Stat::max_coin));   // a best: the larger
  TEST_ASSERT_EQUAL_STRING("DOWSER", rankName(car));
  char b[64];
  goalText(car, Stat::nights_paid, 10, "Pay 10 Tithes", b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("PAY 10 TITHES (8/10)", b);
  endRun(r, car, atlas);
  TEST_ASSERT_FALSE(r.active);
  TEST_ASSERT_EQUAL_UINT32(8, careerStat(car, Stat::nights_paid));   // folded in once
  TEST_ASSERT_EQUAL_UINT32(16, careerStat(car, Stat::unique_wells));
  runNow = nullptr;
}

static void test_challenges() {
  newRun(r, car, Calling_wayfarer, 0, 0, 9, 1);   // Shoestring
  r.coin = 40;
  clampCoin(r);
  TEST_ASSERT_EQUAL(15, (int)r.coin);
  newRun(r, car, Calling_wayfarer, 0, 9, 9, 1);   // Redline
  beginNight(r);
  TEST_ASSERT_TRUE(owns(r, R_overclock) && owns(r, R_witching_hour));
  TEST_ASSERT_EQUAL(12, r.n.visits);   // 14, and Overclock takes 2
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 410 * 2.5, r.n.tithe);
  newRun(r, car, Calling_wayfarer, 0, 8, 9, 1);   // Grand tour: the Tithe doubles unless all four wards
  beginNight(r);
  r.n.tithe = 25;
  r.hold[Tide] = 6;   // 6 units at 4.4 to 6.9 each: 26 to 41, enough for 25 but not 50
  Sold s = sell(r, atlas, well(1), here(8, Fringe), Tide);
  TEST_ASSERT_TRUE(s.resonance >= 25 && s.resonance < 50);
  TEST_ASSERT_FALSE(s.closed);
}

static void test_unlocks_are_revealed_two_a_binding() {
  car.stats[(int)Stat::hidden_found] = 40;   // earns Hushed step and Unseen hand
  car.stats[(int)Stat::dowses] = 40;         // earns Dowser's knot and the Diviner
  TEST_ASSERT_EQUAL(2, reveal(car));
  TEST_ASSERT_TRUE(car.nQueue >= 2);
  TEST_ASSERT_EQUAL(2, reveal(car));
}

static void test_winning_records_seals() {
  r.won = 1;
  r.pact = 2;
  addRelic(r, R_hollow_saint);
  addRelic(r, R_quiet_hours);
  endRun(r, car, atlas);
  TEST_ASSERT_EQUAL(1, (int)car.wins);
  TEST_ASSERT_EQUAL(2, car.seals[Calling_wayfarer]);
  TEST_ASSERT_TRUE((car.schoolWins >> (int)School::Hollow) & 1);
  TEST_ASSERT_TRUE(getBit(car.wonWith, R_quiet_hours));
}

static void test_a_finished_job_cant_be_taken_again() {
  Job j{};
  WellView w;
  for (uint32_t id = 1; id < 400 && !(j.kind == Jobs::Wander); id++) { w = well(id, Tide, Warded, -45); j = jobOffer(r, w); }
  TEST_ASSERT_EQUAL(Jobs::Wander, j.kind);
  TEST_ASSERT_TRUE(takeJob(r, j));
  const int wards[3] = {Hollow, Fringe, Nexus};
  for (int k = 0; k < 3; k++) { r.hold[Tide] = 4; sell(r, atlas, well(500 + k), here(8, wards[k]), Tide); }
  TEST_ASSERT_EQUAL(1, (int)r.stats[(int)Stat::jobs_done]);
  TEST_ASSERT_FALSE(takeJob(r, j));                 // not again tonight
  TEST_ASSERT_EQUAL(Jobs::None, jobOffer(r, w).kind);
  const double coin = r.coin;
  r.hold[Tide] = 4;
  sell(r, atlas, well(600), here(8, Fringe), Tide);
  TEST_ASSERT_EQUAL(1, (int)r.stats[(int)Stat::jobs_done]);
  TEST_ASSERT_TRUE(r.coin <= coin + 0.001);   // no second payout
}

static void test_haste_keeps_the_week_alive() {
  r.n.used = r.n.visits;
  r.coin = 0;
  r.spells[r.nSpells++] = Spell_haste;
  TEST_ASSERT_FALSE(stuck(r));
  castSpell(r, atlas, car, hasteSlot(r), nullptr, here());
  TEST_ASSERT_EQUAL(r.n.used + 2, r.n.visits);
  r.n.used = r.n.visits;
  TEST_ASSERT_TRUE(stuck(r) || hasteSlot(r) >= 0);   // HASTE is gone, GLAMOUR doesn't help
}

static void test_grand_tour_loans_never_lower_the_next_tithe() {
  newRun(r, car, Calling_wayfarer, 0, 8, 3, 1);
  r.night = 2;
  beginNight(r);
  r.n.total = r.n.tithe * 1.4;   // over the plain Tithe, short of the doubled one
  r.n.used = r.n.visits;
  TEST_ASSERT_TRUE(canTakeLoan(r));
  takeLoan(r);
  TEST_ASSERT_TRUE(r.owed > 0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, (r.n.tithe * 2 - r.n.tithe * 1.4) * 1.25, r.owed);
}

static void test_hermit_draws_twice_only_at_old_wells() {
  newRun(r, car, Calling_hermit, 0, -1, 4, 1);
  beginNight(r);
  WellView w = well(9, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  TEST_ASSERT_EQUAL((int)Why::Dry, (int)canDraw(r, atlas, w));   // new tonight: once
  r.n.met = 1;
  endNight(r, car, 80);
  beginNight(r);
  draw(r, atlas, w, here(), 1);
  r.hold[Tide] = 0;
  TEST_ASSERT_EQUAL((int)Why::Ok, (int)canDraw(r, atlas, w));    // visited last Night: twice
}

static void test_transmute_lasts_four_sales() {
  r.spells[0] = Spell_transmute;
  r.hold[Gale] = 3;
  castSpell(r, atlas, car, 0, nullptr, here());
  TEST_ASSERT_EQUAL(3, r.hold[r.focus]);
  for (int k = 0; k < 4; k++) {
    Drawn d = draw(r, atlas, well(700 + k, Static, Hedge, -45), here(), 1);
    TEST_ASSERT_EQUAL(r.focus, d.energy);
    sell(r, atlas, well(700 + k, Static, Hedge, -45), here(), r.focus);
  }
  Drawn d = draw(r, atlas, well(800, Static, Hedge, -45), here(), 1);
  TEST_ASSERT_EQUAL(Static, d.energy);
}

static void test_glamour_isnt_wasted_on_a_taken_sale() {
  r.heat = 4;
  r.n.glamour = 1;
  int blocked = 0;
  for (int k = 0; k < 40 && !blocked; k++) {
    r.hold[Tide] = 4;
    r.n.used = 0;
    Sold s = sell(r, atlas, well(900 + k), here(), Tide);
    if (s.blocked) { blocked = 1; TEST_ASSERT_EQUAL(1, r.n.glamour); }
    else { r.n.glamour = 1; }
  }
  TEST_ASSERT_EQUAL(1, blocked);
}

static ClockMark mark(uint32_t id, uint64_t secs) {
  ClockMark m{};
  m.id = id;
  m.channel = 6;
  m.tsf = secs * 1000000ULL;
  return m;
}

void test_routers_say_how_long_the_deck_was_off() {
  ClockNote was{};
  was.n = 3;
  was.m[0] = mark(1, 1000);
  was.m[1] = mark(2, 50000);
  was.m[2] = mark(3, 7000);
  was.playMs = 154 * 60000;
  was.seal();
  TEST_ASSERT_TRUE(was.valid());
  // Eight hours later: two routers agree, the third restarted and reads lower.
  const ClockMark now[4] = {mark(9, 5), mark(2, 50000 + 8 * 3600), mark(1, 1000 + 8 * 3600 + 1), mark(3, 120)};
  const int64_t secs = secondsSince(was, now, 4);
  TEST_ASSERT_TRUE(secs >= 8 * 3600 && secs <= 8 * 3600 + 1);
  TEST_ASSERT_EQUAL_UINT32(480, offMinutes(was, 154 * 60000, secs));
  // A save from two minutes after the note: those minutes were already played.
  TEST_ASSERT_EQUAL_UINT32(478, offMinutes(was, 156 * 60000, secs));
  // No router in common: nothing is known.
  const ClockMark away[1] = {mark(42, 99999)};
  TEST_ASSERT_EQUAL_INT64(-1, secondsSince(was, away, 1));
  TEST_ASSERT_EQUAL_UINT32(0, offMinutes(was, 154 * 60000, -1));
  // One odd router can't swing the median.
  const ClockMark odd[3] = {mark(1, 1000 + 600), mark(2, 50000 + 610), mark(3, 7000 + 900000)};
  TEST_ASSERT_TRUE(secondsSince(was, odd, 3) <= 610);
  was.playMs++;
  TEST_ASSERT_FALSE(was.valid());   // a damaged note is ignored
}

void test_wells_settle_while_the_deck_is_off() {
  Atlas atlas;
  atlas.clear();
  atlas.touch(1, 1, 1, 154);   // drawn just before switching off
  atlas.touch(2, 2, 1, 100);   // long settled
  atlas.touch(3, 3, 1, 150);
  TEST_ASSERT_EQUAL_INT(2, settleFor(atlas, 480, 154));
  playMinNow = 154;
  TEST_ASSERT_TRUE(settled(atlas.find(1)));
  TEST_ASSERT_TRUE(settled(atlas.find(3)));
  TEST_ASSERT_EQUAL_UINT32(100, atlas.find(2)->lastPlay);   // left alone
  // A short nap only takes off what passed.
  atlas.touch(4, 4, 1, 154);
  TEST_ASSERT_EQUAL_INT(0, settleFor(atlas, 5, 154));
  TEST_ASSERT_EQUAL_INT(15, settleLeft(atlas.find(4)));
}

// Lead: paying the Tithe earns no coin at dawn; trade, spare visits and interest still do.
static void test_lead_pays_nothing_for_the_tithe() {
  newRun(r, car, Calling_wayfarer, 1, -1, 1234, 1);
  beginNight(r);
  TEST_ASSERT_EQUAL(12, r.n.visits);   // Lead takes no visit
  r.n.tithe = 10;
  r.coin = 12;
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  const NightEnd e = endNight(r, car, 80);
  TEST_ASSERT_TRUE(e.paid);
  TEST_ASSERT_EQUAL(0, e.reward);
  TEST_ASSERT_EQUAL(11, e.spareCoin);
  TEST_ASSERT_TRUE(e.trade > 0);
  TEST_ASSERT_TRUE(e.interest >= 2);
  // The Tithe curve is the plain one: Salt's growth is gone from every rung.
  newRun(r, car, Calling_wayfarer, 7, -1, 1, 1);
  TEST_ASSERT_EQUAL_DOUBLE(Tithes[6], titheFor(r, 6, -1, -1));
  rules.saltRung = 1;   // the bot can still measure the old rung
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, Tithes[6] * 1.3, titheFor(r, 6, -1, -1));
  rules = Rules{};
}

// Each rule turns on at its rung and stays on above it.
static void test_each_rung_keeps_its_rule() {
  newRun(r, car, Calling_wayfarer, 2, -1, 1, 1);
  beginNight(r);
  TEST_ASSERT_EQUAL(11, r.n.visits);              // Iron
  TEST_ASSERT_TRUE(pactOn(r, PR_lead));            // and Lead stays on
  TEST_ASSERT_FALSE(pactOn(r, PR_ash));
  r.pact = 3;
  r.hold[Tide] = 10;
  Sale c;
  saleContext(c, r, atlas, well(3), here(), Tide);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 9, c.units);   // Ash
  r.pact = 5;
  TEST_ASSERT_EQUAL(12, polishPrice(r, OwnedRelic{R_crowd_feeder, -1, -1, 0}));   // Silver
  r.pact = 6;
  TEST_ASSERT_EQUAL(8, candlePrice(r));           // Obsidian
  r.pact = 7;
  TEST_ASSERT_EQUAL(4, relicSlots(r));            // Gold
}

// Below the fourth rung each Portent is one of two, kept later; the rungs above get theirs at once.
static void test_a_portent_can_be_one_of_two() {
  r.n.tithe = 10;
  r.coin = 12;
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  PortentChoice ch;
  NightEnd e = endNight(r, car, 80, &ch);
  TEST_ASSERT_EQUAL(3, e.portents);
  TEST_ASSERT_EQUAL(7, ch.pending);
  for (int k = 0; k < 3; k++) {
    TEST_ASSERT_TRUE(ch.alt[k] >= 1 && ch.alt[k] <= PortentCount);
    TEST_ASSERT_NOT_EQUAL(e.got[k].portent, ch.alt[k] - 1);   // two different ones
    TEST_ASSERT_EQUAL(-1, e.got[k].relic);
  }
  TEST_ASSERT_EQUAL(0, (int)r.stats[(int)Stat::portents]);   // nothing given yet
  TEST_ASSERT_EQUAL(0, (int)(r.shopFlags & (SF_RareSight | SF_FreeRerolls | SF_Shine | SF_DoubleInterest)));
  const int kept = ch.alt[0] - 1;
  const PortentResult got = choosePortent(r, car, e, ch, 0, true);
  TEST_ASSERT_EQUAL(kept, got.portent);
  TEST_ASSERT_EQUAL(kept, e.got[0].portent);
  TEST_ASSERT_EQUAL(6, ch.pending);
  const int first = e.got[1].portent;
  choosePortent(r, car, e, ch, 1, false);
  TEST_ASSERT_EQUAL(first, e.got[1].portent);
  TEST_ASSERT_EQUAL(4, ch.pending);
  choosePortent(r, car, e, ch, 1, true);   // settled: nothing changes
  TEST_ASSERT_EQUAL(first, e.got[1].portent);
  TEST_ASSERT_EQUAL(4, ch.pending);
  TEST_ASSERT_EQUAL(2, (int)r.stats[(int)Stat::portents]);
  // From the fourth rung (Ash) the Portent is given at once, as before.
  newRun(r, car, Calling_wayfarer, 3, -1, 1234, 1);
  beginNight(r);
  atlas.clear();
  r.n.tithe = 10;
  r.coin = 12;
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  e = endNight(r, car, 80, &ch);
  TEST_ASSERT_EQUAL(2, e.portents);   // Iron: 11 visits, 10 to spare
  TEST_ASSERT_EQUAL(0, ch.pending);
  TEST_ASSERT_EQUAL(2, (int)r.stats[(int)Stat::portents]);
}

// A Doubled interest kept at the dawn screen pays that Night's interest on the spot.
static void test_doubled_interest_kept_late_pays_now() {
  r.n.tithe = 10;
  r.coin = 12;
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide);
  PortentChoice ch;
  NightEnd e = endNight(r, car, 80, &ch);
  TEST_ASSERT_EQUAL(2, ch.interest);   // 12 coin held: 2
  ch.alt[0] = (uint8_t)(Portent_doubled_interest + 1);
  const double before = r.coin;
  choosePortent(r, car, e, ch, 0, true);
  TEST_ASSERT_EQUAL_DOUBLE(before + 2, r.coin);
  TEST_ASSERT_EQUAL(0, (int)(r.shopFlags & SF_DoubleInterest));
  ch.alt[1] = (uint8_t)(Portent_windfall + 1);
  choosePortent(r, car, e, ch, 1, true);
  TEST_ASSERT_EQUAL_DOUBLE(before + 12, r.coin);
}

// Every Pact past Chalk has a Trial that teaches it, each a different one.
static void test_each_pact_has_its_trial() {
  TEST_ASSERT_EQUAL(-1, trialForPact(0));
  TEST_ASSERT_EQUAL_STRING("Shoestring", Challenges[trialForPact(1)].name);   // Lead
  TEST_ASSERT_EQUAL_STRING("Night owl", Challenges[trialForPact(2)].name);    // Iron
  TEST_ASSERT_EQUAL_STRING("Homebody", Challenges[trialForPact(3)].name);     // Ash
  TEST_ASSERT_EQUAL_STRING("Empty pockets", Challenges[trialForPact(4)].name);   // Bone
  TEST_ASSERT_EQUAL_STRING("One note", Challenges[trialForPact(5)].name);     // Silver
  TEST_ASSERT_EQUAL_STRING("Glass house", Challenges[trialForPact(6)].name);  // Obsidian
  TEST_ASSERT_EQUAL_STRING("Redline", Challenges[trialForPact(7)].name);      // Gold
  for (int p = 1; p < PactCount; p++)
    for (int q = p + 1; q < PactCount; q++) TEST_ASSERT_NOT_EQUAL(trialForPact(p), trialForPact(q));
  TEST_ASSERT_EQUAL(-1, trialForPact(PactCount));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lead_pays_nothing_for_the_tithe);
  RUN_TEST(test_each_rung_keeps_its_rule);
  RUN_TEST(test_a_portent_can_be_one_of_two);
  RUN_TEST(test_doubled_interest_kept_late_pays_now);
  RUN_TEST(test_each_pact_has_its_trial);
  RUN_TEST(test_a_new_run);
  RUN_TEST(test_callings_and_pacts_change_the_start);
  RUN_TEST(test_drawing_takes_more_closer);
  RUN_TEST(test_a_shaky_hand_spills);
  RUN_TEST(test_the_hold_has_a_limit);
  RUN_TEST(test_selling_pays_and_uses_a_visit);
  RUN_TEST(test_a_well_settles);
  RUN_TEST(test_a_settled_well_gives_one_draw);
  RUN_TEST(test_heat_stays_in_tenths);
  RUN_TEST(test_the_warded_refuse_static_unless_fenced);
  RUN_TEST(test_paying_the_tithe_closes_the_night);
  RUN_TEST(test_night_end_pays_out);
  RUN_TEST(test_candles_then_a_loan);
  RUN_TEST(test_stuck_means_the_binding_is_lost);
  RUN_TEST(test_revisits_and_nights_seen);
  RUN_TEST(test_wardens_walk_starts_the_night_at_heat_3);
  RUN_TEST(test_doubled_interest_pays_tonight);
  RUN_TEST(test_static_raises_heat_and_a_ward_holds_the_wardens);
  RUN_TEST(test_the_first_shop_has_your_caller);
  RUN_TEST(test_buying_with_full_slots_needs_a_sale);
  RUN_TEST(test_polish_tiers_and_prices);
  RUN_TEST(test_rerolls_get_dearer);
  RUN_TEST(test_mercy_helps_the_next_shop);
  RUN_TEST(test_spells);
  RUN_TEST(test_jobs_pay_and_raise_standing);
  RUN_TEST(test_deliveries_add_up_and_show_at_dawn);
  RUN_TEST(test_settling_off_keeps_the_atlas_order);
  RUN_TEST(test_counters_read_the_live_week);
  RUN_TEST(test_challenges);
  RUN_TEST(test_unlocks_are_revealed_two_a_binding);
  RUN_TEST(test_winning_records_seals);
  RUN_TEST(test_a_finished_job_cant_be_taken_again);
  RUN_TEST(test_haste_keeps_the_week_alive);
  RUN_TEST(test_grand_tour_loans_never_lower_the_next_tithe);
  RUN_TEST(test_hermit_draws_twice_only_at_old_wells);
  RUN_TEST(test_transmute_lasts_four_sales);
  RUN_TEST(test_glamour_isnt_wasted_on_a_taken_sale);
  RUN_TEST(test_overstock_grows_the_stall_youre_at);
  RUN_TEST(test_a_full_spell_pocket_can_swap);
  RUN_TEST(test_routers_say_how_long_the_deck_was_off);
  RUN_TEST(test_wells_settle_while_the_deck_is_off);
  return UNITY_END();
}
