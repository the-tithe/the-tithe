#pragma once
#include "AppState.h"
#include "FoundScreens.h"
#include "Lore.h"
#include "RunScreens.h"
#include "ShopScreens.h"
#include "VisitScreens.h"

// The Deck's books: your build, the menu, the atlas of wells you've visited,
// and the Grimoire with its pages and themes.

namespace App {

// ---- Your build -------------------------------------------------------------------

// A line of THE REST, and what it means: the pane under the list shows the
// selected line's title and text.
struct RestRow {
  char line[44];
  uint16_t color;
  char title[28];
  char text[150];
  const char* flavor;
};
inline RestRow restRows[40];

inline int buildRows() {
  using namespace Game;
  const Run& r = run();
  int n = 0;
  auto add = [&](uint16_t c, const char* title, const char* text, const char* flavor, const char* fmt, auto... args) -> RestRow* {
    if (n >= 40) return nullptr;
    RestRow& w = restRows[n++];
    snprintf(w.line, sizeof(w.line), fmt, args...);
    w.color = c;
    snprintf(w.title, sizeof(w.title), "%s", title);
    snprintf(w.text, sizeof(w.text), "%s", text);
    w.flavor = flavor;
    return &w;
  };
  char a[24], b[24], t[150];
  const bool night = game.phase == Phase::Night;
  // Tonight's Omen (or the Eclipse), since the Night's first screen is the only other place it shows.
  if (night && r.n.omen >= 0) {
    upper(Cat::Omens[r.n.omen].name, a, sizeof(a));
    if (r.n.omen == Cat::Omen_fade) snprintf(t, sizeof(t), "Tonight %s is worth half.", energyWord(r.n.fadeE));
    else snprintf(t, sizeof(t), "%s. Tonight only.", Cat::Omens[r.n.omen].text);
    add(UI::EnergyColor[3], a, t, nullptr, "OMEN: %s", a);
  }
  if (night && r.n.eclipse >= 0) {
    upper(Cat::Eclipses[r.n.eclipse].name, a, sizeof(a));
    snprintf(t, sizeof(t), "%s. The seventh Night's rule.", Cat::Eclipses[r.n.eclipse].text);
    add(UI::Rec, a, t, nullptr, "ECLIPSE: %s", a);
  }
  if (night && seesAhead(r) && omensAhead(r, t, sizeof(t))) {   // the Diviner's two Nights ahead
    char line[44];
    snprintf(line, sizeof(line), "%s", t);
    add(UI::Accent, "OMENS AHEAD", "Tomorrow's Omen, then the Night after's. The Diviner sees two Nights ahead.", nullptr, "%s", line);
  }
  // Favors first: what you owe, and what it pays.
  for (int j = 0; j < 2; j++) {
    const Job& f = r.jobs[j];
    if (!f.kind) continue;
    const uint16_t col = f.done ? UI::Dim : UI::factionColor((Faction)f.faction);
    char task[40];
    jobText(f, task, sizeof(task));
    snprintf(t, sizeof(t), "%s. Pays coin and %s standing when it's done.", task, factionWord(f.faction));
    snprintf(b, sizeof(b), "%s FAVOR", factionWord(f.faction));
    if (f.done) add(col, b, t, nullptr, "%s FAVOR: DONE, +%dc", factionWord(f.faction), (int)f.reward);
    else if (f.need > 1) add(col, b, t, nullptr, "%s FAVOR %d/%d: +%dc", factionWord(f.faction), f.got, f.need, (int)f.reward);
    else add(col, b, t, nullptr, "%s FAVOR: +%dc", factionWord(f.faction), (int)f.reward);
  }
  // Effects you've set up and haven't used yet.
  if (r.n.glamour) add(UI::Accent, "GLAMOUR", "Your next offering counts double.", nullptr, "READY: GLAMOUR");
  if (r.n.surge) add(UI::Accent, "SURGE", "Your next draw takes 2.5x the units.", nullptr, "READY: SURGE");
  if (r.n.echo) add(UI::Accent, "ECHO", "Your next offering can go to any well you've visited, from anywhere.", nullptr, "READY: ECHO");
  if (r.n.veil) add(UI::Accent, "VEIL", "Tonight, Static raises no Heat and gets +3 Thirst.", nullptr, "TONIGHT: VEIL");
  if (r.n.transmuteLeft) {
    snprintf(t, sizeof(t), "Your hold and your draws turn into %s for %d more offerings.", energyWord(r.n.transmuteE), r.n.transmuteLeft);
    add(UI::Accent, "TRANSMUTE", t, nullptr, "TRANSMUTE: %d LEFT", r.n.transmuteLeft);
  }
  if (r.bargain) add(UI::Accent, "BARGAIN", "Everything at the next stall is half price.", nullptr, "NEXT STALL: BARGAIN");
  if (r.shopFlags & SF_RareSight) add(UI::Accent, "RARE SIGHT", Cat::Portents[Cat::Portent_rare_sight].text, nullptr, "NEXT STALL: RARE SIGHT");
  if (r.shopFlags & SF_FreeRerolls) add(UI::Accent, "FREE REROLLS", Cat::Portents[Cat::Portent_free_rerolls].text, nullptr, "NEXT STALL: FREE REROLLS");
  if (r.shopFlags & SF_Shine) add(UI::Accent, "SHINE", Cat::Portents[Cat::Portent_shine].text, nullptr, "NEXT RELIC: A SHINE");
  if (r.shopFlags & SF_DoubleInterest) add(UI::Accent, "DOUBLED INTEREST", "Interest pays double at the next dawn.", nullptr, "NEXT DAWN: DOUBLE INTEREST");
  if (r.shopFlags & SF_Mercy) add(UI::Accent, "MERCY", "You scraped through. The next stall adds a half-price relic, a free reroll and a focus Attunement.", nullptr, "NEXT STALL: MERCY");
  if (night && owns(r, Cat::R_glass_crown)) {
    snprintf(t, sizeof(t), "x3 Amp, but it refuses one energy each Night. Tonight %s is worth nothing.", energyWord(r.n.crownBan));
    add(UI::Rec, "GLASS CROWN", t, nullptr, "CROWN REFUSES %s", energyWord(r.n.crownBan));
  }
  // Where tonight's Tithe comes from.
  if (night && r.night < Nights) {
    char n1[16], n2[16];
    int k = snprintf(t, sizeof(t), "Night %d starts at %s.", r.night + 1, UI::num(Tithes[r.night], n1, sizeof(n1)));
    if (owns(r, Cat::R_blood_moon)) k += snprintf(t + k, sizeof(t) - k, " Blood moon x1.35.");
    if (r.n.omen == Cat::Omen_blood_price) k += snprintf(t + k, sizeof(t) - k, " Blood price x1.2.");
    if (r.n.eclipse == Cat::Eclipse_hunger) k += snprintf(t + k, sizeof(t) - k, " The Hunger x1.25.");
    if (r.challenge == 9 || r.challenge == 3) k += snprintf(t + k, sizeof(t) - k, " Trial x2.5.");
    const double loan = r.n.tithe - titheFor(r, r.night, r.n.omen, r.n.eclipse);   // the loan, folded in at dusk
    if (loan >= 1) k += snprintf(t + k, sizeof(t) - k, " Loan +%s.", UI::num(loan, n2, sizeof(n2)));
    if (effectiveTithe(r) > r.n.tithe) snprintf(t + k, sizeof(t) - k, " Doubled until all 4 wards.");
    add(UI::Text, "TONIGHT'S TITHE", t, nullptr, "TITHE %s", UI::num(effectiveTithe(r), n1, sizeof(n1)));
  }
  // Who you are this week.
  upper(Cat::Callings[r.calling].name, a, sizeof(a));
  add(UI::Text, a, Cat::Callings[r.calling].text, nullptr, "CALLING: %s", a);
  if (r.challenge >= 0) {
    upper(Cat::Challenges[r.challenge].name, a, sizeof(a));
    add(UI::Accent, a, Cat::Challenges[r.challenge].text, nullptr, "TRIAL: %s", a);
  } else {
    upper(Cat::Pacts[r.pact].name, a, sizeof(a));
    const char* pt = Cat::Pacts[r.pact].text;
    const size_t pl = strlen(pt);
    int k = snprintf(t, sizeof(t), "%.*s.", (int)(pl && pt[pl - 1] == '.' ? pl - 1 : pl), pt);
    if (r.pact >= 2) {   // every Pact adds to the ones before it
      k += snprintf(t + k, sizeof(t) - k, " Also");
      for (int i = 1; i < r.pact && k < (int)sizeof(t) - 12; i++)
        k += snprintf(t + k, sizeof(t) - k, "%s %s", i == 1 ? "" : ",", Cat::Pacts[i].name);
      snprintf(t + k, sizeof(t) - k, ".");
    }
    add(UI::Text, a, t, nullptr, "PACT: %s", a);
  }
  snprintf(t, sizeof(t), "About %s of tonight's wells also give your focus, on a second DRAW line.", hasMod(r, Cat::Mod_lodestone) ? "two thirds" : "half");
  add(UI::energyColor(r.focus), "FOCUS", t, nullptr, "FOCUS: %s", energyWord(r.focus));
  snprintf(t, sizeof(t), "Each level adds +%d Amp to every offering of that energy. The stall sells Attunements.", hasMod(r, Cat::Mod_tuning_core) ? 2 : 1);
  add(UI::Text, "ATTUNED", t, nullptr, "\x01" "ATTUNED");   // drawn with the energy symbols (see drawRestLine)
  // What you carry.
  for (int i = 0; i < r.nTrinkets; i++) {
    upper(Cat::Trinkets[r.trinkets[i]].name, a, sizeof(a));
    add(UI::Text, a, Cat::Trinkets[r.trinkets[i]].text, Cat::Trinkets[r.trinkets[i]].flavor, "TRINKET: %s", a);
  }
  for (int i = 0; i < r.nSpells; i++) {
    const Cat::SpellDef& d = Cat::Spells[r.spells[i]];
    add(UI::Text, d.name, d.text, d.flavor, "SPELL: %s", d.name);
  }
  for (int m = 0; m < Cat::ModCount; m++) {
    if (!hasMod(r, m)) continue;
    upper(Cat::Mods[m].name, a, sizeof(a));
    add(UI::Text, a, Cat::Mods[m].text, nullptr, "MOD: %s", a);
  }
  for (int i = 0; i < r.nGlyphTokens; i++) {
    const Cat::GlyphDef& d = Cat::Glyphs[r.glyphTokens[i]];
    upper(d.name, a, sizeof(a));
    snprintf(t, sizeof(t), "%s. Carve it from a well's visit menu.", d.text);
    add(UI::Text, a, t, d.flavor, "GLYPH TO CARVE: %s", a);
  }
  // The world's opinion of you.
  if (wardensCanCome(r))
    add(r.heat >= 3 ? UI::Rec : UI::Text, "HEAT",
        "Static adds 0.5 and other offerings cool it by 0.1. At 3 the Wardens listen and may take any offering.",
        nullptr, "HEAT %.1f%s", r.heat, r.heat >= 3 ? ": THE WARDENS LISTEN" : " (WARDENS AT 3)");
  add(UI::Text, "STANDING",
      "Hedge, Hearth, Warded, Syndicate. Offering at their wells raises it; Static lowers Warded and Syndicate. Higher means more Thirst there.",
      nullptr, "\x02" "STANDING");   // each number in its faction's color
  if (r.owed > 0) add(UI::Rec, "THE LOAN", "What you owe the Syndicate is added to the next Tithe.", nullptr, "OWED TO THE SYNDICATE: %d", (int)r.owed);
  // Across every week.
  const int k = rankIndex(game.career);
  const uint32_t paid = careerStat(game.career, Cat::Stat::nights_paid);
  if (k < 6) snprintf(t, sizeof(t), "Your title rises with every Tithe you pay. %u paid so far. %s at %u.", (unsigned)paid, RankNames[k + 1], (unsigned)RankAt[k + 1]);
  else snprintf(t, sizeof(t), "Your title rises with every Tithe you pay. %u paid so far. There's no higher title.", (unsigned)paid);
  add(UI::Text, "RANK", t, nullptr, "RANK: %s", RankNames[k]);
  return n;
}

// A line of THE REST. Two are drawn specially: attunement with the energy
// symbols, and standing with each faction's number in its color.
inline void drawRestLine(Canvas& g, int x, int y, const char* line, uint16_t color) {
  const Game::Run& r = run();
  char b[12];
  if (line[0] == 1) {
    UI::text(g, x, y, "ATTUNED", UI::Text);
    int cx = x + 8 * UI::CharW;
    for (int e = 0; e < Game::EnergyN; e++) {
      UI::energyGlyph(g, cx, y, (Energy)e, UI::energyColor(e));
      snprintf(b, sizeof(b), "%d", r.attune[e]);
      UI::text(g, cx + 9, y, b, r.attune[e] ? UI::Text : UI::Dim);
      cx += 9 + UI::textW(b) + 9;
    }
  } else if (line[0] == 2) {
    UI::text(g, x, y, "STANDING", UI::Text);
    static const char* const who[4] = {"HEDG", "HRTH", "WARD", "SYND"};
    int cx = x + 9 * UI::CharW;
    for (int f = 0; f < 4; f++) {
      snprintf(b, sizeof(b), "%s %d", who[f], (int)r.standing[f]);
      UI::text(g, cx, y, b, UI::factionColor((Faction)f));
      cx += UI::textW(b) + 6;
    }
  } else {
    UI::text(g, x, y, line, color);
  }
}

// THE WEEK: each Night so far, its Resonance against its Tithe (the tick), and
// the visits it had to spare. Tonight is drawn live, in dim.
inline void drawWeek(Canvas& g) {
  const Game::Run& r = run();
  const Game::Log& log = game.log;
  const bool mine = log.runNo == r.runNo;
  const int last = r.night < Game::Log::MaxNights ? r.night : Game::Log::MaxNights - 1;
  const int first = last >= 8 ? last - 7 : 0;   // the newest eight
  char b[32], n1[16], n2[16];
  for (int k = first; k <= last; k++) {
    const int y = 18 + (k - first) * 12;
    const bool tonight = k == r.night && game.phase == Game::Phase::Night;
    Game::NightRow row{};
    if (tonight) {
      row.total = (float)r.n.total;
      row.tithe = (float)Game::effectiveTithe(r);
      row.spare = (int8_t)(r.n.visits - r.n.used);
    } else if (mine && k < log.nNights) {
      row = log.nights[k];
    } else {
      continue;
    }
    if (!tonight && row.how == Game::NightNone) continue;
    snprintf(b, sizeof(b), "N%d", k + 1);
    UI::text(g, 4, y, b, tonight ? UI::Dim : UI::Text);
    // The bar runs to twice the Tithe; the tick is the Tithe.
    const int bx = 24, bw = 80;
    const float f = row.tithe > 0 ? row.total / row.tithe : 0;
    const uint16_t col = tonight ? UI::dimmed(UI::Accent)
                         : row.how == Game::NightBroke ? UI::Rec
                         : row.how == Game::NightLoan ? UI::EnergyColor[(int)Energy::Static] : UI::Accent;
    g.drawRect(bx, y + 1, bw, 6, UI::Faint);
    int fill = (int)(f / 2 * (bw - 2));
    if (fill > bw - 2) fill = bw - 2;
    if (fill > 0) g.fillRect(bx + 1, y + 2, fill, 4, col);
    g.drawFastVLine(bx + bw / 2, y - 1, 10, UI::Text);
    snprintf(b, sizeof(b), "%s/%s", UI::num(row.total, n1, sizeof(n1)), UI::num(row.tithe, n2, sizeof(n2)));
    UI::text(g, bx + bw + 6, y, b, tonight ? UI::Dim : UI::Text);
    if (row.how == Game::NightBroke) snprintf(b, sizeof(b), "BROKE");
    else if (row.how == Game::NightLoan) snprintf(b, sizeof(b), "LOAN");
    else if (tonight) snprintf(b, sizeof(b), "%d LEFT", row.spare);   // still going: visits left, not spare
    else snprintf(b, sizeof(b), "+%d", row.spare);
    UI::textRight(g, UI::W - 4, y, b, row.how == Game::NightBroke ? UI::Rec : UI::Dim);
  }
}

inline void updateBuild(const Ctx& c, const Press& p) {
  (void)c;
  if (p.back) { go(back); return; }
  if (p.right || p.left) { tab = (tab + (p.right ? 1 : 2)) % 3; sel = 0; top = 0; }
  const int rows = tab == 0 ? run().nRelics : tab == 1 ? buildRows() : 0;
  if (p.down && sel < rows - 1) sel++;
  if (p.up && sel > 0) sel--;
}

inline void drawBuild(Canvas& g, const Ctx& c) {
  (void)c;
  using namespace Game;
  g.fillSprite(UI::Bg);
  const Run& r = run();
  char b[48];
  static const char* const tabNames[3] = {"RELICS", "THE REST", "THE WEEK"};
  UI::text(g, 3, 3, tabNames[tab], UI::SelText);
  snprintf(b, sizeof(b), "%dc  N%d", (int)r.coin, r.night + 1);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  if (tab == 0) {
    if (r.nRelics == 0) UI::textCenter(g, 40, "NO RELICS YET", UI::Dim);
    const int show = 6;   // rows above the details pane
    if (sel < top) top = sel;
    if (sel >= top + show) top = sel - show + 1;
    for (int i = top; i < r.nRelics && i < top + show; i++) {
      const int y = 17 + (i - top) * 10;
      if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
      relicLine(g, y, r.relics[i].id, r.relics[i].ed, r.relics[i].tier, i == sel, false);
      if (r.state[r.relics[i].id] > 0 && !(r.relics[i].id == Cat::R_long_receipt && game.phase == Phase::Shop)) {
        snprintf(b, sizeof(b), "+%.2g", r.state[r.relics[i].id]);
        UI::textRight(g, UI::W - 4, y, b, UI::Accent);
      }
    }
    if (sel < r.nRelics) {
      const Cat::RelicDef& d = Cat::Relics[r.relics[sel].id];
      char n[28];
      upper(d.name, n, sizeof(n));
      char sub[32], habit[16];
      upper(Cat::SchoolNames[d.school], habit, sizeof(habit));
      snprintf(sub, sizeof(sub), "%s %s", Cat::RarityNames[d.rarity], habit);
      char ex[96];
      const OwnedRelic& o = r.relics[sel];
      details(g, n, sub, d.text, relicExtras(o.tier, o.ed, o.withered, d.flavor, ex, sizeof(ex)), UI::rarityColor(d.rarity));
    }
  } else if (tab == 2) {
    drawWeek(g);
  } else {
    const int n = buildRows();
    if (sel > n - 1) sel = n - 1;
    const int show = 6;   // rows above the details pane
    if (sel < top) top = sel;
    if (sel >= top + show) top = sel - show + 1;
    for (int i = top; i < n && i < top + show; i++) {
      const int y = 17 + (i - top) * 10;
      if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
      drawRestLine(g, 4, y, restRows[i].line, restRows[i].color);
    }
    if (sel >= 0 && sel < n) details(g, restRows[sel].title, nullptr, restRows[sel].text, restRows[sel].flavor, UI::Accent);
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  UI::keyHint(g, x, "< >", tabNames[(tab + 1) % 3], UI::Text);
}

// ---- The atlas -------------------------------------------------------------------

inline int atlasOrder[Game::Atlas::Max];
inline int atlasN = 0;
inline int atlasSort = 0;   // 0 most visited, 1 most recent, 2 by faction, 3 this week only, 4 pays most (in a week)
constexpr int AtlasSortCount = 5;
inline const char* const AtlasSorts[AtlasSortCount] = {"MOST VISITED", "MOST RECENT", "BY FACTION", "THIS WEEK", "PAYS MOST"};
inline float atlasEst[Game::Atlas::Max];   // what each well would pay now for your hold, 0 out of a week

inline int atlasSorts() { return inRun() ? AtlasSortCount : AtlasSortCount - 1; }

// The estimates, worked out when the atlas opens and when the sort changes, never every frame.
inline void estimateAtlas() {
  atlasEstDirty = false;
  if (inRun()) Game::atlasEstimates(run(), game.atlas, game.playMs / 60000, atlasEst);
  else for (int i = 0; i < game.atlas.count; i++) atlasEst[i] = 0;
}

inline bool atlasBefore(int x, int y) {
  const Game::AtlasEntry& a = game.atlas.e[x];
  const Game::AtlasEntry& b = game.atlas.e[y];
  if (atlasSort == 4 && atlasEst[x] != atlasEst[y]) return atlasEst[x] > atlasEst[y];
  if (atlasSort == 1 || atlasSort == 3) return a.lastPlay > b.lastPlay;
  if (atlasSort == 2 && a.faction != b.faction) return a.faction < b.faction;
  return a.visits > b.visits;
}

inline void sortAtlas() {
  if (atlasEstDirty) estimateAtlas();
  if (atlasSort >= atlasSorts()) atlasSort = 0;
  atlasN = 0;
  for (int i = 0; i < game.atlas.count; i++) {
    if (atlasSort == 3 && !(inRun() && game.atlas.e[i].runNo == run().runNo && game.atlas.e[i].nights)) continue;
    atlasOrder[atlasN++] = i;
  }
  for (int i = 1; i < atlasN; i++) {
    const int x = atlasOrder[i];
    int j = i - 1;
    while (j >= 0 && atlasBefore(x, atlasOrder[j])) { atlasOrder[j + 1] = atlasOrder[j]; j--; }
    atlasOrder[j + 1] = x;
  }
}

inline bool echoPicking() { return inRun() && run().n.echo && (back == Mode::Visit || back == Mode::Scanner); }

inline void updateAtlas(const Ctx& c, const Press& p) {
  (void)c;
  if ((p.left || p.right) && !echoPicking()) {   // picking a well for ECHO, the sort stays put
    atlasSort = (atlasSort + (p.right ? 1 : atlasSorts() - 1)) % atlasSorts();
    atlasEstDirty = true;
    sel = 0;
    top = 0;
  }
  sortAtlas();
  if (p.back) {
    if (back == Mode::Visit && echoFrom) wellId = echoFrom;   // back to the well ECHO was cast from
    go(back);
    return;
  }
  if (p.down && sel < atlasN - 1) sel++;
  if (p.up && sel > 0) sel--;
  // The well under the cursor plays its tune, once the cursor has rested on it.
  static uint32_t tunedId = 0;
  if (atlasN > 0 && game.atlas.e[atlasOrder[sel]].id != tunedId) {
    const Game::AtlasEntry& a = game.atlas.e[atlasOrder[sel]];
    tunedId = a.id;
    cueWellTune(a.seed, a.energy, a.id);
  }
  if (p.enter && echoPicking() && atlasN > 0) {
    wellId = game.atlas.e[atlasOrder[sel]].id;
    echoMode = true;
    go(Mode::Visit);
  }
}

// What you know about one well: its faction, your visits and this week's
// Nights there, your best and total offerings, when you first offered, what it
// likes, whether it's settling, and its glyph.
inline void drawAtlasWell(Canvas& g, const Game::AtlasEntry& a) {
  using namespace Game;
  char b[48], n[17], n1[16], n2[16];
  constexpr int Y = 69;
  g.drawFastHLine(0, Y, UI::W, UI::Sep);
  atlasName(a, n);
  const int e = inRun() ? tonightsEnergy(run(), a.energy) : a.energy;
  UI::text(g, 4, Y + 4, n, UI::energyColor(e));
  UI::textRight(g, UI::W - 4, Y + 4, factionWord(a.faction), UI::factionColor((Faction)a.faction));
  if (a.ward) {   // the ward it was in last time
    const WardKind wk = (WardKind)(a.ward - 1);
    UI::textRight(g, UI::W - 4 - UI::textW(factionWord(a.faction)) - UI::CharW, Y + 4, wardName(wk), UI::wardColor(wk));
  }
  const bool thisWeek = inRun() && a.runNo == run().runNo;
  const int left = settleLeft(&a);
  const bool settling = thisWeek && left > 0;
  char s[20] = "";
  if (settling) snprintf(s, sizeof(s), "SETTLING %dM", left);
  const int room = UI::W - 4 - (settling ? UI::textW(s) + 6 : 0);   // the Nights list stops short of SETTLING
  int k = snprintf(b, sizeof(b), "%u VISIT%s", a.visits, a.visits == 1 ? "" : "S");
  if (thisWeek && a.nights) {
    int nn = 0;
    k += snprintf(b + k, sizeof(b) - k, ", NIGHTS");
    for (int i = 0; i < Nights && k < (int)sizeof(b) - 4; i++)
      if (a.nights & (1u << Atlas::nightBit(i))) { k += snprintf(b + k, sizeof(b) - k, " %d", i + 1); nn++; }
    if (4 + UI::textW(b) > room) snprintf(b, sizeof(b), "%u VISIT%s, %d NIGHT%s", a.visits, a.visits == 1 ? "" : "S", nn, nn == 1 ? "" : "S");
  }
  UI::text(g, 4, Y + 14, b, UI::Text);
  if (settling) UI::textRight(g, UI::W - 4, Y + 14, s, UI::Dim);
  if (const WellMemory* m = game.wells.find(a.id)) {   // what you've given it, across every week
    snprintf(b, sizeof(b), "BEST %s  GIVEN %s", UI::num(m->best, n1, sizeof(n1)), UI::num(m->total, n2, sizeof(n2)));
    UI::text(g, 4, Y + 24, b, UI::Accent);
    snprintf(b, sizeof(b), "SINCE WEEK %u", (unsigned)m->firstRun);
    UI::textRight(g, UI::W - 4, Y + 24, b, UI::Dim);
  }
  // Its taste, the same on every Deck: what it pays most and least for.
  int hi = 0, lo = 0;
  for (int x = 1; x < EnergyN; x++) {
    if (taste(a.seed, x) > taste(a.seed, hi)) hi = x;
    if (taste(a.seed, x) < taste(a.seed, lo)) lo = x;
  }
  snprintf(b, sizeof(b), "LIKES %s MOST, %s LEAST", energyWord(hi), energyWord(lo));
  UI::text(g, 4, Y + 34, b, UI::Dim);
  const int gl = thisWeek ? a.glyph : -1;
  if (gl >= 0) {
    UI::text(g, 4, Y + 44, Cat::Glyphs[gl].text, UI::Accent);   // its name is on the list row
  }
}

inline void drawAtlas(Canvas& g, const Ctx& c) {
  (void)c;
  sortAtlas();
  g.fillSprite(UI::Bg);
  char b[48], n[17];
  if (echoPicking()) UI::text(g, 3, 3, "ECHO: OFFER TO WHICH WELL?", UI::SelText);
  else {
    UI::text(g, 3, 3, "ATLAS", UI::SelText);
    UI::text(g, 3 + 6 * UI::CharW, 3, AtlasSorts[atlasSort], UI::Dim);
  }
  snprintf(b, sizeof(b), "%d WELL%s", atlasN, atlasN == 1 ? "" : "S");   // what's listed: offered at or only drawn from
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  if (atlasN == 0) UI::textCenter(g, 40, atlasSort == 3 ? "NO OFFERINGS THIS WEEK YET" : "NO WELLS VISITED YET", UI::Dim);
  const int show = 5;   // rows above the well's details
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < atlasN && i < top + show; i++) {
    const Game::AtlasEntry& a = game.atlas.e[atlasOrder[i]];
    const int y = 17 + (i - top) * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    const int e = inRun() ? Game::tonightsEnergy(run(), a.energy) : a.energy;
    UI::energyGlyph(g, 4, y, (Energy)e, UI::energyColor(e));
    atlasName(a, n);
    UI::text(g, 15, y, n, i == sel ? UI::SelText : UI::Text);
    if (a.glyph >= 0 && inRun() && a.runNo == run().runNo) {
      char gn[12];
      upper(Cat::Glyphs[a.glyph].name, gn, sizeof(gn));
      UI::text(g, 120, y, gn, UI::EnergyColor[4]);
    }
    if (inRun() && atlasEst[atlasOrder[i]] > 0) {   // what it would pay now for your hold
      char n1[12];
      snprintf(b, sizeof(b), "~%s", UI::num(atlasEst[atlasOrder[i]], n1, sizeof(n1)));
      UI::textRight(g, 205, y, b, i == sel ? UI::Text : UI::Dim);
    }
    if (a.visits) snprintf(b, sizeof(b), "%ux", a.visits);
    else snprintf(b, sizeof(b), "DRAWN");   // drawn from, never offered at
    UI::textRight(g, UI::W - 4, y, b, UI::Dim);
  }
  if (atlasN > 0 && sel < atlasN) drawAtlasWell(g, game.atlas.e[atlasOrder[sel]]);
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  if (echoPicking()) UI::keyHint(g, x, "ENTER", "OFFER THERE", UI::Text);
  else UI::keyHint(g, x, "< >", "SORT", UI::Text);
}

// ---- The Grimoire --------------------------------------------------------------------

inline int pageOpen = 0;

// ---- Records: the Grimoire's second tab ----------------------------------------------

constexpr int RecordCount = 8;

// A best kept as a career stat, counting the week in progress.
inline uint32_t bestStat(Cat::Stat s) {
  uint32_t v = game.career.stats[(int)s];
  if (inRun() && run().stats[(int)s] > v) v = run().stats[(int)s];
  return v;
}

// Record `i`: its name, its value, and where it happened ("" when that isn't kept).
inline void recordAt(int i, const char*& name, char* value, int vcap, char* where, int wcap) {
  const Game::Log& log = game.log;
  char n[16];
  where[0] = 0;
  auto when = [&](const Game::Record& r, const char* at) {
    if (at) snprintf(where, wcap, "AT %s, WEEK %u, NIGHT %u", at, (unsigned)r.week, (unsigned)r.night + 1);
    else snprintf(where, wcap, "WEEK %u, NIGHT %u", (unsigned)r.week, (unsigned)r.night + 1);
  };
  switch (i) {
    case 0: {
      name = "LOUDEST OFFERING";
      const Game::Record& r = log.loudest;
      if (r.value <= 0) { snprintf(value, vcap, "-"); break; }
      snprintf(value, vcap, "%s", UI::num(r.value, n, sizeof(n)));
      char well[17];
      WellRules::nameOf(r.wellSeed, (Faction)r.faction, well);
      when(r, well);
      break;
    }
    case 1:
      name = "BIGGEST NIGHT";
      if (log.biggestNight.value <= 0) { snprintf(value, vcap, "-"); break; }
      snprintf(value, vcap, "%s", UI::num(log.biggestNight.value, n, sizeof(n)));
      when(log.biggestNight, nullptr);
      break;
    case 2:
      name = "QUICKEST TITHE";
      if (log.quickest.value <= 0) { snprintf(value, vcap, "-"); break; }
      snprintf(value, vcap, "%d VISIT%s", (int)log.quickest.value, log.quickest.value == 1 ? "" : "S");
      when(log.quickest, nullptr);
      break;
    case 3: name = "MOST UNITS AT ONCE"; snprintf(value, vcap, "%u", (unsigned)bestStat(Cat::Stat::big_units)); break;
    case 4: name = "MOST WELLS IN RANGE"; snprintf(value, vcap, "%u", (unsigned)bestStat(Cat::Stat::max_networks)); break;
    case 5: name = "MOST COIN HELD"; snprintf(value, vcap, "%uc", (unsigned)bestStat(Cat::Stat::max_coin)); break;
    case 6: {
      name = "LONGEST OVERNIGHT";
      const unsigned v = (unsigned)bestStat(Cat::Stat::overnight_best);
      snprintf(value, vcap, "%u NIGHT%s", v, v == 1 ? "" : "S");
      break;
    }
    default: name = "WELLS FOUND"; snprintf(value, vcap, "%u", (unsigned)game.atlas.unique); break;
  }
  if (value[0] == '0' && (value[1] == 0 || value[1] == ' ' || value[1] == 'c')) snprintf(value, vcap, "-");   // not set yet
}

inline void drawRecords(Canvas& g) {
  char value[24], where[48];
  const char* name = "";
  for (int i = 0; i < RecordCount; i++) {
    const int y = 17 + i * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    recordAt(i, name, value, sizeof(value), where, sizeof(where));
    UI::text(g, 6, y, name, i == sel ? UI::SelText : UI::Text);
    UI::textRight(g, UI::W - 4, y, value, value[0] == '-' ? UI::Dim : UI::Accent);   // Dim: Faint reads as nothing on the Deck
  }
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  recordAt(sel, name, value, sizeof(value), where, sizeof(where));
  if (where[0]) UI::wrap(g, 4, 103, UI::W - 8, where, UI::Dim, 2);
}

// ---- The collection: the Grimoire's relics tab ---------------------------------

inline bool relicSeen(int i) { return Game::getBit(game.seen, i) || Game::getBit(game.career.wonWith, i); }
inline bool relicHeld(int i) { return Game::getBit(game.career.wonWith, i); }   // in a week you held

inline void drawCollection(Canvas& g) {
  char b[48], n[32];
  int seen = 0;
  for (int i = 0; i < Cat::RelicCount; i++) seen += relicSeen(i);
  snprintf(b, sizeof(b), "%d/%d SEEN", seen, (int)Cat::RelicCount);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  const int show = 8;
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < Cat::RelicCount && i < top + show; i++) {
    const int y = 17 + (i - top) * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    const Cat::RelicDef& d = Cat::Relics[i];
    if (relicSeen(i)) {
      upper(d.name, n, sizeof(n));
      UI::text(g, 6, y, n, i == sel ? UI::SelText : UI::rarityColor(d.rarity));
      if (relicHeld(i)) UI::textRight(g, UI::W - 4, y, "HELD", UI::Accent);
    } else {
      UI::text(g, 6, y, "? ? ? ? ?", i == sel ? UI::Dim : UI::Faint);
    }
  }
  if (top > 0) g.fillTriangle(UI::W - 3, 15, UI::W - 5, 17, UI::W - 1, 17, UI::Dim);
  if (top + show < Cat::RelicCount) g.fillTriangle(UI::W - 3, 97, UI::W - 5, 95, UI::W - 1, 95, UI::Dim);
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  const Cat::RelicDef& d = Cat::Relics[sel];
  if (relicSeen(sel)) UI::wrap(g, 4, 103, UI::W - 8, d.text, UI::Text, 2, 10, 0, true);
  else if (!Game::unlocked(game.career, Game::Kind::Relic, sel) && d.unlockText[0]) {
    char u[80];
    UI::wrap(g, 4, 103, UI::W - 8, Game::goalText(game.career, d.unlock, d.need, d.unlockText, u, sizeof(u)), UI::Dim, 2);
  } else {
    UI::text(g, 4, 103, "NOT SEEN YET", UI::Dim);
  }
}

