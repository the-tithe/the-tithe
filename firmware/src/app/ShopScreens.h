#pragma once
#include "AppState.h"
#include "RunScreens.h"
#include "../game/Shop.h"

// The Collector's shop: three tabs (relics, other wares, your own relics to
// Polish or sell), a details pane, rerolls, and the hint star.

namespace App {

inline int pendingBuy = -1;   // a relic offer waiting for you to pick what to sell for room

enum ShopTab { TabRelics, TabWares, TabYours, TabCount };

// The wares tab: consumables, then the trinket, then the mod.
inline int waresCount() {
  const Game::Shop& s = game.shop;
  return s.nCons + (s.trinket.kind != Game::OfferKind::None) + (s.mod.kind != Game::OfferKind::None);
}
inline Game::Offer* wareAt(int i) {
  Game::Shop& s = game.shop;
  if (i < s.nCons) return &s.cons[i];
  i -= s.nCons;
  if (s.trinket.kind != Game::OfferKind::None) { if (i == 0) return &s.trinket; i--; }
  if (s.mod.kind != Game::OfferKind::None && i == 0) return &s.mod;
  return nullptr;
}

inline int rowsIn(int t) {
  switch (t) {
    case TabRelics: return game.shop.nRelics;
    case TabWares: return waresCount();
    default: return run().nRelics;
  }
}

inline void leaveShop() {
  Game::closeShop(game.shop, run());
  beginNightNow();
}

inline void bought(Game::Buy b, const char* what) {
  char m[40];
  switch (b) {
    case Game::Buy::Ok: snprintf(m, sizeof(m), "%s", what); toast(m, UI::Accent); beep(988, 60); beep(1319, 90); break;
    case Game::Buy::NoCoin: toast("NOT ENOUGH COIN", UI::Rec); beep(180, 80); break;
    case Game::Buy::NoSlot: toast("NO ROOM FOR IT", UI::Rec); beep(180, 80); break;
    case Game::Buy::Sold: toast("ALREADY BOUGHT", UI::Dim); break;
    case Game::Buy::NoPolish: toast("THIS RELIC HAS NOTHING TO POLISH", UI::Dim); break;
    case Game::Buy::MaxPolish: toast("ALREADY AT POLISH III", UI::Dim); break;
    case Game::Buy::TooMany: toast("8 RELICS IS THE MOST YOU CAN CARRY", UI::Rec); beep(180, 80); break;
    default: toast("THAT CAN'T BE DONE", UI::Rec); beep(180, 80); break;
  }
  if (b == Game::Buy::Ok) {
    Game::starBest(game.shop, run(), game.recent);
    save(true);
  }
}

// Leaving the stall, selling a relic and polishing one each take two presses: the
// first asks, the second does it, and any move of the cursor clears the question.
// So does a reroll that would throw away something the shelf has for a reason.
inline bool asked(ShopAsk what, int row) {
  if (shopAsk == what && shopAskRow == row) { shopAsk = AskNone; return true; }
  shopAsk = what;
  shopAskRow = row;
  return false;
}

// Why relic i of yours can't be sold to make room, or nullptr. The gift screen
// at the dawn asks the same.
inline const char* cantSellForRoom(const Game::Run& r, int i) {
  if (Game::glassHouseCrown(r, i)) return "THE GLASS CROWN CAN'T BE SOLD";
  if (r.relics[i].ed == Cat::Edition_phantom) return "PHANTOMS TAKE NO SLOT: PICK ANOTHER";
  return nullptr;
}

// What a reroll would take with it, as the question R asks first; false when
// the shelf has nothing special.
inline bool rerollLoses(const Game::Shop& s, char* b, int cap) {
  int n = 0;
  const char* what = nullptr;
  char caller[24];
  for (int i = 0; i < s.nRelics; i++) {
    const Game::Offer& o = s.relics[i];
    if (o.sold) continue;
    if (o.half) { n++; what = what ? what : "THE HALF-PRICE RELIC"; }
    if (o.marks & Game::Mark_Caller) {
      n++;
      if (!what) { char nm[20]; upper(Cat::Relics[o.id].name, nm, sizeof(nm)); snprintf(caller, sizeof(caller), "THE %s", nm); what = caller; }
    }
    if (o.marks & Game::Mark_Rare) { n++; what = what ? what : "THE RARE SIGHT RARE"; }
    if (o.marks & Game::Mark_Shine) { n++; what = what ? what : "YOUR SHINE"; }
  }
  if (n == 0) return false;
  if (n == 1) snprintf(b, cap, "R AGAIN: %s GOES TOO", what);
  else snprintf(b, cap, "R AGAIN: %d SPECIAL OFFERS GO TOO", n);
  return true;
}

// Buy Attunement k on energy e, and say if Attuner's fork made it two.
inline void attune(int k, int e) {
  Game::Run& r = run();
  const int before = r.attune[e];
  const Game::Buy b = Game::buyConsumable(game.shop, r, k, e);
  char m[40];
  snprintf(m, sizeof(m), "%s ATTUNED%s", energyWord(e), r.attune[e] - before >= 2 ? " TWICE" : "");
  bought(b, m);
}

// What the stall couldn't do when it opened (Shop.h stallNotes), said once.
inline void sayStallNotes() {
  using namespace Game;
  if (!stallNotes) return;
  if (stallNotes & Note_NoRare) toastAfter("RARE SIGHT: NO RARES LEFT TO SHOW", UI::Rec);
  if (stallNotes & Note_MercyNoRelic) toastAfter("MERCY: NO RELIC LEFT TO HALVE", UI::Rec);
  if (stallNotes & Note_MercyOneNote) toastAfter("MERCY: +1 ATTUNEMENT ON YOUR FOCUS", UI::Accent);
  stallNotes = 0;
}

inline void updateShop(const Ctx& c, const Press& p) {
  (void)c;
  using namespace Game;
  Run& r = run();
  Shop& s = game.shop;
  sayStallNotes();
  if (p.up || p.down || p.left || p.right) { shopAsk = AskNone; if (toastColor == UI::Rec) toastT = 0; }
  if (p.back) {
    if (asked(AskLeave, -1)) { leaveShop(); return; }
    char m[40];   // the stall is gone for the Night once you leave, and its free rerolls with it
    if (s.freeRerolls > 0) snprintf(m, sizeof(m), "ESC AGAIN: %d FREE REROLL%s UNUSED", s.freeRerolls, s.freeRerolls == 1 ? "" : "S");
    else snprintf(m, sizeof(m), "ESC AGAIN TO LEAVE THE STALL");
    toast(m, UI::Rec);
    return;
  }
  if (p.right) { tab = (tab + 1) % TabCount; sel = 0; }
  if (p.left) { tab = (tab + TabCount - 1) % TabCount; sel = 0; }
  const int rows = rowsIn(tab);
  if (p.down && sel < rows - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (p.ch == 'r') {
    char m[40];
    if (oneNote(r)) toast("NOTHING TO REROLL ON ONE NOTE", UI::Dim);
    else if (s.freeRerolls <= 0 && r.coin < s.rerollCost) bought(Buy::NoCoin, "");
    else if (rerollLoses(s, m, sizeof(m)) && !asked(AskReroll, -1)) toast(m, UI::Rec);
    else {
      shopAsk = AskNone;
      bought(reroll(s, r, game.career), "NEW RELICS");
      if (tab == TabRelics) sel = 0;
    }
  }
  if (tab == TabYours && p.ch == 'x' && sel < r.nRelics) {
    if (glassHouseCrown(r, sel)) { toast(cantSellForRoom(r, sel), UI::Rec); beep(180, 80); return; }
    char n[24], m[40];
    upper(Cat::Relics[r.relics[sel].id].name, n, sizeof(n));
    if (!asked(AskSell, sel)) {
      const int gain = sellGain(r, sel);
      const bool capped = gain < sellBack(r, r.relics[sel]), polished = r.relics[sel].tier > 0;
      if (capped && polished) snprintf(m, sizeof(m), "X AGAIN: +%dc (15c MOST), POLISH LOST", gain);
      else if (capped) snprintf(m, sizeof(m), "X AGAIN TO SELL: +%dc (15c IS THE MOST)", gain);
      else if (polished) snprintf(m, sizeof(m), "X AGAIN TO SELL FOR %dc (POLISH LOST)", gain);
      else snprintf(m, sizeof(m), "X AGAIN TO SELL IT FOR %dc", gain);
      toast(m, UI::Rec);
      return;
    }
    snprintf(m, sizeof(m), "SOLD %s", n);
    bought(sellRelic(r, sel), m);
    if (sel >= r.nRelics && sel > 0) sel--;
  }
  if (!p.enter || rows == 0) return;
  if (tab == TabRelics) {
    const Offer& o = s.relics[sel];
    if (o.sold) { bought(Buy::Sold, ""); return; }
    const bool full = o.ed != Cat::Edition_phantom && slotsUsed(r) >= relicSlots(r);
    if (full) {   // pick a relic to sell for room; the buy checks the coin
      pendingBuy = sel;
      go(Mode::ShopDrop);
      sel = weakestRelic(r, game.recent) >= 0 ? weakestRelic(r, game.recent) : 0;
      return;
    }
    char n[24], m[40];
    upper(Cat::Relics[o.id].name, n, sizeof(n));
    snprintf(m, sizeof(m), "BOUGHT %s", n);
    bought(buyRelic(s, r, sel), m);
  } else if (tab == TabWares) {
    Offer* o = wareAt(sel);
    if (!o) return;
    if (o->kind == OfferKind::Attune) {
      if (o->sold) { bought(Buy::Sold, ""); return; }
      if (r.coin < cost(r, *o)) { bought(Buy::NoCoin, ""); return; }
      if (focusOnly(r)) { attune(sel, r.focus); return; }   // nothing else to tune: no picker
      pendingBuy = sel;
      go(Mode::ShopAttune);
      sel = r.focus;
      return;
    }
    if (o->kind == OfferKind::Trinket && !o->sold && r.nTrinkets >= TrinketSlots) {
      char m[40];
      snprintf(m, sizeof(m), "TRINKET SLOTS FULL (%d OF %d)", r.nTrinkets, TrinketSlots);
      toast(m, UI::Rec);
      beep(180, 80);
      return;
    }
    const bool spellsFull = o->kind == OfferKind::Spell && r.nSpells >= spellSlots(r);
    const bool glyphsFull = o->kind == OfferKind::Glyph && r.nGlyphTokens >= GlyphTokens;
    if ((spellsFull || glyphsFull) && !o->sold) {   // let one go to make room; the buy checks the coin
      if (r.coin < cost(r, *o)) { bought(Buy::NoCoin, ""); return; }
      pendingBuy = sel;
      go(Mode::ShopForget);
      sel = 0;
      return;
    }
    if (o->kind == OfferKind::Trinket) bought(buyTrinket(s, r), "TRINKET BOUGHT");
    else if (o->kind == OfferKind::Mod) bought(buyMod(s, r, game.career), "THE DECK IS CHANGED");
    else bought(buyConsumable(s, r, sel), o->kind == OfferKind::Spell ? "SPELL LEARNED" : "GLYPH TO CARVE");
  } else {
    if (!canPolish(r, sel)) { bought(polish(r, sel), "POLISHED"); return; }   // says why not
    if (!asked(AskPolish, sel)) {
      char m[40];
      snprintf(m, sizeof(m), "ENTER AGAIN TO POLISH FOR %dc", polishPrice(r, r.relics[sel]));
      toast(m, UI::Rec);
      return;
    }
    bought(polish(r, sel), "POLISHED");
  }
}

inline void updateShopDrop(const Ctx& c, const Press& p) {
  (void)c;
  Game::Run& r = run();
  if (p.back) { go(Mode::Shop); sel = pendingBuy; return; }   // back on the relic you were buying
  if (p.down && sel < r.nRelics - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (!p.enter) return;
  if (const char* why = cantSellForRoom(r, sel)) { toast(why, UI::Rec); beep(180, 80); return; }   // stay and pick again
  const int k = pendingBuy, drop = sel;   // go() puts the cursor back to 0
  go(Mode::Shop);
  char n[24], m[40];
  upper(Cat::Relics[game.shop.relics[k].id].name, n, sizeof(n));
  snprintf(m, sizeof(m), "BOUGHT %s", n);
  bought(Game::buyRelic(game.shop, r, k, drop), m);
  sel = k;
}

inline void updateShopAttune(const Ctx& c, const Press& p) {
  (void)c;
  if (p.back) { go(Mode::Shop); tab = TabWares; sel = pendingBuy; return; }
  if (p.right) sel = (sel + 1) % Game::EnergyN;
  if (p.left) sel = (sel + Game::EnergyN - 1) % Game::EnergyN;
  if (!p.enter) return;
  const int e = sel, k = pendingBuy;
  go(Mode::Shop);
  tab = TabWares;
  sel = k;
  attune(k, e);
}

// Full of spells or glyphs: pick one to let go of for the one at the stall.
inline bool forgettingSpells() {
  const Game::Offer* o = wareAt(pendingBuy);
  return o && o->kind == Game::OfferKind::Spell;
}
inline int forgetCount() { return forgettingSpells() ? run().nSpells : run().nGlyphTokens; }

inline void updateShopForget(const Ctx& c, const Press& p) {
  (void)c;
  if (p.back) { go(Mode::Shop); tab = TabWares; sel = pendingBuy; return; }
  if (p.down && sel < forgetCount() - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (!p.enter) return;
  const int k = pendingBuy, drop = sel;   // go() puts the cursor back to 0
  const bool spells = forgettingSpells();
  go(Mode::Shop);
  tab = TabWares;
  sel = k;
  bought(Game::buyConsumable(game.shop, run(), k, -1, drop), spells ? "SPELL LEARNED" : "GLYPH TO CARVE");
}

// ---- Drawing ----------------------------------------------------------------------

// A relic's row: its rarity dot, name, Polish tier and shine. The shine is left
// off when it would run into whatever sits at `maxX` (the details pane says it).
inline void relicLine(Canvas& g, int y, int id, int ed, int tier, bool selRow, bool dim, int maxX = UI::W - 4) {
  const Cat::RelicDef& d = Cat::Relics[id];
  g.fillRect(4, y + 2, 3, 3, dim ? UI::Faint : UI::rarityColor(d.rarity));
  char n[28];
  upper(d.name, n, sizeof(n));
  UI::text(g, 11, y, n, dim ? UI::Dim : selRow ? UI::SelText : UI::Text);
  int x = 11 + UI::textW(n) + 6;
  if (tier > 0) {
    static const char* const t[] = {"", "I", "II", "III"};
    UI::text(g, x, y, t[tier], UI::Accent);
    x += UI::textW(t[tier]) + 6;
  }
  if (ed >= 0) {
    char e[12];
    upper(Cat::Editions[ed].name, e, sizeof(e));
    if (x + UI::textW(e) <= maxX) UI::text(g, x, y, e, UI::EnergyColor[4]);
  }
}

// The pane under a list: a name, a note on the right, the text and a flavor
// line. Three lines at the usual pitch; when the words need a fourth, the lines
// close up so nothing is cut (the flavor goes first).
inline void details(Canvas& g, const char* name, const char* sub, const char* text, const char* flavor, uint16_t c) {
  g.drawFastHLine(0, 78, UI::W, UI::Sep);
  UI::text(g, 4, 82, name, c);
  if (sub) UI::textRight(g, UI::W - 4, 82, sub, UI::Dim);
  const int width = UI::W - 8;
  const int tl = UI::wrapLines(width, text), fl = flavor ? UI::wrapLines(width, flavor) : 0;
  const bool tight = tl + fl > 3;
  const int y = tight ? 91 : 92, h = tight ? 8 : 10, room = tight ? 4 : 3;
  int lines = UI::wrap(g, 4, y, width, text, UI::Text, room, h, 0, true);
  if (flavor && lines < room) UI::wrap(g, 4, y + lines * h, width, flavor, UI::Dim, room - lines, h);
}

// What a relic has picked up: its Polish, its shine, and whether it withers,
// after `lead` (what Copycat sigil copies) when there is one. Shown in place of
// the flavor line when there's anything to say.
inline const char* relicExtras(int tier, int ed, int withered, const char* flavor, char* b, int cap, const char* lead = nullptr) {
  int k = 0;
  b[0] = 0;
  static const char* const t[] = {"", "I", "II", "III"};
  static const char* const x[] = {"", "x1.5", "x2", "x2.5"};
  if (lead) k += snprintf(b + k, cap - k, "%s ", lead);
  if (tier > 0 && k < cap) k += snprintf(b + k, cap - k, "Polish %s: %s strength. ", t[tier], x[tier]);
  if (ed >= 0 && k < cap) k += snprintf(b + k, cap - k, "%s: %s. ", Cat::Editions[ed].name, Cat::Editions[ed].text);
  if (withered >= 0 && k < cap) k += snprintf(b + k, cap - k, "Withers in %d Night%s. ", withered, withered == 1 ? "" : "s");
  if (k == 0) return flavor;
  if (k > 0 && k < cap && b[k - 1] == ' ') b[k - 1] = 0;
  return b;
}

// What Copycat sigil copies (`id` is the relic in the details pane), or nullptr
// for any other relic. From the shelf it's what it would copy once yours.
inline const char* copyLine(const Game::Run& r, int id, bool owned, char* b, int cap) {
  if (id != Cat::R_copycat_sigil) return nullptr;
  const int t = Game::copycatPick(r);
  if (t < 0) snprintf(b, cap, "Nothing to copy yet.");
  else snprintf(b, cap, "%s: %s.", owned ? "Copying" : "Would copy", Cat::Relics[t].name);
  return b;
}

inline void wareName(const Game::Offer& o, char* b, int cap) {
  char n[24];
  switch (o.kind) {
    case Game::OfferKind::Attune: snprintf(b, cap, "ATTUNEMENT"); break;
    case Game::OfferKind::Spell: snprintf(b, cap, "SPELL: %s", Cat::Spells[o.id].name); break;
    case Game::OfferKind::Glyph: upper(Cat::Glyphs[o.id].name, n, sizeof(n)); snprintf(b, cap, "GLYPH: %s", n); break;
    case Game::OfferKind::Trinket: upper(Cat::Trinkets[o.id].name, n, sizeof(n)); snprintf(b, cap, "TRINKET: %s", n); break;
    case Game::OfferKind::Mod: upper(Cat::Mods[o.id].name, n, sizeof(n)); snprintf(b, cap, "MOD: %s", n); break;
    default: snprintf(b, cap, "-"); break;
  }
}

inline void drawShop(Canvas& g, const Ctx& c) {
  (void)c;
  using namespace Game;
  g.fillSprite(UI::Bg);
  const Run& r = run();
  const Shop& s = game.shop;
  char b[48];
  // The top line, from the right: your coin, the Tithe ahead, then what's
  // different about this stall. The title gives way when they need the room.
  snprintf(b, sizeof(b), "%dc", (int)r.coin);
  UI::textRight(g, UI::W - 3, 3, b, UI::SelText);
  int hx = UI::W - 3 - UI::textW(b) - 10;
  // Charm ward and Eclipse ward may cancel the Omen or the Eclipse ahead: what
  // they'd change is shown with a "?".
  const int omen = stallOmen(r);
  const int ecl = r.night == Nights - 1 ? r.eclipse : -1;
  const bool omenMaybe = omen >= 0 && hasMod(r, Cat::Mod_charm_ward);
  const bool eclMaybe = ecl >= 0 && hasMod(r, Cat::Mod_eclipse_ward);
  if (r.night < Game::Nights) {   // the Night this stall is for
    char n[16];
    const double t = titheAhead(r);
    const bool maybe = (omenMaybe && titheFor(r, r.night, -1, ecl) != t) || (eclMaybe && titheFor(r, r.night, omen, -1) != t);
    snprintf(b, sizeof(b), "TITHE %s%s", UI::num(t, n, sizeof(n)), maybe ? "?" : "");
    UI::textRight(g, hx, 3, b, UI::Dim);
    hx -= UI::textW(b) + 10;
  }
  if (r.bargain) { UI::textRight(g, hx, 3, "BARGAIN", UI::Accent); hx -= UI::textW("BARGAIN") + 10; }
  if (s.mercy) { UI::textRight(g, hx, 3, "MERCY", UI::Accent); hx -= UI::textW("MERCY") + 10; }
  if (3 + UI::textW("THE COLLECTOR") <= hx) UI::text(g, 3, 3, "THE COLLECTOR", UI::SelText);
  else if (3 + UI::textW("COLLECTOR") <= hx) UI::text(g, 3, 3, "COLLECTOR", UI::SelText);
  // The tabs. YOURS carries your relic slots, and the star when a Polish there
  // beats the shelf.
  static const char* const tabs[] = {"RELICS", "WARES", "YOURS"};
  int tx = 3;
  for (int i = 0; i < TabCount; i++) {
    char label[16];
    if (i == TabYours) snprintf(label, sizeof(label), "%s %d/%d", tabs[i], slotsUsed(r), relicSlots(r));
    else snprintf(label, sizeof(label), "%s", tabs[i]);
    if (i == TabYours && s.starPolish) { UI::text(g, tx, 14, "*", UI::SelText); tx += UI::CharW; }
    if (i == tab) g.fillRect(tx - 2, 13, UI::textW(label) + 4, 10, UI::SelBg);
    UI::text(g, tx, 14, label, i == tab ? UI::SelText : UI::Dim);
    tx += UI::textW(label) + 12;
  }
  // On the right: the Omen the Night ahead will have, once the stall can see
  // it; else, with full slots, the other way to spend.
  if (omen >= 0) {
    char nm[24];
    upper(Cat::Omens[omen].name, nm, sizeof(nm));
    if (omenMaybe) strncat(nm, "?", sizeof(nm) - strlen(nm) - 1);
    UI::textRight(g, UI::W - 3, 14, nm, omenMaybe ? UI::Dim : UI::EnergyColor[3]);
  } else {
    bool canPolishAny = false;
    for (int i = 0; i < r.nRelics; i++) canPolishAny |= canPolish(r, i);
    if (tab == TabRelics && slotsUsed(r) >= relicSlots(r) && canPolishAny) UI::textRight(g, UI::W - 3, 14, "POLISH >", UI::Accent);
  }
  const int y0 = 27, rowH = 10, show = 5;
  const int rows = rowsIn(tab);
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < rows && i < top + show; i++) {
    const int y = y0 + (i - top) * rowH;
    if (i == sel) g.fillRect(0, y - 1, UI::W, rowH, UI::SelBg);
    if (tab == TabRelics) {
      const Offer& o = s.relics[i];
      const bool starred = i == s.star && !s.starPolish;
      if (o.sold) snprintf(b, sizeof(b), "BOUGHT");   // yours now; SOLD is what you do on the YOURS tab
      else snprintf(b, sizeof(b), "%s%dc", starred ? "* " : "", cost(r, o));
      relicLine(g, y, o.id, o.ed, 0, i == sel, o.sold, UI::W - 4 - UI::textW(b) - 6);
      UI::textRight(g, UI::W - 4, y, b, o.sold ? UI::Dim : starred ? UI::SelText : UI::Text);
    } else if (tab == TabWares) {
      const Offer* o = wareAt(i);
      const bool full = o->kind == OfferKind::Trinket && !o->sold && r.nTrinkets >= TrinketSlots;   // no room for another
      wareName(*o, b, sizeof(b));
      UI::text(g, 11, y, b, o->sold || full ? UI::Dim : i == sel ? UI::SelText : UI::Text);
      if (full) snprintf(b, sizeof(b), "FULL");
      else snprintf(b, sizeof(b), o->sold ? "BOUGHT" : "%dc", cost(r, *o));
      UI::textRight(g, UI::W - 4, y, b, o->sold || full ? UI::Dim : UI::Text);
    } else {
      const OwnedRelic& x = r.relics[i];
      const bool starred = i + 1 == s.starPolish;
      if (!Cat::Relics[x.id].hasFn) snprintf(b, sizeof(b), "-");   // nothing to polish
      else if (x.tier < 3) snprintf(b, sizeof(b), "%s%dc", starred ? "* " : "", polishPrice(r, x));
      else snprintf(b, sizeof(b), "MAX");
      int left = UI::W - 4 - UI::textW(b) - 6;
      UI::textRight(g, UI::W - 4, y, b, starred ? UI::SelText : UI::Dim);
      if (x.withered >= 0) {   // on a Bone Pact: how many Nights it has left
        char w[16];
        snprintf(w, sizeof(w), "WITHERS %d", x.withered);
        UI::textRight(g, left, y, w, UI::Rec);
        left -= UI::textW(w) + 6;
      }
      if (r.state[x.id] > 0 && x.id != Cat::R_long_receipt) {   // a growing relic: how far it has come
        char gt[16];
        growthText(x.id, r.state[x.id], gt, sizeof(gt));
        UI::textRight(g, left, y, gt, UI::Accent);
        left -= UI::textW(gt) + 6;
      }
      relicLine(g, y, x.id, x.ed, x.tier, i == sel, false, left);
    }
  }
  if (rows == 0) UI::textCenter(g, 45, tab == TabYours ? "NO RELICS YET" : "NOTHING LEFT", UI::Dim);
  // Details of the row under the cursor.
  if (rows > 0 && sel < rows) {
    char n[28], sub[28];
    if (tab == TabRelics || tab == TabYours) {
      const int id = tab == TabRelics ? s.relics[sel].id : r.relics[sel].id;
      const Cat::RelicDef& d = Cat::Relics[id];
      upper(d.name, n, sizeof(n));
      char school[16];
      upper(Cat::SchoolNames[d.school], school, sizeof(school));
      snprintf(sub, sizeof(sub), "%s %s", Cat::RarityNames[d.rarity], school);
      if (tab == TabRelics && s.relics[sel].stall >= 0) snprintf(sub, sizeof(sub), "%s STALL", factionWord(s.relics[sel].stall));
      if (tab == TabRelics && (s.relics[sel].marks & Mark_Shine)) snprintf(sub, sizeof(sub), "YOUR SHINE");   // from the Portent
      char ex[128], cp[40];
      const char* lead = copyLine(r, id, tab == TabYours, cp, sizeof(cp));
      const char* fl = tab == TabRelics
                           ? relicExtras(0, s.relics[sel].ed, s.relics[sel].withered, d.flavor, ex, sizeof(ex), lead)
                           : relicExtras(r.relics[sel].tier, r.relics[sel].ed, r.relics[sel].withered, d.flavor, ex, sizeof(ex), lead);
      details(g, n, sub, d.text, fl, UI::rarityColor(d.rarity));
    } else {
      const Offer* o = wareAt(sel);
      wareName(*o, n, sizeof(n));
      const char* text = "", *fl = nullptr;
      switch (o->kind) {
        case OfferKind::Attune:
          text = focusOnly(r) ? "+1 Amp on your focus energy, for the rest of the week."
                              : "+1 Amp on one energy, for the rest of the week. You choose which.";
          break;
        case OfferKind::Spell: text = Cat::Spells[o->id].text; fl = Cat::Spells[o->id].flavor; break;
        case OfferKind::Glyph: text = Cat::Glyphs[o->id].text; fl = "Carve it into a well you visit."; break;
        case OfferKind::Trinket: text = Cat::Trinkets[o->id].text; fl = Cat::Trinkets[o->id].flavor; break;
        case OfferKind::Mod: text = Cat::Mods[o->id].text; break;
        default: break;
      }
      details(g, n, nullptr, text, fl, UI::Accent);
    }
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "LEAVE", UI::Text);
  if (tab == TabYours) {
    x = UI::keyHint(g, x, "ENTER", "POLISH", UI::Text);
    if (sel < r.nRelics && !glassHouseCrown(r, sel)) {   // the crown can't be sold
      snprintf(b, sizeof(b), "SELL +%dc", sellGain(r, sel));
      UI::keyHint(g, x, "X", b, UI::Text);
    }
  } else {
    x = UI::keyHint(g, x, "ENTER", "BUY", UI::Text);
    if (!oneNote(r)) {   // One note's shelf has nothing to reroll
      if (s.freeRerolls > 1) snprintf(b, sizeof(b), "REROLL FREE x%d", s.freeRerolls);
      else if (s.freeRerolls == 1) snprintf(b, sizeof(b), "REROLL FREE");
      else snprintf(b, sizeof(b), "REROLL %dc", s.rerollCost);
      UI::keyHint(g, x, "R", b, UI::Text);
    }
  }
}

inline void drawShopDrop(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  const Game::Offer& o = game.shop.relics[pendingBuy];
  char b[48], n[28];
  upper(Cat::Relics[o.id].name, n, sizeof(n));
  snprintf(b, sizeof(b), "SELL ONE FOR %s", n);
  UI::text(g, 3, 3, b, UI::SelText);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  const int show = 6;   // rows above the details pane (Phantoms can make it 8)
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < r.nRelics && i < top + show; i++) {
    const int y = 17 + (i - top) * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    const Game::OwnedRelic& x = r.relics[i];
    const bool cant = cantSellForRoom(r, i) != nullptr;   // dimmed, as on the gift screen
    int left = UI::W - 4;
    if (!cant) {
      snprintf(b, sizeof(b), "+%dc", Game::sellBack(r, x));
      UI::textRight(g, left, y, b, UI::Dim);
      left -= UI::textW(b) + 6;
    }
    if (x.withered >= 0) {   // as on the YOURS tab
      char w[16];
      snprintf(w, sizeof(w), "WITHERS %d", x.withered);
      UI::textRight(g, left, y, w, UI::Rec);
      left -= UI::textW(w) + 6;
    }
    relicLine(g, y, x.id, x.ed, x.tier, i == sel, cant, left);
  }
  if (sel < r.nRelics) {   // what you'd be giving up, as on the YOURS tab
    const Game::OwnedRelic& x = r.relics[sel];
    const Cat::RelicDef& d = Cat::Relics[x.id];
    char sub[32], habit[16], ex[128], cp[40];
    upper(d.name, n, sizeof(n));
    upper(Cat::SchoolNames[d.school], habit, sizeof(habit));
    snprintf(sub, sizeof(sub), "%s %s", Cat::RarityNames[d.rarity], habit);
    details(g, n, sub, d.text, relicExtras(x.tier, x.ed, x.withered, d.flavor, ex, sizeof(ex), copyLine(r, x.id, true, cp, sizeof(cp))),
            UI::rarityColor(d.rarity));
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "KEEP THEM", UI::Text);
  UI::keyHint(g, x, "ENTER", "SELL AND BUY", UI::Text);
}

inline void drawShopForget(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  const Game::Offer* o = wareAt(pendingBuy);
  if (!o) return;
  const bool spells = forgettingSpells();
  char b[48], n[24];
  if (spells) snprintf(b, sizeof(b), "FORGET ONE FOR %s", Cat::Spells[o->id].name);
  else { upper(Cat::Glyphs[o->id].name, n, sizeof(n)); snprintf(b, sizeof(b), "LET ONE GO FOR %s", n); }
  UI::text(g, 3, 3, b, UI::SelText);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  const int count = forgetCount();
  for (int i = 0; i < count; i++) {
    const int y = 17 + i * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    if (spells) snprintf(b, sizeof(b), "%s", Cat::Spells[r.spells[i]].name);
    else { upper(Cat::Glyphs[r.glyphTokens[i]].name, n, sizeof(n)); snprintf(b, sizeof(b), "%s GLYPH", n); }
    UI::text(g, 11, y, b, i == sel ? UI::SelText : UI::Text);
  }
  if (sel < count) {   // what you'd be letting go of
    if (spells) {
      const Cat::SpellDef& d = Cat::Spells[r.spells[sel]];
      details(g, d.name, "YOURS", d.text, d.flavor, UI::Accent);
    } else {
      const Cat::GlyphDef& d = Cat::Glyphs[r.glyphTokens[sel]];
      upper(d.name, n, sizeof(n));
      details(g, n, "YOURS", d.text, d.flavor, UI::Accent);
    }
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "KEEP THEM", UI::Text);
  UI::keyHint(g, x, "ENTER", spells ? "FORGET AND BUY" : "SWAP", UI::Text);
}

inline void drawShopAttune(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  UI::textCenter(g, 8, "ATTUNE WHICH ENERGY?", UI::SelText);
  for (int e = 0; e < Game::EnergyN; e++) {
    const int x = 22 + e * 44, y = 50;
    if (e == sel) g.drawRect(x - 8, y - 8, 30, 40, UI::SelText);
    UI::energyGlyph(g, x + 4, y, (Energy)e, UI::energyColor(e));
    char b[8];
    snprintf(b, sizeof(b), "%d", r.attune[e]);
    UI::text(g, x + 5, y + 14, b, UI::Text);
  }
  UI::textCenter(g, 32, energyWord(sel), UI::energyColor(sel));
  if (sel == r.focus) UI::textCenter(g, 98, "YOUR FOCUS ENERGY", UI::Accent);
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "< >", "CHOOSE", UI::Text);
  UI::keyHint(g, x, "ENTER", "ATTUNE", UI::Text);
}

// ---- A free relic at the dawn with no room for it (Gift, Conjunction, Empty pockets) --
// First the relic itself, then (ENTER) your relics, to sell one for it. ESC
// twice on the relic leaves it. Row k is a Portent's (0-2), or 3 for Empty
// pockets' relic of the Night.

inline bool giftPicking = false;   // on the list of yours, not the gift
inline bool giftAskLeave = false;  // ESC once on the gift: once more leaves it

struct WaitingRelic { int id, ed, withered; };
inline WaitingRelic waitingRelic(int k) {
  const Game::GiftSwap& s = game.moments.swap;
  const Game::DawnNotes& d = game.moments.dawn;
  if (k == 3) return {d.gift - 1, d.giftEd, -1};
  return {s.id[k] - 1, s.ed[k], s.withered[k]};
}
inline Game::Buy takeWaitingRelic(int k, int drop) {
  if (k == 3) return Game::takeNightRelic(run(), game.moments.dawn, drop);
  return Game::takeGift(run(), game.moments.swap, k, drop);
}

inline void updateGiftSwap(int k, const Press& p) {
  using namespace Game;
  Run& r = run();
  if (!giftPicking) {
    if (p.back) {
      if (!giftAskLeave) { giftAskLeave = true; toast("ESC AGAIN TO LEAVE IT", UI::Rec); return; }
      giftAskLeave = false;
      if (k == 3) {
        game.lastEnd.gift = -2 - waitingRelic(k).id;   // the dawn's line says it was left
        leaveNightRelic(game.moments.dawn);
      } else {
        leaveGift(game.moments.swap, k);
        game.lastEnd.got[k].relic = -2;
      }
      toast("LEFT IT", UI::Dim);
      save(true);
      return;
    }
    if (p.any) giftAskLeave = false;
    if (!p.enter) return;
    if (slotsUsed(r) < relicSlots(r)) {   // room after all
      takeWaitingRelic(k, -1);
      beep(1319, 60);
      save(true);
      return;
    }
    giftPicking = true;
    const int w = weakestRelic(r, game.recent);
    sel = w >= 0 ? w : 0;
    top = 0;
    return;
  }
  if (p.back) { giftPicking = false; return; }
  if (p.down && sel < r.nRelics - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (!p.enter) return;
  char sold[28], m[48];
  upper(Cat::Relics[r.relics[sel].id].name, sold, sizeof(sold));
  const int back = sellGain(r, sel);   // what the sale adds (Shoestring keeps you at 15)
  if (takeWaitingRelic(k, sel) != Buy::Ok) {
    beep(180, 80);
    const char* why = cantSellForRoom(r, sel);
    toast(why ? why : "THAT CAN'T BE DONE", UI::Rec);
    return;
  }
  snprintf(m, sizeof(m), "%s SOLD, +%dc", sold, back);
  toast(m, UI::Accent);
  beep(1319, 60);
  beep(1976, 90);
  giftPicking = false;
  sel = 0;
  save(true);
}

inline void drawGiftSwap(Canvas& g, int k) {
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  const WaitingRelic w = waitingRelic(k);
  const Cat::RelicDef& gd = Cat::Relics[w.id];
  char b[48], n[28], sub[32], habit[16], ex[96];
  upper(gd.name, n, sizeof(n));
  if (!giftPicking) {
    const bool room = Game::slotsUsed(r) < Game::relicSlots(r);   // a relic crumbled at this dawn
    UI::text(g, 3, 3, room ? "A FREE RELIC" : "A FREE RELIC, BUT NO ROOM", UI::SelText);
    g.drawFastHLine(0, 13, UI::W, UI::Sep);
    UI::textCenter(g, 30, n, UI::rarityColor(gd.rarity), 2);
    if (!room) UI::textCenter(g, 56, "SELL ONE OF YOURS TO TAKE IT", UI::Dim);
    upper(Cat::SchoolNames[gd.school], habit, sizeof(habit));
    snprintf(sub, sizeof(sub), "%s %s", Cat::RarityNames[gd.rarity], habit);
    details(g, n, sub, gd.text, relicExtras(0, w.ed, w.withered, gd.flavor, ex, sizeof(ex)), UI::rarityColor(gd.rarity));
    UI::footerLine(g);
    const int x = UI::keyHint(g, 2, "ESC", "LEAVE IT", UI::Text);
    UI::keyHint(g, x, "ENTER", room ? "TAKE IT" : "MAKE ROOM", UI::Text);
    return;
  }
  snprintf(b, sizeof(b), "SELL ONE FOR %s", n);
  UI::text(g, 3, 3, b, UI::SelText);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  const int show = 6;
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < r.nRelics && i < top + show; i++) {
    const int y = 17 + (i - top) * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    const bool cant = Game::glassHouseCrown(r, i) || r.relics[i].ed == Cat::Edition_phantom;
    snprintf(b, sizeof(b), "+%dc", Game::sellBack(r, r.relics[i]));
    relicLine(g, y, r.relics[i].id, r.relics[i].ed, r.relics[i].tier, i == sel, cant, UI::W - 4 - UI::textW(b) - 6);
    if (!cant) UI::textRight(g, UI::W - 4, y, b, UI::Dim);
  }
  if (sel < r.nRelics) {   // what you'd be giving up
    const Game::OwnedRelic& x = r.relics[sel];
    const Cat::RelicDef& d = Cat::Relics[x.id];
    upper(d.name, n, sizeof(n));
    upper(Cat::SchoolNames[d.school], habit, sizeof(habit));
    snprintf(sub, sizeof(sub), "%s %s", Cat::RarityNames[d.rarity], habit);
    details(g, n, sub, d.text, relicExtras(x.tier, x.ed, x.withered, d.flavor, ex, sizeof(ex)), UI::rarityColor(d.rarity));
  }
  UI::footerLine(g);
  const int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  UI::keyHint(g, x, "ENTER", "SELL AND TAKE", UI::Text);
}

// ---- A free spell at the dawn with no slot for it (Scroll) ---------------------------
// First the spell, then (ENTER) your spells, to forget one for it. ESC twice
// on the spell leaves it.

inline bool spellPicking = false;   // on the list of yours, not the Scroll's
inline bool spellAskLeave = false;  // ESC once on the spell: once more leaves it

inline void updateSpellSwap(int k, const Press& p) {
  using namespace Game;
  Run& r = run();
  GiftSwap& s = game.moments.swap;
  if (!spellPicking) {
    if (p.back) {
      if (!spellAskLeave) { spellAskLeave = true; toast("ESC AGAIN TO LEAVE IT", UI::Rec); return; }
      spellAskLeave = false;
      leaveGift(s, k);
      game.lastEnd.got[k].spell = -2;   // the dawn's line says it was left
      toast("LEFT IT", UI::Dim);
      save(true);
      return;
    }
    if (p.any) spellAskLeave = false;
    if (!p.enter) return;
    if (r.nSpells < spellSlots(r)) {   // room after all
      takeSpell(r, s, k, -1);
      beep(1319, 60);
      save(true);
      return;
    }
    spellPicking = true;
    sel = 0;
    return;
  }
  if (p.back) { spellPicking = false; return; }
  if (p.down && sel < r.nSpells - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (!p.enter) return;
  char m[48];
  snprintf(m, sizeof(m), "%s FORGOTTEN", Cat::Spells[r.spells[sel]].name);
  if (takeSpell(r, s, k, sel) != Buy::Ok) { beep(180, 80); return; }
  toast(m, UI::Accent);
  beep(1319, 60);
  beep(1976, 90);
  spellPicking = false;
  sel = 0;
  save(true);
}

inline void drawSpellSwap(Canvas& g, int k) {
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  const int sp = Game::waitingSpell(game.moments.swap, k);
  if (sp < 0) return;
  const Cat::SpellDef& d = Cat::Spells[sp];
  char b[48];
  if (!spellPicking) {
    const bool room = r.nSpells < Game::spellSlots(r);
    UI::text(g, 3, 3, room ? "A FREE SPELL" : "A FREE SPELL, BUT NO SLOT", UI::SelText);
    g.drawFastHLine(0, 13, UI::W, UI::Sep);
    UI::textCenter(g, 30, d.name, UI::Accent, 2);
    if (!room) UI::textCenter(g, 56, "FORGET ONE OF YOURS TO TAKE IT", UI::Dim);
    details(g, d.name, "SCROLL", d.text, d.flavor, UI::Accent);
    UI::footerLine(g);
    const int x = UI::keyHint(g, 2, "ESC", "LEAVE IT", UI::Text);
    UI::keyHint(g, x, "ENTER", room ? "TAKE IT" : "MAKE ROOM", UI::Text);
    return;
  }
  snprintf(b, sizeof(b), "FORGET ONE FOR %s", d.name);
  UI::text(g, 3, 3, b, UI::SelText);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  for (int i = 0; i < r.nSpells; i++) {
    const int y = 17 + i * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    UI::text(g, 11, y, Cat::Spells[r.spells[i]].name, i == sel ? UI::SelText : UI::Text);
  }
  if (sel < r.nSpells) {   // what you'd be letting go of
    const Cat::SpellDef& y = Cat::Spells[r.spells[sel]];
    details(g, y.name, "YOURS", y.text, y.flavor, UI::Accent);
  }
  UI::footerLine(g);
  const int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  UI::keyHint(g, x, "ENTER", "FORGET AND TAKE", UI::Text);
}

} // namespace App
