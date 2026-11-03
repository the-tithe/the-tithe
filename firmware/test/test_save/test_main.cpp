#include <stddef.h>
#include <stdio.h>
// Saves: a round trip keeps everything, and damage is caught. pio test -e native -f test_save
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "game/Game.h"
#include "game/Save.h"

using namespace Game;

static Saved a, b;
static uint8_t buf[SaveFormat::Bytes + 16];

void setUp() {
  memset(&a, 0, sizeof(a));
  newCareer(a.career);
  a.atlas.clear();
  newRun(a.run, a.career, Cat::Calling_hermit, 3, -1, 42, 7);
  beginNight(a.run);
  a.run.coin = 17.5;
  a.run.hold[Tide] = 6;
  a.atlas.touch(0xABCD, 0x1234, 7, 3).visits = 4;
  a.atlas.unique = 1;
  a.phase = Phase::Night;
  a.playMs = 123456;
  a.career.stats[(int)Cat::Stat::sales_total] = 99;
}
void tearDown() {}

static void test_round_trip_keeps_everything() {
  SaveFormat::encode(a, buf);
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));
  TEST_ASSERT_EQUAL(Cat::Calling_hermit, b.run.calling);
  TEST_ASSERT_EQUAL(4, b.atlas.find(0xABCD)->visits);
  // The run's random numbers carry on exactly where they left off.
  TEST_ASSERT_EQUAL_UINT32(a.run.rng.next(), b.run.rng.next());
}

static void test_damage_is_caught() {
  SaveFormat::encode(a, buf);
  for (int i = 0; i < 50; i++) {
    const size_t at = sizeof(SaveFormat::Header) + (size_t)rand() % sizeof(Saved);
    buf[at] ^= (uint8_t)(1 + rand() % 255);
    TEST_ASSERT_EQUAL((int)SaveFormat::Load::BadChecksum, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
    SaveFormat::encode(a, buf);
  }
}

static void test_short_wrong_and_newer_saves_are_refused() {
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Empty, (int)SaveFormat::decode(buf, 0, b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::WrongSize, (int)SaveFormat::decode(buf, SaveFormat::Bytes - 1, b));
  buf[0] ^= 1;
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::BadMagic, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  SaveFormat::encode(a, buf);
  SaveFormat::Header h;
  memcpy(&h, buf, sizeof(h));
  h.version = SaveFormat::Version + 1;
  memcpy(buf, &h, sizeof(h));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::NewerVersion, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
}

static void test_a_refused_save_leaves_the_game_alone() {
  memset(&b, 0x55, sizeof(b));
  Saved before = b;
  SaveFormat::encode(a, buf);
  buf[100] ^= 0xFF;
  SaveFormat::decode(buf, SaveFormat::Bytes, b);
  TEST_ASSERT_EQUAL_MEMORY(&before, &b, sizeof(b));
}

// The Deck boots by reading each copy straight into the game (decodeInto), a
// piece at a time from the file, with nothing borrowed. It loads what decode() does.
static size_t fileLen = 0;
static size_t fileRead(size_t at, uint8_t* to, size_t len) {
  if (at >= fileLen) return 0;
  const size_t k = len < fileLen - at ? len : fileLen - at;
  memcpy(to, buf + at, k);
  return k;
}
static void test_reading_straight_in_loads_the_save() {
  SaveFormat::encode(a, buf);
  fileLen = SaveFormat::Bytes;
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decodeInto(fileLen, fileRead, b));
  TEST_ASSERT_EQUAL_MEMORY(&a, &b, sizeof(a));
}
static void test_reading_straight_in_refuses_short_and_damaged_files() {
  SaveFormat::encode(a, buf);
  fileLen = SaveFormat::Bytes - 1;   // cut off
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::WrongSize, (int)SaveFormat::decodeInto(fileLen, fileRead, b));
  fileLen = SaveFormat::Bytes;
  buf[sizeof(SaveFormat::Header) + 500] ^= 0x10;
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::BadChecksum, (int)SaveFormat::decodeInto(fileLen, fileRead, b));
  fileLen = 0;
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Empty, (int)SaveFormat::decodeInto(fileLen, fileRead, b));
}
// A real save from a Deck, when TITHE_SAVE names one: it reads straight in.
static void test_a_real_save_reads_straight_in() {
  const char* path = getenv("TITHE_SAVE");
  if (!path) TEST_IGNORE_MESSAGE("set TITHE_SAVE to a deck.sav to check it");
  FILE* f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL(f);
  fileLen = fread(buf, 1, sizeof(buf), f);
  fclose(f);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decodeInto(fileLen, fileRead, b));
  printf("real save: %u bytes, %u weeks, play %lu s, %d wells in the atlas\n", (unsigned)fileLen, (unsigned)b.career.runs,
         (unsigned long)(b.playMs / 1000), b.atlas.count);
}