// ---- HEARD: every Overheard fragment and Collector line you've caught -------------
// Radio lines first, then the stall's, each in alphabetical order (not the order
// they were written in, so the list gives no story away).

constexpr int HeardAll = Lore::OverheardCount + Lore::CL_count;
inline const char* heardText(int k) { return k < Lore::OverheardCount ? Lore::Overheard[k].text : Lore::Collector[k - Lore::OverheardCount]; }
inline bool heardYet(int k) {
  return k < Lore::OverheardCount ? Game::getBit(game.heard.radio, k) : Game::getBit(game.heard.stall, k - Lore::OverheardCount);
}
inline const char* heardKey(int k) {
  const char* t = heardText(k);
  while (*t == '.' || *t == '"') t++;
  return t;
}
inline const int* heardOrder() {
  static int order[HeardAll];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < HeardAll; i++) order[i] = i;
    for (int i = 1; i < HeardAll; i++) {   // insertion sort, once
      const int x = order[i];
      int j = i - 1;
      auto before = [](int p, int q) {
        const bool pr = p < Lore::OverheardCount, qr = q < Lore::OverheardCount;
        if (pr != qr) return pr;
        return strcasecmp(heardKey(p), heardKey(q)) < 0;
      };
      while (j >= 0 && before(x, order[j])) { order[j + 1] = order[j]; j--; }
      order[j + 1] = x;
    }
    ready = true;
  }
  return order;
}
// The heard entries, in order. Returns how many.
inline int heardList(int* out) {
  const int* order = heardOrder();
  int n = 0;
  for (int i = 0; i < HeardAll; i++) if (heardYet(order[i])) out[n++] = order[i];
  return n;
}

