// Logic tests that run on a computer: pio test -e native
#include <unity.h>
#include <string.h>
#include <stdlib.h>
#include "SipHash.h"
#include "Well.h"
#include "Sightings.h"
#include "WalkFormat.h"
#include "Tune.h"
#include "Strays.h"

static Sightings sky;

void setUp() {
  for (int i = 0; i < 16; i++) Keys::device[i] = (uint8_t)(0xA0 + i);
  sky.reset(0);
}
void tearDown() {}

static Net net(uint8_t b0, uint8_t b5, int8_t rssi, uint8_t ch, Lock lock, bool hidden = false,
               uint8_t b4 = 0x44) {
  Net n{};
  const uint8_t bssid[6] = {b0, 0x11, 0x22, 0x33, b4, b5};
  memcpy(n.bssid, bssid, 6);
  n.rssi = rssi;
  n.channel = ch;
  n.lock = lock;
  n.hidden = hidden;
  n.nameKey = hidden ? 0 : 0xC0FFEE00u + b5;
  return n;
}

static void test_siphash_reference_vectors() {
  uint8_t key[16], msg[15];
  for (int i = 0; i < 16; i++) key[i] = (uint8_t)i;
  for (int i = 0; i < 15; i++) msg[i] = (uint8_t)i;
  TEST_ASSERT_EQUAL_HEX64(0x726fdb47dd0e0e31ULL, SipHash::hash(key, msg, 0));
  TEST_ASSERT_EQUAL_HEX64(0xa129ca6149be45e5ULL, SipHash::hash(key, msg, 15));
}

static void test_every_name_fits_16_chars() {
  char name[17];
  for (int f = 0; f < 4; f++)
    for (uint32_t s = 0; s < 2048; s++) {
      WellRules::nameOf(s, (Faction)f, name);
      TEST_ASSERT_TRUE(strlen(name) <= 16);
      TEST_ASSERT_TRUE(strlen(name) >= 5);
    }
}

static void test_networks_of_one_router_fold_into_one_well() {
  // Home network, guest network (locally administered first byte) and a
  // third network on the same box: one well. Three networks at once is a
  // business box, so Syndicate; energy from channel 6.
  Net n[3] = {net(0x10, 0x40, -60, 6, Lock::Wpa2), net(0x12, 0x41, -62, 6, Lock::Open),
              net(0x16, 0x42, -61, 6, Lock::Wpa2)};
  sky.ingest(n, 3, 1000);
  TEST_ASSERT_EQUAL(1, sky.near());
  const WellRec* list[8];
  TEST_ASSERT_EQUAL(1, sky.listed(list, 8));
  TEST_ASSERT_EQUAL(3, list[0]->nets);
  TEST_ASSERT_EQUAL((int)Faction::Syndicate, (int)list[0]->faction);
  TEST_ASSERT_EQUAL((int)Energy::Tide, (int)list[0]->energy);
  TEST_ASSERT_TRUE(list[0]->anyOpen);
}

static void test_energy_rules() {
  Net n[5] = {net(0x10, 0x10, -50, 1, Lock::Wpa2, false, 0x01), net(0x10, 0x10, -50, 11, Lock::Wpa3, false, 0x02),
              net(0x10, 0x10, -50, 6, Lock::Open, false, 0x03), net(0x10, 0x10, -50, 3, Lock::Wpa2, true, 0x04),
              net(0x10, 0x10, -50, 9, Lock::Enterprise, false, 0x05)};
  sky.ingest(n, 5, 1000);
  TEST_ASSERT_EQUAL(5, sky.near());
  TEST_ASSERT_EQUAL(1, sky.energyNear(Energy::Ember));
  TEST_ASSERT_EQUAL(1, sky.energyNear(Energy::Gale));
  TEST_ASSERT_EQUAL(1, sky.energyNear(Energy::Static));
  TEST_ASSERT_EQUAL(1, sky.energyNear(Energy::Hush));
  TEST_ASSERT_EQUAL(1, sky.energyNear(Energy::Tide));
}

