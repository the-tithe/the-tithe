// The Night-sized numbers the screens show: offerings to go, the Portent
// notches, the stall's star priced in offerings, the Omen the stall can see,
// the atlas's estimates, and ATTUNED as the show's first Amp step.
//   pio test -e native -f test_estimates
#include <unity.h>
#include <string.h>
#include "game/Game.h"

using namespace Game;
using namespace Cat;

static Career car;
static Run r;
static Atlas atlas;
static Shop shop;
static RecentSales mem;

static WellView well(uint32_t id, int energy = Tide, int faction = Hearth, float sig = -50) {
  WellView w{};
  w.id = id;
  w.seed = id * 2654435761u;
  w.energy = (uint8_t)energy;
  w.faction = (uint8_t)faction;
  w.channel = 6;
  w.sig = sig;
  return w;
}
static Here here(int n = 8, int ward = Fringe) {
  Here h{};
  h.n = n;
  h.ward = ward;
  h.noise = 0.5;
  h.battery = 80;
  return h;
}

void setUp() {
  playMinNow = 0;
  newCareer(car);
  atlas.clear();
  memset(&mem, 0, sizeof(mem));
  memset(&shop, 0, sizeof(shop));
  newRun(r, car, Calling_wayfarer, 0, -1, 1234, 1);
  beginNight(r);
}
void tearDown() {}

// ---- Tonight ------------------------------------------------------------------------

static void test_offerings_to_go_rounds_up() {
  TEST_ASSERT_EQUAL(3, offeringsToGo(1000, 400, 250));   // 600 / 250 = 2.4
  TEST_ASSERT_EQUAL(2, offeringsToGo(1000, 400, 300));
  TEST_ASSERT_EQUAL(1, offeringsToGo(1000, 999, 5000));
  TEST_ASSERT_EQUAL(0, offeringsToGo(1000, 1000, 250));   // paid
  TEST_ASSERT_EQUAL(0, offeringsToGo(1000, 400, 0));      // nothing known
  TEST_ASSERT_EQUAL(99, offeringsToGo(1e6, 0, 1));         // never more than two digits
}

static void test_notches_are_the_pace_for_each_portent() {
  TEST_ASSERT_EQUAL_DOUBLE(0, paceNotch(0, 12, 6));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0 / 6, paceNotch(1, 12, 6));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0 / 3, paceNotch(1, 12, 9));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, paceNotch(1, 12, 11));
  TEST_ASSERT_TRUE(paceNotch(2, 12, 11) > 1);   // 11 to spare is gone after the second visit
  TEST_ASSERT_TRUE(paceNotch(1, 8, 9) > 1);     // more spare than visits: never
  double f[3];
  TEST_ASSERT_EQUAL(0, portentNotches(r, f));   // nothing before the first offering
  r.n.used = 1;
  TEST_ASSERT_EQUAL(3, portentNotches(r, f));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0 / 6, f[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, f[2]);
  r.n.used = 2;
  TEST_ASSERT_EQUAL(2, portentNotches(r, f));
  // At the close, the notches still on the bar are exactly the Portents earned.
  r.n.used = 12 - 7;   // 7 to spare: the 6 Portent, not the 9
  TEST_ASSERT_EQUAL(1, portentNotches(r, f));
  TEST_ASSERT_TRUE(f[0] <= 1);
  // The Night owl's Portents come 2 sooner.
  newRun(r, car, Calling_wayfarer, 0, 2, 7, 1);
  beginNight(r);
  TEST_ASSERT_EQUAL(8, r.n.visits);
  r.n.used = 1;
  TEST_ASSERT_EQUAL(2, portentNotches(r, f));   // 4 and 7 to spare fit in 8 visits; 9 never
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.25, f[0]);
}

static void test_more_tonight_uses_reach_then_the_average() {
  TEST_ASSERT_EQUAL(0, moreTonight(r, 100));   // nothing before the first offering
  r.n.tithe = 1000;
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(), 1);
  sell(r, atlas, w, here(), Tide, false, &mem);
  TEST_ASSERT_EQUAL(1, r.n.used);
  const double left = 1000 - r.n.total;
  TEST_ASSERT_EQUAL((int)ceil(left / 100 - 1e-9), moreTonight(r, 100));             // the star's number
  TEST_ASSERT_EQUAL((int)ceil(left / r.n.total - 1e-9), moreTonight(r, 0));         // none in reach: tonight's average
  r.n.total = 1000;
  r.n.met = 1;
  TEST_ASSERT_EQUAL(0, moreTonight(r, 100));   // paid
}

