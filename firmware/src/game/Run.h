#pragma once
#include <stdint.h>
#include <string.h>
#include "CatalogData.h"
#include "Rng.h"

// One Binding (a run): a week of seven Nights. Everything in here is saved
// as-is (Save.h), so it's plain data with no pointers. The numbers match
// tools/balance_sim.py, which is where they were tuned.

namespace Game {

// ---- Tuning (tools/balance_sim.py) -----------------------------------------
constexpr int Nights = 7;
constexpr int Visits = 12;
constexpr double Tithes[Nights] = {410, 900, 2200, 4700, 9000, 17500, 26000};
constexpr double OvernightGrowth = 2.0;
constexpr int StartCoin = 4;
constexpr int NightReward = 5;
constexpr double CoinPerBase = 1.0 / 30;   // trading profit: coin per point of un-multiplied sale
constexpr int InterestPer = 5, InterestCap = 5;
constexpr int RelicSlots = 5, TrinketSlots = 2, SpellSlots = 2;
constexpr int PriceCommon = 4, PriceUncommon = 7, PriceRare = 10, PriceTrinket = 5, PriceSpell = 3,
              PriceAttune = 3, PriceGlyph = 3, PriceMod = 10, PriceEdition = 3;
constexpr double RarityW[3] = {0.67, 0.27, 0.06};
constexpr int Reroll = 3, RerollStep = 2;   // the first reroll at a stall, and how much each one after adds
constexpr int ShopRelics = 4, ShopConsumables = 3;
constexpr double TrinketOfferP = 0.4;
constexpr double ConsumableW[3] = {0.45, 0.4, 0.15};   // attune, spell, glyph
constexpr double BuildBias = 0.35, LocaleBias = 0.5;
constexpr int UnlocksPerRun = 2;
constexpr double LoanFloor = 0.6, LoanInterest = 1.25;
constexpr int PortentAt[3] = {6, 9, 11};
constexpr double RareGrowth = 0.03, UncommonGrowth = 0.01, EditionGrowth = 0.25;
constexpr double WardenBlock = 0.15;
constexpr int Candles[3] = {4, 6, 8};
constexpr double BasePrice[5] = {5, 5, 5, 9, 8};   // ember tide gale hush static
constexpr int PolishCost[3] = {6, 12, 20};
constexpr double PolishStep = 0.5;
constexpr int HoldBase = 8;                        // units a hold carries

enum E : uint8_t { Ember, Tide, Gale, Hush, Static, EnergyN };
enum W : uint8_t { Hollow, Fringe, WardW, Nexus };
enum F : uint8_t { Hedge, Hearth, Warded, Syndicate };

// Shop flags carried between a Night and the next shop.
enum ShopFlag : uint32_t {
  SF_Mercy = 1, SF_RareSight = 2, SF_FreeRerolls = 4, SF_Shine = 8, SF_DoubleInterest = 16,
};

constexpr uint8_t None8 = 0xFF;

struct OwnedRelic {
  uint8_t id;        // Cat::Relic
  int8_t ed;         // Cat::Edition, -1 for none
  int8_t withered;   // Nights left before it crumbles (Bone Pact), -1 for never
  uint8_t tier;      // Polish tier, 0..3
};

// One faction's request, taken at a well (Jobs.h).
struct Job {
  uint8_t kind;      // Jobs::Kind, 0 = empty
  uint8_t faction;
  uint8_t energy;
  uint8_t need, got;
  uint8_t reward;    // coin
  uint8_t done;
  uint8_t pad;
};

// Room for this many relics in the save, so adding relics to the catalog never
// moves anything else in it. Saves made with the 72-relic catalog are upgraded
// on load (Save.h).
constexpr int RelicIdCap = 128;
static_assert(Cat::RelicCount <= RelicIdCap, "raise RelicIdCap, and add a save upgrade");

template <int RC>
struct RunT {
  // ---- Who and how
  uint32_t runNo;         // career run counter, for the atlas's per-run marks
  uint8_t calling;        // Cat::Calling
  uint8_t pact;           // 0 Chalk .. 7 Gold
  int8_t challenge;       // -1 for none
  uint8_t focus;          // the energy this run leans on: Ember, Tide or Gale
  uint8_t active, won, overnight, pad0;
  int16_t night;          // 0-based; 7 and up is Overnight
  int16_t overnightNights;
  Rng rng;

