# The Tithe: balance report

How the numbers in PROGRESSION.md, UPGRADES.md and UNLOCKS.md were tuned, what was checked, what
changed because of it, and what's still a guess.

## How it was tested

`tools/balance_sim.py` plays the whole game: every relic, trinket, spell, glyph, edition, Deck mod,
Portent, Omen, Eclipse, Calling and Pact in `tools/catalog.py`, plus candles, loans, Mercy, Polish and
Overnight. A bot plays each run and makes the choices a person would:

- **Which well to visit** from the ones the scanner shows. That's 1 or 2 choices in a Hollow, 2 in
  the Fringe, and 3 in busier wards.
- **What to buy, swap, Polish or reroll** in the shop.
- **When to cast a spell or light a candle.**

Four kinds of players walk the world: city, suburb, rural and commuter, each with their own mix of
busy and empty places and their own routines. Three skill levels set how well the bot judges its
options.

Each check below plays thousands of runs. Results move by about ±1.5 points between batches of
2,000, so differences smaller than that are noise.

## Targets and results

Base game: an average player, the Wayfarer Calling, the Chalk Pact, the starting pool. 3,000 runs.

| Target | Result |
|---|---|
| About 50% break the game open, 40% scrape by, 10% fail | **54% / 39% / 7%** |
| Night 1 barely passable | Needs a median of **10 of 12 visits**, and **0%** fail |
| Few late losses (losing on Night 7 wastes a week) | **1.7%** of runs fail on Night 7. Most failures are on Nights 3–6. |
| Every kind of player within a few points | Fail: city 6%, suburb 9%, rural 9%, commuter 5%. Break open: 49–63%. |
| No Omen is a run-killer | The worst Omen fails 1.6% of the Nights it appears. The worst Eclipse fails 2.5%. |
| Novices still win most runs | Novice: **25%** fail and 25% break open. Expert: **5%** fail and 58% break open. |
| Each Pact harder than the last | Average-player win rate: 90, 64, 57, 46, 39, 38, 30, 21% (300 runs; the second rung is Lead, where the Tithe pays no coin, and the simulator's players never take favors, the income it leaves). Gold is still won by about a third of experts. |
| Something new to unlock for weeks | An unlock in **every run** through run 25. 90% of the catalog by run 28, with legendaries and 96 seals beyond that. |

"Breaks the game open" means the build reaches 3× the Tithe on some Night from Night 3 on, and stays
at 2× or more through Night 7, Eclipse included. "Scrapes by" is every other win.

### The ceiling

How many times over a build could pay each Night's Tithe, for the median build and for the
strongest builds:

| Night | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| Median build | 1.27 | 2.65 | 3.02 | 2.70 | 2.65 | 2.61 | 2.63 |
| Strongest 10% | 1.37 | 5.29 | 5.49 | 4.91 | 5.07 | 5.32 | 5.53 |
| Strongest 1% | 1.45 | 8.18 | 8.80 | 8.28 | 9.54 | 11.55 | 11.73 |

The Tithe doubles about every Night, so a flat line here is already exponential growth in real
numbers. A top-10% build makes about 30 times as much Resonance on Night 7 as it did on Night 2. On top of that,
the strongest builds pull further ahead from Night 4 on, and the top 1% climb from 8× to almost 12×.
Overnight is where the ceiling shows: winning builds survive a median of 5 more Nights of doubling
Tithes, and the best survive 8 or more.

## What the simulator changed

Each of these came from a failed check:

| Finding | Change |
|---|---|
| Paying the Tithe in coin made late shops free, since coin ran into the thousands | Split into Resonance, which pays the Tithe, and coin, which stays small and buys things |
| Money relics took relic slots, and runs that bought them early failed more | Gave money pieces their own Trinket slots |
| The Syndicate loan's 1.5× interest caused debt spirals | Cut it to 1.25× |
| "The fade" Omen, a worthless energy, killed runs when it hit your main energy | Changed it to "sells for half" |
| Builds stalled once their 5 slots filled, and coin piled up unspent | Added **Polish**: upgrade a relic you own, in 3 tiers |
| Strong builds stopped pulling ahead late | Rare stock grows each Night. Portents get a third tier. The Gift Portent gives rares from Night 4. |
| Builds with no multipliers died early | From the second shop on, every shop offers a ×Mult relic |
| One unlucky Night could kill a decent build | Added **candles**: buy up to 3 extra visits when you run short |
| The weakest runs had no way back | Added **the Collector's mercy** after a close call. It cut failures from 11% to 6% on the same Tithes. |
| Steep early Tithes killed runs on Night 3, after only 2 shops | Gentler early curve, steeper mid-week |
| Rural players failed 24% of the time with few wells to choose from | Hollow wells pay more, and you get 2 choices in a Hollow when 2+ networks are in range |
| Novices failed over 60% of the time | The scanner marks the best well for your build, and the shop stars an offer and suggests a swap |
| Unlocking everything doubled the failure rate, because the pool got diluted | The shop leans toward schools you own and wards you play in |
| One-energy relics (Mirror glass, Ascetic, Purist) and Blood moon dominated | Trimmed all four |
| Ghost key, Overclock and Witching hour were almost never kept | Reworked all three |
| The Sleeper Eclipse caused most Night 7 losses | Now it halves Polish for the Night |
| Chorus, Night clerk and Collector failed 29–38% of the time, and Hermit was too strong | Reworked their rules |
| Iron and Obsidian Pacts were no harder than the Pact before them | Iron: 11 visits. Obsidian: no loan, double-price candles. Silver: pricier rerolls and Polish. |
| 68% of unlocks came in the first 5 runs, then almost nothing | The Grimoire reveals up to 2 unlocks per Binding |
| Some unlocks could never happen | Fixed their trackers or conditions |

## The Deck plays by the same rules

The game on the Cardputer (`firmware/src/game/`) is a port of this simulator, and three checks
keep the two in step:

- **Every sale.** `tools/gen_parity.py` scores 3,000 random sales in the simulator (random relic
  sets, editions, Polish, Omens, Eclipses, Callings and world conditions), and the Deck must give
  the same Resonance and coin on all of them (`firmware/test/test_parity`). It catches a 2% change
  to a single relic.
- **Whole weeks.** A bot plays 1,500 weeks on the Deck's own engine in a made-up world shaped like
  the simulator's (`firmware/test/test_autoplay`):

  | | Deck | Simulator |
  |---|---|---|
  | Weeks won, base game | 91% | 88–93% |
  | Build power ÷ Tithe, Nights 1 / 4 / 7 (median) | 1.30 / 2.96 / 2.50 | 1.27 / 2.70 / 2.63 |
  | Strongest 10% on Night 7 | 5.6× | 5.5× |
  | Wins on the Bone / Gold Pact | 55% / 27% | 63% / 29% |

- **Every screen.** A scripted playthrough drives the game app on a computer, saves and reloads
  it at every step (as if the Deck were switched off), and draws every screen.

Building the game for the Deck turned up three things the simulator had assumed:

| Finding | Change |
|---|---|
| The simulator sells your focus energy on half of all visits, but on the Deck you can only sell what you've drawn. Without that, the bot won 44%. | About half of all wells each Night also give your focus energy (65% with a Lodestone). |
| The shop's hint star valued Overclock's ×1.5 without its 2 lost visits, and Blood moon without its dearer Tithes | The star weighs Resonance per sale × visits per Night ÷ Tithe size |
| Hermit's "revisits are 25% more common" can't happen in a real city | Hermit draws twice a Night at wells you've visited before |

The Challenges were never simulated, so they were tuned on the Deck's engine (the bot, everything
unlocked, 300 weeks each). They now range from 21% (Empty pockets) to 79% (The long way).
Homebody and Empty pockets couldn't be won at all at first, Redline and Glass house almost
always could, and their rules changed (UNLOCKS.md has the new texts).

