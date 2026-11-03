#pragma once
#include "AppState.h"
#include "../game/Shop.h"
#include "Lore.h"
#include "Walks.h"

// The run's flow: binding a new run, the start of each Night (its Omen),
// the end of a Night (the payout and Portents), running out of visits
// (candles and the Syndicate's loan), winning, losing, and the Grimoire's
// reveal of what the Binding earned.

namespace App {

inline uint32_t (*randomSeed)() = nullptr;

// Each Eclipse and Omen in a few words, for screens with one line to spare.
inline const char* const EclipseShort[Cat::EclipseCount] = {"POLISH COUNTS FOR HALF", "THE TITHE IS 25% HIGHER",
                                                            "2 FEWER VISITS", "MULTIPLIERS GIVE A QUARTER LESS"};
inline const char* const OmenShort[Cat::OmenCount] = {
    "IS WORTH HALF", "UNITS x0.75", "2 FEWER VISITS", "YOUR FIRST 2 VISITS MAKE NOTHING",
    "THIRST x0.7 IN QUARTER AND NEXUS", "THIRST x0.7 IN HOLLOW AND FRINGE", "RELICS ADDING THIRST GIVE HALF",
    "YOU START AT HEAT 3", "RELICS SEE HALF THE WELLS", "THE TITHE IS 20% HIGHER", "RELICS MULTIPLY FIRST, THEN ADD"};   // set by the firmware; the renderer leaves it

// ---- Binding a run ------------------------------------------------------------------

inline int bindCalling = 0, bindPact = 0, bindChallenge = -1;
inline int bindOffer = -1;   // a Trial offered after a week lost on a Pact: the lesson, -1 none
inline uint16_t pagesBefore = 0;

inline int challengesOpen() { return game.career.wins < 10 ? (int)game.career.wins : 10; }
inline int pactsOpen(int calling) {
  const int s = game.career.seals[calling];
  return s + 2 > Cat::PactCount ? Cat::PactCount : s + 2;   // Chalk, plus one past your best seal
}

inline bool callingOpen(int i) { return Game::unlocked(game.career, Game::Kind::Calling, i); }

// Locked Callings are in the list too, so you can see what opens them.
inline void nextCalling(int dir) {
  bindCalling = (bindCalling + dir + Cat::CallingCount) % Cat::CallingCount;
  if (bindPact >= pactsOpen(bindCalling)) bindPact = pactsOpen(bindCalling) - 1;
}

inline void beginNightNow() {
  toastT = 0;   // nothing from the stall left over the Night's first screen
  Game::beginNight(run(), &game.moments.dawn.turned);
  game.phase = Game::Phase::Night;
  save(true);
  go(Mode::NightStart);
}

inline void startRun() {
  const uint32_t seed = randomSeed ? randomSeed() : 0x1234u + game.career.runs * 977u;
  Game::newRun(game.run, game.career, bindCalling, bindChallenge >= 0 ? 0 : bindPact, bindChallenge, seed,
               game.career.runs + 1);
  bindOffer = -1;
  memset(&game.recent, 0, sizeof(game.recent));
  memset(&game.shop, 0, sizeof(game.shop));
  beginNightNow();
  if (game.career.runs == 0) go(Mode::Premise);
}

inline void updateBind(const Ctx& c, const Press& p) {
  (void)c;
  if (p.back) { go(Mode::Menu); return; }
  const int rows = challengesOpen() ? 4 : 3;
  if (p.down) sel = (sel + 1) % rows;
  if (p.up) sel = (sel + rows - 1) % rows;
  const int dir = p.right ? 1 : p.left ? -1 : 0;
  if (dir) {
    if (sel == 0) nextCalling(dir);
    if (sel == 1) bindPact = (bindPact + dir + pactsOpen(bindCalling)) % pactsOpen(bindCalling);
    if (sel == 2 && rows == 4) {
      bindChallenge += dir;
      if (bindChallenge < -1) bindChallenge = challengesOpen() - 1;
      if (bindChallenge >= challengesOpen()) bindChallenge = -1;
    }
  }
  if (p.enter && sel < rows - 1) { sel++; return; }   // a choice is made with < >; ENTER moves on to BIND
  if (p.enter) {
    if (!callingOpen(bindCalling)) { toast("THAT CALLING IS STILL LOCKED", UI::Rec); beep(180, 80); return; }
    beep(523, 80);
    beep(784, 140);
    startRun();
  }
}

inline void drawBind(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  UI::text(g, 3, 3, "BIND A NEW WEEK", UI::SelText);
  char b[48];
  snprintf(b, sizeof(b), "%s", Game::rankName(game.career));
  UI::textRight(g, UI::W - 3, 3, b, UI::Dim);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  const int rows = challengesOpen() ? 4 : 3;
  const char* labels[4] = {"CALLING", "PACT", rows == 4 ? "TRIAL" : "BIND", "BIND"};
  for (int i = 0; i < rows; i++) {
    const int y = 18 + i * 12;
    if (i == sel) g.fillRect(0, y - 2, UI::W, 12, UI::SelBg);
    const bool isBind = i == rows - 1;
    UI::text(g, 6, y, labels[i], isBind ? (i == sel ? UI::SelText : UI::Accent) : UI::Dim);
    if (isBind) continue;
    if (i == 0) upper(Cat::Callings[bindCalling].name, b, sizeof(b));
    if (i == 0 && !callingOpen(bindCalling)) {
      if (Game::secretGoal(Cat::Callings[bindCalling].unlock)) snprintf(b, sizeof(b), "? ? ? ? ?");
      UI::text(g, 76, y, b, UI::Faint);
      UI::textRight(g, UI::W - 16, y, "LOCKED", UI::Rec);
      if (i == sel) { UI::text(g, 66, y, "<", UI::Dim); UI::textRight(g, UI::W - 6, y, ">", UI::Dim); }
      continue;
    }
    if (i == 1) {
      upper(Cat::Pacts[bindPact].name, b, sizeof(b));
      if (bindChallenge >= 0) snprintf(b, sizeof(b), "-");
    }
    if (i == 2) {
      if (bindChallenge < 0) snprintf(b, sizeof(b), "NONE");
      else upper(Cat::Challenges[bindChallenge].name, b, sizeof(b));
    }
    UI::text(g, 76, y, b, i == sel ? UI::SelText : UI::Text);
    const bool choices = i == 0 || (i == 1 && pactsOpen(bindCalling) > 1) || i == 2;   // arrows only where they do something
    if (i == sel && choices) { UI::text(g, 66, y, "<", UI::Dim); UI::textRight(g, UI::W - 6, y, ">", UI::Dim); }
  }
  // What the choice under the cursor means.
  const char* desc = "";
  char lock[200];
  if (sel == 0) desc = Cat::Callings[bindCalling].text;
  if (sel == 0 && !callingOpen(bindCalling)) {
    const Cat::CallingDef& cd = Cat::Callings[bindCalling];
    char u[80];
    Game::goalText(game.career, cd.unlock, cd.need, cd.unlockText, u, sizeof(u));
    if (Game::secretGoal(cd.unlock)) snprintf(lock, sizeof(lock), "%s", u);
    else snprintf(lock, sizeof(lock), "%s TO OPEN. %s", u, cd.text);
    desc = lock;
  }
  if (sel == 1) desc = Cat::Pacts[bindPact].text;
  if (sel == 1 && bindPact == pactsOpen(bindCalling) - 1 && bindPact + 1 < Cat::PactCount) {
    const char* pt = Cat::Pacts[bindPact].text;
    const size_t pl = strlen(pt);
    snprintf(lock, sizeof(lock), "%.*s. Hold a week on it to open %s.", (int)(pl && pt[pl - 1] == '.' ? pl - 1 : pl), pt, Cat::Pacts[bindPact + 1].name);
    desc = lock;
  }
  if (sel == 2 && rows == 4) desc = bindChallenge < 0 ? "A normal week. A Trial adds one hard rule; < > picks one." : Cat::Challenges[bindChallenge].text;
  if (sel == 2 && rows == 4 && bindChallenge >= 0 && bindChallenge == bindOffer) {   // the lesson of the week just lost
    snprintf(lock, sizeof(lock), "Lost on %s: %s is the lesson. %s", Cat::Pacts[bindPact].name, Cat::Challenges[bindOffer].name, Cat::Challenges[bindOffer].text);
    desc = lock;
  }
  if (sel == rows - 1) desc = "Seven Nights. A Tithe each Night. Losing costs only the week. Relics and coin stay with their week; what you unlock stays with you.";
  UI::wrap(g, 6, 70, UI::W - 12, desc, UI::Text, 4);
  if (sel == 0) {
    const int seal = game.career.seals[bindCalling];
    if (seal >= 0) {
      char p[16];
      upper(Cat::Pacts[seal].name, p, sizeof(p));
      snprintf(b, sizeof(b), "SEAL: %s", p);
      UI::text(g, 6, 110, b, UI::Accent);
    }
  }
  UI::footerLine(g);
  int x = UI::keyHint(g, 2, "< >", "CHOOSE", UI::Text);
  UI::keyHint(g, x, "ENTER", "BIND", UI::Text);
}

// ---- Why you're out here (shown before the first Night ever) ----------------------

// Text between { and } is drawn in the accent color.
inline constexpr const char* PremiseText =
    "Every router leaves an uncanny trace in the air: {the current}. Left unmoved, it goes stale, and "
    "people on that street feel it without knowing why. If you're reading this, a Deck took to you. "
    "There are fewer of you every year. Draw the current and offer it where it's wanted. Your Deck "
    "goes stale too unless current runs through it, so each Night you owe a Tithe, or it goes back to hiss.";

inline bool premiseFromMenu = false;   // opened from the menu, not before a first Night

inline void updatePremise(const Ctx& c, const Press& p) {
  (void)c;
  if (!p.enter && !p.back) return;
  if (premiseFromMenu) { premiseFromMenu = false; go(Mode::Menu); }
  else go(Mode::NightStart);
}

// Wrapped like UI::wrap, with {marked} words in the accent color.
inline void drawMarked(Canvas& g, int x, int y, int width, const char* src, int lineH) {
  char s[512];
  bool hot[512];
  int n = 0;
  bool on = false;
  for (const char* p = src; *p && n < (int)sizeof(s) - 1; p++) {
    if (*p == '{') { on = true; continue; }
    if (*p == '}') { on = false; continue; }
    hot[n] = on;
    s[n++] = *p;
  }
  s[n] = 0;
  const int perLine = width / UI::CharW;
  int at = 0, line = 0;
  while (at < n) {
    int take = n - at <= perLine ? n - at : perLine;
    if (n - at > perLine) {
      int sp = take;
      while (sp > 0 && s[at + sp] != ' ') sp--;
      if (sp > 0) take = sp;
    }
    for (int i = 0; i < take; i++) {
      const char ch[2] = {s[at + i], 0};
      UI::text(g, x + i * UI::CharW, y + line * lineH, ch, hot[at + i] ? UI::Accent : UI::Text);
    }
    line++;
    at += take;
    while (at < n && s[at] == ' ') at++;
  }
}

inline void drawPremise(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  UI::text(g, 3, 3, "THE CURRENT", UI::SelText);
  g.drawFastHLine(0, 13, UI::W, UI::Sep);
  drawMarked(g, 6, 18, UI::W - 12, PremiseText, 9);
  UI::footerLine(g);
  if (premiseFromMenu) UI::keyHint(g, 2, "ESC", "BACK", UI::Text);
  else UI::keyHint(g, 2, "ENTER", "GO OUT", UI::Text);
}

// ---- The start of a Night ---------------------------------------------------------

inline void updateNightStart(const Ctx& c, const Press& p) {
  (void)c;
  if (p.enter || p.back) go(Mode::Scanner);
}

// "NEXT: FOG, THEN LOW TIDE": the Omens of the next two Nights, for the
// Diviner. False when neither has one. 39 characters at the longest.
inline bool omensAhead(const Game::Run& r, char* b, int cap) {
  const int a = Game::omenOf(r, r.night + 1), then = Game::omenOf(r, r.night + 2);
  if (a < 0) return false;
  char n1[24], n2[24];
  upper(Cat::Omens[a].name, n1, sizeof(n1));
  if (then < 0) { snprintf(b, cap, "NEXT: %s", n1); return true; }
  upper(Cat::Omens[then].name, n2, sizeof(n2));
  snprintf(b, cap, "NEXT: %s, THEN %s", n1, n2);
  return true;
}

// The Night start's other news, a line each (36 characters at the longest):
// an Omen or Eclipse a ward turned away, the energy Glass crown refuses, and
// Glitch's focus and shuffle. Returns how many.
inline int nightStartNotes(const Game::Run& r, char lines[4][40], uint16_t cols[4]) {
  int n = 0;
  char nm[24];
  const uint8_t turned = game.moments.dawn.turned;
  if ((turned & 0xF) && (turned & 0xF) <= Cat::OmenCount) {
    upper(Cat::Omens[(turned & 0xF) - 1].name, nm, sizeof(nm));
    snprintf(lines[n], 40, "CHARM WARD TURNED AWAY %s", nm);
    cols[n++] = UI::Accent;
  }
  if ((turned >> 4) && (turned >> 4) <= Cat::EclipseCount) {
    upper(Cat::Eclipses[(turned >> 4) - 1].name, nm, sizeof(nm));
    snprintf(lines[n], 40, "ECLIPSE WARD HELD OFF %s", nm);
    cols[n++] = UI::Accent;
  }
  if (Game::owns(r, Cat::R_glass_crown)) {
    snprintf(lines[n], 40, "THE CROWN REFUSES %s TONIGHT", energyWord(r.n.crownBan));
    cols[n++] = UI::Rec;
  }
  if (Game::isCalling(r, Cat::Calling_glitch)) {
    snprintf(lines[n], 40, "GLITCH FOCUS TONIGHT: %s", energyWord(r.focus));
    cols[n++] = UI::energyColor(r.focus);
    int k = 0;
    for (int e = 0; e < 3; e++) {   // the wells whose energy moved: EMBER>TIDE
      if (r.n.perm[e] == e) continue;
      k += snprintf(lines[n] + k, 40 - k, "%s%s>%s", k ? "  " : "", energyWord(e), energyWord(r.n.perm[e]));
    }
    if (k) cols[n++] = UI::Text;
  }
  return n;
}

inline void drawNightStart(Canvas& g, const Ctx& c, float t) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  char b[48], n[16];
  if (r.night < Game::Nights) snprintf(b, sizeof(b), "NIGHT %d", r.night + 1);
  else snprintf(b, sizeof(b), "OVERNIGHT %d", r.night - Game::Nights + 1);
  UI::textCenter(g, 8, b, UI::SelText, 2);
  if (r.challenge == 8) snprintf(b, sizeof(b), "TITHE %s, DOUBLED TILL ALL 4 WARDS", UI::num(r.n.tithe, n, sizeof(n)));
  else snprintf(b, sizeof(b), "TITHE %s", UI::num(r.n.tithe, n, sizeof(n)));
  UI::textCenter(g, 30, b, UI::Text);
  snprintf(b, sizeof(b), "%d VISITS", r.n.visits);
  UI::textCenter(g, 41, b, UI::Dim);
  int y = 58;
  if (r.n.omen >= 0) {
    char nm[24];
    upper(Cat::Omens[r.n.omen].name, nm, sizeof(nm));
    snprintf(b, sizeof(b), "OMEN: %s", nm);
    UI::textCenter(g, y, b, UI::EnergyColor[3]);
    y += 11;
    char fx[64];
    if (r.n.omen == Cat::Omen_fade) snprintf(fx, sizeof(fx), "%s is worth half tonight", energyWord(r.n.fadeE));
    else snprintf(fx, sizeof(fx), "%s", Cat::Omens[r.n.omen].text);
    y += 10 * UI::wrap(g, 12, y, UI::W - 24, fx, UI::Text, 2, 10, 0, true);
  }
  if (r.n.eclipse >= 0) {
    char nm[24];
    upper(Cat::Eclipses[r.n.eclipse].name, nm, sizeof(nm));
    snprintf(b, sizeof(b), "ECLIPSE: %s", nm);
    UI::textCenter(g, y, b, UI::Rec);
    y += 11;
    y += 10 * UI::wrap(g, 12, y, UI::W - 24, Cat::Eclipses[r.n.eclipse].text, UI::Text, 2, 10, 0, true);
  }
  // What else is different tonight: a ward that worked, the crown's refusal, Glitch's shuffle.
  char lines[4][40];
  uint16_t cols[4];
  const int extra = nightStartNotes(r, lines, cols);
  for (int i = 0; i < extra && y <= 113; i++, y += 10) UI::textCenter(g, y, lines[i], cols[i]);
  if (r.n.eclipse < 0 && r.night < Game::Nights - 1 && r.eclipse >= 0 && y <= 92) {
    char nm[24];
    upper(Cat::Eclipses[r.eclipse].name, nm, sizeof(nm));
    snprintf(b, sizeof(b), "NIGHT 7: %s", nm);
    UI::textCenter(g, 92, b, UI::dimmed(UI::Rec));
    UI::textCenter(g, 102, EclipseShort[r.eclipse], UI::Dim);
  }
  // The Diviner sees two Nights ahead.
  if (Game::seesAhead(r) && y <= 112 && omensAhead(r, b, sizeof(b))) UI::textCenter(g, 112, b, UI::Accent);
  UI::footerLine(g);
  UI::keyHint(g, 2, "ENTER", "GO OUT", fmodf(t, 1.0f) < 0.6f ? UI::SelText : UI::Text);
}