static void test_ward_thresholds() {
  TEST_ASSERT_EQUAL((int)WardKind::Hollow, (int)WellRules::wardOf(0));
  TEST_ASSERT_EQUAL((int)WardKind::Hollow, (int)WellRules::wardOf(3));
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)WellRules::wardOf(4));
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)WellRules::wardOf(10));
  TEST_ASSERT_EQUAL((int)WardKind::Ward, (int)WellRules::wardOf(11));
  TEST_ASSERT_EQUAL((int)WardKind::Ward, (int)WellRules::wardOf(25));
  TEST_ASSERT_EQUAL((int)WardKind::Nexus, (int)WellRules::wardOf(26));
}

static void test_missed_scans_dim_then_drop() {
  Net a = net(0x10, 0x10, -60, 6, Lock::Wpa2, false, 0x01);
  Net b = net(0x10, 0x10, -70, 1, Lock::Wpa2, false, 0x02);
  Net both[2] = {a, b};
  sky.ingest(both, 2, 0);
  const WellRec* list[8];
  for (int miss = 1; miss <= 2; miss++) {
    sky.ingest(&a, 1, miss * 2500);
    TEST_ASSERT_EQUAL(2, sky.listed(list, 8));
  }
  TEST_ASSERT_EQUAL(1, sky.near());   // b missed 2 scans: no longer counts for the ward
  sky.ingest(&a, 1, 7500);
  TEST_ASSERT_EQUAL(1, sky.listed(list, 8));
}

static void test_one_missed_scan_keeps_the_ward() {
  Net n[4];
  for (int i = 0; i < 4; i++) n[i] = net(0x10, 0x10, -60, 6, Lock::Wpa2, false, (uint8_t)i);
  sky.ingest(n, 4, 0);
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)sky.ward());
  sky.ingest(n, 3, 2500);
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)sky.ward());
}

static void test_faint_wells_dont_set_the_ward() {
  // Eleven wells heard, but seven of them faint: a Fringe, not a Quarter.
  Net n[11];
  for (int i = 0; i < 11; i++) n[i] = net(0x10, 0x10, i < 4 ? -60 : -85, 6, Lock::Wpa2, false, (uint8_t)i);
  sky.ingest(n, 11, 0);
  TEST_ASSERT_EQUAL(4, sky.near());
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)sky.ward());
  const WellRec* list[16];
  TEST_ASSERT_EQUAL(11, sky.listed(list, 16));   // still listed, faint
}

static void test_the_ward_moves_once_two_scans_agree() {
  Net n[12];
  for (int i = 0; i < 12; i++) n[i] = net(0x10, 0x10, -60, 6, Lock::Wpa2, false, (uint8_t)i);
  sky.ingest(n, 4, 0);
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)sky.ward());   // the first reading is taken at once
  sky.ingest(n, 12, 2500);
  TEST_ASSERT_EQUAL(12, sky.near());
  TEST_ASSERT_EQUAL((int)WardKind::Fringe, (int)sky.ward());   // one busy scan: not yet
  sky.ingest(n, 12, 5000);
  TEST_ASSERT_EQUAL((int)WardKind::Ward, (int)sky.ward());     // two in a row: a Quarter
  sky.ingest(n, 4, 7500);
  sky.ingest(n, 12, 10000);
  TEST_ASSERT_EQUAL((int)WardKind::Ward, (int)sky.ward());     // a flicker doesn't flip it
  TEST_ASSERT_EQUAL(4u, sky.wardScans(WardKind::Ward) + sky.wardScans(WardKind::Fringe) - 1);
}

static void test_smoothing_moves_toward_new_signal() {
  Net a = net(0x10, 0x10, -80, 6, Lock::Wpa2);
  sky.ingest(&a, 1, 0);
  a.rssi = -50;
  sky.ingest(&a, 1, 2500);
  const WellRec* list[1];
  sky.listed(list, 1);
  TEST_ASSERT_TRUE(list[0]->sig > -80 && list[0]->sig < -50);
  TEST_ASSERT_EQUAL(-50, list[0]->last);
}

// A guest-named network makes its router the Hedge's, even behind a password,
// and it stays that way when a scan misses the guest network.
static void test_a_guest_network_is_hedge() {
  TEST_ASSERT_TRUE(WellRules::isGuestName("CASA_Invitados", 14));
  TEST_ASSERT_TRUE(WellRules::isGuestName("Cafe GUEST", 10));
  TEST_ASSERT_FALSE(WellRules::isGuestName("HomeNet-1234", 12));
  Net home = net(0x10, 0x40, -60, 6, Lock::Wpa2), guest = net(0x12, 0x41, -60, 6, Lock::Wpa2);
  guest.guest = true;
  Net both[2] = {home, guest};
  sky.ingest(both, 2, 0);
  sky.ingest(&home, 1, 2500);   // the guest network missed this scan
  const WellRec* list[1];
  sky.listed(list, 1);
  TEST_ASSERT_EQUAL((int)Faction::Hedge, (int)list[0]->faction);
  TEST_ASSERT_EQUAL((int)Energy::Static, (int)list[0]->energy);
}