## Callings, with everything unlocked

400 runs each.

| Calling | Break open | Scrape by | Fail |
|---|---|---|---|
| Wayfarer | 58% | 31% | 11% |
| Hedge witch | 62% | 29% | 8% |
| Hermit | 76% | 18% | 5% |
| Courier | 68% | 27% | 5% |
| Diviner | 69% | 25% | 6% |
| Night clerk | 58% | 31% | 11% |
| Chorus | 62% | 26% | 13% |
| Cartomancer | 72% | 23% | 5% |
| Pilgrim | 68% | 24% | 8% |
| Magpie | 57% | 33% | 10% |
| Contractor | 73% | 20% | 6% |
| Glitch (the hard one) | 62% | 21% | 18% |

## Growing relics and the Iron jump (Nov 2026)

The goal: Chalk and Lead stay easy enough for a new player, and from Iron the habits that won them stop
being enough, so the next rung asks for smarter relic choices and a different way of playing energies,
not luck or a busy street. Balatro is the model: on its high stakes a run is won by scaling jokers bought
before they pay, and its shop offers about 12% xMult jokers.

What changed: five new growing relics (Pedometer, Mixtape, Rain gauge, Prism jar, Seed vault), every
growing relic grows 3x what it did, the shelf lost its always-a-multiplier slot and leans toward growing
relics (about 25% of shelf relics now multiply, from 49%), Iron makes Nights 5, 6 and 7 ask x1.25, x1.5
and x1.75, Ash makes the same energy twice in a row pay 30% less, and Overnight accelerates (x2, x2.5,
x3...) so a growing build still ends.