// ---- The end of a Night ---------------------------------------------------------

// A question the stall is asking before it does something (ShopScreens.h): leaving,
// selling, polishing, rerolling. Cleared when the stall opens and whenever the cursor moves.
enum ShopAsk : uint8_t { AskNone, AskLeave, AskSell, AskPolish, AskReroll };
inline ShopAsk shopAsk = AskNone;
inline int shopAskRow = -1;

// The stall opens. Shared by the dawn and THE BINDING HOLDS.
inline void openStall() {
  shopAsk = AskNone;
  Game::openShop(game.shop, run(), game.career);
  if (!game.shop.open) { beginNightNow(); return; }   // Empty pockets: no shop
  Game::starBest(game.shop, run(), game.recent);
  game.phase = Game::Phase::Shop;
  save(true);
  go(Mode::Shop);
  if (game.shop.mercy && !Game::oneNote(run())) teachToast(Taught::Mercy, "MERCY: A CHEAP RELIC AND A FREE REROLL", "PLUS ONE ATTUNEMENT ON YOUR FOCUS");
}

inline void toShopOrNext() {
  if (run().won && run().night == Game::Nights) { go(Mode::Won); return; }
  openStall();
}

// The dawn tally: each payout drops in with a coin tick while COIN counts
// up, then the Portents turn over one at a time.
struct TallyRow { const char* label; int v; };
constexpr float TallyStart = 0.45f, TallyRowT = 0.3f, TallyCountT = 0.2f, TallyPortentT = 0.4f;