inline void drawHeard(Canvas& g) {
  static int list[HeardAll];
  const int n = heardList(list);
  int radio = 0, stall = 0;
  for (int i = 0; i < n; i++) (list[i] < Lore::OverheardCount ? radio : stall)++;
  char b[48];
  snprintf(b, sizeof(b), "OVERHEARD %d/%d  STALL %d/%d", radio, (int)Lore::OverheardCount, stall, (int)Lore::CL_count);
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  if (n == 0) { UI::text(g, 6, 17, "NOTHING YET", UI::Dim); return; }
  if (sel >= n) sel = n - 1;
  const int show = 8;
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < n && i < top + show; i++) {
    const int y = 17 + (i - top) * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    char t[40];
    snprintf(t, sizeof(t), "%s", heardText(list[i]));
    if (strlen(heardText(list[i])) > 37) { t[34] = '.'; t[35] = '.'; t[36] = '.'; t[37] = 0; }
    const uint16_t col = list[i] < Lore::OverheardCount ? UI::EnergyColor[(int)Energy::Hush] : UI::Text;
    UI::text(g, 4, y, t, i == sel ? UI::SelText : col);
  }
  if (top > 0) g.fillTriangle(UI::W - 3, 15, UI::W - 5, 17, UI::W - 1, 17, UI::Dim);
  if (top + show < n) g.fillTriangle(UI::W - 3, 97, UI::W - 5, 95, UI::W - 1, 95, UI::Dim);
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  if (list[sel] < Lore::OverheardCount) UI::text(g, 4, 103, "OVERHEARD", UI::Dim);
  else UI::wrap(g, 4, 103, UI::W - 8, heardText(list[sel]), UI::Text, 2);   // the whole line, at dawn
}