Measured with the habit bot (firmware/test/test_depth, the real engine, everything unlocked, 500 weeks a
cell). "Follows the star" buys what the stall's star marks, and the star rates a growing relic at what
it adds today, which is nothing. "Buys growers early" values a growing relic for what it will be by
Night 7.

| Habit | Chalk | Lead | Iron | Ash | Bone | Silver | Obsidian | Gold |
|---|---|---|---|---|---|---|---|---|
| Follows the star | 94 | 83 | 66 | 55 | 46 | 42 | 39 | 28 |
| Buys growers early | 99 | 93 | 87 | 81 | 72 | 72 | 66 | 54 |
| Focus only (one energy) | 94 | 86 | 70 | 16 | 12 | 10 | 3 | 3 |
| Before (old shelf and growth), follows the star | 95 | 82 | 61 | 50 | 39 | 37 | 32 | 21 |
| Before, buys growers early | 95 | 81 | 61 | 54 | 40 | 38 | 33 | 22 |

Before, buying growing relics early was worth nothing; now it is worth about 20 points from Iron up.
Focus-only play wins Iron about as often as the star does and then falls to 16% at Ash.

What didn't work: coin relics (a Tip jar paying 2 coin a dawn, a Paper route that pays one more each
dawn) changed nothing, because the bot finishes winning weeks with about 45 coin; coin isn't scarce
enough for an economy habit to matter. Rerolling more didn't help either, for the same reason.

balance_sim.py mirrors every rule, but its players all value growing relics ahead (GROWTH), novices
included, so its Pact table is kinder from Iron up (novice 95, 90, 88, 87 on Chalk to Ash). The habit
bot is the measure for this change.

## Still open

- **One-energy builds still lead.** 57% of broken-open builds lean on the Elemental school. Part
  of that is counting: nearly every build owns a Caller early. The top pairs are still Ascetic,
  Purist and Mirror glass. A next pass could make focused play cost something, such as Mirror
  glass giving ×0.75 when the streak breaks.
- **A few relics are rarely kept,** in under 2% of runs: Hollow saint, Hermit's lamp, Lantern-bearer,
  Static eater, Smuggler's lining, Wanted poster, Doppel, Ghost key and Witching hour. The Hollow
  ones are meant for rural players, who are 10% of the simulated players. The Static ones need a
  stronger Static build.
- **Hermit, Cartomancer and Contractor run a little easy** (72–76% break open). Glitch is hard on purpose.
- **The Diviner sees each Omen a Night early on the Deck,** as its text says, while the simulator
  models it as dodging 30% of Omens. Worth a check once people play it.
- **Experts beat average players by less than they would in real life.** The bot judges well,
  but it doesn't plan a build several shops ahead the way a strong player does.
- **Faction stalls** are simulated as an extra random relic, not one from that faction's schools.
- **Settling isn't simulated.** A used well settles after 20 minutes of play and can be used again the
  same Night (x0.75 Thirst in a Quarter or Nexus). The simulator still treats wells as once a Night,
  so sparse players do a little better on the Deck than in the simulator.
- **Factions widened for cities (0.2.3):** after 117 minutes of city walks found 2% open networks
  and under 1% enterprise, a router with a guest-named network (guest, invitados, gratis, free wifi...)
  now counts as Hedge, and a router running 3+ networks counts as Syndicate. Re-scored, the old walks
  give Syndicate 2-9% by ward. Walk log version 2 records the guest flag, so the Hedge share can be
  measured from new walks.
- **First walk logs (a city centre, 23 minutes, untagged, 64% Nexus):** 92% of networks are WPA2 homes;
  open networks 1.6% (simulator guessed 15%), enterprise 0.2%, hidden 4% (guessed 10%). Run on these
  numbers, the simulator gets easier (novices fail 12% of weeks, not 21%). Too thin to retune yet.
- **Everything rests on guesses about the real world:** how many networks are around a typical
  home, street, café and office, how often people revisit wells, and how far apart wells are.
  The scanner firmware (`firmware/`) records calibration walks, `tools/walk_report.py` compares
  them with these guesses, and `WORLD=world.json` plays the simulator on the measured numbers.
  If the outcome mix moves, refit the curve with `tools/tune_tithes.py`.

## Running it

```bash
python3 tools/balance_sim.py run 3000
```

The other reports are `relics`, `callings`, `pacts` and `career`. `tools/try_curves.py` compares
Tithe curves, and `tools/tune_tithes.py` fits a curve to target ratios. After changing
`catalog.py`, refresh the doc tables:

```bash
python3 tools/build_docs.py
```