constexpr int TallyMax = 10;

// The rows: what the dawn paid, a Windfall's coin, and on Shoestring what the
// 15 cap took (in red). OFFERINGS and FOR THE TITHE always show.
inline int tallyRows(const Game::NightEnd& e, TallyRow out[TallyMax]) {
  int windfall = 0;
  for (int k = 0; k < e.portents; k++)
    if (!((game.moments.portent.pending >> k) & 1) && e.got[k].portent == Cat::Portent_windfall) windfall += 10;
  const TallyRow all[] = {
    {"OFFERINGS", e.trade}, {"FAVORS", (int)e.favors}, {"SPARE VISITS", e.spareCoin}, {"FOR THE TITHE", e.reward},
    {"INTEREST", e.interest}, {"GILT RELICS", e.gilt}, {"GOLDEN THREAD", e.thread}, {"TITHE-SKIMMER", e.skim},
    {"WINDFALL", windfall}, {"OVER THE 15 CAP", -(int)game.moments.dawn.capped},
  };
  int n = 0;
  for (const TallyRow& row : all)
    if (row.v != 0 || !strcmp(row.label, "OFFERINGS") || !strcmp(row.label, "FOR THE TITHE")) out[n++] = row;
  return n;
}
// FAVORS paid out as they finished, during the Night: COIN already holds them.
inline bool paidBefore(const TallyRow& row) { return !strcmp(row.label, "FAVORS"); }
inline int tallyTurns(const Game::NightEnd& e) { return e.portents + (Game::nightRelic(e) >= 0 ? 1 : 0); }
inline float tallyRowAt(int i) { return TallyStart + i * TallyRowT; }
inline float tallyTurnAt(int nRows, int j) { return tallyRowAt(nRows) + 0.15f + j * TallyPortentT; }
inline float tallyEnd(const Game::NightEnd& e) {
  TallyRow rows[TallyMax];
  const int n = tallyRows(e, rows), turns = tallyTurns(e);
  return (turns ? tallyTurnAt(n, turns - 1) : tallyRowAt(n - 1) + TallyCountT) + 0.25f;
}
inline int tallyHeard = 0;   // tally events already given their sound
inline float tallyLastT = 0;

// A free relic or spell with no room for it: their screens are in ShopScreens.h.
inline void updateGiftSwap(int k, const Press& p);
inline void drawGiftSwap(Canvas& g, int k);
inline void updateSpellSwap(int k, const Press& p);
inline void drawSpellSwap(Canvas& g, int k);

