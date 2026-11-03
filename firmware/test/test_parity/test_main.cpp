// The Deck's sale rules against the balance simulator's, on thousands of
// random sales from tools/gen_parity.py: pio test -e native -f test_parity
#include <unity.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game/Rules.h"
#include "vectors.h"

using namespace Game;

void setUp() {}
void tearDown() {}

static int growingId(int i) {
  for (int k = 0; k < Cat::RelicCount; k++) if (strcmp(Cat::Relics[k].key, GrowingKeys[i]) == 0) return k;
  return -1;
}

static bool close(double a, double b) { return fabs(a - b) <= 1e-9 * (1 + fabs(a) + fabs(b)); }

static void test_every_sale_matches_the_simulator() {
  const int count = sizeof(Vectors) / sizeof(Vectors[0]);
  int bad = 0;
  static Run r;
  for (int i = 0; i < count; i++) {
    const V& v = Vectors[i];
    memset(&r, 0, sizeof(r));
    r.nRelics = v.nr;
    for (int k = 0; k < v.nr; k++) r.relics[k] = OwnedRelic{(uint8_t)v.rel[k], (int8_t)v.ed[k], -1, (uint8_t)v.tier[k]};
    r.calling = v.calling;
    r.n.veil = v.veil;
    if (v.pickpocket) { r.trinkets[0] = Cat::Trinket_pickpocket; r.nTrinkets = 1; }
    for (int g = 0; g < GrowingCount; g++) r.state[growingId(g)] = v.state[g];
    r.standing[Hearth] = v.hearth;
    r.n.newTonight = (uint8_t)v.newTonight;
    r.n.hollows = v.hollows;
    r.newWells = v.newWells;
    r.n.kinds = v.kinds;
    r.heat = v.heat;
    r.night = v.night;
    r.sales = v.sales;
    r.coin = v.coin;
    r.n.crownBan = v.crownBan;
    r.n.omen = v.omen;
    r.n.eclipse = v.eclipse;
    r.n.fadeE = v.fadeE;
    r.n.noPolish = v.eclipse == Cat::Eclipse_sleep;
    Sale c;
    clearSale(c);
    c.e = v.e; c.units = v.units; c.price = v.price; c.mult = v.mult;
    c.n = v.n; c.ward = v.ward; c.faction = v.faction; c.hidden = v.hidden; c.channel = v.channel;
    c.ch11 = v.ch11; c.strong = v.strong; c.mesh = v.mesh; c.revisit = v.revisit; c.nightsSeen = v.nightsSeen;
    c.isNew = v.isNew; c.wardChanged = v.wardChanged; c.wardsTonight = v.wardsTonight; c.glyph = v.glyph;
    c.noise = v.noise; c.still = v.still; c.batteryLow = v.batteryLow; c.dowsed = v.dowsed; c.oneKind = v.oneKind;
    c.holdKinds = v.holdKinds; c.lvl = v.lvl; c.vi = v.vi; c.lastE = v.lastE; c.ghost = v.ghost;
    const SaleResult got = score(c, r);
    if (!close(got.resonance, v.res) || !close(got.coin, v.coin_)) {
      if (bad < 8) {
        printf("sale %d: deck %.6f / %.6f, simulator %.6f / %.6f, relics", i, got.resonance, got.coin, v.res, v.coin_);
        for (int k = 0; k < v.nr; k++) printf(" %s(ed%d,t%d)", Cat::Relics[v.rel[k]].key, v.ed[k], v.tier[k]);
        printf(" omen %d eclipse %d\n", v.omen, v.eclipse);
      }
      bad++;
    }
  }
  printf("%d of %d sales match\n", count - bad, count);
  TEST_ASSERT_EQUAL(0, bad);
}

static void test_every_relic_is_covered() {
  bool seen[Cat::RelicCount] = {false};
  for (const V& v : Vectors) for (int k = 0; k < v.nr; k++) seen[v.rel[k]] = true;
  for (int i = 0; i < Cat::RelicCount; i++) TEST_ASSERT_TRUE_MESSAGE(seen[i], Cat::Relics[i].key);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_relic_is_covered);
  RUN_TEST(test_every_sale_matches_the_simulator);
  return UNITY_END();
}