inline int heardCount() {
  static int list[HeardAll];
  return heardList(list);
}

// ---- STREETS: the list in the back, kept in the Deck's own memory -----------------
// Every owner wrote in their own hand, so each one's entries look a little different.

struct StreetHand { uint16_t color; uint8_t advance; int8_t wobble; };   // px per letter; px up and down
struct StreetEntry { const char* who; const char* street; const char* year; uint8_t hand; bool crossed; };
inline const StreetHand StreetHands[] = {
  {UI::rgb(170, 160, 140), 6, 0},    // E.W.
  {UI::rgb(150, 170, 195), 7, 0},    // R.H.
  {UI::rgb(185, 150, 175), 5, 1},    // A.N.
  {UI::rgb(235, 200, 150), 6, 0},    // K.A.
  {UI::rgb(225, 228, 235), 6, 0},    // M.A.
};
inline const StreetEntry Streets[] = {
  {"E.W.", "COBB ST", "2008", 0, true},
  {"E.W.", "ALDER ST", "2009", 0, true},
  {"R.H.", "NORRIS RD", "2012", 1, true},
  {"R.H.", "SAWYER ST", "2012", 1, true},
  {"R.H.", "ORCHARD ST", "2015", 1, true},
  {"A.N.", "VESEY LN", "2017", 2, true},
  {"A.N.", "HALSEY ST", "2019", 2, true},
  {"K.A.", "CALDER", "2023", 3, false},
  {"K.A.", nullptr, "2023", 3, false},   // a home street, in a scribble nobody else can read
  {"M.A.", "ROWAN ST", "2027", 4, false},
};
constexpr int StreetCount = sizeof(Streets) / sizeof(Streets[0]);