static void test_saves_from_another_catalog_are_refused() {
  SaveFormat::encode(a, buf);
  SaveFormat::Header h;
  memcpy(&h, buf, sizeof(h));
  h.catalog ^= 1;
  memcpy(buf, &h, sizeof(h));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::OtherCatalog, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
}

static void test_impossible_values_are_refused() {
  a.run.nRelics = 1;
  a.run.relics[0].id = 250;   // no such relic
  SaveFormat::encode(a, buf);   // a correct checksum over a bad value
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Invalid, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  a.run.relics[0].id = 0;
  a.run.nSpells = 9;
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Invalid, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
}

// Saves made with the 72-relic catalog (versions 1 to 4): written in that layout here, then loaded.
static SaveFormat::Saved72 o;
static uint8_t* oldSave(int version, size_t size, size_t copy) {
  SaveFormat::upgrade(a, o);   // today's test save, in the old layout
  uint8_t* out = (uint8_t*)malloc(sizeof(SaveFormat::Header) + size);
  memset(out, 0xAA, sizeof(SaveFormat::Header) + size);   // old tail padding is junk
  memcpy(out + sizeof(SaveFormat::Header), &o, copy);
  SaveFormat::Header h{SaveFormat::Magic, (uint16_t)version, (uint16_t)sizeof(SaveFormat::Header), (uint32_t)size,
                       SaveFormat::Catalog72, SaveFormat::crc32(out + sizeof(SaveFormat::Header), size)};
  memcpy(out, &h, sizeof(h));
  return out;
}

// What every old save keeps.
static void keepsTheBasics() {
  TEST_ASSERT_EQUAL(Cat::Calling_hermit, b.run.calling);
  TEST_ASSERT_EQUAL_DOUBLE(17.5, b.run.coin);
  TEST_ASSERT_EQUAL(6, b.run.hold[Tide]);
  TEST_ASSERT_EQUAL(4, b.atlas.find(0xABCD)->visits);
  TEST_ASSERT_EQUAL(99, b.career.stats[(int)Cat::Stat::sales_total]);
  TEST_ASSERT_EQUAL_DOUBLE(2.5, b.run.state[5]);
  TEST_ASSERT_TRUE(getBit(b.career.relics, 70));
  TEST_ASSERT_EQUAL_UINT32(a.run.rng.next(), b.run.rng.next());
}

static void prepare() {
  a.run.state[5] = 2.5;
  setBit(a.career.relics, 70);
}

// A save from before the log (version 1) still loads, with an empty log.
static void test_a_version_1_save_upgrades() {
  prepare();
  a.log.loudest.value = 500;   // not in a version 1 save, so it must come back empty
  uint8_t* old = oldSave(1, SaveFormat::V1Size, SaveFormat::V1Size);
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(old, sizeof(SaveFormat::Header) + SaveFormat::V1Size, b));
  keepsTheBasics();
  TEST_ASSERT_EQUAL(0, (int)b.log.loudest.value);
  TEST_ASSERT_EQUAL(0, b.log.nNights);
  free(old);
}

// A save from before the relics seen (version 2) keeps its log and loads with none seen.
static void test_a_version_2_save_upgrades() {
  prepare();
  a.log.loudest.value = 500;
  a.seen[0] = 0xFF;   // not in a version 2 save
  uint8_t* old = oldSave(2, SaveFormat::V2Size, SaveFormat::V2Size);
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(old, sizeof(SaveFormat::Header) + SaveFormat::V2Size, b));
  keepsTheBasics();
  TEST_ASSERT_EQUAL(500, (int)b.log.loudest.value);
  TEST_ASSERT_EQUAL(0, b.seen[0]);
  free(old);
}