  // ---- The build
  static constexpr int MaxRelics = 8;
  OwnedRelic relics[MaxRelics];
  uint8_t nRelics;
  uint8_t trinkets[TrinketSlots];
  uint8_t nTrinkets;
  uint8_t spells[4];
  uint8_t nSpells;
  uint8_t glyphTokens[4]; // glyphs bought and waiting to be carved into a well
  uint8_t nGlyphTokens;
  uint8_t attune[EnergyN];
  uint8_t pad1;
  uint32_t mods;          // bit per Cat::Mod
  double state[RC];      // what growing relics have grown to, by relic id

  // ---- Money and the world's opinion of you
  double coin;
  double heat;
  double standing[4];
  double owed;            // a loan, added to the next Tithe
  double owedN2;          // the Contractor's loan, added to Night 2
  uint8_t loansLeft, hadLoan, bargain, pad2;
  uint32_t shopFlags;
  int16_t streak;         // Tithes in a row paid with 6+ visits to spare

  // ---- Across the run
  double newWells;
  uint32_t sales;
  int8_t lastE;           // energy of the last sale, -1 before the first
  int8_t lastSpell;
  uint8_t omens[Nights];  // Cat::Omen + 1, 0 for none (Night 1 never has one)
  int8_t eclipse;         // Cat::Eclipse for Night 7
  uint16_t wardSeen[4];
  uint16_t channels;      // bit per channel sold on

  // ---- Tonight
  struct Night {
    double tithe;
    double total;         // Resonance so far
    double coinEarned;    // trading profit so far
    int16_t visits;       // visits tonight, after Omens, mods and spells
    int16_t used;         // visits used
    int16_t candles;
    int8_t omen;          // Cat::Omen, -1 none
    int8_t eclipse;       // Cat::Eclipse, -1 none
    int8_t fadeE;         // energy the Fade halves
    uint8_t noPolish;     // the Sleeper
    uint8_t met;          // the Tithe is paid
    uint8_t kinds;        // bit per energy sold tonight
    uint16_t hollows;     // Hollow-ward sales tonight
    uint16_t wards;       // wards entered tonight (1 + changes)
    int8_t lastWard;      // ward of the last sale tonight, -1 before the first
    int8_t crownBan;      // energy Glass crown refuses tonight
    uint8_t veil, transmuteLeft;
    int8_t transmuteE;
    uint8_t wardCast;     // a ward gesture was used tonight
    uint8_t wardKinds;    // bit per kind of ward sold in tonight (Grand tour)
    uint8_t newTonight;
    uint8_t glamour;      // GLAMOUR cast: the next sale counts double
    uint8_t surge;        // SURGE cast: the next draw takes 2.5x the units
    uint8_t echo;         // ECHO cast: the next sale can be at any well you've visited
    uint8_t perm[3];      // Glitch: tonight's shuffle of Ember, Tide and Gale
    uint8_t closedBy;     // how the Night closed: 0 open, 1 paid, 2 loan
    uint8_t jobsDone;     // bit per faction whose job was finished tonight
    double wardUntil;     // seconds of play when the ward wears off
  } n;

  // ---- The hold: energy you carry
  uint8_t hold[EnergyN];
  uint8_t pad3[3];
  uint32_t holdLeakMs;    // play time when the hold last leaked

  Job jobs[2];

