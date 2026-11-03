#pragma once
#include "Game.h"

// The Collector's shop between Nights: relics, Attunements, spells, glyphs,
// a trinket and a Deck mod, plus Polish, rerolls and selling relics back.
// Follows shop() and roll_relic() in tools/balance_sim.py, where the bot's
// choices become the player's.

namespace Game {

enum class OfferKind : uint8_t { None, Relic, Attune, Spell, Glyph, Trinket, Mod };

// Why a relic is on the shelf, when it's there for a reason: a reroll asks
// before it throws one of these away, and the shine's relic says whose it is.
enum OfferMark : uint8_t {
  Mark_Shine = 1,    // carries your Shine Portent
  Mark_Caller = 2,   // the first stall's Caller for your focus
  Mark_Rare = 4,     // Rare sight's rare
  Mark_Mult = 8,     // the multiplier every shelf from Night 2 has
};

struct Offer {
  OfferKind kind;
  uint8_t id;          // relic / spell / glyph / trinket / mod
  int8_t ed;           // relic edition, -1 none
  int8_t withered;     // relic crumbles after this many Nights, -1 never
  uint8_t half;        // half price (the Collector's mercy)
  uint8_t sold;
  int8_t stall;        // a faction stall's offer: the faction, -1 none
  uint8_t marks;       // OfferMark bits (this byte was padding)
};
static_assert(sizeof(Offer) == 8, "offers are saved as part of the stall: nothing may move");

struct Shop {
  static constexpr int MaxRelics = 6, MaxCons = 4;
  Offer relics[MaxRelics];
  uint8_t nRelics;
  Offer cons[MaxCons];
  uint8_t nCons;
  Offer trinket, mod;
  int16_t rerollCost;
  int8_t freeRerolls;
  uint8_t open;
  uint8_t night;       // the Night just finished (0-based)
  int8_t star;         // the hint star: an index into relics, -1 none
  uint8_t mercy;
  uint8_t starPolish;  // ...or on a Polish of one of your relics: its index + 1, 0 none (this byte was padding)
};
static_assert(sizeof(Shop) == 106, "the stall is saved as-is: nothing may move");

// ---- Rolling stock ---------------------------------------------------------------

inline bool emptyPockets(const Run& r) { return r.challenge == 7; }
inline bool oneNote(const Run& r) { return r.challenge == 1; }
// A Pilgrim or One note week: only your focus energy ever comes up.
inline bool focusOnly(const Run& r) { return isCalling(r, Cat::Calling_pilgrim) || oneNote(r); }
inline bool isCaller(int relic) { return relic >= Cat::R_ember_caller && relic <= Cat::R_static_caller; }

// The shine a Shine Portent gives.
inline int shineEdition(Run& r) {
  static const int eds[3] = {Cat::Edition_gilt, Cat::Edition_lit, Cat::Edition_prismatic};
  return eds[r.rng.below(3)];
}

// `night` is the Night the stock is for: the one just played (0-based). A
// Shine Portent lands on the next stall's shelf (rollRelics); with no stall
// (Empty pockets) it goes on the next free relic instead.
inline bool rollEdition(Run& r, double mult, int& ed, int night) {
  using namespace Cat;
  if ((r.shopFlags & SF_Shine) && emptyPockets(r)) {
    ed = shineEdition(r);
    r.shopFlags &= ~SF_Shine;
    return true;
  }
  for (int i = 0; i < EditionCount; i++) {
    if (r.rng.chance(Editions[i].chance * mult * (1 + EditionGrowth * night))) { ed = i; return true; }
  }
  ed = -1;
  return false;
}

// A random relic for the shop, as roll_relic() does: rarer stock as the week
// goes on, leaning toward the schools you own and the wards you play in.
inline int rollRelic(Run& r, const Career& car, int rarity, bool multiplier, int& ed, int& withered, int night) {
  using namespace Cat;
  ed = -1;
  withered = -1;
  if (rarity < 0) {
    double w[3] = {RarityW[0], RarityW[1] + UncommonGrowth * night, RarityW[2] + RareGrowth * night};
    if (hasTrinket(r, Trinket_lucky_coin)) w[2] *= 2;
    rarity = r.rng.weighted(w, 3);
  }
  int pool[RelicCount], k = 0;
  for (int i = 0; i < RelicCount; i++) {
    const RelicDef& d = Relics[i];
    if (d.rarity != rarity || d.legendary || owns(r, i) || !unlocked(car, Kind::Relic, i)) continue;
    if (multiplier && d.phase != 2 && d.phase != 3) continue;
    if (focusOnly(r) && isCaller(i) && i != R_ember_caller + r.focus) continue;   // a Caller for an energy you never get
    pool[k++] = i;
  }
  if (multiplier && k == 0) return rarity == Common ? rollRelic(r, car, Uncommon, true, ed, withered, night) : -1;
  if (k == 0) return -1;
  uint32_t schools = 0;
  for (int i = 0; i < r.nRelics; i++) schools |= 1u << Relics[r.relics[i].id].school;
  if (schools && r.rng.chance(BuildBias)) {
    int fit[RelicCount], f = 0;
    for (int i = 0; i < k; i++) if ((schools >> Relics[pool[i]].school) & 1) fit[f++] = pool[i];
    if (f) { memcpy(pool, fit, f * sizeof(int)); k = f; }
  }
  int seen = 0;
  for (int w = 0; w < 4; w++) seen += r.wardSeen[w];
  if (seen && r.rng.chance(LocaleBias)) {
    uint8_t mine = 0;
    for (int w = 0; w < 4; w++) if (r.wardSeen[w] * 4 >= seen) mine |= (uint8_t)(1u << w);
    int fit[RelicCount], f = 0;
    for (int i = 0; i < k; i++) if (Relics[pool[i]].suits == 0xF || (Relics[pool[i]].suits & mine)) fit[f++] = pool[i];
    if (f) { memcpy(pool, fit, f * sizeof(int)); k = f; }
  }
  int id = pool[r.rng.below(k)];
  if (!multiplier && rules.multKeep < 1 && (Relics[id].phase == 2 || Relics[id].phase == 3) && !r.rng.chance(rules.multKeep)) {
    int plain[RelicCount], np = 0;   // a multiplier turned away: something that adds instead, if this rarity has one
    for (int i = 0; i < k; i++) if (Relics[pool[i]].phase < 2) plain[np++] = pool[i];
    if (np) id = plain[r.rng.below(np)];
  }
  if (!multiplier && rules.growerPull > 0 && r.rng.chance(rules.growerPull)) {
    int g[RelicCount], ng = 0;
    for (int i = 0; i < k; i++) if (growingRelic(pool[i])) g[ng++] = pool[i];
    if (ng) id = g[r.rng.below(ng)];
  }
  const double edMult = (hasTrinket(r, Trinket_collectors_eye) ? 3 : 1) * (isCalling(r, Calling_collector) ? 3 : 1);
  rollEdition(r, edMult, ed, night);
  if (pactOn(r, PR_bone) && r.rng.chance(0.3)) withered = 3;
  return id;
}

inline Offer relicOffer(int id, int ed, int withered) {
  Offer o{};
  o.kind = OfferKind::Relic;
  o.id = (uint8_t)id;
  o.ed = (int8_t)ed;
  o.withered = (int8_t)withered;
  o.stall = -1;
  return o;
}

// ---- Prices ------------------------------------------------------------------------

inline int relicPrice(const Run& r, int id, int ed) {
  static const int byRarity[4] = {PriceCommon, PriceUncommon, PriceRare, 0};
  return byRarity[Cat::Relics[id].rarity] + (ed >= 0 ? PriceEdition : 0) +
         (isCalling(r, Cat::Calling_collector) || isCalling(r, Cat::Calling_chorus) ? 1 : 0);
}

inline int cost(const Run& r, const Offer& o) {
  int p;
  switch (o.kind) {
    case OfferKind::Relic: p = relicPrice(r, o.id, o.ed); break;
    case OfferKind::Attune: p = PriceAttune; break;
    case OfferKind::Spell: p = PriceSpell; break;
    case OfferKind::Glyph: p = PriceGlyph; break;
    case OfferKind::Trinket: p = PriceTrinket; break;
    case OfferKind::Mod: p = PriceMod; break;
    default: return 0;
  }
  if (hasMod(r, Cat::Mod_clearance)) p -= 1;
  if (r.bargain || o.half) p /= 2;
  return p < 1 ? 1 : p;
}

// What a relic you own sells back for: half its full price (all of it with a
// Pawn ticket). BARGAIN, Clearance and mercy lower only what you pay, and the
// Polish you put into it isn't refunded.
inline int sellBack(const Run& r, const OwnedRelic& x) {
  const int p = relicPrice(r, x.id, x.ed);
  return hasTrinket(r, Cat::Trinket_pawn_ticket) ? p : p / 2;
}

// What selling relic i really adds to your coin: Shoestring keeps you at 15.
inline int sellGain(const Run& r, int i) {
  const int back = sellBack(r, r.relics[i]);
  if (r.challenge != 0 || r.coin + back <= 15) return back;   // the cap is clampCoin's (Game.h)
  return r.coin >= 15 ? 0 : 15 - (int)r.coin;
}

inline int polishPrice(const Run& r, const OwnedRelic& x) {
  if (x.tier >= 3) return 0;
  return PolishCost[x.tier] * (pactOn(r, PR_silver) ? 2 : 1);
}

inline bool canPolish(const Run& r, int i) {
  return i >= 0 && i < r.nRelics && Cat::Relics[r.relics[i].id].hasFn && r.relics[i].tier < 3 &&
         r.coin >= polishPrice(r, r.relics[i]);
}

// ---- Opening the shop -----------------------------------------------------------

// What the stall that just opened couldn't do, for the screen to say once
// (not saved): bits of StallNote.
enum StallNote : uint8_t { Note_NoRare = 1, Note_MercyNoRelic = 2, Note_MercyOneNote = 4 };
inline uint8_t stallNotes = 0;

// Put a relic that's there for a reason on the shelf: in a free place, or over
// the last plain relic when the shelf is full. The Caller, a faction's offer
// and other special relics stay, and the multiplier goes only if nothing else is plain.
inline bool placeOffer(Shop& s, const Offer& o) {
  if (s.nRelics < Shop::MaxRelics) { s.relics[s.nRelics++] = o; return true; }
  for (int pass = 0; pass < 2; pass++)
    for (int i = s.nRelics - 1; i >= 0; i--) {
      const Offer& x = s.relics[i];
      if (x.sold || x.half || x.stall >= 0 || (x.marks & ~Mark_Mult) || (pass == 0 && (x.marks & Mark_Mult))) continue;
      s.relics[i] = o;
      return true;
    }
  return false;
}

// The shelf. On the first roll at a stall (`first`), Rare sight adds its rare
// and a Shine Portent puts its shine on one of the relics; a reroll has neither.
inline void rollRelics(Shop& s, Run& r, const Career& car, bool first) {
  using namespace Cat;
  s.nRelics = 0;
  const bool shine = first && (r.shopFlags & SF_Shine), rare = first && (r.shopFlags & SF_RareSight);
  if (first) r.shopFlags &= ~(SF_Shine | SF_RareSight);
  if (oneNote(r)) {   // One note: only the Caller for your focus, until you have it
    const int id = R_ember_caller + r.focus;
    if (!owns(r, id)) {
      Offer o = relicOffer(id, -1, -1);
      if (shine) { o.ed = (int8_t)shineEdition(r); o.marks |= Mark_Shine; }
      s.relics[s.nRelics++] = o;
    }
    return;
  }
  int n = ShopRelics + hasMod(r, Mod_overstock);
  double topStanding = 0;
  int topFaction = -1;
  for (int f = 0; f < 4; f++) if (r.standing[f] > topStanding) { topStanding = r.standing[f]; topFaction = f; }
  const bool stall = topStanding >= 30;
  if (stall) n++;
  const bool caller = s.night == 0 && first;   // the first shop always has a Caller for your focus energy
  int ids[8], eds[8], wis[8];
  for (int i = 0; i < n; i++) {
    if (i == 0 && caller) { ids[0] = R_ember_caller + r.focus; eds[0] = wis[0] = -1; continue; }
    const bool mult = i == n - 1 && s.night >= 1 && rules.multSlot;
    for (int tries = 0; tries < 6; tries++) {   // a shelf never shows the same relic twice
      ids[i] = rollRelic(r, car, -1, mult, eds[i], wis[i], s.night);
      bool dup = false;
      for (int k = 0; k < i; k++) dup |= ids[k] == ids[i];
      if (!dup) break;
    }
  }
  for (int i = 0; i < n && s.nRelics < Shop::MaxRelics; i++) {
    if (ids[i] < 0 || owns(r, ids[i])) continue;
    bool dup = false;
    for (int k = 0; k < s.nRelics; k++) dup |= s.relics[k].id == ids[i];
    if (dup) continue;
    Offer o = relicOffer(ids[i], eds[i], wis[i]);
    if (stall && i == n - 2) o.stall = (int8_t)topFaction;   // the stall's offer
    if (caller && i == 0) o.marks |= Mark_Caller;
    if (i == n - 1 && s.night >= 1) o.marks |= Mark_Mult;
    s.relics[s.nRelics++] = o;
  }
  if (rare) {   // Rare sight: a rare on top of the usual shelf
    int id = -1, ed = -1, wi = -1;
    for (int tries = 0; tries < 6; tries++) {
      id = rollRelic(r, car, Rare, false, ed, wi, s.night);
      bool dup = false;
      for (int k = 0; k < s.nRelics; k++) dup |= s.relics[k].id == id;
      if (id < 0 || !dup) break;
    }
    int k = -1;
    for (int i = 0; i < s.nRelics; i++) if (s.relics[i].id == id) k = i;
    if (id < 0) stallNotes |= Note_NoRare;
    else if (k >= 0) s.relics[k].marks |= Mark_Rare;   // it's on the shelf already
    else {
      Offer o = relicOffer(id, ed, wi);
      o.marks |= Mark_Rare;
      placeOffer(s, o);
    }
  }
  if (shine) {   // on a relic that stays on the shelf: never the Caller, and one with no shine of its own if there is one
    int pick[Shop::MaxRelics], k = 0;
    for (int pass = 0; pass < 2 && k == 0; pass++)
      for (int i = 0; i < s.nRelics; i++)
        if (!(s.relics[i].marks & Mark_Caller) && (pass == 1 || s.relics[i].ed < 0)) pick[k++] = i;
    if (k) {
      Offer& o = s.relics[pick[r.rng.below(k)]];
      o.ed = (int8_t)shineEdition(r);
      o.marks |= Mark_Shine;
    }
  }
}

// One more relic on the shelf: the Collector's mercy (`half`), which always
// finds a place, or Overstock bought mid-stall, which needs a free one.
inline bool addShelfRelic(Shop& s, Run& r, const Career& car, bool half) {
  using namespace Cat;
  if (oneNote(r) || (!half && s.nRelics >= Shop::MaxRelics)) return false;
  for (int tries = 0; tries < 6; tries++) {
    int ed, wi;
    const int m = rollRelic(r, car, -1, true, ed, wi, s.night);
    if (m < 0) continue;
    bool dup = owns(r, m);
    for (int k = 0; k < s.nRelics; k++) dup |= s.relics[k].id == m;
    if (dup) continue;
    Offer o = relicOffer(m, ed, wi);
    o.half = half ? 1 : 0;
    return placeOffer(s, o);
  }
  return false;
}

// One more consumable (attunement, spell or glyph) on the wares shelf.
inline bool addShelfConsumable(Shop& s, Run& r, const Career& car) {
  using namespace Cat;
  if (s.nCons >= Shop::MaxCons) return false;
  Offer o{};
  o.stall = -1;
  o.ed = o.withered = -1;
  const int kind = oneNote(r) ? 0 : r.rng.weighted(ConsumableW, 3);
  if (kind == 0) { o.kind = OfferKind::Attune; }
  else if (kind == 1) {
    double w[SpellCount];
    for (int k = 0; k < SpellCount; k++) w[k] = unlocked(car, Kind::Spell, k) ? Spells[k].weight : 0;
    if (focusOnly(r)) w[Spell_transmute] = 0;   // your hold is your focus already
    o.kind = OfferKind::Spell;
    o.id = (uint8_t)r.rng.weighted(w, SpellCount);
  } else {
    o.kind = OfferKind::Glyph;
    o.id = (uint8_t)r.rng.below(GlyphCount);
  }
  s.cons[s.nCons++] = o;
  return true;
}

// Trinkets and mods that can do nothing for this week's setup aren't stocked.
inline bool uselessTrinket(const Run& r, int t) {
  return (t == Cat::Trinket_moneylender && pactOn(r, PR_obsidian)) ||   // no loans from OBSIDIAN up
         (t == Cat::Trinket_misers_purse && r.challenge == 0);          // Shoestring's 15 coin earn 3 interest at most
}
inline bool uselessMod(const Run& r, int m) {   // a Pilgrim has 4 slots, and every well gives the focus
  return isCalling(r, Cat::Calling_pilgrim) && (m == Cat::Mod_sixth_socket || m == Cat::Mod_lodestone);
}

// The coin you came into the stall with, kept for Long receipt in a place in
// Run::state that no relic uses (it has room for RelicIdCap relics).
constexpr int CoinInSlot = RelicIdCap - 1;
static_assert(Cat::RelicCount <= CoinInSlot, "Long receipt's coin-in needs a spare place in Run::state");

inline void openShop(Shop& s, Run& r, const Career& car) {
  using namespace Cat;
  memset(&s, 0, sizeof(s));
  stallNotes = 0;
  s.open = 1;
  s.night = (uint8_t)(r.night - 1);
  s.star = -1;
  if (emptyPockets(r)) {   // no stall: what was waiting for one is gone (a shine waits for the next free relic)
    s.open = 0;
    r.shopFlags &= ~(SF_Mercy | SF_RareSight | SF_FreeRerolls);
    r.bargain = 0;
    return;
  }
  // One note has nothing to reroll (rollRelics), so no free rerolls either.
  s.freeRerolls = oneNote(r) ? 0 : (int8_t)(hasTrinket(r, Trinket_haggler) + 2 * ((r.shopFlags & SF_FreeRerolls) != 0));
  r.shopFlags &= ~SF_FreeRerolls;
  s.rerollCost = (int16_t)((pactOn(r, PR_silver) ? 8 : Reroll) - 2 * hasMod(r, Mod_liquidation) +
                           2 * isCalling(r, Calling_night_clerk));
  if (s.rerollCost < 1) s.rerollCost = 1;
  rollRelics(s, r, car, true);
  r.state[CoinInSlot] = r.coin;   // until closeShop turns what you spent into Long receipt's x
  if (r.shopFlags & SF_Mercy) {
    r.shopFlags &= ~SF_Mercy;
    s.mercy = 1;
    if (oneNote(r)) stallNotes |= Note_MercyOneNote;   // no relic to halve and nothing to reroll: only the Attunement
    else {
      if (!addShelfRelic(s, r, car, true)) stallNotes |= Note_MercyNoRelic;
      s.freeRerolls++;
    }
    if (r.attune[r.focus] < 250) r.attune[r.focus]++;
  }
  const int nc = ShopConsumables + hasMod(r, Mod_night_market);
  for (int i = 0; i < nc; i++) addShelfConsumable(s, r, car);
  if (!oneNote(r)) {
    int pool[TrinketCount], k = 0;
    for (int t = 0; t < TrinketCount; t++)
      if (unlocked(car, Kind::Trinket, t) && !hasTrinket(r, t) && !uselessTrinket(r, t)) pool[k++] = t;
    if (k && r.rng.chance(TrinketOfferP)) {
      s.trinket.kind = OfferKind::Trinket;
      s.trinket.id = (uint8_t)pool[r.rng.below(k)];
    }
    int mods[ModCount], m = 0;
    for (int i = 0; i < ModCount; i++) {   // a tier 2 mod shows once you own its tier 1, or once its tier 1 is no use to you
      if (hasMod(r, i) || uselessMod(r, i)) continue;
      if (Mods[i].needs >= 0 && !hasMod(r, Mods[i].needs) && !uselessMod(r, Mods[i].needs)) continue;
      mods[m++] = i;
    }
    if (m) {
      s.mod.kind = OfferKind::Mod;
      s.mod.id = (uint8_t)mods[r.rng.below(m)];
    }
  }
}

// ---- Buying, selling, Polish, rerolls ------------------------------------------------

// NoPolish: the relic has no Polish. MaxPolish: it's at III. TooMany: you carry
// Run::MaxRelics already (Phantoms count, though they take no slot).
enum class Buy : uint8_t { Ok, NoCoin, NoSlot, Sold, Cant, NoPolish, MaxPolish, TooMany };

constexpr int GlyphTokens = 4;   // glyphs you can carry, uncarved

inline bool glassHouseCrown(const Run& r, int i) { return r.challenge == 3 && r.relics[i].id == Cat::R_glass_crown; }

// Sell relic i, leaving the coin unclamped for a buy that pays straight after.
inline void sellAt(Run& r, int i) {
  r.coin += sellBack(r, r.relics[i]);
  removeRelicAt(r, i);
  stat(r, Cat::Stat::relics_sold);
}

inline Buy sellRelic(Run& r, int i) {
  if (i < 0 || i >= r.nRelics || glassHouseCrown(r, i)) return Buy::Cant;
  sellAt(r, i);
  clampCoin(r);
  return Buy::Ok;
}

// ---- A free relic with no room for it (Gift, Conjunction) --------------------------

// The first Portent row with a relic waiting, or -1.
inline int giftWaiting(const GiftSwap& s) {
  for (int k = 0; k < 3; k++) if (s.id[k] && !(s.id[k] & SwapSpell)) return k;
  return -1;
}

// Take row k's relic, selling relic `drop` for room if your slots are full.
inline Buy takeGift(Run& r, GiftSwap& s, int k, int drop) {
  if (k < 0 || k >= 3 || !s.id[k] || (s.id[k] & SwapSpell)) return Buy::Cant;
  if (slotsUsed(r) >= relicSlots(r)) {
    if (drop < 0 || drop >= r.nRelics || glassHouseCrown(r, drop) || r.relics[drop].ed == Cat::Edition_phantom)
      return Buy::NoSlot;
    sellRelic(r, drop);
  }
  if (r.nRelics >= Run::MaxRelics) return Buy::NoSlot;
  addRelic(r, s.id[k] - 1, s.ed[k], s.withered[k]);
  s.id[k] = 0;
  return Buy::Ok;
}

// Leave row k's relic (or spell) where it is.
inline void leaveGift(GiftSwap& s, int k) {
  if (k >= 0 && k < 3) s.id[k] = 0;
}

// ---- A free spell with no slot for it (Scroll) ------------------------------------

// The first Portent row with a spell waiting, or -1.
inline int spellWaiting(const GiftSwap& s) {
  for (int k = 0; k < 3; k++) if (s.id[k] & SwapSpell) return k;
  return -1;
}
inline int waitingSpell(const GiftSwap& s, int k) {
  return k >= 0 && k < 3 && (s.id[k] & SwapSpell) ? (s.id[k] & ~SwapSpell) - 1 : -1;
}

// Take row k's spell, forgetting spell `forget` for it if your slots are full.
inline Buy takeSpell(Run& r, GiftSwap& s, int k, int forget) {
  const int sp = waitingSpell(s, k);
  if (sp < 0) return Buy::Cant;
  if (r.nSpells >= spellSlots(r)) {
    if (forget < 0 || forget >= r.nSpells) return Buy::NoSlot;
    removeSpellAt(r, forget);
  }
  r.spells[r.nSpells++] = (uint8_t)sp;
  s.id[k] = 0;
  return Buy::Ok;
}

// ---- Empty pockets: the Night's relic with no room for it --------------------------

// Take it, selling relic `drop` for room if your slots are full.
inline Buy takeNightRelic(Run& r, DawnNotes& n, int drop) {
  if (!n.gift) return Buy::Cant;
  GiftSwap s{};
  s.id[0] = n.gift;
  s.ed[0] = n.giftEd;
  s.withered[0] = -1;
  const Buy b = takeGift(r, s, 0, drop);
  if (b == Buy::Ok) n.gift = 0;
  return b;
}
inline void leaveNightRelic(DawnNotes& n) { n.gift = 0; }

// Buy relic offer `k`. If your slots are full, `drop` is the relic to sell to
// make room: it's sold and the price paid before Shoestring's cap applies.
inline Buy buyRelic(Shop& s, Run& r, int k, int drop = -1) {
  if (k < 0 || k >= s.nRelics) return Buy::Cant;
  Offer& o = s.relics[k];
  if (o.sold) return Buy::Sold;
  const int p = cost(r, o);
  const bool needsSlot = o.ed != Cat::Edition_phantom && slotsUsed(r) >= relicSlots(r);
  if (needsSlot && (drop < 0 || drop >= r.nRelics || glassHouseCrown(r, drop) ||
                    r.relics[drop].ed == Cat::Edition_phantom))
    return Buy::NoSlot;
  if (r.nRelics >= Run::MaxRelics && !needsSlot) return Buy::TooMany;
  const int back = needsSlot ? sellBack(r, r.relics[drop]) : 0;
  if (r.coin + back < p) return Buy::NoCoin;
  if (needsSlot) sellAt(r, drop);
  r.coin -= p;
  clampCoin(r);
  addRelic(r, o.id, o.ed, o.withered);
  o.sold = 1;
  if (Cat::Relics[o.id].rarity == Cat::Rare) Game::stat(r, Cat::Stat::rares_bought);
  stat(r, Cat::Stat::bought_total);
  return Buy::Ok;
}

// Buy a consumable. For an Attunement, `energy` says which energy to tune. For a
// spell or glyph with no room, `drop` is the one you're holding to let go of.
inline Buy buyConsumable(Shop& s, Run& r, int k, int energy = -1, int drop = -1) {
  if (k < 0 || k >= s.nCons) return Buy::Cant;
  Offer& o = s.cons[k];
  if (o.sold) return Buy::Sold;
  const int p = cost(r, o);
  if (r.coin < p) return Buy::NoCoin;
  switch (o.kind) {
    case OfferKind::Attune: {   // a Pilgrim or One note week has only the focus to tune
      const int e = energy >= 0 && energy < EnergyN && !focusOnly(r) ? energy : r.focus;
      const int lv = hasMod(r, Cat::Mod_attuners_fork) && r.rng.chance(0.33) ? 2 : 1;
      r.attune[e] = (uint8_t)(r.attune[e] + lv > 250 ? 250 : r.attune[e] + lv);
      stat(r, Cat::Stat::attunes_run);
      best(r, Cat::Stat::attune_level, r.attune[e]);
      break;
    }
    case OfferKind::Spell:
      if (r.nSpells >= spellSlots(r)) {
        if (drop < 0 || drop >= r.nSpells) return Buy::NoSlot;
        removeSpellAt(r, drop);
      }
      r.spells[r.nSpells++] = o.id;
      break;
    case OfferKind::Glyph:
      if (r.nGlyphTokens >= GlyphTokens) {
        if (drop < 0 || drop >= r.nGlyphTokens) return Buy::NoSlot;
        for (int g = drop; g + 1 < r.nGlyphTokens; g++) r.glyphTokens[g] = r.glyphTokens[g + 1];
        r.nGlyphTokens--;
      }
      r.glyphTokens[r.nGlyphTokens++] = o.id;
      stat(r, Cat::Stat::glyphs_total);
      break;
    default: return Buy::Cant;
  }
  r.coin -= p;
  o.sold = 1;
  stat(r, Cat::Stat::bought_total);
  return Buy::Ok;
}

inline Buy buyTrinket(Shop& s, Run& r) {
  Offer& o = s.trinket;
  if (o.kind != OfferKind::Trinket) return Buy::Cant;
  if (o.sold) return Buy::Sold;
  if (r.nTrinkets >= TrinketSlots) return Buy::NoSlot;
  const int p = cost(r, o);
  if (r.coin < p) return Buy::NoCoin;
  r.coin -= p;
  r.trinkets[r.nTrinkets++] = o.id;
  if (o.id == Cat::Trinket_moneylender) r.loansLeft++;
  o.sold = 1;
  stat(r, Cat::Stat::bought_total);
  return Buy::Ok;
}

inline Buy buyMod(Shop& s, Run& r, const Career& car) {
  Offer& o = s.mod;
  if (o.kind != OfferKind::Mod) return Buy::Cant;
  if (o.sold) return Buy::Sold;
  const int p = cost(r, o);
  if (r.coin < p) return Buy::NoCoin;
  r.coin -= p;
  r.mods |= 1u << o.id;
  o.sold = 1;
  stat(r, Cat::Stat::bought_total);
  // Mods that change the stall change this one too, not just the next.
  if (o.id == Cat::Mod_overstock) addShelfRelic(s, r, car, false);
  if (o.id == Cat::Mod_night_market) addShelfConsumable(s, r, car);
  if (o.id == Cat::Mod_liquidation) s.rerollCost = (int16_t)(s.rerollCost - 2 < 1 ? 1 : s.rerollCost - 2);
  return Buy::Ok;
}

inline Buy polish(Run& r, int i) {
  if (i < 0 || i >= r.nRelics) return Buy::Cant;
  if (!Cat::Relics[r.relics[i].id].hasFn) return Buy::NoPolish;
  if (r.relics[i].tier >= 3) return Buy::MaxPolish;
  if (!canPolish(r, i)) return Buy::NoCoin;
  r.coin -= polishPrice(r, r.relics[i]);
  r.relics[i].tier++;
  stat(r, Cat::Stat::polishes);
  return Buy::Ok;
}

// New relics on the shelf (the wares stay). One note's shelf is only ever its
// Caller, so there's nothing to reroll there.
inline Buy reroll(Shop& s, Run& r, const Career& car) {
  if (oneNote(r)) return Buy::Cant;
  if (s.freeRerolls > 0) {
    s.freeRerolls--;
  } else {
    if (r.coin < s.rerollCost) return Buy::NoCoin;
    r.coin -= s.rerollCost;
    s.rerollCost += RerollStep;
  }
  rollRelics(s, r, car, false);
  s.star = -1;
  return Buy::Ok;
}

// Leave the shop: the BARGAIN wears off, and records are kept.
inline void closeShop(Shop& s, Run& r) {
  // Long receipt: every coin spent at this stall, net of sales (openShop noted the coin you came in with).
  // A stall saved before the coin-in had a place of its own kept it in Long receipt's, where x never passes 2.
  double in = r.state[CoinInSlot];
  if (in == 0 && r.state[Cat::R_long_receipt] > 2) in = r.state[Cat::R_long_receipt];
  const double spent = in - r.coin;
  r.state[Cat::R_long_receipt] = owns(r, Cat::R_long_receipt) ? (spent > 0 ? (0.05 * spent > 2 ? 2 : 0.05 * spent) : 0) : 0;
  r.state[CoinInSlot] = 0;
  if (r.coin == 0) stat(r, Cat::Stat::broke_shop);
  best(r, Cat::Stat::max_coin, (uint32_t)r.coin);
  r.bargain = 0;
  s.open = 0;
}

// ---- The hint star ------------------------------------------------------------------

// How much Resonance a build makes on your recent sales (or a typical sale,
// before there are any).
inline double recentPower(const Run& r, const RecentSales& mem) {
  double total = 0;
  if (mem.n == 0) {
    Sale c;
    clearSale(c);
    c.e = r.focus;
    c.units = 6;
    c.price = 5.5;
    c.n = 8;
    c.ward = Fringe;
    c.faction = Hearth;
    c.channel = 6;
    c.holdKinds = 1;
    c.oneKind = true;
    c.lastE = r.focus;
    c.nightsSeen = 1;
    for (int vi = 0; vi < 6; vi++) {
      Sale k = c;
      k.vi = vi;
      total += score(k, r).resonance;
    }
    return total;
  }
  for (int i = 0; i < mem.n; i++) {
    Sale k = mem.s[i];
    const int lvl = r.attune[k.e];
    k.lvl = lvl;
    k.mult = 1 + lvl + lvl * hasMod(r, Cat::Mod_tuning_core);   // as the build stands now
    total += score(k, r).resonance;
  }
  return total;
}

// What a build is worth for paying Tithes: Resonance per sale, times visits a
// Night, over how big its Tithes are. So Overclock's lost visits and Blood
// moon's dearer Tithes count against them.
inline double buildValue(const Run& r, const RecentSales& mem) {
  double v = recentPower(r, mem) * visitsFor(r);
  if (owns(r, Cat::R_blood_moon)) v /= 1.35;
  return v;
}

inline Run& scratchRun() {
  static Run t;
  return t;
}

// The gain from adding relic `id` (dropping `drop` if your slots are full).
inline double relicGain(const Run& r, const RecentSales& mem, int id, int ed, int drop) {
  const double base = buildValue(r, mem);
  Run& t = scratchRun();
  t = r;
  if (drop >= 0) removeRelicAt(t, drop);
  addRelic(t, id, ed);
  return buildValue(t, mem) / (base > 1 ? base : 1) - 1;
}

// The weakest relic to sell for room, judged the same way.
inline int weakestRelic(const Run& r, const RecentSales& mem) {
  int best = -1;
  double bestPower = -1;
  for (int i = 0; i < r.nRelics; i++) {
    if (glassHouseCrown(r, i) || r.relics[i].ed == Cat::Edition_phantom) continue;
    Run& t = scratchRun();
    t = r;
    removeRelicAt(t, i);
    const double p = buildValue(t, mem);
    if (p > bestPower) { bestPower = p; best = i; }
  }
  return best;
}

// The gain from the next Polish tier of relic `i`.
inline double polishGain(const Run& r, const RecentSales& mem, int i) {
  if (i < 0 || i >= r.nRelics || !Cat::Relics[r.relics[i].id].hasFn || r.relics[i].tier >= 3) return 0;
  const double base = buildValue(r, mem);
  Run& t = scratchRun();
  t = r;
  t.relics[i].tier++;
  return buildValue(t, mem) / (base > 1 ? base : 1) - 1;
}

// Star what would help most, if anything would: `star` is the best relic on
// the shelf (what the bot buys, as the simulator does), and `starPolish` a
// Polish of one you hold that beats it, which is where the screen then puts
// the star (on the YOURS tab). From the quiet rung there is no star.
inline void starBest(Shop& s, const Run& r, const RecentSales& mem) {
  s.star = -1;
  s.starPolish = 0;
  if (deckQuiet(r)) return;
  double bestGain = 0.05;
  const bool full = slotsUsed(r) >= relicSlots(r);
  const int drop = full ? weakestRelic(r, mem) : -1;
  for (int k = 0; k < s.nRelics; k++) {
    const Offer& o = s.relics[k];
    if (o.sold) continue;
    // A first stall with nothing owned yet: the Caller for your focus, always.
    if (s.night == 0 && r.nRelics == 0 && o.id == Cat::R_ember_caller + r.focus) { s.star = (int8_t)k; return; }
    const double g = relicGain(r, mem, o.id, o.ed, o.ed == Cat::Edition_phantom ? -1 : drop);
    if (g > bestGain) { bestGain = g; s.star = (int8_t)k; }
  }
  for (int i = 0; i < r.nRelics; i++) {
    const double g = polishGain(r, mem, i);
    if (g > bestGain) { bestGain = g; s.starPolish = (uint8_t)(i + 1); }
  }
}

// The gain of what the star marks on screen, 0 with no star.
inline double starGain(const Shop& s, const Run& r, const RecentSales& mem) {
  if (s.starPolish) return polishGain(r, mem, s.starPolish - 1);
  if (s.star < 0 || s.star >= s.nRelics) return 0;
  const Offer& o = s.relics[s.star];
  const bool full = slotsUsed(r) >= relicSlots(r);
  return relicGain(r, mem, o.id, o.ed, full && o.ed != Cat::Edition_phantom ? weakestRelic(r, mem) : -1);
}

} // namespace Game