// Text in a hand, returning where it ended.
inline int handText(Canvas& g, int x, int y, const char* s, const StreetHand& h) {
  char c[2] = {0, 0};
  for (int i = 0; s[i]; i++) {
    c[0] = s[i];
    const int dy = h.wobble && (i & 1) ? h.wobble : 0;
    UI::text(g, x, y + dy, c, h.color);
    x += h.advance;
  }
  return x;
}

// A word nobody but its writer could read: a squiggle about as long as a street name.
inline int scribble(Canvas& g, int x, int y, const StreetHand& h) {
  static const int8_t wave[] = {0, -2, -3, -1, 1, 2, 0, -2, -1, 1, 3, 1, -1, -3, -2, 0, 2, 1, -1, 0};
  int px = x, py = y + 4;
  for (int i = 1; i < (int)sizeof(wave); i++) {
    const int nx = x + i * 3, ny = y + 4 + wave[i];
    g.drawLine(px, py, nx, ny, h.color);
    px = nx;
    py = ny;
  }
  return px;
}

// ---- Your own streets, written in under the others ---------------------------------
// Walk the same stretch on three Nights and the list offers ADD: your initials
// and a name, in a hand made from how you typed them, dated by the Deck's own
// count of weeks. They live only in the Deck's memory and do nothing but stay.