  // ---- This run's numbers, merged into the career when it ends
  uint32_t stats[(int)Cat::Stat::Count];
};
using Run = RunT<RelicIdCap>;

// ---- Small helpers used everywhere -----------------------------------------

inline bool owns(const Run& r, int relic) {
  for (int i = 0; i < r.nRelics; i++) if (r.relics[i].id == relic) return true;
  return false;
}
inline bool hasTrinket(const Run& r, int t) {
  for (int i = 0; i < r.nTrinkets; i++) if (r.trinkets[i] == t) return true;
  return false;
}
inline bool hasMod(const Run& r, int m) { return (r.mods >> m) & 1u; }
inline bool isCalling(const Run& r, int c) { return r.calling == c; }

// ---- The Pacts' rules, rung by rung ---------------------------------------------
// Each rule turns on at its rung (Cat::Pacts' index) and stays on above it; the
// rung table is the ladder. test/test_depth moves rules between rungs to
// measure other ladders. The Deck never writes `rules`.
enum PactRule : uint8_t { PR_lead, PR_iron, PR_ash, PR_bone, PR_silver, PR_obsidian, PR_gold, PactRuleCount };
constexpr uint8_t RungOff = 99;
struct Rules {
  uint8_t rung[PactRuleCount] = {1, 2, 3, 4, 5, 6, 7};   // Lead (the Tithe pays no coin), Iron, Ash, Bone, Silver, Obsidian, Gold
  uint8_t saltRung = RungOff;         // the rung Lead replaced: the Tithe grows x(1 + 0.05 x Night); the bot still measures it
  uint8_t portentChoiceBelow = 3;     // below this rung the dawn offers two Portents and you keep one (0: never)
  uint8_t quietRung = 4;              // from this rung the Deck goes quiet: no offerings-to-go on the Tithe bar,
                                      // no star at the stall, and the Omen waits for its Night (Estimates.h)
};
inline Rules rules;
inline bool pactRung(const Run& r, uint8_t rung) { return r.pact >= rung; }
inline bool pactOn(const Run& r, int rule) { return pactRung(r, rules.rung[rule]); }
inline bool deckQuiet(const Run& r) { return pactRung(r, rules.quietRung); }

// A dawn's Portents still waiting on a choice (Game.h endNight): bit k of
// `pending` says Portent k is one of two, alt[k] is the other (Cat::Portent + 1),
// and `interest` is the Night's interest, for a Doubled interest kept late.
struct PortentChoice { uint8_t pending; uint8_t alt[3]; uint8_t interest; };

inline int relicSlots(const Run& r) {
  if (isCalling(r, Cat::Calling_pilgrim)) return 4 - pactOn(r, PR_gold);
  return RelicSlots + hasMod(r, Cat::Mod_sixth_socket) + isCalling(r, Cat::Calling_chorus) - pactOn(r, PR_gold);
}
inline int slotsUsed(const Run& r) {
  int n = 0;
  for (int i = 0; i < r.nRelics; i++) n += r.relics[i].ed != Cat::Edition_phantom;
  return n;
}
inline int spellSlots(const Run& r) { return SpellSlots + hasMod(r, Cat::Mod_fifth_pocket); }
inline int holdCap(const Run& r) {
  return HoldBase + 2 * hasMod(r, Cat::Mod_satchel) + 3 * hasMod(r, Cat::Mod_bottomless_satchel);
}
inline int holdKinds(const Run& r) {
  int k = 0;
  for (int e = 0; e < EnergyN; e++) k += r.hold[e] > 0;
  return k;
}
inline int bits(uint32_t v) {
  int n = 0;
  while (v) { n += v & 1u; v >>= 1; }
  return n;
}
inline void stat(Run& r, Cat::Stat s, uint32_t add = 1) { r.stats[(int)s] += add; }
inline void best(Run& r, Cat::Stat s, uint32_t v) {
  if (v > r.stats[(int)s]) r.stats[(int)s] = v;
}

} // namespace Game