static void test_faction_only_moves_up() {
  Net home = net(0x10, 0x40, -60, 6, Lock::Wpa2), guest = net(0x12, 0x41, -60, 6, Lock::Open);
  Net both[2] = {home, guest};
  sky.ingest(both, 2, 0);
  sky.ingest(&guest, 1, 2500);   // the home network missed this scan
  const WellRec* list[1];
  sky.listed(list, 1);
  TEST_ASSERT_EQUAL((int)Faction::Hearth, (int)list[0]->faction);
  TEST_ASSERT_EQUAL((int)Energy::Tide, (int)list[0]->energy);
}

static void test_hidden_router_stays_hush_only_if_never_named() {
  Net h = net(0x10, 0x40, -60, 6, Lock::Wpa2, true), v = net(0x10, 0x41, -60, 6, Lock::Wpa2);
  sky.ingest(&h, 1, 0);
  const WellRec* list[1];
  sky.listed(list, 1);
  TEST_ASSERT_EQUAL((int)Energy::Hush, (int)list[0]->energy);
  Net both[2] = {h, v};
  sky.ingest(both, 2, 2500);
  sky.ingest(&h, 1, 5000);
  sky.listed(list, 1);
  TEST_ASSERT_EQUAL((int)Energy::Tide, (int)list[0]->energy);
}

static void test_names_are_shared_ids_are_private() {
  Net a = net(0x10, 0x40, -60, 6, Lock::Wpa2);
  sky.ingest(&a, 1, 0);
  const WellRec* list[1];
  sky.listed(list, 1);
  char name1[17];
  strcpy(name1, list[0]->name);
  const uint32_t id1 = list[0]->id;
  Keys::device[0] ^= 0xFF;   // a different Deck
  sky.reset(0);
  sky.ingest(&a, 1, 0);
  sky.listed(list, 1);
  TEST_ASSERT_EQUAL_STRING(name1, list[0]->name);
  TEST_ASSERT_NOT_EQUAL(id1, list[0]->id);
}

static void test_new_wells_are_fresh_after_the_first_scan() {
  Net a = net(0x10, 0x10, -60, 6, Lock::Wpa2, false, 0x01);
  Net b = net(0x10, 0x10, -60, 6, Lock::Wpa2, false, 0x02);
  sky.ingest(&a, 1, 0);
  Net both[2] = {a, b};
  sky.ingest(both, 2, 2500);
  TEST_ASSERT_EQUAL(1, sky.newThisScan());
  const WellRec* list[2];
  sky.listed(list, 2);
  int fresh = sky.isFresh(*list[0]) + sky.isFresh(*list[1]);
  TEST_ASSERT_EQUAL(1, fresh);
}

static void test_table_reuses_oldest_when_full() {
  for (int i = 0; i < Sightings::MaxWells + 20; i++) {
    Net n = net(0x10, 0x10, -60, 6, Lock::Wpa2, false, (uint8_t)i);
    n.bssid[3] = (uint8_t)(i >> 8);
    sky.ingest(&n, 1, i * 2500);
  }
  TEST_ASSERT_EQUAL(Sightings::MaxWells + 20, sky.found());
}

static void test_log_lines_hold_no_real_names_or_ids() {
  Net n = net(0x10, 0x40, -61, 6, Lock::Wpa2);
  char line[128];
  WalkFormat::netLine(line, sizeof(line), 7, n);
  TEST_ASSERT_EQUAL_STRING_LEN("N,7,", line, 4);
  TEST_ASSERT_NULL(strstr(line, "11223344"));   // the raw hardware ID
  TEST_ASSERT_NOT_NULL(strstr(line, ",-61,6,wpa2,0,0\n"));
  int commas = 0;
  for (char* p = line; *p; p++) commas += *p == ',';
  TEST_ASSERT_EQUAL(9, commas);
  Net h = net(0x10, 0x40, -61, 6, Lock::Wpa2, true);
  WalkFormat::netLine(line, sizeof(line), 7, h);
  TEST_ASSERT_NOT_NULL(strstr(line, ",-,-61,6,wpa2,1,0\n"));
  WalkFormat::scanLine(line, sizeof(line), 12345, 7, WalkFormat::Place::Street, 14, 11, 87);
  TEST_ASSERT_EQUAL_STRING("S,12345,7,street,14,11,87\n", line);
}