// The first Portent still to choose between, or -1.
inline int portentToChoose() {
  const Game::PortentChoice& ch = game.moments.portent;
  for (int k = 0; k < game.lastEnd.portents; k++) if ((ch.pending >> k) & 1) return k;
  return -1;
}

// The free relic waiting for room, once the tally and any choice are done: a
// Portent's row (0-2), then 3 for Empty pockets' own relic; -1 none.
inline int giftToPlace(const Game::NightEnd& e) {
  if (modeT < tallyEnd(e) || portentToChoose() >= 0) return -1;
  const int k = Game::giftWaiting(game.moments.swap);
  return k >= 0 ? k : game.moments.dawn.gift ? 3 : -1;
}
// The Scroll row whose spell is waiting for a slot, once any relic is placed; -1 none.
inline int spellToPlace(const Game::NightEnd& e) {
  if (giftToPlace(e) >= 0 || modeT < tallyEnd(e) || portentToChoose() >= 0) return -1;
  return Game::spellWaiting(game.moments.swap);
}

inline void updateNightEnd(const Ctx& c, const Press& p) {
  (void)c;
  const Game::NightEnd& e = game.lastEnd;
  TallyRow rows[TallyMax];
  const int n = tallyRows(e, rows), turns = tallyTurns(e);
  if (modeT < tallyLastT) tallyHeard = 0;   // a fresh tally
  tallyLastT = modeT;
  const int choosing = modeT >= tallyEnd(e) ? portentToChoose() : -1;
  const int gift = giftToPlace(e);
  if (gift >= 0) { updateGiftSwap(gift, p); return; }
  const int spell = spellToPlace(e);
  if (spell >= 0) { updateSpellSwap(spell, p); return; }
  if (modeT >= tallyEnd(e) && choosing < 0 && game.moments.lastToken) teachToast(Taught::Token, "IT'S KEPT IN GRIMOIRE > FOUND > TOKENS");
  if (choosing >= 0 && (p.up || p.down)) sel = 1 - sel;
  if (p.enter) {
    if (modeT < tallyEnd(e)) { modeT = tallyEnd(e); tallyHeard = n + turns; return; }   // skip to the end
    if (choosing >= 0) {   // keep one of the two
      Game::choosePortent(run(), game.career, game.lastEnd, game.moments.portent, choosing, sel == 1,
                          &game.moments.swap, &game.moments.dawn);
      sel = 0;
      beep(1319, 60);
      beep(1976, 90);
      if (modeT < tallyEnd(e)) { modeT = tallyEnd(e); tallyLastT = modeT; }   // a Windfall's row lands at once
      save(true);
      return;
    }
    toShopOrNext();
    return;
  }
  while (tallyHeard < n + turns) {
    const int k = tallyHeard;
    if (modeT < (k < n ? tallyRowAt(k) : tallyTurnAt(n, k - n))) break;
    if (k < n) { beep(880 + 70 * k, 30); beep(1760 + 140 * k, 20); }   // a coin, a little higher each time
    else { beep(1319, 60); beep(1976, 90); }
    tallyHeard++;
  }
  tallyLastT = modeT;
}

// The Collector says one thing at every dawn, and it's always about your Night.
// Only shown when no Portent turns over, the week isn't won and there's room
// (collectorShown). A number in Lore::Collector, so the HEARD log can keep it.
inline int collectorLineId(const Game::NightEnd& e, const Game::Run& r) {
  using namespace Lore;
  const int paidNight = r.night - 1;   // endNight has already moved on
  const unsigned h = r.runNo * 7u + (unsigned)paidNight * 3u;
  if (r.runNo == 1 && paidNight == 0) return CL_first;
  if (r.n.closedBy == 2) return CL_loan;
  if (e.spare == 0) return CL_close;
  if (r.heat >= 3) return CL_loud;
  if (e.mercy) return CL_pencil;
  if (paidNight == Game::Nights - 2) return CL_night6;
  if (e.repaid) return CL_paid;
  if (e.spare >= 4 && h % 3 == 0) return CL_coat;   // Portents start at 6, so 4 or 5 to spare
  if (Game::isCalling(r, Cat::Calling_glitch) && paidNight <= 1 && h % 2 == 0) return CL_wobble;   // once a week
  // Sometimes, who owned a relic you carry.
  if (h % 3 == 1) {
    static const struct { int relic; uint8_t line; } owners[] = {
      {Cat::R_grid_saint, CL_grid_saint}, {Cat::R_the_unheard, CL_unheard}, {Cat::R_seven_league_boots, CL_boots},
      {Cat::R_old_keeper, CL_old_keeper}, {Cat::R_midnight_engine, CL_engine},
      {Cat::R_night_shift, CL_night_shift}, {Cat::R_borderwalker, CL_borderwalker},
      {Cat::R_tally_stick, CL_coast}, {Cat::R_seashell, CL_coast},   // both owners moved to the coast
    };
    int held[9], n = 0;
    for (const auto& o : owners) if (Game::owns(r, o.relic)) held[n++] = o.line;
    if (n) return held[(h / 3) % n];
  }
  static const uint8_t any[] = {CL_any0, CL_any1, CL_any2, CL_any3, CL_any4, CL_any5, CL_any6, CL_any7, CL_any8, CL_any9, CL_keeper};
  return any[h % sizeof(any)];
}
inline const char* collectorLine(const Game::NightEnd& e, const Game::Run& r) { return Lore::Collector[collectorLineId(e, r)]; }

// ---- What the dawn says under the tally ---------------------------------------------

// The right column, under COIN: what turns over for turn j, a Portent's name
// (ONE OF TWO while it's still a choice) or Empty pockets' relic (dim once left).
constexpr int ColX = 162, ColChars = (UI::W - ColX) / UI::CharW;   // 13 characters
inline uint16_t turnName(const Game::NightEnd& e, int j, char* nm, int cap) {
  if (j < e.portents && ((game.moments.portent.pending >> j) & 1)) { snprintf(nm, cap, "ONE OF TWO"); return UI::Dim; }
  if (j < e.portents) { upper(Cat::Portents[e.got[j].portent].name, nm, cap); return UI::Accent; }
  const int id = Game::nightRelic(e);
  upper(Cat::Relics[id].name, nm, cap);
  return e.gift >= 0 ? UI::rarityColor(Cat::Relics[id].rarity) : UI::Dim;
}
// A name too long for the column breaks at its last space that fits, onto a
// second line (with `wrap` off, only that first line shows). Returns the lines it takes.
inline int splitName(const char* nm, char a[24], char b[24], bool wrap = true) {
  const int len = (int)strlen(nm);
  b[0] = 0;
  if (len <= ColChars) { snprintf(a, 24, "%s", nm); return 1; }
  int sp = ColChars;
  while (sp > 0 && nm[sp] != ' ') sp--;
  if (sp == 0) sp = ColChars;
  snprintf(a, 24, "%.*s", sp, nm);
  if (!wrap) return 1;
  const char* rest = nm + sp;
  while (*rest == ' ') rest++;
  snprintf(b, 24, "%.*s", ColChars, rest);
  return 2;
}
// Long names wrap while the column stays within 4 lines, so the lines under
// the tally keep room for every Portent; past that they're cut to fit.
constexpr int ColMaxLines = 4;
inline int wrappedLines(const Game::NightEnd& e) {
  int n = 0;
  char nm[28], a[24], b[24];
  for (int j = 0; j < tallyTurns(e); j++) {
    turnName(e, j, nm, sizeof(nm));
    n += splitName(nm, a, b);
  }
  return n;
}
inline bool columnWraps(const Game::NightEnd& e) { return wrappedLines(e) <= ColMaxLines; }
inline int columnLines(const Game::NightEnd& e) { return columnWraps(e) ? wrappedLines(e) : tallyTurns(e); }