// A save from before the well book (version 3) keeps everything and loads with no wells remembered.
static void test_a_version_3_save_upgrades() {
  prepare();
  a.log.loudest.value = 700;
  a.seen[0] = 0x0F;
  a.wells.count = 3;   // not in a version 3 save
  uint8_t* old = oldSave(3, SaveFormat::V3Size, offsetof(SaveFormat::Saved72, seen) + sizeof(o.seen));
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(old, sizeof(SaveFormat::Header) + SaveFormat::V3Size, b));
  keepsTheBasics();
  TEST_ASSERT_EQUAL(700, (int)b.log.loudest.value);
  TEST_ASSERT_EQUAL(0x0F, b.seen[0]);
  TEST_ASSERT_EQUAL(0, b.wells.count);
  free(old);
}

// A 72-relic save of the last format (version 4) keeps its well book too.
static void test_a_version_4_save_upgrades() {
  prepare();
  a.seen[8] = 0x80;   // relic 71, the last of the 72
  a.wells.note(0xABCD, 321, 7, 2);
  a.heard.radio[0] = 0xFF;   // not in a version 4 save
  uint8_t* old = oldSave(4, SaveFormat::V4Size, SaveFormat::V4Size);
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(old, sizeof(SaveFormat::Header) + SaveFormat::V4Size, b));
  keepsTheBasics();
  TEST_ASSERT_TRUE(getBit(b.seen, 71));
  TEST_ASSERT_FALSE(getBit(b.seen, 72));   // the new relics start unseen
  TEST_ASSERT_EQUAL_FLOAT(321, b.wells.find(0xABCD)->best);
  TEST_ASSERT_EQUAL(0, b.heard.radio[0]);
  // And it saves back in today's format, unchanged.
  SaveFormat::encode(b, buf);
  memset(&a, 0, sizeof(a));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, a));
  TEST_ASSERT_EQUAL_MEMORY(&b, &a, sizeof(a));
  free(old);
}

// A save from before the HEARD log (version 5) keeps everything and loads with nothing heard.
static void test_a_version_5_save_upgrades() {
  prepare();
  a.wells.note(0xABCD, 321, 7, 2);
  a.heard.radio[0] = 0xFF;   // not in a version 5 save
  a.heard.stall[0] = 0xFF;
  const size_t size = SaveFormat::V5Size;
  uint8_t* old = (uint8_t*)malloc(sizeof(SaveFormat::Header) + size);
  memset(old, 0xAA, sizeof(SaveFormat::Header) + size);
  memcpy(old + sizeof(SaveFormat::Header), &a, offsetof(Saved, heard));
  SaveFormat::Header h{SaveFormat::Magic, 5, (uint16_t)sizeof(SaveFormat::Header), (uint32_t)size, Cat::CatalogHash,
                       SaveFormat::crc32(old + sizeof(SaveFormat::Header), size)};
  memcpy(old, &h, sizeof(h));
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(old, sizeof(SaveFormat::Header) + size, b));
  keepsTheBasics();
  TEST_ASSERT_EQUAL_FLOAT(321, b.wells.find(0xABCD)->best);
  for (size_t i = 0; i < sizeof(b.heard); i++) TEST_ASSERT_EQUAL(0, ((const uint8_t*)&b.heard)[i]);
  for (size_t i = 0; i < sizeof(b.moments); i++) TEST_ASSERT_EQUAL(0, ((const uint8_t*)&b.moments)[i]);
  free(old);
}