// ---- The fixed-well filter: phones and wells that travel with you -------------------

// Router number `k`, distinct by its middle bytes. `local`: a made-up hardware ID.
static Net router(int k, int8_t rssi, bool phone = false, bool local = false) {
  Net n{};
  const uint8_t b[6] = {(uint8_t)(local ? 0x12 : 0x10), (uint8_t)(k & 0xFF), (uint8_t)(k >> 8), 0x5A, 0x44, 0x60};
  memcpy(n.bssid, b, 6);
  n.rssi = rssi;
  n.channel = 6;
  n.lock = Lock::Wpa2;
  n.nameKey = 0xBEEF0000u + (uint32_t)k;
  n.phone = phone;
  return n;
}

static uint32_t idOf(const Net& n) {
  uint8_t rb[5];
  WellRules::routerBytes(n.bssid, rb);
  return Keys::deviceHash(rb, 5);
}

// One scan of a walk: the routers along the street at scan `s` (each heard for
// fifteen scans, then left behind), plus `extra`.
static uint32_t walkMs = 0;
static void walkScan(int s, const Net* extra, int nExtra) {
  Net nets[16];
  int k = 0;
  for (int i = 0; i < nExtra; i++) nets[k++] = extra[i];
  for (int p = s / 3; p < s / 3 + 5; p++) nets[k++] = router(1000 + p, (int8_t)(-70 - (p % 7)));
  walkMs += 2400;
  sky.ingest(nets, k, walkMs);
}

static void test_phone_names_are_flagged() {
  const char* phones[] = {"Mia's iPhone", "HOTSPOT-1234", "Galaxy A52", "AndroidAP_3f", "Pixel 8", "Moto phone"};
  const char* routers[] = {"NETGEAR42", "Lucky's Cafe", "TP-Link_7781", "BTHub6-XX", "linksys"};
  for (const char* n : phones) TEST_ASSERT_TRUE_MESSAGE(WellRules::isPhoneName(n, (int)strlen(n)), n);
  for (const char* n : routers) TEST_ASSERT_FALSE_MESSAGE(WellRules::isPhoneName(n, (int)strlen(n)), n);
}

// Standing at your door, a phone's hotspot walks past, louder then quieter: never a find.
static void test_a_passing_phone_is_never_a_find() {
  const Net home = router(1, -50);
  const int8_t past[] = {-80, -66, -58, -71, -85};
  for (int s = 0; s < 8; s++) {
    Net nets[2] = {home, router(2, s < 5 ? past[s] : -90, true)};
    sky.ingest(nets, s < 5 ? 2 : 1, s * 2400);
  }
  TEST_ASSERT_FALSE(WellRules::fixedish(*sky.byId(idOf(router(2, 0)))));
  TEST_ASSERT_TRUE(WellRules::fixedish(*sky.byId(idOf(home))));
}

// A companion's phone, with an ordinary name, at your start and all the way
// along: travelling with you after ten minutes, so never a find. Your own
// router, left behind at the start, stays a well, and so it does when you come
// back past it at the end of a loop.
static void test_a_phone_at_your_start_travels_with_you() {
  walkMs = 0;
  const Net friendPhone = router(7, -48);
  const Net door = router(8, -55);
  const uint32_t friendId = idOf(friendPhone), doorId = idOf(door);
  for (int s = 0; s < 270; s++) {
    Net extra[2] = {friendPhone, door};
    walkScan(s, extra, s < 10 || s >= 262 ? 2 : 1);
    if (s == 200) TEST_ASSERT_TRUE(WellRules::fixedish(*sky.byId(friendId)));   // not ten minutes yet
  }
  TEST_ASSERT_TRUE(sky.byId(friendId)->travelling);
  TEST_ASSERT_FALSE(WellRules::fixedish(*sky.byId(friendId)));
  TEST_ASSERT_TRUE(WellRules::fixedish(*sky.byId(doorId)));
}