// What Portent j did, in a line of 36 characters at most.
inline uint16_t portentLine(const Game::NightEnd& e, int j, const Game::Run& r, char* b, int cap) {
  using namespace Cat;
  const Game::PortentResult& pr = e.got[j];
  const bool later = e.won;   // the seventh dawn: there's only a next stall Overnight
  char nm[24];
  switch (pr.portent) {
    case Portent_gift:
    case Portent_conjunction: {
      const char* what = pr.portent == Portent_gift ? "GIFT" : "CONJUNCTION";
      if (pr.relic >= 0) {
        upper(Relics[pr.relic].name, nm, sizeof(nm));
        snprintf(b, cap, "%s: %s", what, nm);
        return UI::Accent;
      }
      if (pr.relic == -3) {
        bool any = false;
        for (int i = 0; i < RelicCount; i++) any |= Relics[i].legendary && Game::unlocked(game.career, Game::Kind::Relic, i);
        snprintf(b, cap, any ? "CONJUNCTION: NO LEGENDARY LEFT" : "CONJUNCTION: NO LEGENDARY OPEN YET");
      } else if (pr.relic == -2 || pr.portent == Portent_conjunction) {
        snprintf(b, cap, "NO ROOM, SO YOU LEFT THE FREE RELIC");
      } else {
        snprintf(b, cap, "GIFT: NO RELIC LEFT TO GIVE");
      }
      return UI::Dim;
    }
    case Portent_windfall:   // only Shoestring's cap takes any of it
      if (pr.coin >= 10 || r.challenge != 0) snprintf(b, cap, "WINDFALL: +10 COIN");
      else if (pr.coin > 0) snprintf(b, cap, "WINDFALL: +%d, THEN THE 15 CAP", pr.coin);
      else snprintf(b, cap, "WINDFALL: YOU'RE AT THE 15 CAP");
      return pr.coin > 0 ? UI::Accent : UI::Dim;
    case Portent_doubled_interest:
      if (game.moments.portent.interest == 0) { snprintf(b, cap, "NO INTEREST TO DOUBLE"); return UI::Dim; }
      snprintf(b, cap, "DOUBLED INTEREST: +%d COIN", pr.coin);
      return UI::Accent;
    case Portent_rare_sight:
      snprintf(b, cap, later ? "A SURE RARE, IF YOU GO OVERNIGHT" : "A SURE RARE RELIC AT THE NEXT STALL");
      return UI::Accent;
    case Portent_free_rerolls:
      snprintf(b, cap, later ? "2 FREE REROLLS, IF YOU GO OVERNIGHT" : "TWO FREE REROLLS AT THE NEXT STALL");
      return UI::Accent;
    case Portent_shine:
      snprintf(b, cap, later ? "A SHINE, IF YOU GO OVERNIGHT" : "YOUR NEXT RELIC COMES WITH A SHINE");
      return UI::Accent;
    case Portent_tuning:
      snprintf(b, cap, "+1 ATTUNEMENT: %s", energyWord(r.focus));
      return UI::Accent;
    case Portent_scroll:
      if (pr.spell >= 0) { snprintf(b, cap, "SCROLL: %s", Spells[pr.spell].name); return UI::Accent; }
      snprintf(b, cap, pr.spell == -2 ? "NO SLOT, SO YOU LEFT THE FREE SPELL" : "SCROLL: NO SPELL TO GIVE");
      return UI::Dim;
    case Portent_chalk_mark:
      if (pr.spell < 0) { snprintf(b, cap, "CHALK MARK: YOU HOLD 4 ALREADY"); return UI::Dim; }
      upper(Glyphs[pr.spell].name, nm, sizeof(nm));
      snprintf(b, cap, "CHALK MARK: %s GLYPH", nm);
      return UI::Accent;
    default:
      upper(Portents[pr.portent].text, b, cap);
      return UI::Accent;
  }
}

// One line under the tally. The Collector's remark (`say`) wraps to one or two rows.
struct DawnLine {
  char text[40];
  const char* say;
  uint16_t col;
  uint8_t keep;    // when there isn't room, the lowest goes first
  uint8_t rows;
  bool center;
};
constexpr int DawnLineMax = 9, DawnPitch = 9, DawnBottom = 113;

// Everything the dawn has to say, in screen order, before any is dropped for
// room: the token, each Portent, Empty pockets' relic, the Collector, a loan
// paid off, mercy, and a relic that crumbled.
inline int dawnLines(const Game::NightEnd& e, const Game::Run& r, DawnLine out[DawnLineMax]) {
  int n = 0;
  auto add = [&](uint16_t col, uint8_t keep, bool center) -> DawnLine& {
    DawnLine& l = out[n++];
    l = DawnLine{};
    l.col = col;
    l.keep = keep;
    l.rows = 1;
    l.center = center;
    return l;
  };
  const int token = game.moments.lastToken - 1;
  if (token >= 0) {   // what turned up with the coin; its picture stays beside FOR THE TITHE
    DawnLine& l = add(UI::Accent, 3, false);
    if (token == Game::T_Stranger) snprintf(l.text, sizeof(l.text), "A TOKEN TURNED UP. NOT FROM HERE");
    else snprintf(l.text, sizeof(l.text), "A %s TOKEN TURNED UP", TokenName[token]);
  }
  for (int j = 0; j < e.portents; j++) {
    DawnLine& l = add(UI::Accent, 10, false);
    l.col = portentLine(e, j, r, l.text, sizeof(l.text));
  }
  const bool pockets = r.challenge == 7;   // Empty pockets
  if (pockets) {
    DawnLine& l = add(UI::Accent, 8, false);
    const bool left = Game::nightRelic(e) >= 0 && e.gift < 0;
    snprintf(l.text, sizeof(l.text), "%s+1 ATTUNEMENT: %s", left ? "LEFT THE RELIC. " : "", energyWord(r.focus));
  }
  if (!e.won && e.portents == 0) {   // waiting at the stall
    DawnLine& l = add(UI::Dim, 2, false);
    l.say = collectorLine(e, r);
    const int rows = UI::wrapLines(UI::W - 16, l.say);
    l.rows = (uint8_t)(rows > 2 ? 2 : rows);
  }
  if (e.repaid) snprintf(add(UI::Accent, 5, true).text, sizeof(DawnLine::text), "LOAN PAID OFF");
  if (e.mercy && !pockets)
    snprintf(add(UI::Accent, 9, true).text, sizeof(DawnLine::text), "%s",
             e.won ? "MERCY: IF YOU GO OVERNIGHT" : "MERCY: THE NEXT STALL GOES EASY ON YOU");
  if (e.withered) {
    DawnLine& l = add(UI::Rec, 9, true);
    const Game::DawnNotes& d = game.moments.dawn;
    char nm[24];
    if (d.nCrumbled >= 2) snprintf(l.text, sizeof(l.text), "%d WITHERED RELICS CRUMBLED", d.nCrumbled);
    else if (d.crumbled) { upper(Cat::Relics[d.crumbled - 1].name, nm, sizeof(nm)); snprintf(l.text, sizeof(l.text), "%s CRUMBLED", nm); }
    else snprintf(l.text, sizeof(l.text), "A WITHERED RELIC CRUMBLED");
  }
  return n;
}