// A save from before the walks' block (version 6) keeps everything, its HEARD
// log too, and loads with nothing remembered from walks.
static void test_a_version_6_save_upgrades() {
  prepare();
  a.heard.radio[3] = 0x5A;
  a.heard.stall[1] = 0x21;
  a.moments.known.add(0xABCD);   // not in a version 6 save
  a.moments.nMarks = 2;
  const size_t size = SaveFormat::V6Size;
  uint8_t* old = (uint8_t*)malloc(sizeof(SaveFormat::Header) + size);
  memset(old, 0xAA, sizeof(SaveFormat::Header) + size);   // old tail padding is junk
  memcpy(old + sizeof(SaveFormat::Header), &a, offsetof(Saved, moments));
  SaveFormat::Header h{SaveFormat::Magic, 6, (uint16_t)sizeof(SaveFormat::Header), (uint32_t)size, Cat::CatalogHash,
                       SaveFormat::crc32(old + sizeof(SaveFormat::Header), size)};
  memcpy(old, &h, sizeof(h));
  memset(&b, 0x55, sizeof(b));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(old, sizeof(SaveFormat::Header) + size, b));
  keepsTheBasics();
  TEST_ASSERT_EQUAL_MEMORY(&a.heard, &b.heard, sizeof(b.heard));
  for (size_t i = 0; i < sizeof(b.moments); i++) TEST_ASSERT_EQUAL(0, ((const uint8_t*)&b.moments)[i]);
  TEST_ASSERT_FALSE(b.moments.known.has(0xABCD));
  // And it saves back in today's format, unchanged.
  SaveFormat::encode(b, buf);
  memset(&a, 0, sizeof(a));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, a));
  TEST_ASSERT_EQUAL_MEMORY(&b, &a, sizeof(a));
  free(old);
}

// Version 6 is exactly what a Deck wrote: a real 0.5 save's header says 21520 bytes.
static void test_version_6_is_the_size_a_deck_wrote() {
  TEST_ASSERT_EQUAL(21520, (int)SaveFormat::V6Size);
  TEST_ASSERT_TRUE(offsetof(Saved, moments) <= SaveFormat::V6Size);   // its tail padding is cleared on load
}

// The Deck's memory of every well it has heard: never forgets, rarely imagines.
static void test_the_deck_remembers_every_well_it_heard() {
  Moments& m = a.moments;
  memset(&m.known, 0, sizeof(m.known));
  uint32_t x = 12345;
  auto next = [&x]() { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; };
  for (int i = 0; i < 3500; i++) {
    const uint32_t id = next();
    m.known.add(id);
    TEST_ASSERT_TRUE(m.known.has(id));
    TEST_ASSERT_FALSE(m.known.add(id));   // the second time, it's known
  }
  int imagined = 0;
  for (int i = 0; i < 10000; i++) imagined += m.known.has(next());
  printf("known wells: %d in 10000 never heard look heard, at 3500 known\n", imagined);
  TEST_ASSERT_TRUE(imagined < 400);
  // It round-trips with the save.
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  x = 12345;
  for (int i = 0; i < 3500; i++) TEST_ASSERT_TRUE(b.moments.known.has(next()));
}

static void test_travellers_are_kept_by_id() {
  Moments& m = a.moments;
  for (uint32_t id = 1; id <= Moments::MaxTravellers + 3; id++) m.noteTraveller(id);
  m.noteTraveller(5 + Moments::MaxTravellers);   // already kept: no change
  TEST_ASSERT_EQUAL(Moments::MaxTravellers, m.nTravellers);
  TEST_ASSERT_FALSE(m.travelled(1));   // the oldest let go
  TEST_ASSERT_TRUE(m.travelled(Moments::MaxTravellers + 3));
}

static void test_impossible_walk_counts_are_refused() {
  a.moments.nMarks = Moments::MaxMarks + 1;
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Invalid, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  a.moments.nMarks = 0;
  a.moments.ribbons[3].len = Ribbon::Slivers + 1;
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Invalid, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
}

// A true name fits in 16 bits and comes back the same.
static void test_names_pack_small() {
  for (uint32_t seed = 0; seed < 4096; seed += 13)
    for (int f = 0; f < 4; f++) {
      const uint16_t p = packName(seed * 2654435761u, f);
      TEST_ASSERT_TRUE(p != 0);
      TEST_ASSERT_EQUAL((seed * 2654435761u) & 2047u, nameSeed(p));
      TEST_ASSERT_EQUAL(f, nameFaction(p));
    }
}