struct StreetEdit {
  bool on;
  int field;            // 0 your initials, 1 the street
  char who[3];
  char name[13];
  bool typed;           // the suggested name has been typed over
  uint32_t lastMs;      // play ms of the last key, 0 before the first
  uint16_t gaps[24];    // ms between keys: the hand
  int nGaps;
};
inline StreetEdit edit{};
inline int crossAsk = -1;   // your street waiting for a second ENTER to cross it out
constexpr int StreetShow = 10;

inline bool canAddStreet() {
  const Game::Moments& m = game.moments;
  return streetsOpen() && m.keepName && m.nStreets < Game::Moments::MaxStreets;
}
inline int streetRows() { return StreetCount + game.moments.nStreets + (canAddStreet() ? 1 : 0); }
inline bool streetsInteractive() { return streetsOpen() && streetRows() > StreetCount; }

// "BY " and the stretch's loudest well, or its second word when the whole name won't fit.
inline void suggestedStreet(char out[13]) {
  char n[17];
  trueName(game.moments.keepName, n);
  const char* sp = strchr(n, ' ');
  snprintf(out, 13, "BY %s", strlen(n) <= 9 || !sp ? n : sp + 1);
}

// A stored street as text: only plain capitals, digits and a little punctuation.
inline void streetText(const char* in, int n, char* out) {
  int k = 0;
  for (int i = 0; i < n && in[i]; i++) {
    const char c = in[i];
    out[k++] = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'' ? c : '?';
  }
  out[k] = 0;
}

// The name as it would be written: what you typed, or the suggestion while it's untouched.
inline int streetTyped() {
  int len = (int)strlen(edit.name);
  while (len > 0 && edit.name[len - 1] == ' ') len--;
  return len;
}

inline Game::OwnStreet typedStreet() {
  Game::OwnStreet o{};
  o.who[0] = edit.who[0];
  o.who[1] = edit.who[1];
  memcpy(o.street, edit.name, streetTyped());
  o.week = weekNow();
  Game::handFrom(edit.gaps, edit.nGaps, o.color, o.advance, o.wobble);
  return o;
}

inline void startStreet() {
  edit = StreetEdit{};
  edit.on = true;
  suggestedStreet(edit.name);
}

inline void writeStreetIn() {
  Game::Moments& m = game.moments;
  m.streets[m.nStreets++] = typedStreet();
  m.keepName = 0;
  edit.on = false;
  sel = StreetCount + m.nStreets - 1;
  playTune(Tunes::written());   // its own pair, not the Tithe's chord
  save(true);
}

// A key while writing a street in.
inline void typeStreet(const Press& p) {
  StreetEdit& e = edit;
  char* f = e.field == 0 ? e.who : e.name;
  const int cap = e.field == 0 ? 2 : 12;
  if (p.del) {
    if (e.field == 1 && !e.typed) { e.typed = true; e.name[0] = 0; return; }
    const int len = (int)strlen(f);
    if (len) f[len - 1] = 0;
    else if (e.field == 1) { e.field = 0; e.who[1] = 0; }
    else e.on = false;
    return;
  }
  if (p.back) { e.on = false; toast("NOT WRITTEN IN", UI::Dim); return; }
  if (p.enter) {
    if (e.field == 0 && strlen(e.who) == 2) e.field = 1;
    else if (e.field == 1 && streetTyped() == 0) beep(180, 80);   // an empty name: the line stays blank until you type one
    else if (e.field == 1) writeStreetIn();
    return;
  }
  const char k = p.key;
  if (!k || (e.field == 0 && !(k >= 'A' && k <= 'Z'))) return;
  if (e.field == 1 && !e.typed) { e.typed = true; e.name[0] = 0; }
  const int len = (int)strlen(f);
  if (len >= cap) { beep(180, 80); return; }   // twelve letters is the line
  if (k == ' ' && len == 0) return;
  f[len] = k;
  f[len + 1] = 0;
  if (e.lastMs && e.nGaps < (int)(sizeof(e.gaps) / sizeof(e.gaps[0]))) {
    const uint32_t gap = game.playMs - e.lastMs;
    e.gaps[e.nGaps++] = (uint16_t)(gap > 1500 ? 1500 : gap);
  }
  e.lastMs = game.playMs ? game.playMs : 1;
  if (e.field == 0 && len + 1 == 2) e.field = 1;   // both initials: on to the street
}

// Opening the tab: the cursor where there's something to do.
inline void streetsEnter() {
  edit.on = false;
  crossAsk = -1;
  sel = streetsInteractive() ? streetRows() - 1 : 0;
  top = sel >= StreetShow ? sel - StreetShow + 1 : 0;
}

