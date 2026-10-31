#pragma once
#include "Shop.h"

// Night-sized numbers, worked out from what the engine already knows: how many
// more offerings tonight needs, where the Portent thresholds land on the Tithe
// bar, what a stall's star is worth in offerings a Night, the Omen the stall
// can see, and what each well in the atlas would pay now. All display: nothing
// here changes a rule. From the quiet rung (Run.h deckQuiet) the Deck keeps
// these to itself.

namespace Game {

// ---- Tonight -------------------------------------------------------------------------

// How many more offerings of about `perOffering` Resonance the Tithe still
// needs: 0 when it's paid or nothing is known, never more than 99.
inline int offeringsToGo(double tithe, double total, double perOffering) {
  if (perOffering <= 0 || total >= tithe) return 0;
  const double n = ceil((tithe - total) / perOffering - 1e-9);
  return n > 99 ? 99 : n < 1 ? 1 : (int)n;
}

// Where closing with `spare` visits to spare lands on the Tithe bar: the share
// of the Tithe that has to be paid by now, `used` visits in, if the offerings
// are about equal. 0 before the first offering; past 1 once it can't be done.
inline double paceNotch(int used, int visits, int spare) {
  if (used <= 0) return 0;
  const int allowed = visits - spare;
  return allowed <= 0 ? 2 : (double)used / allowed;
}

// The Portent notches on the bar tonight (6, 9 and 11 to spare; 2 sooner for
// the Night owl), those still on it. Returns how many were written to `out`.
inline int portentNotches(const Run& r, double out[3]) {
  int n = 0;
  const int shift = r.challenge == 2 ? 2 : 0;
  for (int k = 0; k < 3; k++) {
    const double f = paceNotch(r.n.used, r.n.visits, PortentAt[k] - shift);
    if (f > 0 && f <= 1) out[n++] = f;
  }
  return n;
}

// What one offering makes tonight: the build's estimate at the wells in reach
// (`inReach`, the star's number), or the average of tonight's offerings.
inline double perOfferingTonight(const Run& r, double inReach) {
  if (inReach > 0) return inReach;
  return r.n.used > 0 ? r.n.total / r.n.used : 0;
}

// The gauge: offerings still to go tonight, after the first one. 0 when there's
// nothing to say (before the first offering, once paid, or on a quiet Deck).
inline int moreTonight(const Run& r, double inReach) {
  if (!r.active || deckQuiet(r) || r.n.used == 0 || r.n.met) return 0;
  return offeringsToGo(effectiveTithe(r), r.n.total, perOfferingTonight(r, inReach));
}

// ---- The stall ------------------------------------------------------------------------

// The Omen Night `night` (0-based) will have, or -1: Night 1 and the seventh never have one.
inline int omenOf(const Run& r, int night) {
  return night >= 1 && night < Nights - 1 ? (int)r.omens[night] - 1 : -1;
}

// The stall shows the Night ahead's Omen. From the quiet rung it waits for its
// Night, as it always did.
inline int stallOmen(const Run& r) { return deckQuiet(r) ? -1 : omenOf(r, r.night); }

// The Diviner sees two Nights ahead.
inline bool seesAhead(const Run& r) { return isCalling(r, Cat::Calling_diviner); }

// The Night ahead's Tithe as the stall knows it: with its Omen once that shows.
inline double titheAhead(const Run& r) {
  return titheFor(r, r.night, stallOmen(r), r.night == Nights - 1 ? r.eclipse : -1);
}

// What one offering makes with the build as it stands: your recent offerings
// scored again (a typical one before there are any).
inline double perOffering(const Run& r, const RecentSales& mem) {
  return recentPower(r, mem) / (mem.n ? mem.n : 6);
}

// How many fewer offerings a Night a gain of `gain` (0.5 is half again) is
// worth, against a Tithe of `tithe` at `per` Resonance an offering.
inline double fewerOfferings(double gain, double tithe, double per) {
  if (gain <= 0 || per <= 0 || tithe <= 0) return 0;
  const double now = tithe / per;
  return now - now / (1 + gain);
}

// "~1 FEWER OFFERING A NIGHT": one decimal under one, halves above.
inline const char* fewerText(double fewer, char* b, int cap) {
  if (fewer < 0.95) {
    snprintf(b, cap, "~%.1f FEWER OFFERINGS A NIGHT", fewer < 0.1 ? 0.1 : fewer);
    return b;
  }
  const double h = round(fewer * 2) / 2;
  if (h == 1) snprintf(b, cap, "~1 FEWER OFFERING A NIGHT");
  else snprintf(b, cap, "~%.3g FEWER OFFERINGS A NIGHT", h);
  return b;
}

// What the stall's star is worth in offerings a Night, 0 with no star.
inline double starFewer(const Shop& s, const Run& r, const RecentSales& mem) {
  return fewerOfferings(starGain(s, r, mem), titheAhead(r), perOffering(r, mem));
}

// ---- The atlas -------------------------------------------------------------------------

// About how many wells a ward like that has in range.
inline int wardCrowd(int ward) {
  static const int n[4] = {2, 7, 18, 30};
  return n[ward & 3];
}

// The hold's biggest stack, -1 with an empty hold.
inline int biggestStack(const Run& r) {
  int e = -1;
  for (int k = 0; k < EnergyN; k++) if (r.hold[k] > 0 && (e < 0 || r.hold[k] > r.hold[e])) e = k;
  return e;
}

// What offering energy `e` from `r`'s hold would make at a well as the atlas
// last saw it: in its ward, with a crowd like that ward's, in reach. 0 where
// it can't be offered.
inline double atlasEstimate(const Run& r, const Atlas& atlas, const AtlasEntry& a, int e, uint32_t playMin) {
  WellView w{};
  w.id = a.id;
  w.seed = a.seed;
  w.energy = a.energy;
  w.faction = a.faction;
  w.hidden = a.energy == Hush;
  w.sig = -60;
  Here h{};
  h.ward = a.ward ? a.ward - 1 : WardW;
  h.n = wardCrowd(h.ward);
  h.noise = 0.5;
  h.battery = 80;
  h.playMin = playMin;
  if (e < 0 || r.hold[e] == 0) return 0;
  if (e == Static && wellFaction(r, w) == Warded && !hasTrinket(r, Cat::Trinket_fence)) return 0;
  Sale s;
  saleContext(s, r, atlas, w, h, e);
  if (r.challenge == 5 && s.revisit) return 0;   // The long way
  return score(s, r).resonance;
}

// One estimate per atlas entry, for the hold's biggest stack, or a typical draw
// of your focus with an empty hold. Cheap enough to do on opening the atlas.
inline void atlasEstimates(const Run& r, const Atlas& atlas, uint32_t playMin, float* out) {
  Run& t = scratchRun();
  t = r;
  int e = biggestStack(r);
  if (e < 0) { e = r.focus; t.hold[e] = 6; }
  for (int i = 0; i < atlas.count; i++) out[i] = (float)atlasEstimate(t, atlas, atlas.e[i], e, playMin);
}

} // namespace Game