// A walk's ribbon: a short walk and a long one both fill the same width. A
// long walk folds, so it never earns more ribbon, and folding keeps every
// offering and Hush well and adds up the new wells.
// A walk drawn as the app draws it: a sliver each time one's length has passed.
static Ribbon walkOf(int seconds, int offerAt = -1, int hushAt = -1) {
  Ribbon r{};
  int t = 0;
  bool offer = false, hush = false;
  for (int s = 0; s < seconds; s++) {
    offer |= s == offerAt;
    hush |= s == hushAt;
    if (++t * 1000u >= r.sliverMs()) {
      r.push(Ribbon::sliver(s < seconds / 2 ? 0 : 3, 1, offer, hush));
      t = 0;
      offer = hush = false;
    }
  }
  return r;
}

// A short walk and a long one both fill the same width: a long walk folds, so
// it never earns more ribbon. Folding keeps every offering and Hush well and
// adds up the new wells.
static void test_every_walk_fills_its_ribbon() {
  const Ribbon shortWalk = walkOf(120), longWalk = walkOf(3 * 3600, 1500, 7000);
  TEST_ASSERT_EQUAL(8, shortWalk.len);
  TEST_ASSERT_EQUAL(0, shortWalk.scale);
  TEST_ASSERT_TRUE(longWalk.len >= Ribbon::Slivers / 2 && longWalk.len <= Ribbon::Slivers);
  const uint32_t covered = longWalk.len * longWalk.sliverMs();
  TEST_ASSERT_TRUE(covered <= 3 * 3600 * 1000u && covered > 3 * 3600 * 1000u - longWalk.sliverMs());
  // Both reach from the first pixel to the last.
  const int W = 210;
  const Ribbon* both[2] = {&shortWalk, &longWalk};
  for (const Ribbon* r : both) {
    TEST_ASSERT_EQUAL(0, r->at(0, W));
    TEST_ASSERT_EQUAL(r->len - 1, r->at(W - 1, W));
  }
  int offers = 0, hush = 0;
  for (int i = 0; i < longWalk.len; i++) {
    offers += (longWalk.s[i] & Ribbon::Offer) != 0;
    hush += (longWalk.s[i] & Ribbon::Hush) != 0;
  }
  TEST_ASSERT_EQUAL(1, offers);
  TEST_ASSERT_EQUAL(1, hush);
  TEST_ASSERT_EQUAL(15, Ribbon::fresh(longWalk.s[0]));   // many slivers' new wells, up to 15
  TEST_ASSERT_EQUAL(0, Ribbon::ward(longWalk.s[0]));
  TEST_ASSERT_EQUAL(3, Ribbon::ward(longWalk.s[longWalk.len - 1]));
  TEST_ASSERT_EQUAL(5, Ribbon::fresh(Ribbon::merge(Ribbon::sliver(0, 2, false, false), Ribbon::sliver(0, 3, false, false))));
}

// The dawn token's kind follows where you offered, every kind turns up on an
// ordinary street, and about one dawn in forty brings one nobody recognises,
// whatever the Night was like.
static void test_the_dawn_token_follows_where_you_offered() {
  uint8_t f[4] = {0}, w[4] = {0}, e[5] = {0};
  const uint32_t plain = 1;   // a roll that brings no stranger
  TEST_ASSERT_EQUAL(T_Subway, dawnToken(f, w, e, plain));   // nothing offered
  f[Hearth] = 5; w[Fringe] = 5; e[Tide] = 5;                   // home streets
  TEST_ASSERT_EQUAL(T_Laundromat, dawnToken(f, w, e, plain));
  TEST_ASSERT_EQUAL(T_Laundromat, dawnToken(f, w, e, plain + 4));
  memset(f, 0, 4); memset(w, 0, 4);
  f[Hedge] = 4; w[WardW] = 4;                                  // free shop networks
  TEST_ASSERT_EQUAL(T_Arcade, dawnToken(f, w, e, plain));
  memset(e, 0, 5); e[Hush] = 5;                                // a Night of Hush is no different
  TEST_ASSERT_EQUAL(T_Arcade, dawnToken(f, w, e, plain));
  // Every kind somewhere off the quiet edges.
  bool seen[TokenCount] = {false};
  for (int fa = 0; fa < 4; fa++)
    for (int wa = Fringe; wa < 4; wa++)
      for (uint32_t roll = 1; roll < 9; roll++) {
        memset(f, 0, 4); memset(w, 0, 4);
        f[fa] = 1; w[wa] = 1;
        seen[dawnToken(f, w, e, roll)] = true;
      }
  for (int k = 0; k < T_Stranger; k++) TEST_ASSERT_TRUE(seen[k]);
  // The stranger: the same odds after a quiet Night as after a long one.
  int quiet = 0, busy = 0;
  memset(f, 0, 4); memset(w, 0, 4);
  for (uint32_t roll = 0; roll < 4000; roll++) quiet += dawnToken(f, w, e, roll) == T_Stranger;
  f[Syndicate] = 200; w[Nexus] = 200; w[Hollow] = 199;
  for (uint32_t roll = 0; roll < 4000; roll++) {
    const int k = dawnToken(f, w, e, roll);
    busy += k == T_Stranger;
    TEST_ASSERT_TRUE(k < TokenCount);
  }
  TEST_ASSERT_EQUAL(4000 / (int)StrangerOdds, quiet);
  TEST_ASSERT_EQUAL(quiet, busy);
  TEST_ASSERT_TRUE(TokenCount <= Moments::TokenKinds);
}