// A hotspot with one network on a made-up hardware ID is suspected after five minutes.
static void test_made_up_ids_travel_sooner() {
  walkMs = 0;
  const Net phoneLike = router(9, -50, false, true), plain = router(10, -52);
  for (int s = 0; s < 140; s++) {
    Net extra[2] = {phoneLike, plain};
    walkScan(s, extra, 2);
  }
  TEST_ASSERT_TRUE(sky.byId(idOf(phoneLike))->travelling);
  TEST_ASSERT_FALSE(sky.byId(idOf(plain))->travelling);
}

// At home for half an hour while cars and passers-by come and go for a scan or
// two each: your router doesn't travel.
static void test_passers_by_dont_make_home_travel() {
  const Net home = router(1, -45);
  for (int s = 0; s < 750; s++) {
    Net nets[2] = {home, router(2000 + s / 2, -78, (s & 4) != 0)};
    sky.ingest(nets, 2, (uint32_t)s * 2400);
  }
  TEST_ASSERT_TRUE(WellRules::fixedish(*sky.byId(idOf(home))));
}

// ---- Tunes -----------------------------------------------------------------------

static void test_a_wells_tune_is_its_own() {
  for (uint32_t seed = 1; seed < 400000; seed += 7919) {
    for (int e = 0; e < 5; e++) {
      const Tune a = Tunes::of(seed, e, false), b = Tunes::of(seed, e, false);
      TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));   // the same on every Deck
      const int first = e == 4 ? 2 : 0;              // Static crackles first
      TEST_ASSERT_EQUAL(first + 3, a.count);
      TEST_ASSERT_EQUAL(Tunes::Root[e], a.n[first].hz);   // it opens on its energy's own note
      const int last = a.n[a.count - 1].hz;
      TEST_ASSERT_TRUE(last != Tunes::Root[e] && last != Tunes::Root[e] * 2);   // and never quite comes home
      for (int i = 1; i < a.count; i++) TEST_ASSERT_TRUE(a.n[i].at > a.n[i - 1].at);
      const Tune k = Tunes::of(seed, e, true);   // a well you keep does
      TEST_ASSERT_EQUAL(a.count + 1, k.count);
      TEST_ASSERT_EQUAL(Tunes::Root[e], k.n[k.count - 1].hz);
    }
  }
  // Wells differ.
  int differ = 0;
  for (uint32_t seed = 0; seed < 64; seed++) {
    const Tune a = Tunes::of(seed << 12, 0, false), b = Tunes::of((seed + 1) << 12, 0, false);
    differ += memcmp(&a, &b, sizeof(Tune)) != 0;
  }
  TEST_ASSERT_TRUE(differ > 50);
}

static void test_the_sequencer_plays_each_note_on_time() {
  Sequencer q;
  static int heard[16][3], n;
  n = 0;
  auto out = [](int hz, int ms) { heard[n][0] = hz; heard[n][1] = ms; n++; };
  const Tune t = Tunes::of(0x12345678, 0, true);
  q.play(t, 1000);
  for (uint32_t ms = 1000; ms < 3000; ms += 10) {
    const int before = n;
    q.tick(ms, out);
    for (int i = before; i < n; i++) heard[i][2] = (int)ms;
  }
  TEST_ASSERT_EQUAL(t.count, n);
  for (int i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL(t.n[i].hz, heard[i][0]);
    TEST_ASSERT_INT_WITHIN(10, 1000 + t.n[i].at, heard[i][2]);
  }
  TEST_ASSERT_FALSE(q.busy());
  // A chord's notes start together; a new tune replaces what was still to come.
  n = 0;
  q.play(Tunes::of(1, 2, false), 5000);
  q.play(Tunes::chord(1, 2, true, 3), 5000);
  q.tick(5139, out);
  TEST_ASSERT_EQUAL(0, n);
  q.tick(5140, out);
  TEST_ASSERT_EQUAL(5, n);   // three notes, the octave of a kept well, and the energy brought
}

// ---- Drifting glyphs ---------------------------------------------------------------