// The highest the lines can start: under the tally and the right column.
inline int dawnTop(const Game::NightEnd& e) {
  TallyRow rows[TallyMax];
  const int under = 36 + tallyRows(e, rows) * 9 + 2, beside = 50 + columnLines(e) * 10;
  return under > beside ? under : beside;
}

// Drop the least needed lines until the rest fit above the footer, then
// return where the first one goes: as low as 84, higher when there are more.
inline int fitDawnLines(DawnLine l[], int& n, int top) {
  auto rowsOf = [&] { int t = 0; for (int i = 0; i < n; i++) t += l[i].rows; return t; };
  while (n > 0 && top + (rowsOf() - 1) * DawnPitch > DawnBottom) {
    int drop = 0;
    for (int i = 1; i < n; i++) if (l[i].keep <= l[drop].keep) drop = i;
    for (int i = drop; i + 1 < n; i++) l[i] = l[i + 1];
    n--;
  }
  const int low = DawnBottom - (rowsOf() - 1) * DawnPitch;
  const int y = low < 84 ? low : 84;
  return y > top ? y : top;
}

// Whether the Collector's remark has room at this dawn (MERCY and a crumble come first).
inline bool collectorShown(const Game::NightEnd& e, const Game::Run& r) {
  DawnLine l[DawnLineMax];
  int n = dawnLines(e, r, l);
  fitDawnLines(l, n, dawnTop(e));
  for (int i = 0; i < n; i++) if (l[i].say) return true;
  return false;
}
// The dawn just closed: if the Collector speaks, the HEARD log keeps it.
inline void noteCollectorLine() {
  const Game::NightEnd& e = game.lastEnd;
  if (e.paid && collectorShown(e, run())) Game::setBit(game.heard.stall, collectorLineId(e, run()));
}

// A Portent's name in the choice of two; Doubled interest says what it would pay.
inline void choiceName(int kind, char* b, int cap) {
  upper(Cat::Portents[kind].name, b, cap);
  if (kind == Cat::Portent_doubled_interest) {
    const int k = (int)strlen(b);
    snprintf(b + k, cap - k, ": +%d COIN", game.moments.portent.interest);
  }
}

inline void drawNightEnd(Canvas& g, const Ctx& c) {
  (void)c;
  const Game::NightEnd& e = game.lastEnd;
  if (const int gift = giftToPlace(e); gift >= 0) { drawGiftSwap(g, gift); return; }
  if (const int spell = spellToPlace(e); spell >= 0) { drawSpellSwap(g, spell); return; }
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  char b[48];
  UI::textCenter(g, 4, r.n.closedBy == 2 ? "THE SYNDICATE PAID" : "TITHE PAID", UI::SelText, 2);
  if (e.spare > 0) snprintf(b, sizeof(b), "%d VISIT%s TO SPARE", e.spare, e.spare == 1 ? "" : "S");
  else snprintf(b, sizeof(b), "ON THE LAST VISIT");
  UI::textCenter(g, 23, b, UI::Dim);
  TallyRow rows[TallyMax];
  const int nRows = tallyRows(e, rows);
  const int token = game.moments.lastToken - 1;
  int total = 0;
  for (int i = 0; i < nRows; i++) if (!paidBefore(rows[i])) total += rows[i].v;
  // Each row drops in, its number counting up; COIN counts with them, from
  // what you held before the dawn.
  int y = 36, landed = 0;
  for (int i = 0; i < nRows; i++, y += 9) {
    const float age = modeT - tallyRowAt(i);
    if (age < 0) break;
    const float k = age >= TallyCountT ? 1 : age / TallyCountT;
    const int v = (int)lroundf(rows[i].v * k);
    const int dy = age < 0.12f ? (int)((0.12f - age) / 0.12f * -4) : 0;   // it drops into place
    const bool lost = rows[i].v < 0;   // the Shoestring cap
    UI::text(g, 30, y + dy, rows[i].label, lost ? UI::Rec : UI::Dim);
    const bool loaned = r.n.closedBy == 2 && !strcmp(rows[i].label, "OFFERINGS");   // the Syndicate keeps it
    snprintf(b, sizeof(b), lost ? "-%d" : "%+d", lost ? -v : v);   // Hungry coin can leave the offerings' coin below 0
    UI::textRight(g, 150, y + dy, loaned ? "THE LOAN'S" : b, loaned || lost ? UI::Rec : age < 0.25f ? UI::SelText : UI::Text);
    if (!strcmp(rows[i].label, "FOR THE TITHE") && token >= 0) drawToken(g, 8, y + dy - 5, token, 2);   // the coin's token
    if (!paidBefore(rows[i])) landed += v;
  }
  snprintf(b, sizeof(b), "COIN %d", (int)r.coin - total + landed);
  UI::text(g, ColX, 36, b, UI::SelText);
  // Then the Portents (and Empty pockets' relic) turn over on the right, a letter at a time.
  int ry = 50;
  const int turns = tallyTurns(e);
  const bool wraps = columnWraps(e);
  for (int j = 0; j < turns; j++) {
    const float age = modeT - tallyTurnAt(nRows, j);
    if (age < 0) break;
    char nm[28], line[2][24];
    const uint16_t col = turnName(e, j, nm, sizeof(nm));
    const int lines = splitName(nm, line[0], line[1], wraps);
    const int len = (int)(strlen(line[0]) + strlen(line[1]));
    int shown = age >= 0.25f ? len : (int)(len * age / 0.25f) + 1;
    for (int i = 0; i < lines; i++, ry += 10) {
      const int l = (int)strlen(line[i]);
      if (shown < l) line[i][shown < 0 ? 0 : shown] = 0;
      shown -= l;
      UI::text(g, ColX, ry, line[i], age < 0.35f ? UI::Text : col);
    }
  }
  const int choosing = modeT >= tallyEnd(e) ? portentToChoose() : -1;
  if (choosing >= 0) {   // two Portents; the cursor on the one to keep, its text under them
    int ny = dawnTop(e);
    ny = ny < 84 ? 84 : ny > 93 ? 93 : ny;
    const int kinds[2] = {e.got[choosing].portent, game.moments.portent.alt[choosing] - 1};
    for (int i = 0; i < 2; i++) {
      const int cy = ny + i * 10;
      if (i == sel) g.fillRect(0, cy - 2, UI::W, 11, UI::SelBg);
      choiceName(kinds[i], b, sizeof(b));
      UI::text(g, 8, cy, b, i == sel ? UI::SelText : UI::Accent);
    }
    UI::text(g, 8, ny + 20, Cat::Portents[kinds[sel]].text, UI::Text);
  } else if (modeT >= tallyEnd(e)) {
    DawnLine l[DawnLineMax];
    int n = dawnLines(e, r, l);
    int ny = fitDawnLines(l, n, dawnTop(e));
    for (int i = 0; i < n; i++) {
      if (l[i].say) UI::wrap(g, 8, ny, UI::W - 16, l[i].say, l[i].col, l[i].rows, DawnPitch);
      else if (l[i].center) UI::textCenter(g, ny, l[i].text, l[i].col);
      else UI::text(g, 8, ny, l[i].text, l[i].col);
      ny += l[i].rows * DawnPitch;
    }
  }
  UI::footerLine(g);
  char next[24] = "TO THE COLLECTOR";
  if (r.challenge == 7) {   // Empty pockets: no stall, straight on to the next Night
    if (r.night < Game::Nights) snprintf(next, sizeof(next), "TO NIGHT %d", r.night + 1);
    else snprintf(next, sizeof(next), "TO OVERNIGHT %d", r.night - Game::Nights + 1);
  }
  UI::keyHint(g, 2, "ENTER", choosing >= 0 ? "KEEP THIS ONE" : e.won ? "THE SEVENTH NIGHT IS PAID" : next, UI::Text);
}