// Your hand from how you typed: quick and even writes close and straight,
// slow and uneven writes wide and wobbles. The same typing, the same hand.
static void test_your_hand_comes_from_your_typing() {
  const uint16_t quick[6] = {120, 130, 110, 125, 120, 115}, slow[6] = {600, 200, 900, 300, 800, 450};
  uint16_t c1, c2, c3;
  uint8_t a1, a2, a3;
  int8_t w1, w2, w3;
  handFrom(quick, 6, c1, a1, w1);
  handFrom(slow, 6, c2, a2, w2);
  handFrom(slow, 6, c3, a3, w3);
  TEST_ASSERT_EQUAL(5, a1);
  TEST_ASSERT_EQUAL(0, w1);
  TEST_ASSERT_EQUAL(7, a2);
  TEST_ASSERT_TRUE(w2 != 0);
  TEST_ASSERT_TRUE(c1 != c2);
  TEST_ASSERT_EQUAL(c2, c3);
  TEST_ASSERT_EQUAL(a2, a3);
  TEST_ASSERT_EQUAL(w2, w3);
  handFrom(nullptr, 0, c3, a3, w3);   // nothing typed: a plain hand
  TEST_ASSERT_EQUAL(6, a3);
  TEST_ASSERT_EQUAL(0, w3);
}

// A stretch counts each Night once, three at most, by the low half of each ID.
static void test_a_stretch_counts_each_night_once() {
  Stretch s{};
  s.ids[0] = Stretch::low(0x12340000u);   // a low half of 0 is kept as 1
  s.ids[1] = Stretch::low(0xABCD5678u);
  TEST_ASSERT_TRUE(s.has(0x99990000u));
  TEST_ASSERT_TRUE(s.has(0x00005678u));
  TEST_ASSERT_FALSE(s.has(0x00005679u));
  TEST_ASSERT_TRUE(s.walk(Stretch::nightCode(2, 0)));
  TEST_ASSERT_FALSE(s.walk(Stretch::nightCode(2, 0)));
  TEST_ASSERT_TRUE(s.walk(Stretch::nightCode(2, 1)));
  TEST_ASSERT_TRUE(s.walk(Stretch::nightCode(3, 0)));
  TEST_ASSERT_FALSE(s.walk(Stretch::nightCode(3, 1)));
  TEST_ASSERT_EQUAL(3, s.nNights);
}

