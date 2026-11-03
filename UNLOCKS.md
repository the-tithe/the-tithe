# The Tithe: unlocks

How the game keeps opening up over weeks of play. Tables between the `BEGIN` and `END` markers
are built from `tools/catalog.py`.

## How unlocking works

- **You start with a full game.** Most relics in the starting pool are commons and uncommons, plus
  a few rares. Every school has something in it, and every kind of place has a build that works.
- **Unlocks come from doing things.** Selling Hush 30 times opens the Hush-caller. Finding wells on
  all 13 channels opens Channel surfer. Each unlock teaches you something about the game, or about
  the city around you.
- **Many unlocks are about real places.** Visit a Nexus ward. Offer with 40 networks in range.
  Find 10 hidden networks. The unlock list doubles as a list of places to go.
- **Progress always counts.** A lost run still counts toward every unlock. Losing never takes
  away anything you've earned.
- **Unlocks are shared** across every Calling.
- **Unlocks add choices, not power.** A new relic joins the shop pool. It doesn't make old relics
  weaker, and the simulator checks that win rates stay steady as the pool grows.
- **Restart on the spot.** When a run fails, `BIND AGAIN` starts a new one right where you
  stand, with the same Calling and Pact. Night 1 starts with the wells around you.

- **Up to 2 a Binding.** The Grimoire reveals at most 2 new things at the end of each Binding, and
  anything else you've earned waits for the next one. In the simulator that gives a typical
  player something new in every run for the first 25 runs, and about 90% of the catalog by run 28.
  For someone who plays one Binding a week, that's about half a year of new things. The
  legendaries, 96 Pact seals, 10 Challenges and 12 Grimoire pages go on after that.

## Relic, trinket and spell unlocks

The unlock for every relic is in its table in UPGRADES.md. Grouped by kind:

| Kind of unlock | Examples | What it teaches |
|---|---|---|
| Go somewhere | Visit a Nexus ward. Offer with 40+ networks. Visit 10 Hollows in one week. | Places change the game |
| Find things | 25 new wells in a run. 10 hidden networks. All 13 channels. 100 different wells. | Look around more |
| Offer things | Hush 30 times. Static 50 times. 30 Hearth wells. | Each energy and faction has its own builds |
| Play a way | Pay a Tithe in one visit. Change wards 8 times in a Night. Carve 10 glyphs. | Try a new style |
| Use the Deck | Dowse 30 times. Pay a Tithe under 10% battery. | The hardware is part of the game |
| Win with it | Win holding Prism wheel. Win holding 3 relics of one school. | Mastery opens the next step |
| Money | Hold 40 coin. Leave a stall with 0. Take and repay a loan. | Coin is a tool, not only a score |

## Callings

A Calling is how a week starts. Each one changes a rule, so the same city plays differently.