static void test_a_quiet_deck_says_nothing() {
  newRun(r, car, Calling_wayfarer, rules.quietRung, -1, 5, 1);
  beginNight(r);
  TEST_ASSERT_TRUE(deckQuiet(r));
  r.n.tithe = 1000;
  r.n.used = 3;
  r.n.total = 200;
  TEST_ASSERT_EQUAL(0, moreTonight(r, 100));
  TEST_ASSERT_EQUAL(-1, stallOmen(r));
  newRun(r, car, Calling_wayfarer, rules.quietRung - 1, -1, 5, 1);
  beginNight(r);
  TEST_ASSERT_FALSE(deckQuiet(r));
  r.n.tithe = 1000;
  r.n.used = 3;
  r.n.total = 200;
  TEST_ASSERT_EQUAL(8, moreTonight(r, 100));
}

// ---- The stall --------------------------------------------------------------------

static void test_fewer_offerings_and_its_words() {
  // At 1,000 an offering a 4,700 Tithe is 4.7 offerings; half again makes it 3.13.
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 1.57, fewerOfferings(0.5, 4700, 1000));
  TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.43, fewerOfferings(0.1, 4700, 1000));
  TEST_ASSERT_EQUAL_DOUBLE(0, fewerOfferings(0, 4700, 1000));
  TEST_ASSERT_EQUAL_DOUBLE(0, fewerOfferings(0.5, 4700, 0));
  char b[40];
  TEST_ASSERT_EQUAL_STRING("~1.5 FEWER OFFERINGS A NIGHT", fewerText(1.57, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("~0.4 FEWER OFFERINGS A NIGHT", fewerText(0.43, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("~1 FEWER OFFERING A NIGHT", fewerText(1.0, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("~1 FEWER OFFERING A NIGHT", fewerText(1.2, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("~2 FEWER OFFERINGS A NIGHT", fewerText(2.2, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("~0.1 FEWER OFFERINGS A NIGHT", fewerText(0.01, b, sizeof(b)));
  TEST_ASSERT_TRUE(strlen(fewerText(12.3, b, sizeof(b))) <= 40);
}

static void test_the_star_ranges_over_polish() {
  r.night = 3;
  for (int i = 0; i < 6; i++) {   // a few sales, so the star has something to judge by
    WellView w = well(10 + i, Tide, Hearth, -45);
    r.hold[Tide] = 6;
    sell(r, atlas, w, here(), Tide, false, &mem);
  }
  addRelic(r, R_tide_caller);
  addRelic(r, R_leyline);
  shop.nRelics = 0;
  shop.night = 2;
  starBest(shop, r, mem);
  TEST_ASSERT_EQUAL(-1, shop.star);           // an empty shelf
  TEST_ASSERT_TRUE(shop.starPolish >= 1);     // ...so a Polish is the star
  TEST_ASSERT_TRUE(starGain(shop, r, mem) > 0.05);
  TEST_ASSERT_TRUE(starFewer(shop, r, mem) > 0);
  // A relic that beats every Polish takes the star back from YOURS.
  shop.relics[shop.nRelics++] = relicOffer(R_blood_moon, -1, -1);
  starBest(shop, r, mem);
  TEST_ASSERT_EQUAL(0, shop.star);
  TEST_ASSERT_EQUAL(0, shop.starPolish);
  // Relics at tier III, or with no effect to polish, are never starred.
  shop.nRelics = 0;
  r.relics[0].tier = r.relics[1].tier = 3;
  starBest(shop, r, mem);
  TEST_ASSERT_EQUAL(0, shop.starPolish);
}

static void test_a_quiet_deck_has_no_star() {
  newRun(r, car, Calling_wayfarer, rules.quietRung, -1, 5, 1);
  beginNight(r);
  addRelic(r, R_leyline);
  shop.nRelics = 0;
  shop.relics[shop.nRelics++] = relicOffer(R_blood_moon, -1, -1);
  shop.night = 2;
  starBest(shop, r, mem);
  TEST_ASSERT_EQUAL(-1, shop.star);
  TEST_ASSERT_EQUAL(0, shop.starPolish);
  TEST_ASSERT_EQUAL_DOUBLE(0, starGain(shop, r, mem));
}

static void test_the_stall_sees_the_night_ahead() {
  TEST_ASSERT_EQUAL(-1, omenOf(r, 0));          // Night 1 never has one
  TEST_ASSERT_EQUAL(-1, omenOf(r, Nights - 1)); // the seventh has the Eclipse
  for (int n = 1; n < Nights - 1; n++) TEST_ASSERT_EQUAL((int)r.omens[n] - 1, omenOf(r, n));
  r.night = 1;   // the stall after Night 1
  TEST_ASSERT_EQUAL((int)r.omens[1] - 1, stallOmen(r));
  r.omens[1] = (uint8_t)(Omen_blood_price + 1);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, Tithes[1] * 1.2, titheAhead(r));   // the Tithe with its Omen
  r.omens[1] = (uint8_t)(Omen_fog + 1);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, Tithes[1], titheAhead(r));
  r.night = Nights - 1;
  TEST_ASSERT_EQUAL(-1, stallOmen(r));
  TEST_ASSERT_FALSE(seesAhead(r));
  newRun(r, car, Calling_diviner, 0, -1, 5, 1);
  TEST_ASSERT_TRUE(seesAhead(r));
}

// ---- The atlas ---------------------------------------------------------------------

static void test_the_atlas_remembers_the_ward() {
  WellView w = well(1, Tide, Hearth, -45);
  draw(r, atlas, w, here(8, Hollow), 1);
  TEST_ASSERT_EQUAL(Hollow + 1, atlas.find(1)->ward);
  sell(r, atlas, w, here(20, Nexus), Tide, false, &mem);
  TEST_ASSERT_EQUAL(Nexus + 1, atlas.find(1)->ward);
}

static void test_atlas_estimates_score_the_hold_at_each_well() {
  WellView a = well(1, Tide, Hearth, -45), b = well(2, Gale, Hedge, -45);
  r.hold[Tide] = 5;
  sell(r, atlas, a, here(2, Hollow), Tide, false, &mem);
  r.hold[Gale] = 5;
  sell(r, atlas, b, here(20, Nexus), Gale, false, &mem);
  r.night = 1;   // the next Night, both wells open again
  beginNight(r);
  r.hold[Tide] = 7;
  r.hold[Ember] = 2;
  float est[Atlas::Max];
  atlasEstimates(r, atlas, 0, est);
  // The biggest stack (7 Tide) at each well, in the ward it was last seen in.
  for (int i = 0; i < 2; i++) {
    const int ward = i == 0 ? Hollow : Nexus;
    Here h = here(wardCrowd(ward), ward);
    WellView v = i == 0 ? a : b;
    v.sig = -60;
    Sale s;
    saleContext(s, r, atlas, v, h, Tide);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)score(s, r).resonance, est[i]);
    TEST_ASSERT_TRUE(est[i] > 0);
  }
  TEST_ASSERT_TRUE(est[0] != est[1]);   // the ward and the well's taste tell them apart
  // An empty hold: a typical draw of your focus.
  memset(r.hold, 0, sizeof(r.hold));
  atlasEstimates(r, atlas, 0, est);
  TEST_ASSERT_TRUE(est[0] > 0);
  TEST_ASSERT_EQUAL(0, r.hold[r.focus]);   // the real hold is untouched
  // The Warded won't take Static.
  WellView c = well(3, Static, Warded, -45);
  r.hold[Static] = 5;
  r.trinkets[r.nTrinkets++] = Trinket_fence;
  sell(r, atlas, c, here(), Static, false, &mem);
  r.nTrinkets = 0;
  r.hold[Static] = 5;
  atlasEstimates(r, atlas, 0, est);
  TEST_ASSERT_EQUAL_FLOAT(0, est[2]);
}

// ---- The show -----------------------------------------------------------------------

static void test_attunement_is_the_shows_first_amp_step() {
  WellView w = well(1, Tide, Hearth, -45);
  r.hold[Tide] = 6;
  r.attune[Tide] = 2;
  Sale s;
  Trace t;
  saleContext(s, r, atlas, w, here(), Tide);
  TEST_ASSERT_EQUAL_DOUBLE(3, s.mult);
  const double res = score(s, r, &t).resonance;
  TEST_ASSERT_EQUAL_DOUBLE(1, t.mult0);
  TEST_ASSERT_TRUE(t.n >= 1);
  TEST_ASSERT_EQUAL(Trace::FromAttune, t.s[0].source);
  TEST_ASSERT_EQUAL(Trace::Mult, t.s[0].field);
  TEST_ASSERT_EQUAL_DOUBLE(3, t.s[0].after);
  // The same number as the untraced score.
  Sale s2;
  saleContext(s2, r, atlas, w, here(), Tide);
  TEST_ASSERT_EQUAL_DOUBLE(res, score(s2, r).resonance);
  // Level 0: no such step, and the base is the plain Amp.
  r.attune[Tide] = 0;
  Sale s3;
  Trace t3;
  saleContext(s3, r, atlas, w, here(), Tide);
  score(s3, r, &t3);
  TEST_ASSERT_EQUAL_DOUBLE(1, t3.mult0);
  for (int i = 0; i < t3.n; i++) TEST_ASSERT_NOT_EQUAL(Trace::FromAttune, t3.s[i].source);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_offerings_to_go_rounds_up);
  RUN_TEST(test_notches_are_the_pace_for_each_portent);
  RUN_TEST(test_more_tonight_uses_reach_then_the_average);
  RUN_TEST(test_a_quiet_deck_says_nothing);
  RUN_TEST(test_fewer_offerings_and_its_words);
  RUN_TEST(test_the_star_ranges_over_polish);
  RUN_TEST(test_a_quiet_deck_has_no_star);
  RUN_TEST(test_the_stall_sees_the_night_ahead);
  RUN_TEST(test_the_atlas_remembers_the_ward);
  RUN_TEST(test_atlas_estimates_score_the_hold_at_each_well);
  RUN_TEST(test_attunement_is_the_shows_first_amp_step);
  return UNITY_END();
}