// The walks' block kept its layout: the fields this version fills were set
// aside when it was made, so a save written before them still loads as is.
// The dawn's Portent choice lives in the walks' block and is range-checked like the rest.
static void test_a_pending_portent_choice_is_range_checked() {
  a.moments.portent.pending = 8;
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Invalid, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  a.moments.portent.pending = 1;
  a.moments.portent.alt[0] = (uint8_t)(Cat::PortentCount + 1);
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Invalid, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  a.moments.portent.alt[0] = (uint8_t)Cat::PortentCount;   // a kind + 1
  SaveFormat::encode(a, buf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  TEST_ASSERT_EQUAL(1, b.moments.portent.pending);
  a.moments.portent = PortentChoice{};
}

// Renaming Salt to Lead changed the catalog's fingerprint and nothing in the layout: those saves load.
static void test_a_salt_era_save_still_loads() {
  SaveFormat::encode(a, buf);
  SaveFormat::Header h;
  memcpy(&h, buf, sizeof(h));
  h.catalog = SaveFormat::CatalogSalt;
  memcpy(buf, &h, sizeof(h));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  // The 96-relic catalog (0.6.x), before the five growing relics: same layout, so it loads too.
  h.catalog = SaveFormat::Catalog96;
  memcpy(buf, &h, sizeof(h));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
  h.catalog = 0x12345678u;
  memcpy(buf, &h, sizeof(h));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::OtherCatalog, (int)SaveFormat::decode(buf, SaveFormat::Bytes, b));
}

static void test_the_walks_block_kept_its_layout() {
  TEST_ASSERT_EQUAL(5664, (int)sizeof(Moments));
  TEST_ASSERT_EQUAL(4662, (int)offsetof(Moments, ribbons));
  TEST_ASSERT_EQUAL(5154, (int)offsetof(Moments, streets));
  TEST_ASSERT_EQUAL(5246, (int)offsetof(Moments, stretches));
  TEST_ASSERT_EQUAL(5654, (int)offsetof(Moments, adrift));   // the old spare bytes, from the end
  TEST_ASSERT_EQUAL(5660, (int)offsetof(Moments, spareOutside));
}

// Which copy a boot loads: the newer whole one. A broken one is set aside; a newer
// Tithe's save, or no memory to read one, stops the boot with both kept.
static void test_the_boot_loads_the_newer_copy() {
  using SaveFormat::Boot;
  using SaveFormat::Copy;
  using L = SaveFormat::Load;
  const Copy none{L::Empty, 0, 0}, older{L::Ok, 100, 7}, newer{L::Ok, 200, 7}, untagged{L::Ok, 300, 0};
  const Copy damaged{L::BadChecksum, 0, 0}, later{L::NewerVersion, 0, 0}, noMem{L::NoMemory, 0, 0};
  const Copy otherDeck{L::Ok, 900, 9};
  auto boot = [](const Copy& f, const Copy& c) { return (int)SaveFormat::chooseBoot(f, c, 7); };
  TEST_ASSERT_EQUAL((int)Boot::Fresh, boot(none, none));
  TEST_ASSERT_EQUAL((int)Boot::Flash, boot(newer, older));
  TEST_ASSERT_EQUAL((int)Boot::Flash, boot(newer, newer));     // a tie stays with flash
  TEST_ASSERT_EQUAL((int)Boot::CardNewer, boot(older, newer));  // flash saves were failing
  TEST_ASSERT_EQUAL((int)Boot::CardNewer, boot(older, untagged));
  TEST_ASSERT_EQUAL((int)Boot::Flash, boot(older, otherDeck));  // a card from another Deck never wins
  TEST_ASSERT_EQUAL((int)Boot::Flash, boot(newer, damaged));
  TEST_ASSERT_EQUAL((int)Boot::Card, boot(none, older));
  TEST_ASSERT_EQUAL((int)Boot::CardDamaged, boot(damaged, older));
  TEST_ASSERT_EQUAL((int)Boot::SetAside, boot(damaged, none));
  TEST_ASSERT_EQUAL((int)Boot::SetAside, boot(none, damaged));
  TEST_ASSERT_EQUAL((int)Boot::SetAside, boot(damaged, damaged));
  TEST_ASSERT_EQUAL((int)Boot::Newer, boot(later, older));
  TEST_ASSERT_EQUAL((int)Boot::Newer, boot(newer, later));
  TEST_ASSERT_EQUAL((int)Boot::Newer, boot(later, damaged));
  TEST_ASSERT_EQUAL((int)Boot::NoMemory, boot(noMem, older));
  TEST_ASSERT_EQUAL((int)Boot::NoMemory, boot(damaged, noMem));
  // Only a damaged save, or one from other firmware, is ever set aside.
  TEST_ASSERT_TRUE(SaveFormat::broken(L::BadChecksum));
  TEST_ASSERT_TRUE(SaveFormat::broken(L::OtherCatalog));
  TEST_ASSERT_FALSE(SaveFormat::broken(L::NewerVersion));
  TEST_ASSERT_FALSE(SaveFormat::broken(L::NoMemory));
  TEST_ASSERT_FALSE(SaveFormat::broken(L::Empty));
  TEST_ASSERT_FALSE(SaveFormat::broken(L::Ok));
}