// Keys on the STREETS tab. True when STREETS used the press.
inline bool updateStreets(const Press& p) {
  if (edit.on) { typeStreet(p); return true; }
  if (!streetsInteractive()) return false;
  const int rows = streetRows();
  if (p.down || p.up) {
    crossAsk = -1;
    if (p.down && sel < rows - 1) sel++;
    if (p.up && sel > 0) sel--;
    if (sel < top) top = sel;
    if (sel >= top + StreetShow) top = sel - StreetShow + 1;
    return true;
  }
  if (!p.enter) return false;
  const int own = sel - StreetCount;
  Game::Moments& m = game.moments;
  if (own == m.nStreets && canAddStreet()) { startStreet(); return true; }
  if (own >= 0 && own < m.nStreets && !m.streets[own].crossed) {
    if (crossAsk != own) { crossAsk = own; toast("ENTER AGAIN TO CROSS IT OUT", UI::Rec); return true; }
    m.streets[own].crossed = 1;
    crossAsk = -1;
    save(true);
  }
  return true;
}

// One line of the list, in its writer's hand.
inline void streetLine(Canvas& g, int y, const char* who, const char* street, const char* year, const StreetHand& h,
                       bool crossed, bool scribbled = false) {
  handText(g, 6, y, who, h);
  const int end = scribbled ? scribble(g, 42, y, h) : handText(g, 42, y, street, h);
  const int yx = UI::W - 6 - (int)strlen(year) * h.advance;
  handText(g, yx, y, year, h);
  if (crossed) {
    g.drawFastHLine(4, y + 3, end - 4 + 2, UI::Dim);
    g.drawFastHLine(yx - 1, y + 3, UI::W - 4 - yx, UI::Dim);
  }
}