// ---- Out of visits -----------------------------------------------------------------

inline void endBinding() {
  pagesBefore = game.career.pages;
  game.moments.reveal = (uint16_t)(0x8000u | pagesBefore);   // the reveal waits in the save until it's seen
  Game::endRun(game.run, game.career, game.atlas);
  game.phase = Game::Phase::NoRun;
  bindCalling = run().calling;
  bindPact = run().pact;
  bindChallenge = run().challenge;
  // A week lost on a Pact: the Trial that teaches what it took away is offered, if it's open.
  bindOffer = !run().won && run().challenge < 0 ? Game::trialForPact(run().pact) : -1;
  if (bindOffer >= challengesOpen()) bindOffer = -1;
  if (bindOffer >= 0) bindChallenge = bindOffer;
  save(true);
}

inline void lose() {
  if (game.phase == Game::Phase::Night && inRun()) logNight(Game::NightBroke);
  game.phase = Game::Phase::Lost;
  save(true);
  beep(196, 200);
  beep(147, 400);
  go(Mode::Lost);
}

inline void updateOutOfVisits(const Ctx& c, const Press& p) {
  (void)c;
  Game::Run& r = run();
  const bool candle = Game::canLightCandle(r), loan = Game::canTakeLoan(r);
  const int haste = Game::hasteSlot(r);
  const int rows = 4;
  if (p.down && sel < rows - 1) sel++;
  if (p.up && sel > 0) sel--;
  if (p.ch == 'b') { back = Mode::OutOfVisits; go(Mode::Build); return; }   // read your build before choosing
  if (!p.enter) return;
  if (sel == 2 && haste >= 0) {
    // HASTE, or RECALL and then the HASTE it brings back.
    for (int slot = haste, k = 0; k < 2 && slot >= 0 && r.n.used >= r.n.visits; k++, slot = Game::hasteSlot(r))
      Game::castSpell(r, game.atlas, game.career, slot, nullptr, hereNow(c));
    beep(1175, 60);
    save(true);
    go(Mode::Scanner);
    return;
  }
  if (sel == 0 && candle) {
    Game::lightCandle(r);
    beep(660, 120);
    save(true);
    go(Mode::Scanner);
  } else if (sel == 1 && loan) {
    logNight(Game::NightLoan);
    Game::takeLoan(r);
    game.lastEnd = Game::endNight(r, game.career, c.st.battery, &game.moments.portent, &game.moments.swap, &game.moments.dawn);
    dawnToken();
    noteCollectorLine();
    game.phase = Game::Phase::NightEnd;
    save(true);
    go(Mode::NightEnd);
  } else if (sel == 3) {
    lose();
  } else {
    beep(180, 80);
  }
  (void)haste;
}

inline void drawOutOfVisits(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  char b[48], n1[16], n2[16];
  UI::textCenter(g, 6, "OUT OF VISITS", UI::Rec, 2);
  snprintf(b, sizeof(b), "%dc", (int)r.coin);   // what the candles come out of
  UI::textRight(g, UI::W - 3, 3, b, UI::Text);
  const double tithe = Game::effectiveTithe(r);
  snprintf(b, sizeof(b), "%s OF %s", UI::num(r.n.total, n1, sizeof(n1)), UI::num(tithe, n2, sizeof(n2)));
  UI::textCenter(g, 26, b, UI::Text);
  UI::bar(g, 20, 38, UI::W - 40, 6, r.n.total / tithe, UI::Accent);
  const bool candle = Game::canLightCandle(r), loan = Game::canTakeLoan(r), haste = Game::hasteSlot(r) >= 0;
  snprintf(b, sizeof(b), "LIGHT A CANDLE: ONE MORE VISIT, %dc", Game::candlePrice(r));
  // The spell it uses: HASTE, or RECALL bringing back the HASTE cast before.
  const bool recall = haste && r.spells[Game::hasteSlot(r)] == Cat::Spell_recall;
  const char* rows[4] = {b, "TAKE THE SYNDICATE'S LOAN", recall ? "CAST RECALL: HASTE, TWO MORE VISITS" : "CAST HASTE: TWO MORE VISITS",
                         "LET THE BINDING BREAK"};
  const bool ok[4] = {candle, loan, haste, true};
  for (int i = 0; i < 4; i++) {
    const int y = 52 + i * 12;
    if (i == sel) g.fillRect(0, y - 2, UI::W, 12, UI::SelBg);
    UI::text(g, 8, y, rows[i], ok[i] ? (i == sel ? UI::SelText : UI::Text) : UI::Faint);
  }
  const char* why = "";
  char w[64];
  if (sel == 0 && !candle) why = r.n.candles >= 3 ? "NO MORE CANDLES TONIGHT" : "NOT ENOUGH COIN";
  if (sel == 0 && candle) {
    snprintf(w, sizeof(w), "%d OF 3 LEFT TONIGHT, EACH DEARER", 3 - r.n.candles);
    why = w;
  }
  if (sel == 1 && loan) {   // what it would cost: the gap, plus interest, on the next Tithe
    const double gap = tithe - r.n.total;
    const double owe = (gap > 0 ? gap : 0) * (Game::hasTrinket(r, Cat::Trinket_moneylender) ? 1.1 : Game::LoanInterest);
    char n3[16];
    snprintf(w, sizeof(w), "+%s ON TOMORROW'S TITHE. THEY KEEP TONIGHT'S OFFERING COIN", UI::num(owe, n3, sizeof(n3)));
    why = w;
  } else if (sel == 1) {
    why = Game::pactOn(r, Game::PR_obsidian) ? "THE OBSIDIAN PACT: NO LOANS"
          : r.night >= Game::Nights ? "NO LOANS OVERNIGHT"
          : r.night >= Game::Nights - 1 ? "NO LOANS ON THE SEVENTH NIGHT"
          : r.loansLeft == 0 ? "NO LOANS LEFT THIS WEEK" : "THEY ONLY LEND FROM 60% OF THE TITHE";
  }
  if (sel == 2) why = recall ? "YOUR RECALL SPELL BRINGS BACK HASTE" : haste ? "YOUR HASTE SPELL" : "NO HASTE SPELL";
  if (sel == 3) why = "YOUR UNLOCKS STAY. ONLY THIS WEEK IS LOST.";
  UI::wrap(g, 8, 102, UI::W - 16, why, UI::Dim, 2);
  UI::footerLine(g);
  const int x = UI::keyHint(g, 2, "ENTER", "CHOOSE", UI::Text);
  UI::keyHint(g, x, "B", "YOUR BUILD", UI::Text);
}