<!-- BEGIN callings -->
| Calling | Rule | Unlock |
|---|---|---|
| **Wayfarer** | No twist. The Deck as it comes. | Start |
| **Hedge witch** | Starts with Static eater and Static-caller. Wardens never appear. | Start |
| **Hermit** | Starts with Hearthstone. A well you offered at on an earlier Night can be drawn twice. | Start |
| **Courier** | Units ×1.4 on every draw, but only 10 visits per Night | Hold a week |
| **Diviner** | Starts with Lodestone and True north. You see each Omen two Nights early. | Dowse 30 times |
| **Night clerk** | Interest cap doubled, but rerolls cost 2 more | Hold 30 coin at once |
| **Chorus** | +1 relic slot, but relics cost 1 more | Own 4 rare relics in one week |
| **Cartomancer** | Starts with two CHALK spells. Glyphs are 50% stronger. | Carve 10 glyphs |
| **Pilgrim** | Only your focus energy exists for you, but you have 4 relic slots | Hold a week with Ascetic |
| **Magpie** | Shines are three times as common, but relics cost 1 more | Own 3 relics with shines |
| **Contractor** | Starts with 20 coin and a Syndicate loan (Night 2's Tithe +20%) | Pay off a Syndicate loan |
| **Glitch** | The hard Calling. Energy types reshuffle every Night. Starts with two random rare relics. | Hold weeks with 3 different Callings |
<!-- END callings -->

## Pacts and seals

Pacts are optional difficulty, purely for bragging rights. Each one adds its rule on top of the ones
before it. Winning a Calling on a Pact earns that Calling a **seal** in the Pact's material, and
opens the next Pact for that Calling. There are 12 Callings × 8 Pacts, so 96 seals in all.

<!-- BEGIN pacts -->
| Pact | Adds |
|---|---|
| **Chalk** | Nothing added. The Deck as it comes. |
| **Lead** | The Tithe pays no coin at dawn. Live on trade, favors and spare visits. |
| **Iron** | Nights 5, 6 and 7 ask ×1.25, ×1.5, ×1.75. Buy what grows before you need it. |
| **Ash** | The same energy twice in a row pays 30% less. Carry more than one. |
| **Bone** | Relics don't all last: some on the stall are Withered and crumble after 3 Nights. |
| **Silver** | Rerolls start at 8 coin, and Polish costs twice as much. |
| **Obsidian** | No Syndicate loan, and candles cost twice as much. |
| **Gold** | One fewer relic slot. |
<!-- END pacts -->

## Challenges

Challenges are Bindings with fixed odd rules, made for variety. The first opens with your first
win, then one more with each win after that. Each one awards its own seal.

<!-- BEGIN challenges -->
| Challenge | Rule |
|---|---|
| **Shoestring** | You can never hold more than 15 coin |
| **One note** | Pilgrim rules, and the stall only stocks Callers, Attunements and Polish |
| **Night owl** | 8 visits per Night, but Portents start at 4 visits to spare |
| **Glass house** | Start with Glass crown. It can't be sold or swapped out, and Tithes are x2.5. |
| **Static storm** | Every well is an open network. Heat never cools. |
| **The long way** | Wells you offered at on an earlier Night earn nothing |
| **Homebody** | From Night 2, you can only offer where you offered on an earlier Night, but those wells refill all Night |
| **Empty pockets** | No stall. Each Night you pay brings a free relic and an Attunement instead. |
| **Grand tour** | Visit all four kinds of ward each Night, or the Tithe doubles |
| **Redline** | Start with Overclock and Witching hour, and 2 extra visits a Night to run them. Tithes ×2.5. |
<!-- END challenges -->

## The Grimoire

The Grimoire is the Deck's own record. It keeps every relic, spell, Calling and seal you've found,
with the ones you haven't found shown as unnamed shapes. It also holds the **story** of the secret
world, told one page at a time. Each page unlocks a **Deck theme**, a new boot sigil and screen
colors, so long-time players can show how far they've come. No page changes the odds.

<!-- BEGIN grimoire -->
| Page | Unlock | Deck theme |
|---|---|---|
| **The first Deck** | Bind your first week | Ash |
| **What the Tithe feeds** | Pay 10 Tithes | Ember |
| **The Collector** | Buy 25 things from the Collector | Brass |
| **The Hedge** | Reach +80 standing with the Hedge in one week | Moss |
| **The Hearth** | Reach +80 standing with the Hearth in one week | Hearth |
| **The Warded** | Reach +80 standing with the Warded in one week | Seal |
| **The Syndicate** | Pay off a Syndicate loan | Chrome |
| **Wells** | Find 100 different wells | Tide |
| **The Eclipse** | Survive all four Eclipses | Umbra |
| **Overnight** | Survive 5 Overnight Nights in one week | Midnight |
| **The old ones** | SUMMON a legendary relic | Gold leaf |
| **The maker** | Hold a week on the Gold Pact | White |
<!-- END grimoire -->

## Legendaries

The five legendary relics each belong to a school, and each unlocks by winning with two relics of
that school. Once one is unlocked, it can come from the rare SUMMON spell (unlocked after 3 wins) or
from a Conjunction Portent. That makes legendaries the long-term goal for each kind of player.