inline void drawStreets(Canvas& g) {
  const Game::Career& car = game.career;
  if (!streetsOpen()) {
    char un[80];
    const Cat::PageDef& pd = Cat::Pages[7];
    UI::text(g, 6, 17, "? ? ? ? ?", UI::Faint);
    g.drawFastHLine(0, 99, UI::W, UI::Sep);
    UI::wrap(g, 4, 103, UI::W - 8, Game::goalText(car, pd.unlock, pd.need, pd.unlockText, un, sizeof(un)), UI::Dim, 2);
    return;
  }
  const Game::Moments& m = game.moments;
  const bool cursor = streetsInteractive();
  const int rows = streetRows();
  if (!cursor) top = 0;
  if (edit.on) UI::textRight(g, UI::W - 3, 3, edit.field == 0 ? "YOUR INITIALS" : "THE STREET", UI::Dim);
  for (int i = top; i < rows && i < top + StreetShow; i++) {
    const int y = 17 + (i - top) * 10;
    if (cursor && i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    char who[6], name[16], year[12];
    if (i < StreetCount) {
      const StreetEntry& e = Streets[i];
      streetLine(g, y, e.who, e.street ? e.street : "", e.year, StreetHands[e.hand], e.crossed, !e.street);
      continue;
    }
    const int own = i - StreetCount;
    if (own < m.nStreets) {
      const Game::OwnStreet& o = m.streets[own];
      char w2[3] = {o.who[0], o.who[1], 0};
      streetText(w2, 2, name);
      snprintf(who, sizeof(who), "%c.%c.", name[0] ? name[0] : '?', name[1] ? name[1] : '?');
      streetText(o.street, 12, name);
      snprintf(year, sizeof(year), "WK %u", (unsigned)o.week);
      const StreetHand h{o.color, (uint8_t)(o.advance >= 4 && o.advance <= 8 ? o.advance : 6), o.wobble};
      streetLine(g, y, who, name, year, h, o.crossed);
    } else if (edit.on) {   // the one being written, in the hand it's making
      const Game::OwnStreet o = typedStreet();
      const StreetHand h{o.color, o.advance, o.wobble};
      snprintf(who, sizeof(who), "%c.%c.", edit.who[0] ? edit.who[0] : '_', edit.who[1] ? edit.who[1] : '_');
      snprintf(name, sizeof(name), "%s%s", edit.name, edit.field == 1 && strlen(edit.name) < 12 ? "_" : "");
      snprintf(year, sizeof(year), "WK %u", (unsigned)o.week);
      streetLine(g, y, who, edit.typed ? name : "", year, h, false);
      if (!edit.typed) handText(g, 42, y, edit.name, StreetHand{UI::Dim, h.advance, h.wobble});   // the suggestion, until you type over it
    } else {
      UI::text(g, 6, y, "ADD", i == sel ? UI::SelText : UI::Accent);
    }
  }
  if (top > 0) g.fillTriangle(UI::W - 3, 15, UI::W - 5, 17, UI::W - 1, 17, UI::Dim);
  if (top + StreetShow < rows) g.fillTriangle(UI::W - 3, 117, UI::W - 5, 115, UI::W - 1, 115, UI::Dim);
}

// The footer's key hints while there's something to do. False otherwise.
inline bool streetsFooter(Canvas& g) {
  if (edit.on) {
    const int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
    UI::keyHint(g, x, "ENTER", edit.field == 0 ? "NEXT" : "WRITE IT IN", UI::Text);
    return true;
  }
  if (!streetsInteractive()) return false;
  const int own = sel - StreetCount;
  const Game::Moments& m = game.moments;
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  if (own == m.nStreets) x = UI::keyHint(g, x, "ENTER", "ADD", UI::Text);
  else if (own >= 0 && own < m.nStreets && !m.streets[own].crossed) x = UI::keyHint(g, x, "ENTER", "CROSS OUT", UI::Text);
  UI::keyHint(g, x, "< >", "PAGES", UI::Text);
  return true;
}

constexpr int GrimoireTabs = 6;   // pages, relics, records, heard, found, streets
inline const char* const GrimoireTab[GrimoireTabs] = {"PAGES", "RELICS", "RECORDS", "HEARD", "FOUND", "STREETS"};

inline void updateGrimoire(const Ctx& c, const Press& p) {
  (void)c;
  if (tab == 4 && updateFound(p)) return;     // FOUND has sections of its own
  if (tab == 5 && updateStreets(p)) return;   // and STREETS a line you can add
  if (p.back) { foundOpen = -1; go(Mode::Menu); return; }
  if (p.left || p.right) {
    tab = (tab + (p.right ? 1 : GrimoireTabs - 1)) % GrimoireTabs;
    sel = 0;
    top = 0;
    foundOpen = -1;
    if (tab == 5) streetsEnter();
  }
  const int rows = tab == 0 ? Cat::PageCount : tab == 1 ? Cat::RelicCount : tab == 2 ? RecordCount : tab == 3 ? heardCount() : 1;
  if (p.down && sel < rows - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (tab == 0 && p.enter && ((game.career.pages >> sel) & 1)) { pageOpen = sel; go(Mode::Page); }
}

inline void drawGrimoire(Canvas& g, const Ctx& c) {
  (void)c;
  if (tab > 0) {
    g.fillSprite(UI::Bg);
    UI::text(g, 3, 3, tab == 4 ? foundTitle() : GrimoireTab[tab], UI::SelText);
    g.drawFastHLine(0, 13, UI::W, UI::Sep);
    if (tab == 1) drawCollection(g);
    else if (tab == 2) drawRecords(g);
    else if (tab == 3) drawHeard(g);
    else if (tab == 4) drawFound(g);
    else drawStreets(g);
    UI::footerLine(g);
    if (tab == 4 && foundFooter(g)) return;
    if (tab == 5 && streetsFooter(g)) return;
    int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
    if (tab == 4) x = UI::keyHint(g, x, "ENTER", "OPEN", UI::Text);
    UI::keyHint(g, x, "< >", GrimoireTab[(tab + 1) % GrimoireTabs], UI::Text);
    return;
  }
  g.fillSprite(UI::Bg);
  const Game::Career& car = game.career;
  char b[48], n[32];
  UI::text(g, 3, 3, "GRIMOIRE", UI::SelText);
  int relics = 0;   // the same count as the RELICS tab: relics you've met (held, won with, or on a stall)
  for (int i = 0; i < Cat::RelicCount; i++) relics += relicSeen(i);
  const int pages = Game::bits(car.pages);
  snprintf(b, sizeof(b), "%d PAGE%s  %d/%d RELICS OPEN", pages, pages == 1 ? "" : "S", relics, (int)Cat::RelicCount);   // unlocked; the RELICS tab counts the ones seen
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  const int show = 8;
  if (sel < top) top = sel;
  if (sel >= top + show) top = sel - show + 1;
  for (int i = top; i < Cat::PageCount && i < top + show; i++) {
    const int y = 17 + (i - top) * 10;
    const bool open = (car.pages >> i) & 1;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    if (open) {
      upper(Cat::Pages[i].title, n, sizeof(n));
      UI::text(g, 6, y, n, i == sel ? UI::SelText : UI::Text);
      if (car.theme == i) UI::textRight(g, UI::W - 4, y, "THEME", UI::Accent);
    } else {
      UI::text(g, 6, y, "? ? ? ? ?", i == sel ? UI::Dim : UI::Faint);
    }
  }
  // What opens the page under the cursor.
  g.drawFastHLine(0, 99, UI::W, UI::Sep);
  if (!((car.pages >> sel) & 1)) {
    char un[80];
    const Cat::PageDef& pd = Cat::Pages[sel];
    UI::wrap(g, 4, 103, UI::W - 8, Game::goalText(car, pd.unlock, pd.need, pd.unlockText, un, sizeof(un)), UI::Dim, 2);
  } else {
    upper(Cat::Pages[sel].theme, n, sizeof(n));
    snprintf(b, sizeof(b), "THEME: %s", n);
    UI::text(g, 4, 103, b, UI::Dim);
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  x = UI::keyHint(g, x, "ENTER", "READ", UI::Text);
  UI::keyHint(g, x, "< >", "RELICS", UI::Text);
}

constexpr int PageLines = 10;

inline void updatePage(const Ctx& c, const Press& p) {
  (void)c;
  if (p.back) { go(Mode::Grimoire); sel = pageOpen; return; }
  const int extra = UI::wrapLines(UI::W - 8, Lore::Pages[pageOpen]) - PageLines;
  if (p.down && top < extra) top++;
  if (p.up && top > 0) top--;
  if (p.enter) {
    game.career.theme = (uint8_t)pageOpen;
    UI::setTheme(pageOpen);
    if (hooks.setTheme) hooks.setTheme(pageOpen);
    save(true);
    toast("THE DECK TAKES ITS COLOR", UI::Accent);
  }
}

inline void drawPage(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  char n[32];
  upper(Cat::Pages[pageOpen].title, n, sizeof(n));
  UI::text(g, 4, 4, n, UI::SelText);
  g.drawFastHLine(0, 14, UI::W, UI::Sep);
  UI::wrap(g, 4, 19, UI::W - 8, Lore::Pages[pageOpen], UI::Text, PageLines, 10, top);
  if (UI::wrapLines(UI::W - 8, Lore::Pages[pageOpen]) > PageLines + top) UI::textRight(g, UI::W - 2, 110, "v", UI::Accent);
  if (top > 0) UI::textRight(g, UI::W - 2, 19, "^", UI::Accent);
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  char t[32];
  upper(Cat::Pages[pageOpen].theme, n, sizeof(n));
  snprintf(t, sizeof(t), "USE THE %s THEME", n);
  UI::keyHint(g, x, "ENTER", t, UI::Text);
}

// ---- The menu --------------------------------------------------------------------

enum MenuItem : uint8_t { MContinue, MBuild, MSpells, MAtlas, MGrimoire, MStory, MWalk, MBind, MAbandon, MSound, MCount };
inline MenuItem menuItems[MCount];
inline int nMenu = 0;
inline bool confirmAbandon = false;

inline void buildMenu() {
  nMenu = 0;
  menuItems[nMenu++] = MContinue;
  if (inRun()) { menuItems[nMenu++] = MBuild; menuItems[nMenu++] = MSpells; }
  menuItems[nMenu++] = MAtlas;
  menuItems[nMenu++] = MGrimoire;
  menuItems[nMenu++] = MStory;
  menuItems[nMenu++] = MWalk;
  if (!inRun()) menuItems[nMenu++] = MBind;
  menuItems[nMenu++] = MSound;
  if (inRun()) menuItems[nMenu++] = MAbandon;   // last, away from the row you toggle most
}

// The GIVE UP question is withdrawn with the cursor, and its warning goes with it.
inline void unaskAbandon() {
  if (confirmAbandon) toastT = 0;
  confirmAbandon = false;
}

inline void updateMenu(const Ctx& c, const Press& p) {
  buildMenu();
  if (p.back) { unaskAbandon(); go(listMode); return; }
  if (p.down && sel < nMenu - 1) { sel++; unaskAbandon(); }
  if (p.up && sel > 0) { sel--; unaskAbandon(); }
  if (!p.enter) return;
  switch (menuItems[sel]) {
    case MContinue: go(listMode); break;
    case MBuild: back = Mode::Menu; go(Mode::Build); break;
    case MSpells: back = Mode::Menu; wellId = 0; go(Mode::Spells); break;
    case MAtlas: back = Mode::Menu; go(Mode::Atlas); break;
    case MGrimoire: go(Mode::Grimoire); break;
    case MStory: premiseFromMenu = true; go(Mode::Premise); break;
    case MWalk: back = Mode::Menu; go(Mode::Walk); break;
    case MBind: go(Mode::Bind); break;
    case MAbandon:
      if (!confirmAbandon) { confirmAbandon = true; toast("ENTER AGAIN TO GIVE UP THE WEEK", UI::Rec); break; }
      confirmAbandon = false;
      lose();
      break;
    case MSound:
      c.st.muted = !c.st.muted;
      if (hooks.setMuted) hooks.setMuted(c.st.muted);
      break;
    default: break;
  }
}

inline void drawMenu(Canvas& g, const Ctx& c) {
  buildMenu();
  g.fillSprite(UI::Bg);
  UI::text(g, 3, 3, "DECK", UI::SelText);
  UI::textRight(g, UI::W - 3, 3, Game::rankName(game.career), UI::Dim);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  static const char* const names[MCount] = {"CONTINUE", "YOUR BUILD", "SPELLS", "ATLAS", "GRIMOIRE", "THE CURRENT", "WALK LOG",
                                            "BIND A NEW WEEK", "GIVE UP THIS WEEK", "SOUND"};
  for (int i = 0; i < nMenu; i++) {
    const int y = 17 + i * 10;
    if (i == sel) g.fillRect(0, y - 1, UI::W, 10, UI::SelBg);
    UI::text(g, 8, y, names[menuItems[i]], i == sel ? UI::SelText : UI::Text);
    if (menuItems[i] == MSound) UI::textRight(g, UI::W - 8, y, c.st.muted ? "OFF" : "ON", UI::Dim);
  }
  char b[48];
  if (inRun()) drawHoldLine(g, 8, 110);   // what you're carrying, since the scanner doesn't say
  if (game.career.runs > 0) {
    snprintf(b, sizeof(b), "%u OF %u WEEKS HELD", (unsigned)game.career.wins, (unsigned)game.career.runs);
    UI::textRight(g, UI::W - 3, 110, b, UI::Dim);
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  UI::keyHint(g, x, "ENTER", "OPEN", UI::Text);
}

} // namespace App