// ---- Won and lost -------------------------------------------------------------------

inline void finishAndReveal() {
  endBinding();
  go(Mode::Reveal);
}

inline void updateWon(const Ctx& c, const Press& p) {
  (void)c;
  if (p.down || p.up) sel = 1 - sel;
  if (!p.enter) return;
  if (sel == 1) {   // Overnight: the shop, then Tithes that double
    run().overnight = 1;
    openStall();
  } else {
    finishAndReveal();
  }
}

inline void drawWon(Canvas& g, const Ctx& c, float t) {
  (void)c;
  g.fillSprite(UI::Bg);
  for (int k = 0; k < 3; k++) {
    const float q = fmodf(t * 0.4f + k / 3.0f, 1.0f);
    g.drawCircle(UI::W / 2, 40, 6 + (int)(q * 60), q < 0.5f ? UI::Sep : UI::Faint);
  }
  UI::textCenter(g, 30, "THE BINDING HOLDS", UI::SelText, 2);
  UI::textCenter(g, 54, "SEVEN TITHES PAID", UI::Text);
  const char* rows[2] = {"END THE WEEK HERE", "GO OVERNIGHT (TITHES DOUBLE)"};   // the safe choice first, under the cursor
  for (int i = 0; i < 2; i++) {
    const int y = 76 + i * 14;
    if (i == sel) g.fillRect(0, y - 2, UI::W, 12, UI::SelBg);
    UI::text(g, 8, y, rows[i], i == sel ? UI::SelText : UI::Text);
  }
  UI::footerLine(g);
  UI::keyHint(g, 2, "ENTER", "CHOOSE", UI::Text);
}

inline void updateLost(const Ctx& c, const Press& p) {
  (void)c;
  if (p.enter) finishAndReveal();
}

inline void drawLost(Canvas& g, const Ctx& c) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Run& r = run();
  char b[48], n1[16], n2[16];
  UI::textCenter(g, 14, r.won ? "THE NIGHT RUNS OUT" : "THE BINDING BREAKS", UI::Rec, 2);
  if (r.night < Game::Nights) snprintf(b, sizeof(b), "ON NIGHT %d", r.night + 1);
  else snprintf(b, sizeof(b), "AFTER %d NIGHTS OVERNIGHT", r.overnightNights);
  UI::textCenter(g, 36, b, UI::Text);
  snprintf(b, sizeof(b), "%s OF %s", UI::num(r.n.total, n1, sizeof(n1)), UI::num(Game::effectiveTithe(r), n2, sizeof(n2)));
  UI::textCenter(g, 50, b, UI::Dim);
  if (r.won) UI::textCenter(g, 64, "THE WEEK STILL COUNTS AS HELD", UI::Accent);
  UI::textCenter(g, 88, "EVERYTHING YOU'VE UNLOCKED STAYS", UI::Dim);
  UI::footerLine(g);
  UI::keyHint(g, 2, "ENTER", "SEE WHAT IT EARNED", UI::Text);
}

// ---- The Grimoire's reveal ------------------------------------------------------

inline void revealName(const Game::Career::Pending& p, char* b, int cap, const char** text) {
  switch ((Game::Kind)p.kind) {
    case Game::Kind::Relic: upper(Cat::Relics[p.id].name, b, cap); *text = Cat::Relics[p.id].text; break;
    case Game::Kind::Trinket: upper(Cat::Trinkets[p.id].name, b, cap); *text = Cat::Trinkets[p.id].text; break;
    case Game::Kind::Spell: upper(Cat::Spells[p.id].name, b, cap); *text = Cat::Spells[p.id].text; break;
    case Game::Kind::Calling: upper(Cat::Callings[p.id].name, b, cap); *text = Cat::Callings[p.id].text; break;
  }
}

// After a restart on the reveal: back to it, with the pages from before the week.
static_assert(Cat::PageCount <= 15, "Moments::reveal keeps the pages under its top bit");
inline bool resumeReveal() {
  if (!(game.moments.reveal & 0x8000u)) return false;
  pagesBefore = game.moments.reveal & 0x7FFFu;
  return true;
}

inline void updateReveal(const Ctx& c, const Press& p) {
  (void)c;
  if (!p.enter) return;
  game.moments.reveal = 0;
  save(true);
  go(Mode::Bind);
  if (bindOffer >= 0 && bindChallenge == bindOffer) sel = 2;   // the offered Trial, under the cursor
}

inline void drawReveal(Canvas& g, const Ctx& c, float t) {
  (void)c;
  g.fillSprite(UI::Bg);
  const Game::Career& car = game.career;
  UI::textCenter(g, 4, "THE GRIMOIRE OPENS", UI::SelText);
  g.drawFastHLine(0, 14, UI::W, UI::Sep);
  int y = 20;
  char b[40];
  static const char* const kinds[] = {"RELIC", "TRINKET", "SPELL", "CALLING"};
  for (int i = 0; i < car.nRevealed; i++) {
    const char* text = "";
    revealName(car.revealed[i], b, sizeof(b), &text);
    const float a = t - i * 0.6f;
    if (a < 0) break;
    UI::text(g, 6, y, kinds[car.revealed[i].kind], UI::Dim);
    UI::text(g, 60, y, b, UI::Accent);
    y += 10;
    y += 10 * UI::wrap(g, 60, y, UI::W - 66, text, UI::Text, 2) + 4;
  }
  const uint16_t fresh = car.pages & ~pagesBefore;
  int left = 0;   // new pages still to list
  for (int i = 0; i < Cat::PageCount; i++) left += (fresh >> i) & 1;
  const int last = car.nQueue > 0 ? 100 : 112;   // the lowest row a line can take
  for (int i = 0; i < Cat::PageCount && left > 0; i++) {
    if (!((fresh >> i) & 1)) continue;
    if (y > last) break;
    if (left > 1 && y + 12 > last) {   // no room for them all: say how many more
      snprintf(b, sizeof(b), "AND %d MORE PAGES IN THE GRIMOIRE", left);
      UI::text(g, 6, y, b, UI::Dim);
      break;
    }
    left--;
    upper(Cat::Pages[i].title, b, sizeof(b));
    UI::text(g, 6, y, "PAGE", UI::Dim);
    UI::text(g, 60, y, b, UI::EnergyColor[3]);
    y += 12;
  }
  if (car.nRevealed == 0 && !fresh) UI::textCenter(g, 50, "NOTHING NEW THIS WEEK", UI::Dim);
  if (car.nQueue > 0) {   // earned already; the Grimoire opens a few a week
    snprintf(b, sizeof(b), "%d MORE EARNED. %d OPEN EACH WEEK.", car.nQueue, Game::UnlocksPerRun);
    UI::textCenter(g, 112, b, UI::Dim);
  }
  UI::footerLine(g);
  UI::keyHint(g, 2, "ENTER", "BIND AGAIN", UI::Text);
}

} // namespace App
