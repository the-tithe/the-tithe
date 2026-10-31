#pragma once
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include "Atlas.h"
#include "Career.h"
#include "Rules.h"
#include "Run.h"

// The run's rules: starting a Binding, each Night, drawing and selling at a
// well, spells, Heat and the Wardens, candles, loans, the end of a Night,
// Portents, winning and Overnight. It follows play_run() and play_night()
// in tools/balance_sim.py. The parts the simulator only imagines (walking
// to a well, the hold, drawing) follow DESIGN.md and PROGRESSION.md.
//
// Pure logic with no hardware: the firmware hands it what the scanner and
// motion sensor see, and the tests and the autoplay bot hand it made-up
// worlds.


namespace Game {

// A well as the scanner sees it right now.
struct WellView {
  uint32_t id, seed;
  uint8_t energy;       // the well's own energy (Well.h), before any Glitch shuffle
  uint8_t faction;
  uint8_t channel;
  bool hidden;          // broadcasts no name (a Hush well)
  float sig;            // smoothed dBm
  int mesh;             // routers sharing its network name, 0 if alone
  bool dowsed;          // you dowsed it recently
};

// Where you're standing, and the Deck's own senses.
struct Here {
  int n;                // wells in range
  int ward;
  int ch11;             // wells on channel 11 in range
  bool still;           // held steady (ADV)
  double noise;         // 0..1
  int battery;          // percent, -1 unknown
  uint32_t playMin;     // minutes of play so far, for the atlas
  double playSec;       // seconds of play, for the ward's timer
};

constexpr float ReachDbm = -67;   // close enough to trade (NEAR on the scanner)
constexpr float StrongDbm = -55;  // "standing right next to it"

// ---- Setting up ---------------------------------------------------------------

inline double titheFor(const Run& r, int night, int omen, int eclipse) {
  double t = night < Nights ? Tithes[night] : Tithes[Nights - 1] * pow(OvernightGrowth, night - Nights + 1);
  if (pactRung(r, rules.saltRung)) t *= 1 + 0.05 * night;
  if (owns(r, Cat::R_blood_moon)) t *= 1.35;
  if (omen == Cat::Omen_blood_price) t *= 1.2;
  if (eclipse == Cat::Eclipse_hunger) t *= 1.25;
  if (r.challenge == 9 || r.challenge == 3) t *= 2.5;   // Redline, Glass house
  if (night == 1 && r.owedN2 > 0) t += r.owedN2;
  return t + r.owed;
}

// The Tithe that actually has to be met tonight: Grand tour doubles it until
// you've sold in all four kinds of ward.
inline double effectiveTithe(const Run& r) {
  return r.challenge == 8 && r.n.wardKinds != 0xF ? r.n.tithe * 2 : r.n.tithe;
}
// A record value for the stats, safe for Overnight's huge numbers.
inline uint32_t clampU32(double v) { return v <= 0 ? 0 : v >= 4.0e9 ? 4000000000u : (uint32_t)v; }

inline int visitsFor(const Run& r) {
  int v = isCalling(r, Cat::Calling_courier) ? 10 : Visits;
  if (r.challenge == 2) v = 8;    // Night owl
  if (r.challenge == 9) v = 14;   // Redline
  v += hasMod(r, Cat::Mod_extra_breath) + hasMod(r, Cat::Mod_second_wind);
  v -= 2 * owns(r, Cat::R_overclock) + pactOn(r, PR_iron);
  if (r.n.omen == Cat::Omen_short_night) v -= 2;
  if (r.n.eclipse == Cat::Eclipse_dark) v -= 2;
  return v < 1 ? 1 : v;
}

inline void addRelic(Run& r, int id, int ed = -1, int withered = -1) {
  if (r.nRelics >= Run::MaxRelics) return;
  r.relics[r.nRelics++] = OwnedRelic{(uint8_t)id, (int8_t)ed, (int8_t)withered, 0};
}

inline void removeRelicAt(Run& r, int i) {
  for (int k = i; k + 1 < r.nRelics; k++) r.relics[k] = r.relics[k + 1];
  r.nRelics--;
}

inline void clampCoin(Run& r) {
  if (r.coin < 0) r.coin = 0;
  if (r.challenge == 0 && r.coin > 15) r.coin = 15;   // Shoestring
}

// A new Binding. `seed` makes it reproducible.
inline void newRun(Run& r, const Career& car, int calling, int pact, int challenge, uint32_t seed, uint32_t runNo) {
  memset(&r, 0, sizeof(r));
  r.rng.seed(seed, 7);
  r.runNo = runNo;
  r.calling = (uint8_t)calling;
  r.pact = (uint8_t)pact;
  r.challenge = (int8_t)challenge;
  r.active = 1;
  r.focus = (uint8_t)r.rng.below(3);
  r.coin = StartCoin;
  r.lastE = -1;
  r.lastSpell = -1;
  r.spells[r.nSpells++] = Cat::Spell_glamour;
  r.trinkets[0] = r.trinkets[1] = None8;
  r.loansLeft = 1;
  r.n.lastWard = -1;
  using namespace Cat;
  switch (calling) {
    case Calling_hedge_witch: addRelic(r, R_static_eater); addRelic(r, R_static_caller); break;
    case Calling_hermit: addRelic(r, R_hearthstone); break;
    case Calling_diviner: r.mods |= 1u << Mod_lodestone | 1u << Mod_true_north; break;
    case Calling_cartomancer: r.nSpells = 0; r.spells[r.nSpells++] = Spell_chalk; r.spells[r.nSpells++] = Spell_chalk; break;
    case Calling_contractor: r.coin = 20; r.owedN2 = 0.2 * Tithes[1]; break;
    case Calling_glitch: {
      int rares[RelicCount], k = 0;
      for (int i = 0; i < RelicCount; i++)
        if (Relics[i].rarity == Rare && unlocked(car, Kind::Relic, i)) rares[k++] = i;
      for (int j = 0; j < 2 && k > 0; j++) {
        const int pick = r.rng.below(k);
        addRelic(r, rares[pick]);
        rares[pick] = rares[--k];
      }
      break;
    }
    default: break;
  }
  if (challenge == 3) addRelic(r, R_glass_crown);                              // Glass house
  if (challenge == 9) { addRelic(r, R_overclock); addRelic(r, R_witching_hour); }   // Redline
  // The week's Omens (Nights 2-6) and the Eclipse are drawn now, so the
  // Eclipse can be shown from the start.
  for (int i = 1; i < Nights - 1; i++) r.omens[i] = (uint8_t)(1 + r.rng.below(OmenCount));
  r.eclipse = (int8_t)r.rng.below(EclipseCount);
}

// The Trial that teaches what a Pact takes away, offered after a week lost on
// it (Cat::Challenges index, -1 for none).
inline int trialForPact(int pact) {
  using namespace Cat;
  static const int8_t trial[PactCount] = {-1, 0, 2, 6, 7, 1, 3, 9};
  //  Chalk none; Lead: Shoestring; Iron: Night owl; Ash: Homebody; Bone: Empty pockets;
  //  Silver: One note; Obsidian: Glass house; Gold: Redline.
  return pact >= 0 && pact < PactCount ? trial[pact] : -1;
}

// The map from a well's own energy to what it gives tonight (Glitch shuffles
// Ember, Tide and Gale each Night).
inline int tonightsEnergy(const Run& r, int e) {
  if (isCalling(r, Cat::Calling_glitch) && e < 3) return r.n.perm[e];
  return e;
}

// Start the Night `r.night`.
inline void beginNight(Run& r) {
  using namespace Cat;
  const bool inWeek = r.night < Nights;
  int omen, ecl;
  if (inWeek) {
    omen = r.night < Nights - 1 ? (int)r.omens[r.night] - 1 : -1;
    ecl = r.night == Nights - 1 ? r.eclipse : -1;
  } else {
    omen = (int)r.rng.below(OmenCount);
    ecl = (r.night - Nights) % 3 == 2 ? (int)r.rng.below(EclipseCount) : -1;
  }
  if (omen >= 0 && hasMod(r, Mod_charm_ward) && r.rng.chance(0.5)) omen = -1;
  if (ecl >= 0 && hasMod(r, Mod_eclipse_ward) && r.rng.chance(0.5)) ecl = -1;
  if (omen >= 0) stat(r, Stat::omens_seen);
  memset(&r.n, 0, sizeof(r.n));
  r.n.omen = (int8_t)omen;
  r.n.eclipse = (int8_t)ecl;
  r.n.fadeE = (int8_t)r.rng.below(EnergyN);
  r.n.crownBan = (int8_t)r.rng.below(EnergyN);
  r.n.noPolish = ecl == Eclipse_sleep;
  r.n.wards = 1;
  r.n.lastWard = -1;
  r.n.transmuteE = -1;
  r.n.tithe = titheFor(r, r.night, omen, ecl);
  if (omen == Omen_wardens_walk && r.heat < 3) r.heat = 3;
  r.owed = 0;
  r.n.visits = (int16_t)visitsFor(r);
  for (int e = 0; e < 3; e++) r.n.perm[e] = (uint8_t)e;
  if (isCalling(r, Calling_glitch)) {
    r.focus = (uint8_t)r.rng.below(3);   // the simulator's Glitch: a new focus each Night
    for (int i = 2; i > 0; i--) {
      const int j = r.rng.below(i + 1);
      const uint8_t t = r.n.perm[i]; r.n.perm[i] = r.n.perm[j]; r.n.perm[j] = t;
    }
  }
  if (hasTrinket(r, Trinket_chalk_pouch) && r.nSpells < spellSlots(r)) r.spells[r.nSpells++] = Spell_chalk;
  for (int j = 0; j < 2; j++) r.jobs[j] = Job{};   // jobs only last a Night
}

// ---- Prices ---------------------------------------------------------------------

// How much a well likes each energy: 0.8 to 1.25, fixed by its seed, the same
// on every Deck.
inline double taste(uint32_t seed, int e) {
  const uint32_t bitsE = (seed >> (e * 5 + 3)) & 31;
  return 0.8 + 0.45 * bitsE / 31.0;
}

inline double priceAt(const Run& r, const WellView& w, const Here& h, int e) {
  double p = BasePrice[e] * taste(w.seed, e);
  if (h.ward == Hollow) p *= 1.4;
  else if (h.ward == Fringe) p *= 1.1;
  else if (h.ward == Nexus) p *= 0.85;
  if (r.n.omen == Cat::Omen_crowded_sky && h.ward >= WardW) p *= 0.7;
  if (r.n.omen == Cat::Omen_empty_roads && h.ward <= Fringe) p *= 0.7;
  p *= 1 + 0.2 * r.standing[w.faction] / 100;
  return p;
}

inline int wellFaction(const Run& r, const WellView& w) { return r.challenge == 4 ? (int)Hedge : w.faction; }
inline int wellEnergy(const Run& r, const WellView& w) {
  if (r.challenge == 4) return Static;   // Static storm: every well is open
  return tonightsEnergy(r, w.energy);
}

// ---- Drawing --------------------------------------------------------------------

// Your focus energy resonates: tonight, about half of all wells will also give
// it to you (65% with a Lodestone). Which ones is fixed for the Night, so a
// well you pass twice answers the same way. This is the simulator's focus
// rule (focus_p in make_visit), made into a place.
inline bool givesFocus(const Run& r, const WellView& w) {
  if (isCalling(r, Cat::Calling_pilgrim) || r.challenge == 1) return true;
  uint32_t h = w.seed ^ (r.runNo * 0x27D4EB2Fu) ^ ((uint32_t)(r.night + 1) * 0x165667B1u);
  h ^= h >> 15; h *= 0x85EBCA77u; h ^= h >> 13;
  const double p = 0.5 + 0.15 * hasMod(r, Cat::Mod_lodestone);
  return (h % 1000) < p * 1000;
}

enum class Why : uint8_t {
  Ok, NoRun, NightClosed, OutOfVisits, TooFar, Dry, AlreadySold, Banned, Empty, HoldFull, NewWell,
};

inline const char* whyText(Why w) {
  switch (w) {
    case Why::NoRun: return "NO BINDING";
    case Why::NightClosed: return "THE NIGHT IS PAID";
    case Why::OutOfVisits: return "NO VISITS LEFT";
    case Why::TooFar: return "TOO FAR: WALK CLOSER";
    case Why::Dry: return "DRAWN DRY: IT NEEDS TO SETTLE";
    case Why::AlreadySold: return "OFFERED: IT NEEDS TO SETTLE";
    case Why::Banned: return "THE WARDED WON'T TOUCH STATIC";
    case Why::Empty: return "NOTHING TO OFFER";
    case Why::HoldFull: return "YOUR HOLD IS FULL";
    case Why::NewWell: return "HOMEBODY: ONLY OLD WELLS";
    default: return "";
  }
}

// Homebody (from Night 2): wells you've visited before buy and refill any
// number of times, since they're the only ones that will deal with you.
inline bool homebodyOpen(const Run& r, const AtlasEntry* a) {
  return r.challenge == 6 && r.night >= 1 && a && a->runNo == r.runNo && a->nights != 0;
}

// A used well settles after a while of listening and can be drawn from and
// offered to again the same Night, so a street with few wells still works.
// The app sets the clock each frame (minutes of play, like AtlasEntry::lastPlay).
inline uint32_t playMinNow = 0;
constexpr uint32_t SettleMin = 20;
constexpr double ResettledThirst = 0.75;   // in a Quarter or Nexus, a well that's had an offering tonight wants less
inline bool settled(const AtlasEntry* a) { return a && a->lastPlay + SettleMin <= playMinNow; }
// Minutes until a used well settles, 0 if it has.
inline int settleLeft(const AtlasEntry* a) {
  return !a || settled(a) ? 0 : (int)(a->lastPlay + SettleMin - playMinNow);
}

inline int drawsAllowed(const Run& r, const AtlasEntry* a) {
  if (homebodyOpen(r, a)) return 1000;
  // Visited on an earlier Night of this run (a sale tonight doesn't count).
  const bool visitedBefore =
      a && a->runNo == r.runNo && (a->nights & ~(uint16_t)(1u << Atlas::nightBit(r.night))) != 0;
  return isCalling(r, Cat::Calling_hermit) && visitedBefore ? 2 : 1;
}

inline Why canDraw(const Run& r, const Atlas& atlas, const WellView& w) {
  if (!r.active) return Why::NoRun;
  if (r.n.met) return Why::NightClosed;
  if (w.sig < ReachDbm) return Why::TooFar;
  const AtlasEntry* a = atlas.find(w.id);
  if (a && a->runNo == r.runNo && a->drawnNight >= 0) {
    // drawnNight holds the Night; a second draw tonight is marked by +1000.
    const int night = a->drawnNight % 1000, times = a->drawnNight >= 1000 ? 2 : 1;
    const bool drawnSinceSettling = a->drawnNight >= 2000;   // one draw per settling, until you offer here
    if (night == r.night && times >= drawsAllowed(r, a) && !homebodyOpen(r, a) && (!settled(a) || drawnSinceSettling))
      return Why::Dry;
  }
  int held = 0;
  for (int e = 0; e < EnergyN; e++) held += r.hold[e];
  if (held >= holdCap(r) && !r.n.surge) return Why::HoldFull;
  return Why::Ok;
}

// Units a draw at `w` takes, before a shaky hand spills any: 4 at the edge of
// reach, 8 right next to it, plus satchels, the Courier and SURGE.
inline double drawUnits(const Run& r, const WellView& w) {
  double f = (w.sig - ReachDbm) / (StrongDbm + 10 - ReachDbm);
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  double units = 4 + 4 * f + 2 * hasMod(r, Cat::Mod_satchel) + 3 * hasMod(r, Cat::Mod_bottomless_satchel);
  if (isCalling(r, Cat::Calling_courier)) units *= 1.4;
  if (r.n.surge) units *= 2.5;
  return units;
}

struct Drawn {
  Why why;
  int energy;
  int units;
  int spilled;
};

// Draw energy from a well into the hold. `focus` asks for your focus energy
// instead of the well's own (only where givesFocus). `steadiness` is 0..1
// over the draw (1 without a motion sensor): a shaky hand spills up to a
// quarter.
inline Drawn draw(Run& r, Atlas& atlas, const WellView& w, const Here& h, double steadiness, bool focus = false) {
  Drawn d{canDraw(r, atlas, w), -1, 0, 0};
  if (d.why != Why::Ok) return d;
  using namespace Cat;
  int e = focus && givesFocus(r, w) ? r.focus : wellEnergy(r, w);
  if (owns(r, R_hush_lantern) && r.rng.chance(0.14)) e = Hush;
  else if ((owns(r, R_static_eater) || owns(r, R_black_ledger) || owns(r, R_smugglers_lining) ||
            owns(r, R_wanted_poster)) && r.rng.chance(0.15)) e = Static;
  if (isCalling(r, Calling_pilgrim) || r.challenge == 1) e = r.focus;
  if (r.n.transmuteLeft > 0) e = r.n.transmuteE;   // TRANSMUTE: for its next 4 sales
  const double units = drawUnits(r, w);
  const bool surged = r.n.surge;
  r.n.surge = 0;
  const double s = steadiness < 0 ? 0 : steadiness > 1 ? 1 : steadiness;
  const int whole = (int)lround(units);
  int got = (int)lround(units * (0.75 + 0.25 * s));
  d.spilled = whole - got;
  int held = 0;
  for (int k = 0; k < EnergyN; k++) held += r.hold[k];
  int room = holdCap(r) - held;
  if (got > room && !surged) got = room > 0 ? room : 0;   // a SURGE may overfill the hold
  if (got < 0) got = 0;
  r.hold[e] = (uint8_t)(r.hold[e] + got > 255 ? 255 : r.hold[e] + got);
  d.energy = e;
  d.units = got;
  // A draw doesn't restart the settling clock of a well that's had an offering
  // tonight, so a settled well can be drawn from and then offered to.
  const AtlasEntry* pre = atlas.find(w.id);
  const bool keepClock = pre && pre->runNo == r.runNo && pre->soldNight == r.night;
  const uint32_t clock = keepClock ? pre->lastPlay : 0;
  AtlasEntry& a = atlas.touch(w.id, w.seed, r.runNo, h.playMin);
  if (keepClock) a.lastPlay = clock;
  a.energy = w.energy;   // the well's own energy, before any Glitch shuffle
  a.faction = w.faction;
  a.ward = (uint8_t)(h.ward + 1);
  // drawnNight: the Night, +1000 for a second draw, +2000 for a draw since the
  // well last settled after an offering (one draw per settling, until you offer).
  if (keepClock && settled(pre) && a.drawnNight >= 0 && a.drawnNight % 1000 == r.night) a.drawnNight = (int16_t)(2000 + r.night);
  else if (a.drawnNight >= 0 && a.drawnNight % 1000 == r.night) a.drawnNight = (int16_t)(1000 + r.night);
  else a.drawnNight = r.night;
  return d;
}

// The hold slowly leaks: one unit every 15 minutes of play, from the biggest
// stack. Hush never leaks, nor does Static with a Static eater.
inline int leak(Run& r, uint32_t playMs) {
  if (r.holdLeakMs == 0 || playMs < r.holdLeakMs) { r.holdLeakMs = playMs; return 0; }
  int lost = 0;
  while (playMs - r.holdLeakMs >= 15u * 60000u) {
    r.holdLeakMs += 15u * 60000u;
    int big = -1;
    for (int e = 0; e < EnergyN; e++) {
      if (e == Hush || (e == Static && owns(r, Cat::R_static_eater))) continue;
      if (r.hold[e] > 0 && (big < 0 || r.hold[e] > r.hold[big])) big = e;
    }
    if (big >= 0) { r.hold[big]--; lost++; }
  }
  return lost;
}

// ---- Faction jobs ----------------------------------------------------------------
// About a third of wells offer a small job from their faction for the Night.
// Done, it pays coin at once and raises your standing with that faction,
// which opens faction stalls in the shop and the faction Grimoire pages.

namespace Jobs {
enum Kind : uint8_t { None, Deliver, Wander, Crowd, Smuggle };
}

// The job a well offers tonight, if any. The same well offers the same job
// all Night.
// Heat moves in steps of 0.5 and 0.1; keeping it in whole tenths stops 2.9999 from reading as 3.0.
inline double tenths(double v) { return round(v * 10) / 10; }

// The Wardens never come for a Hedge witch.
inline bool wardensCanCome(const Run& r) { return !isCalling(r, Cat::Calling_hedge_witch); }

// Heat after one more Static offering (the 0.5 a Static offering adds, capped at 4).
inline double heatAfterStatic(const Run& r) {
  double h = r.heat;
  if (!(r.n.veil || owns(r, Cat::R_static_eater))) h = h + 0.5 > 4 ? 4 : h + 0.5;
  if (r.challenge != 4) h = h - 0.1 < 0 ? 0 : h - 0.1;   // every offering cools it (not in a Static storm)
  return tenths(h);
}

inline Job jobOffer(const Run& r, const WellView& w) {
  Job j{};
  uint32_t h = w.seed ^ (r.runNo * 0x9E3779B1u) ^ ((uint32_t)r.night * 0x85EBCA77u);
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
  if (h % 3 != 0 || !r.active || r.n.met) return j;
  const int f = w.faction;
  if ((r.n.jobsDone >> f) & 1) return j;   // one job per faction a Night
  j.faction = (uint8_t)f;
  switch (f) {
    case Hearth: j.kind = Jobs::Deliver; j.energy = (uint8_t)((h >> 4) % 3); j.need = (uint8_t)(5 + (h >> 8) % 4);
                 j.reward = 4; break;
    case Warded: j.kind = Jobs::Wander; j.need = 3; j.reward = 5; break;
    case Syndicate: j.kind = Jobs::Crowd; j.need = 2; j.reward = 5; break;
    default: j.kind = Jobs::Smuggle; j.need = 1; j.reward = 4; break;
  }
  return j;
}

inline const char* jobText(const Job& j, char* buf, int cap) {
  static const char* const en[] = {"EMBER", "TIDE", "GALE", "HUSH", "STATIC"};
  switch (j.kind) {
    case Jobs::Deliver: snprintf(buf, cap, "OFFER %d %s AT HEARTH WELLS", j.need, en[j.energy]); break;
    case Jobs::Wander: snprintf(buf, cap, "OFFER IN %d KINDS OF WARD", j.need); break;
    case Jobs::Crowd: snprintf(buf, cap, "OFFER %d TIMES IN A QUARTER OR NEXUS", j.need); break;
    case Jobs::Smuggle: snprintf(buf, cap, "OFFER STATIC TONIGHT"); break;
    default: snprintf(buf, cap, "-"); break;
  }
  return buf;
}

// Take a well's job. False if you already have one from that faction or two
// in all.
inline bool takeJob(Run& r, const Job& j) {
  if (!j.kind || ((r.n.jobsDone >> j.faction) & 1)) return false;
  int free = -1;
  for (int i = 0; i < 2; i++) {
    if (r.jobs[i].kind && r.jobs[i].faction == j.faction && !r.jobs[i].done) return false;
    if ((!r.jobs[i].kind || r.jobs[i].done) && free < 0) free = i;
  }
  if (free < 0) return false;
  r.jobs[free] = j;
  return true;
}

// After a sale: move jobs along, and pay any that finish. Returns coin paid.
// A delivery adds up across the Night's offerings (7 Tide over two Hearth
// wells is 7 Tide delivered), so the most common favor can be finished.
inline int progressJobs(Run& r, int energy, double units, int faction, int ward) {
  int paid = 0;
  for (int i = 0; i < 2; i++) {
    Job& j = r.jobs[i];
    if (!j.kind || j.done) continue;
    switch (j.kind) {
      case Jobs::Deliver:
        if (faction == Hearth && energy == j.energy) {
          const int got = j.got + (int)units;
          j.got = (uint8_t)(got > j.need ? j.need : got);
        }
        break;
      case Jobs::Wander: j.got = (uint8_t)bits(r.n.wardKinds); break;
      case Jobs::Crowd: if (ward >= WardW) j.got++; break;
      case Jobs::Smuggle: if (energy == Static) j.got = 1; break;
      default: break;
    }
    if (j.got >= j.need) {
      j.done = 1;
      r.n.jobsDone |= (uint8_t)(1u << j.faction);
      r.coin += j.reward;
      r.standing[j.faction] = r.standing[j.faction] + 8 > 100 ? 100 : r.standing[j.faction] + 8;
      paid += j.reward;
      stat(r, Cat::Stat::jobs_done);
    }
  }
  clampCoin(r);
  return paid;
}

// ---- Selling ------------------------------------------------------------------

inline Why canSell(const Run& r, const Atlas& atlas, const WellView& w, int e, bool remote = false) {
  if (!r.active) return Why::NoRun;
  if (r.n.met) return Why::NightClosed;
  if (r.n.used >= r.n.visits) return Why::OutOfVisits;
  if (remote ? !r.n.echo : w.sig < ReachDbm) return Why::TooFar;
  const AtlasEntry* a = atlas.find(w.id);
  const bool visitedBefore = a && a->runNo == r.runNo && a->nights != 0;
  if (a && a->runNo == r.runNo && a->soldNight == r.night && !homebodyOpen(r, a) && !settled(a)) return Why::AlreadySold;
  if (e < 0 || r.hold[e] == 0) return Why::Empty;
  if (r.challenge == 6 && r.night >= 1 && !visitedBefore) return Why::NewWell;   // Homebody
  if (e == Static && wellFaction(r, w) == Warded && !hasTrinket(r, Cat::Trinket_fence)) return Why::Banned;
  return Why::Ok;
}

struct Sold {
  Why why;
  Sale c;              // the sale as scored, for the show
  int energy;
  double unitsHeld;
  double resonance;    // after spells and the Witching hour
  double coin;
  bool blocked;        // the Wardens took it
  double taken;        // what they took, for the show
  bool glamour, witching;
  bool closed;         // this sale paid the Tithe
  bool isNew;
  int jobCoin;         // coin from a job this sale finished
};

// Build the sale context for selling energy `e` at well `w`.
inline void saleContext(Sale& c, const Run& r, const Atlas& atlas, const WellView& w, const Here& h, int e) {
  using namespace Cat;
  clearSale(c);
  const AtlasEntry* a = atlas.find(w.id);
  const bool thisRun = a && a->runNo == r.runNo;
  const uint16_t nights = thisRun ? a->nights : 0;
  const uint16_t tonight = (uint16_t)(1u << Atlas::nightBit(r.night));
  c.e = e;
  const int lvl = r.attune[e];
  c.lvl = lvl;
  c.units = r.hold[e];
  if (h.ward == Hollow) c.units *= 0.85;
  else if (h.ward == Nexus) c.units *= 1.2;
  if (pactOn(r, PR_ash)) c.units *= 0.9;
  if (r.n.omen == Omen_low_tide) c.units *= 0.75;
  c.price = priceAt(r, w, h, e);
  // Offered again after settling: on a busy street, where fresh wells are easy, it wants less.
  if (thisRun && a->soldNight == r.night && !homebodyOpen(r, a) && h.ward >= WardW) c.price *= ResettledThirst;
  c.mult = 1 + lvl + lvl * hasMod(r, Mod_tuning_core);
  c.n = r.n.omen == Omen_fog ? h.n / 2 : h.n;
  c.ward = h.ward;
  c.faction = wellFaction(r, w);
  c.hidden = (w.hidden && r.challenge != 4) || owns(r, R_the_unheard);
  c.channel = w.channel;
  c.ch11 = h.ch11;
  c.strong = w.sig >= StrongDbm;
  c.mesh = w.mesh >= 2 ? w.mesh : 0;
  c.revisit = (nights & ~tonight) != 0;
  c.nightsSeen = bits(nights | tonight);
  c.glyph = thisRun ? a->glyph : -1;
  c.isNew = !a || a->visits == 0 || c.glyph == Glyph_beacon;
  c.wardChanged = r.n.lastWard >= 0 && r.n.lastWard != h.ward;
  c.wardsTonight = r.n.wards + (c.wardChanged ? 1 : 0);
  c.noise = h.noise;
  c.still = h.still;
  c.batteryLow = h.battery >= 0 && h.battery < 20;
  c.dowsed = w.dowsed || (hasMod(r, Mod_true_north) && r.n.used % 3 == 0);
  c.holdKinds = holdKinds(r);
  c.oneKind = c.holdKinds == 1;
  c.vi = r.n.used;
  c.lastE = r.lastE;
  c.ghost = c.hidden && owns(r, R_ghost_key);
}

inline bool wardHolds(const Run& r, const Here& h) { return r.n.wardUntil > h.playSec; }

// Sell everything you carry of energy `e` at well `w`.
inline Sold sell(Run& r, Atlas& atlas, const WellView& w, const Here& h, int e, bool remote = false,
                 RecentSales* recent = nullptr) {
  using namespace Cat;
  Sold s{};
  s.why = canSell(r, atlas, w, e, remote);
  if (s.why != Why::Ok) return s;
  s.energy = e;
  saleContext(s.c, r, atlas, w, h, e);
  s.isNew = s.c.isNew;
  s.unitsHeld = r.hold[e];
  if (recent) recent->add(s.c);
  Sale c = s.c;
  SaleResult sr = score(c, r);
  s.c = c;
  double res = sr.resonance, coin = sr.coin;
  if (r.challenge == 5 && s.c.revisit) { res = 0; coin = 0; }   // The long way
  // The Wardens.
  if (r.heat >= 3 && !isCalling(r, Calling_hedge_witch) && !wardHolds(r, h)) {
    if (r.rng.chance(WardenBlock * (owns(r, R_devils_meter) ? 1.7 : 1))) {
      s.taken = res;
      res = coin = 0;
      s.blocked = true;
    } else {
      stat(r, Stat::warden_escapes);
    }
  }
  if (r.n.glamour && res > 0) { res *= 2; s.glamour = true; r.n.glamour = 0; }   // kept if the sale was taken
  if (owns(r, R_witching_hour) && r.n.total + 3 * res >= effectiveTithe(r)) { res *= 3; s.witching = true; }
  if (r.n.transmuteLeft > 0) r.n.transmuteLeft--;
  // Bookkeeping, as in play_night().
  const int faction = wellFaction(r, w);
  if (s.c.wardChanged) r.n.wards++;
  r.n.lastWard = (int8_t)h.ward;
  r.lastE = (int8_t)e;
  r.sales++;
  r.n.kinds |= (uint8_t)(1u << e);
  r.n.wardKinds |= (uint8_t)(1u << h.ward);
  r.n.hollows += h.ward == Hollow;
  r.wardSeen[h.ward]++;
  if (s.c.isNew) {
    r.newWells += 1;
    r.n.newTonight++;
    if (owns(r, R_pilgrims_staff)) r.state[R_pilgrims_staff] += 0.05;
  }
  if (faction == Hearth && owns(r, R_picket_fence)) r.state[R_picket_fence] += 0.25;
  if (s.c.hidden && owns(r, R_locked_diary)) r.state[R_locked_diary] += 0.15;
  if (e == Static) {
    if (owns(r, R_black_ledger)) r.state[R_black_ledger] += 0.2;
    if (!(owns(r, R_static_eater) || r.n.veil)) r.heat = r.heat + 0.5 > 4 ? 4 : r.heat + 0.5;
    r.standing[Warded] -= 2;
    r.standing[Syndicate] -= 2;
  }
  if (r.challenge != 4) r.heat = r.heat - 0.1 < 0 ? 0 : r.heat - 0.1;   // Static storm: Heat never cools
  r.heat = tenths(r.heat);
  r.standing[faction] = r.standing[faction] + 1 > 100 ? 100 : r.standing[faction] + 1;
  stat(r, Stat::sales_total);
  stat(r, (Stat)((int)Stat::ember_sold + e));
  if (s.c.hidden) stat(r, Stat::hidden_found);
  if (h.ward == Nexus) stat(r, Stat::visit_nexus);
  if (h.ward == Hollow) stat(r, Stat::hollow_sales);
  if (faction == Hearth) stat(r, Stat::hearth_sales);
  best(r, Stat::max_networks, (uint32_t)h.n);
  if (h.ward == Nexus) best(r, Stat::big_sale_nexus, clampU32(res));
  best(r, Stat::big_units, clampU32(s.c.units));
  if (faction <= Warded) best(r, (Stat)((int)Stat::standing_hedge + faction), (uint32_t)(r.standing[faction] > 0 ? r.standing[faction] : 0));
  if (w.channel >= 1 && w.channel <= 13) r.channels |= (uint16_t)(1u << w.channel);
  // The atlas remembers.
  AtlasEntry& a = atlas.touch(w.id, w.seed, r.runNo, h.playMin);
  if (a.visits == 0) atlas.unique++;
  a.visits++;
  a.nights |= (uint16_t)(1u << Atlas::nightBit(r.night));
  a.soldNight = r.night;
  if (a.drawnNight >= 2000) a.drawnNight = (int16_t)(a.drawnNight - 1000);   // the offering starts the next settling
  a.energy = w.energy;
  a.faction = w.faction;
  a.ward = (uint8_t)(h.ward + 1);
  best(r, Stat::same_well_nights, (uint32_t)bits(a.nights));
  // Money and the Tithe.
  if (remote) r.n.echo = 0;
  s.jobCoin = s.blocked ? 0 : progressJobs(r, e, s.unitsHeld, faction, h.ward);   // a taken offering does nothing for a favor
  r.hold[e] = 0;
  r.n.used++;
  s.resonance = res;
  s.coin = coin;
  r.n.total += res;
  r.n.coinEarned += coin;
  if (r.n.total >= effectiveTithe(r)) {
    r.n.met = 1;
    r.n.closedBy = 1;
    s.closed = true;
  }
  return s;
}

// ---- When the visits run out -----------------------------------------------------

inline int candlePrice(const Run& r) {
  const int k = r.n.candles < 3 ? r.n.candles : 2;
  return Candles[k] * (pactOn(r, PR_obsidian) ? 2 : 1);
}
inline bool canLightCandle(const Run& r) {
  return r.active && !r.n.met && r.n.used >= r.n.visits && r.n.candles < 3 && r.coin >= candlePrice(r);
}
inline void lightCandle(Run& r) {
  if (!canLightCandle(r)) return;
  r.coin -= candlePrice(r);
  r.n.candles++;
  r.n.visits++;
  stat(r, Cat::Stat::candles);
}

inline bool canTakeLoan(const Run& r) {
  return r.active && !r.n.met && r.night < Nights - 1 && r.loansLeft > 0 && !pactOn(r, PR_obsidian) &&
         r.n.total >= effectiveTithe(r) * LoanFloor;
}
// The Syndicate pays the rest; 1.25x what they paid goes on the next Tithe.
inline void takeLoan(Run& r) {
  if (!canTakeLoan(r)) return;
  r.loansLeft--;
  const double gap = effectiveTithe(r) - r.n.total;
  r.owed = (gap > 0 ? gap : 0) * (hasTrinket(r, Cat::Trinket_moneylender) ? 1.1 : LoanInterest);
  r.n.coinEarned = 0;
  r.n.used = r.n.visits;
  r.n.met = 1;
  r.n.closedBy = 2;
  stat(r, Cat::Stat::loans);
}

// A spell that would give more visits: HASTE, or RECALL when HASTE was the last one cast.
inline int hasteSlot(const Run& r) {
  for (int i = 0; i < r.nSpells; i++) {
    if (r.spells[i] == Cat::Spell_haste) return i;
    if (r.spells[i] == Cat::Spell_recall && r.lastSpell == Cat::Spell_haste) return i;
  }
  return -1;
}

// Out of visits, no candle affordable, no loan, no HASTE: the Binding is lost.
inline bool stuck(const Run& r) {
  return r.active && !r.n.met && r.n.used >= r.n.visits && !canLightCandle(r) && !canTakeLoan(r) &&
         hasteSlot(r) < 0;
}

// ---- Portents ---------------------------------------------------------------------

inline bool rollEdition(Run& r, double mult, int& ed, int night);
inline int rollRelic(Run& r, const Career& car, int rarity, bool multiplier, int& ed, int& withered, int night);

inline int pickWeighted(Run& r, const double* w, int n) { return r.rng.weighted(w, n); }

struct PortentResult {
  int portent;
  int relic;     // a relic given, -1 none
  int coin;      // coin given (or a gift that didn't fit)
  int spell;     // a spell given, -1 none
};

inline int rollPortent(Run& r) {
  static const double w[Cat::PortentCount] = {3, 2, 3, 2, 2, 2, 2, 2, 2, 0.3};
  return (int)pickWeighted(r, w, Cat::PortentCount);
}

// Give Portent `kind` for the Night just paid (`paidNight`, 0-based). A
// Doubled interest kept at the dawn screen, after endNight has paid out, is
// handed that Night's `interest` and pays it again on the spot; endNight
// itself passes -1 and pays it through the shop flag.
inline PortentResult grantPortent(Run& r, const Career& car, int kind, int paidNight, int interest = -1) {
  using namespace Cat;
  PortentResult out{kind, -1, 0, -1};
  switch (out.portent) {
    case Portent_windfall: r.coin += 10; out.coin = 10; break;
    case Portent_doubled_interest:
      if (interest < 0) r.shopFlags |= SF_DoubleInterest;
      else { r.coin += interest; out.coin = interest; }
      break;
    case Portent_rare_sight: r.shopFlags |= SF_RareSight; break;
    case Portent_free_rerolls: r.shopFlags |= SF_FreeRerolls; break;
    case Portent_shine: r.shopFlags |= SF_Shine; break;
    case Portent_tuning: if (r.attune[r.focus] < 250) r.attune[r.focus]++; break;
    case Portent_scroll:
      if (r.nSpells < spellSlots(r)) {
        int pool[SpellCount], k = 0;
        for (int i = 0; i < SpellCount; i++) if (i != Spell_summon && unlocked(car, Kind::Spell, i)) pool[k++] = i;
        if (k) { out.spell = pool[r.rng.below(k)]; r.spells[r.nSpells++] = (uint8_t)out.spell; }
      }
      break;
    case Portent_chalk_mark:
      if (r.nGlyphTokens < 4) r.glyphTokens[r.nGlyphTokens++] = (uint8_t)r.rng.below(GlyphCount);
      stat(r, Stat::glyphs_total);
      break;
    case Portent_gift: {
      int ed, wi;
      const int id = rollRelic(r, car, paidNight >= 3 ? Rare : Uncommon, false, ed, wi, paidNight);
      if (id >= 0) {
        if (slotsUsed(r) < relicSlots(r) || ed == Edition_phantom) { addRelic(r, id, ed, wi); out.relic = id; }
        else { out.coin = Relics[id].rarity == Rare ? PriceRare : PriceUncommon; r.coin += out.coin; }
      }
      break;
    }
    case Portent_conjunction: {
      int pool[RelicCount], k = 0;
      for (int i = 0; i < RelicCount; i++)
        if (Relics[i].legendary && unlocked(car, Kind::Relic, i) && !owns(r, i)) pool[k++] = i;
      if (k) {
        const int id = pool[r.rng.below(k)];
        if (slotsUsed(r) < relicSlots(r)) { addRelic(r, id); out.relic = id; }
        else { out.coin = PriceRare; r.coin += out.coin; }
      }
      break;
    }
    default: break;
  }
  stat(r, Stat::portents);
  clampCoin(r);
  return out;
}

inline PortentResult applyPortent(Run& r, const Career& car) { return grantPortent(r, car, rollPortent(r), r.night); }

// ---- The end of a Night -------------------------------------------------------

struct NightEnd {
  bool paid;
  bool won;            // Night 7 paid: the Binding is won
  int spare;           // visits left over
  int trade, spareCoin, reward, interest, gilt, thread, skim;   // coin, by source
  int portents;
  PortentResult got[3];
  bool mercy;
  bool withered;       // a Withered relic crumbled
  bool repaid;         // a loan paid off tonight (sits in what was padding, so saves keep their layout)
  uint8_t favors;      // coin from favors finished tonight, for the dawn tally (the last padding byte)
  int gift = -1;       // Empty pockets: the Night's free relic
};
static_assert(sizeof(NightEnd) == 96 && offsetof(NightEnd, gift) == 92, "NightEnd is saved as-is: nothing may move");

// Close a paid Night (by Tithe or loan) and pay out. Call once. With `choice`,
// below the Portent-choice rung each Portent earned is rolled as one of two and
// left for choosePortent(); without it every Portent is given at once.
inline NightEnd endNight(Run& r, const Career& car, int battery, PortentChoice* choice = nullptr) {
  using namespace Cat;
  NightEnd out{};
  if (choice) *choice = PortentChoice{};
  out.paid = r.n.met != 0;
  if (!out.paid) return out;
  const bool loan = r.n.closedBy == 2;
  const bool inWeek = r.night < Nights;
  const bool lean = pactOn(r, PR_lead);   // Lead: paying the Tithe earns no coin
  const bool choose = choice && rules.portentChoiceBelow && r.pact < rules.portentChoiceBelow;
  const int spare = r.n.visits - r.n.used;
  out.spare = spare;
  if (!loan && owns(r, R_patience)) r.state[R_patience] += spare;
  if (!loan && r.hadLoan) { stat(r, Stat::loans_repaid); out.repaid = true; }
  r.hadLoan = r.owed > 0;
  stat(r, Stat::nights_paid);
  if (owns(r, R_tally_stick)) r.state[R_tally_stick] += 1.0;
  if (owns(r, R_ritual_candle)) r.state[R_ritual_candle] += 0.15;
  if (spare <= 1 || r.n.candles > 0 || (loan && r.owed > 0)) {
    r.shopFlags |= SF_Mercy;
    stat(r, Stat::mercies);
    out.mercy = true;
  }
  r.streak = spare >= 6 ? r.streak + 1 : 0;
  best(r, Stat::fast_closes, (uint32_t)r.streak);
  if (r.n.used == 1 && !loan) stat(r, Stat::one_visit_close);
  if (r.n.total >= 3 * effectiveTithe(r) || spare >= 8) stat(r, Stat::op_nights);
  if (battery >= 0 && battery < 10 && !loan) stat(r, Stat::low_battery_close);
  best(r, Stat::kinds_night, (uint32_t)bits(r.n.kinds));
  best(r, Stat::ward_changes_night, (uint32_t)(r.n.wards - 1));
  best(r, Stat::hollows_one_run, r.wardSeen[Hollow]);
  best(r, Stat::new_wells_night, r.n.newTonight);
  if (!inWeek) {
    r.overnightNights++;
    best(r, Stat::overnight_best, (uint32_t)r.overnightNights);
  }
  int cap = InterestCap + 5 * hasTrinket(r, Trinket_misers_purse);
  if (isCalling(r, Calling_night_clerk)) cap *= 2;
  int interest = (int)(r.coin / InterestPer);
  if (interest > cap) interest = cap;
  if (interest < 0) interest = 0;
  if (owns(r, R_piggy_bank)) r.state[R_piggy_bank] += 0.5 * interest;
  int gilt = 0;
  for (int i = 0; i < r.nRelics; i++) gilt += r.relics[i].ed == Edition_gilt ? 2 : 0;
  const int thread = hasTrinket(r, Trinket_golden_thread) && spare >= 4 ? 4 : 0;
  int skim = 0;
  if (hasTrinket(r, Trinket_tithe_skimmer)) {
    skim = (int)((r.n.total / r.n.tithe - 1) / 0.5);
    skim = skim < 0 ? 0 : skim > 10 ? 10 : skim;
  }
  out.trade = loan ? 0 : (int)r.n.coinEarned;
  out.spareCoin = spare;
  out.reward = lean ? 0 : NightReward;
  out.interest = interest;
  out.gilt = gilt;
  out.thread = thread;
  out.skim = skim;
  // Favors paid out as they finished; the tally shows them beside the rest.
  int favors = 0;
  for (int i = 0; i < 2; i++) if (r.jobs[i].kind && r.jobs[i].done) favors += r.jobs[i].reward;
  out.favors = (uint8_t)(favors > 255 ? 255 : favors);
  r.coin = r.coin + out.trade + out.spareCoin + out.reward + interest + gilt + thread + skim;
  clampCoin(r);
  const int shift = r.challenge == 2 ? 2 : 0;   // Night owl: Portents start 2 sooner
  for (int k = 0; k < 3; k++) {
    if (spare < PortentAt[k] - shift || out.portents >= 3) continue;
    if (!choose) { out.got[out.portents++] = applyPortent(r, car); continue; }
    const int a = rollPortent(r);
    int b = rollPortent(r);
    for (int tries = 0; b == a && tries < 8; tries++) b = rollPortent(r);
    if (b == a) { out.got[out.portents++] = grantPortent(r, car, a, r.night); continue; }
    out.got[out.portents] = PortentResult{a, -1, 0, -1};
    choice->alt[out.portents] = (uint8_t)(b + 1);
    choice->pending |= (uint8_t)(1u << out.portents);
    out.portents++;
  }
  if (choice) choice->interest = (uint8_t)(interest > 255 ? 255 : interest);
  // Doubled interest: tonight's interest pays again.
  if (r.shopFlags & SF_DoubleInterest) {
    r.shopFlags &= ~SF_DoubleInterest;
    r.coin += interest;
    out.interest += interest;
    clampCoin(r);
  }
  if (r.challenge == 7) {   // Empty pockets: no shop, but a relic and an Attunement each Night
    int ed, wi;
    const int id = rollRelic(r, car, r.night >= 3 ? Cat::Rare : Cat::Uncommon, false, ed, wi, r.night);
    if (id >= 0 && slotsUsed(r) < relicSlots(r)) { addRelic(r, id, ed, wi); out.gift = id; }
    if (r.attune[r.focus] < 250) r.attune[r.focus]++;
  }
  for (int i = r.nRelics - 1; i >= 0; i--) {
    if (r.relics[i].withered < 0) continue;
    if (--r.relics[i].withered <= 0) { removeRelicAt(r, i); out.withered = true; }
  }
  best(r, Stat::max_coin, (uint32_t)r.coin);
  if (r.night == Nights - 1) {
    out.won = true;
    r.won = 1;
  }
  r.night++;
  r.n.met = 0;
  return out;
}

// Keep one of Portent k's two: the one endNight rolled first, or (`other`) the
// alternative. Does nothing once k is settled.
inline PortentResult choosePortent(Run& r, const Career& car, NightEnd& e, PortentChoice& ch, int k, bool other) {
  if (k < 0 || k >= e.portents || !((ch.pending >> k) & 1)) return e.got[k];
  const int kind = other ? ch.alt[k] - 1 : e.got[k].portent;
  e.got[k] = grantPortent(r, car, kind, r.night - 1, ch.interest);   // endNight has moved on a Night
  ch.pending &= (uint8_t)~(1u << k);
  ch.alt[k] = 0;
  return e.got[k];
}

// ---- Spells ---------------------------------------------------------------------

enum class Cast : uint8_t { Ok, NoSlot, NeedWell, NothingToDo, NoRun };

struct CastResult {
  Cast how;
  int glyph = -1;      // CHALK: the glyph carved
  int relic = -1;      // KINDLE / SUMMON: the relic touched
  int edition = -1;
  int spell = -1;      // RECALL: the spell returned
};

inline void removeSpellAt(Run& r, int i) {
  for (int k = i; k + 1 < r.nSpells; k++) r.spells[k] = r.spells[k + 1];
  r.nSpells--;
}

// Cast the spell in slot `slot`. `here` is the well you're at, or null.
inline CastResult castSpell(Run& r, Atlas& atlas, const Career& car, int slot, const WellView* here, const Here& h) {
  using namespace Cat;
  CastResult out{Cast::Ok};
  if (!r.active) { out.how = Cast::NoRun; return out; }
  if (slot < 0 || slot >= r.nSpells) { out.how = Cast::NothingToDo; return out; }
  const int sp = r.spells[slot];
  // Check first, so a spell that can't work isn't used up.
  if (sp == Spell_chalk && (!here || here->sig < ReachDbm)) { out.how = Cast::NeedWell; return out; }
  if (sp == Spell_summon) {
    bool any = false;
    for (int i = 0; i < RelicCount; i++) any |= Relics[i].legendary && unlocked(car, Kind::Relic, i) && !owns(r, i);
    if (!any) { out.how = Cast::NothingToDo; return out; }
    if (slotsUsed(r) >= relicSlots(r)) { out.how = Cast::NoSlot; return out; }
  }
  if (sp == Spell_recall && r.lastSpell < 0) { out.how = Cast::NothingToDo; return out; }
  if (sp == Spell_kindle) {
    bool plain = false;
    for (int i = 0; i < r.nRelics; i++) plain |= r.relics[i].ed < 0;
    if (!plain) { out.how = Cast::NothingToDo; return out; }
  }
  if (sp == Spell_augur && r.n.omen < 0) { out.how = Cast::NothingToDo; return out; }
  removeSpellAt(r, slot);
  stat(r, Stat::spells_cast);
  if (sp != Spell_recall) r.lastSpell = (int8_t)sp;
  switch (sp) {
    case Spell_glamour: r.n.glamour = 1; break;
    case Spell_surge: r.n.surge = 1; break;
    case Spell_transmute: {
      int total = 0;
      for (int e = 0; e < EnergyN; e++) { total += r.hold[e]; r.hold[e] = 0; }
      r.hold[r.focus] = (uint8_t)(total > 255 ? 255 : total);
      r.n.transmuteE = (int8_t)r.focus;
      r.n.transmuteLeft = 4;
      break;
    }
    case Spell_haste: r.n.visits += 2; break;
    case Spell_echo: r.n.echo = 1; break;
    case Spell_hex: r.coin += 8; clampCoin(r); break;
    case Spell_veil: r.n.veil = 1; break;
    case Spell_chalk: {
      const AtlasEntry* pre = atlas.find(here->id);
      const uint32_t clock = pre ? pre->lastPlay : h.playMin;
      AtlasEntry& a = atlas.touch(here->id, here->seed, r.runNo, h.playMin);
      a.lastPlay = clock;   // carving doesn't restart a well's settling
      a.glyph = (int8_t)r.rng.below(GlyphCount);
      out.glyph = a.glyph;
      stat(r, Stat::glyphs_total);
      break;
    }
    case Spell_kindle: {
      int plain[Run::MaxRelics], k = 0;
      for (int i = 0; i < r.nRelics; i++) if (r.relics[i].ed < 0) plain[k++] = i;
      const int i = plain[r.rng.below(k)];
      static const int eds[3] = {Edition_gilt, Edition_lit, Edition_prismatic};
      r.relics[i].ed = (int8_t)eds[r.rng.below(3)];
      out.relic = r.relics[i].id;
      out.edition = r.relics[i].ed;
      break;
    }
    case Spell_augur: r.n.omen = -1; break;
    case Spell_recall: r.spells[r.nSpells++] = (uint8_t)r.lastSpell; out.spell = r.lastSpell; break;
    case Spell_bargain: r.bargain = 1; break;
    case Spell_summon: {
      int pool[RelicCount], k = 0;
      for (int i = 0; i < RelicCount; i++)
        if (Relics[i].legendary && unlocked(car, Kind::Relic, i) && !owns(r, i)) pool[k++] = i;
      out.relic = pool[r.rng.below(k)];
      addRelic(r, out.relic);
      stat(r, Stat::summoned);
      break;
    }
    default: break;
  }
  return out;
}

// The glyph carved into a well this week, or -1.
inline int glyphAt(const Run& r, const Atlas& atlas, uint32_t id) {
  if (!r.active) return -1;
  const AtlasEntry* a = atlas.find(id);
  return a && a->runNo == r.runNo ? a->glyph : -1;
}

// Carve a glyph you bought into a well in reach.
inline bool carve(Run& r, Atlas& atlas, const WellView& w, const Here& h, int token) {
  if (token < 0 || token >= r.nGlyphTokens || w.sig < ReachDbm) return false;
  const AtlasEntry* pre = atlas.find(w.id);
  const uint32_t clock = pre ? pre->lastPlay : h.playMin;
  AtlasEntry& a = atlas.touch(w.id, w.seed, r.runNo, h.playMin);
  a.lastPlay = clock;   // carving doesn't restart a well's settling
  a.glyph = (int8_t)r.glyphTokens[token];
  for (int k = token; k + 1 < r.nGlyphTokens; k++) r.glyphTokens[k] = r.glyphTokens[k + 1];
  r.nGlyphTokens--;
  return true;
}

// A twist of the wrist: a ward that keeps the Wardens off for a minute, once a Night.
inline bool castWard(Run& r, const Here& h) {
  if (!r.active || r.n.wardCast) return false;
  r.n.wardCast = 1;
  r.n.wardUntil = h.playSec + 60;
  return true;
}

// A dowse that found its well.
inline void noteDowse(Run& r) { stat(r, Cat::Stat::dowses); }

// ---- The end of a Binding --------------------------------------------------------

// Fold a finished Binding into the career and reveal what it earned.
inline int endRun(Run& r, Career& car, const Atlas& atlas) {
  using namespace Cat;
  int eds = 0, rares = 0;
  for (int i = 0; i < r.nRelics; i++) {
    eds += r.relics[i].ed >= 0;
    rares += Relics[r.relics[i].id].rarity == Rare;
  }
  int att = 0;
  for (int e = 0; e < EnergyN; e++) att = r.attune[e] > att ? r.attune[e] : att;
  best(r, Stat::editions_owned, (uint32_t)eds);
  best(r, Stat::rares_one_run, (uint32_t)rares);
  best(r, Stat::attune_level, (uint32_t)att);
  best(r, Stat::new_wells_run, (uint32_t)r.newWells);
  mergeRun(car, r);
  if (atlas.unique > car.stats[(int)Stat::unique_wells]) car.stats[(int)Stat::unique_wells] = atlas.unique;
  if (r.won) {
    car.wins++;
    car.stats[(int)Stat::wins] = car.wins;
    int perSchool[(int)School::Count] = {0};
    for (int i = 0; i < r.nRelics; i++) {
      setBit(car.wonWith, r.relics[i].id);
      perSchool[Relics[r.relics[i].id].school]++;
    }
    for (int s = 0; s < (int)School::Count; s++) if (perSchool[s] >= 2) car.schoolWins |= 1u << s;
    car.callingsWon |= (uint16_t)(1u << r.calling);
    if (r.challenge < 0) {
      if (r.pact > car.seals[r.calling]) car.seals[r.calling] = (int8_t)r.pact;
      if ((int)r.pact > car.bestPact) car.bestPact = (int8_t)r.pact;
    } else {
      car.challengesWon |= (uint16_t)(1u << r.challenge);
    }
    car.eclipsesSurvived |= (uint8_t)(1u << r.eclipse);
  }
  r.active = 0;
  return reveal(car);
}

} // namespace Game

#include "Shop.h"
#include "Estimates.h"
