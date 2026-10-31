# The Tithe: the run

How a week of play works and why you chase more. The world is in DESIGN.md, every upgrade is in
UPGRADES.md, the long-term unlocks are in UNLOCKS.md, and how the numbers were tested is in BALANCE.md.
Tables between `BEGIN` and `END` markers come from `tools/catalog.py` and `tools/balance_sim.py`.

## What makes it fun

The model is Balatro. Its pull comes from the process: you find pieces, learn which ones work
together, and every so often hit a combination so strong it breaks the run open. From then on
the game feels easy, and that is the reward. Five rules carry that over:

1. **Many small pieces that interact.** Each relic is simple on its own. The fun is in how they combine.
2. **Multipliers beat flat bonuses.** Adding gives you a slow climb, and multiplying makes
   numbers explode.
3. **Broken combos are allowed.** We tune the floor (a sensible player wins about 9 runs in 10 on
   the base Pact, and breaks the game open in about half) and leave the ceiling alone. When a run goes off, it goes off.
4. **Every sale is a show.** The math plays out on screen piece by piece, with sound.
5. **Difficulty is optional.** The base game is beatable and fun. Harder Pacts exist for bragging
   rights.

## Two currencies

- **Resonance** is what an offering makes: **Units × Thirst × Amp**. It pays the Tithe and can't be spent.
  Relics make it explode.
- **Coin** buys things. You earn it from trading profit, a reward for each Tithe paid, 1 for every
  visit you didn't need, and interest (+1 for every 5 you hold, up to +5). Coin stays small all
  game, so every purchase is a choice.

The first draft paid the Tithe in coin. The simulator showed why that breaks: late in the week
coin runs into the thousands, so every shop item becomes free and interest stops mattering.
Splitting them keeps the shop interesting to the end.

## A run is one week

A **Binding** is 7 Nights. Each Night gives you **12 visits**. A visit is one stop at a well:
gather energy, trade, or both. Nights count visits, not hours, so a busy day costs you nothing,
and a heavy player can finish a Binding in a day or two.

Each Night ends with a **Tithe**, and the Night closes the moment you pay it. Visits you didn't
need turn into coin, and paying with plenty to spare earns **Portents** (rewards, see
UPGRADES.md). A strong build means shorter walks.

<!-- BEGIN tithes -->
| Night | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| Tithe (Resonance) | 410 | 900 | 2,200 | 4,700 | 9,000 | 17,500 | 26,000 |
<!-- END tithes -->

Night 1 is barely passable: a new run with no relics needs about 10 of its 12 visits. The Tithe
roughly doubles each Night after that, and the shop is what keeps you ahead of it.

## A Night, step by step

1. **The Omen.** At the start of the Night the Deck shows tonight's rule (below).
2. **Scan.** The scanner shows the wells around you: their energy, signal strength and faction.
   It marks the one that fits your build best.
3. **Visit.** Walk up to a well until it's in reach. Draw its energy into your hold, or your focus
   energy where the well offers it (about half of wells each Night). Then offer what you carry: one
   offering is one visit, and each well takes one, then settles for 20 minutes. Each offering plays
   out on screen: Units × Thirst × Amp, with every relic firing in turn.
4. **Pay.** When your Resonance for the Night reaches the Tithe, the Night closes. Unused visits
   become coin, and Portents arrive if you paid fast.
5. **The Collector's stall** opens. Buy, Polish, reroll, then head out for the next Night.

## When you're short

There are three safety nets, in order:

| Net | How it works |
|---|---|
| **Spells** | GLAMOUR, SURGE and friends boost one sale. You start every run with GLAMOUR. |
| **Candles** | Out of visits and still short? Light a candle for one more visit: 4, then 6, then 8 coin, up to 3 a Night. |
| **A Syndicate loan** | Once a run, if you reach 60% of the Tithe, the Syndicate pays the rest. 1.25× what they paid is added to your next Tithe. Not on Night 7, and not on the Obsidian Pact. |

If all three fail, the Binding ends.

**The Collector's mercy.** If you only just paid a Tithe (1 visit or less to spare), or needed a
candle or a loan, the next stall helps you catch up. It adds one ×Amp relic at half price, a free
reroll and a free Attunement level on your main energy. Builds that pay fast never trigger it, so
mercy lifts the weakest runs without raising the ceiling. In the simulator, it cut failures from 11% to 6%
on the same Tithes, while runs broke open about as often (58% before, 61% after).

## Losing, and starting again

A failed run costs only that run. Every unlock you've earned stays, and the run still counts toward
the ones in progress. `BIND AGAIN` starts a new Binding on the spot, with the same Calling and Pact,
from wherever you're standing.

## Omens and the Eclipse

Nights 2 to 6 each have an **Omen**, a rule shown at the start of the Night. Night 7 has the
**Eclipse**, a harder rule that's revealed when the week begins, so you can build to beat it.

<!-- BEGIN omens -->
| Omen | Rule |
|---|---|
| **The fade** | One energy is worth half tonight |
| **Low tide** | Units ×0.75 tonight |
| **Short night** | 2 fewer visits tonight |
| **Silent bell** | Your first 2 visits earn nothing |
| **Crowded sky** | Thirst ×0.7 in Quarter and Nexus wards |
| **Empty roads** | Thirst ×0.7 in Hollow and Fringe wards |
| **Still water** | Relics that add Thirst give half as much tonight |
| **Wardens walk** | You start the Night at Heat 3 |
| **Fog** | Relics see half as many wells as there are |
| **Blood price** | This Tithe is 20% higher |
| **Tangled lines** | Relics fire in the wrong order: multiply first, then add |

| Eclipse | Rule |
|---|---|
| **The Sleeper** | Polish counts for half tonight (tier III works as tier I) |
| **The Hunger** | The Tithe is 25% higher |
| **The Dark** | 2 fewer visits tonight |
| **The Dimming** | Every ×Amp effect gives a quarter less (×3 becomes ×2.5) |
<!-- END omens -->

## Heat and the Wardens

Selling Static raises Heat. At Heat 3 or more, Wardens patrol, and each visit has a 15% chance
of being blocked. Heat cools a little with every visit. The Hedge witch Calling never meets Wardens,
and a few Smuggling relics turn Heat into power.

## Overnight

Survive Night 7 and you win. You can then keep playing **Overnight**: every Night doubles the
Tithe, with an Eclipse every third Night. In the simulator, the median winning build lasts about 5
Overnight Nights, and the best builds last 8 or more.

## Callings and Pacts

A **Calling** is how a week starts. It changes one rule, so the same city plays differently.
**Pacts** are optional difficulty for bragging rights. Each Pact adds a rule, and winning on a Pact
earns that Calling a seal. Both are listed in UNLOCKS.md.