// Telling the two copies apart reads a piece at a time: the checksum, the play clock and the Deck's tag.
static size_t readBuf(size_t at, uint8_t* out, size_t len) {
  if (at >= SaveFormat::Bytes) return 0;
  const size_t n = len < SaveFormat::Bytes - at ? len : SaveFormat::Bytes - at;
  memcpy(out, buf + at, n);
  return n;
}
static void test_a_peek_reads_the_play_clock() {
  a.moments.deckTag = 0xBEEF;
  SaveFormat::encode(a, buf);
  SaveFormat::Copy c = SaveFormat::peek(SaveFormat::Bytes, readBuf);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Ok, (int)c.load);
  TEST_ASSERT_EQUAL_UINT32(123456, c.playMs);
  TEST_ASSERT_EQUAL_HEX16(0xBEEF, c.tag);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::Empty, (int)SaveFormat::peek(0, readBuf).load);
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::WrongSize, (int)SaveFormat::peek(SaveFormat::Bytes - 1, readBuf).load);
  buf[sizeof(SaveFormat::Header) + 1000] ^= 0x10;
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::BadChecksum, (int)SaveFormat::peek(SaveFormat::Bytes, readBuf).load);
  SaveFormat::encode(a, buf);
  SaveFormat::Header h;
  memcpy(&h, buf, sizeof(h));
  h.version = SaveFormat::Version + 1;
  memcpy(buf, &h, sizeof(h));
  TEST_ASSERT_EQUAL((int)SaveFormat::Load::NewerVersion, (int)SaveFormat::peek(SaveFormat::Bytes, readBuf).load);
}

static void test_the_save_fits_the_deck() {
  printf("save size: %u bytes\n", (unsigned)SaveFormat::Bytes);
  TEST_ASSERT_TRUE(SaveFormat::Bytes < 40 * 1024);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_round_trip_keeps_everything);
  RUN_TEST(test_damage_is_caught);
  RUN_TEST(test_short_wrong_and_newer_saves_are_refused);
  RUN_TEST(test_a_refused_save_leaves_the_game_alone);
  RUN_TEST(test_reading_straight_in_loads_the_save);
  RUN_TEST(test_reading_straight_in_refuses_short_and_damaged_files);
  RUN_TEST(test_a_real_save_reads_straight_in);
  RUN_TEST(test_saves_from_another_catalog_are_refused);
  RUN_TEST(test_impossible_values_are_refused);
  RUN_TEST(test_a_version_1_save_upgrades);
  RUN_TEST(test_a_version_2_save_upgrades);
  RUN_TEST(test_a_version_3_save_upgrades);
  RUN_TEST(test_a_version_4_save_upgrades);
  RUN_TEST(test_a_version_5_save_upgrades);
  RUN_TEST(test_a_version_6_save_upgrades);
  RUN_TEST(test_version_6_is_the_size_a_deck_wrote);
  RUN_TEST(test_the_deck_remembers_every_well_it_heard);
  RUN_TEST(test_travellers_are_kept_by_id);
  RUN_TEST(test_impossible_walk_counts_are_refused);
  RUN_TEST(test_names_pack_small);
  RUN_TEST(test_every_walk_fills_its_ribbon);
  RUN_TEST(test_the_dawn_token_follows_where_you_offered);
  RUN_TEST(test_your_hand_comes_from_your_typing);
  RUN_TEST(test_a_stretch_counts_each_night_once);
  RUN_TEST(test_a_pending_portent_choice_is_range_checked);
  RUN_TEST(test_a_salt_era_save_still_loads);
  RUN_TEST(test_the_walks_block_kept_its_layout);
  RUN_TEST(test_the_boot_loads_the_newer_copy);
  RUN_TEST(test_a_peek_reads_the_play_clock);
  RUN_TEST(test_the_save_fits_the_deck);
  return UNITY_END();
}