// Every shape in every hand, at both sizes: one of the six, never empty, and
// nothing small and loose above it (crumbs fall only below). All one colour,
// a neutral chalk white with no yellow in it, Minted included.
static void test_every_stray_keeps_to_its_shape() {
  static Strays::Mask m;
  for (int pitch = 2; pitch <= 3; pitch++)
    for (int shape = 0; shape < Strays::Shapes; shape++)
      for (int hand = 0; hand < Strays::Hands; hand++) {
        Strays::make(shape, (uint16_t)hand, pitch, m);
        TEST_ASSERT_TRUE(m.steps > 0);
        if (!Strays::valid(m, pitch)) {
          char b[48];
          snprintf(b, sizeof(b), "shape %d hand %d pitch %d", shape, hand, pitch);
          TEST_ASSERT_TRUE_MESSAGE(false, b);
        }
      }
  const uint16_t chalk[2] = {Strays::Chalk, Strays::ChalkDry};
  for (uint16_t c : chalk) {
    const int r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
    TEST_ASSERT_TRUE(b >= r && b >= g);
  }
  // Hands differ: the same shape is drawn differently by different hands.
  static Strays::Mask a, b;
  Strays::make(1, 10, 3, a);
  Strays::make(1, 2000, 3, b);
  TEST_ASSERT_TRUE(memcmp(a.px, b.px, sizeof(a.px)) != 0);
}

// The rule itself: two loose dots above a shape break it; crumbs below don't.
static void test_loose_dots_above_a_mark_break_the_rule() {
  static Strays::Mask m;
  memset(&m, 0, sizeof(m));
  for (int y = 12; y < 20; y++)
    for (int x = 8; x < 18; x++) m.px[y][x] = 1;
  TEST_ASSERT_TRUE(Strays::valid(m, 3));
  m.px[24][10] = 2;   // a crumb below
  TEST_ASSERT_TRUE(Strays::valid(m, 3));
  m.px[8][10] = 3;    // two dots above
  m.px[8][14] = 3;
  TEST_ASSERT_FALSE(Strays::valid(m, 3));
  m.px[8][10] = m.px[8][14] = 0;
  m.px[15][19] = 4;   // a loose bit beside it, level with it
  TEST_ASSERT_FALSE(Strays::valid(m, 3));
}

// Each shape plays its own rhythm.
static void test_each_glyph_has_its_rhythm() {
  for (int i = 0; i < Strays::Shapes; i++)
    for (int j = 0; j < i; j++) {
      const Tune a = Tunes::glyph(i), b = Tunes::glyph(j);
      TEST_ASSERT_TRUE(a.count != b.count || a.n[0].hz != b.n[0].hz || a.n[0].ms != b.n[0].ms);
    }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_siphash_reference_vectors);
  RUN_TEST(test_every_name_fits_16_chars);
  RUN_TEST(test_networks_of_one_router_fold_into_one_well);
  RUN_TEST(test_energy_rules);
  RUN_TEST(test_ward_thresholds);
  RUN_TEST(test_missed_scans_dim_then_drop);
  RUN_TEST(test_one_missed_scan_keeps_the_ward);
  RUN_TEST(test_faint_wells_dont_set_the_ward);
  RUN_TEST(test_the_ward_moves_once_two_scans_agree);
  RUN_TEST(test_smoothing_moves_toward_new_signal);
  RUN_TEST(test_a_guest_network_is_hedge);
  RUN_TEST(test_faction_only_moves_up);
  RUN_TEST(test_hidden_router_stays_hush_only_if_never_named);
  RUN_TEST(test_names_are_shared_ids_are_private);
  RUN_TEST(test_new_wells_are_fresh_after_the_first_scan);
  RUN_TEST(test_table_reuses_oldest_when_full);
  RUN_TEST(test_log_lines_hold_no_real_names_or_ids);
  RUN_TEST(test_phone_names_are_flagged);
  RUN_TEST(test_a_passing_phone_is_never_a_find);
  RUN_TEST(test_a_phone_at_your_start_travels_with_you);
  RUN_TEST(test_made_up_ids_travel_sooner);
  RUN_TEST(test_passers_by_dont_make_home_travel);
  RUN_TEST(test_a_wells_tune_is_its_own);
  RUN_TEST(test_the_sequencer_plays_each_note_on_time);
  RUN_TEST(test_every_stray_keeps_to_its_shape);
  RUN_TEST(test_loose_dots_above_a_mark_break_the_rule);
  RUN_TEST(test_each_glyph_has_its_rhythm);
  return UNITY_END();
}
