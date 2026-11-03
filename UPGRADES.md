# The Tithe: upgrades

Every way to get stronger in a run, what each one is for, and the full catalog. The tables between
the `BEGIN` and `END` markers are built from `tools/catalog.py`, the same file the balance
simulator plays, so the numbers here are the numbers that were tested. Rebuild them with
`python3 tools/build_docs.py`.

## Ten ways to get stronger

| Kind | Holds | Where it comes from | What it's for |
|---|---|---|---|
| **Relics** | 5 slots | The Collector's shop, faction stalls, Portents | Your build. Passive pieces that fire on every offering. |
| **Polish** | 3 tiers per relic | The Collector: 6, 12, then 20 coin | Makes a relic you already own 50% stronger per tier |
| **Editions** | One per relic | Rolled on shop relics, the KINDLE spell | A shine on a relic: coin, Mult, ×Mult, or no slot at all |
| **Trinkets** | 2 slots | The shop, now and then | Money and utility, kept apart from relics so they never crowd out your build |
| **Spells** | 2 slots | The shop, Portents | One-use effects you type on the keyboard |
| **Attunements** | No limit | The shop | Level up one energy for good: +1 Mult on it per level |
| **Glyphs** | No limit | The shop, the CHALK spell, Portents | Carved into a real well you visit. That well stays special for the rest of the run. |
| **Deck mods** | No limit | One in every shop, 10 coin | Rule upgrades for the run, in pairs: a first tier, then a stronger second tier |
| **Portents** | Used right away | Paying a Tithe with visits to spare | Rewards for playing fast: free relics, coin, rerolls, Attunements |
| **Faction stalls** | An extra shop slot | Reaching +30 standing with a faction | One more relic on offer, from that faction's kind of relic |

They pull in different directions on purpose. Relics give the big spikes, and Attunements and Polish
give steady growth. Glyphs reward the places you go back to, and Portents reward speed. Trinkets
make coin work harder.

## The sale

Every sale runs one formula: **Resonance = Units × Price × Mult**.

1. Units and Price come from the well, your hold and the energy's Attunement.
2. Relics that add Units or Price fire first.
3. Then relics that add Mult.
4. Then relics that multiply Mult (×).
5. Then relics that make the sale count again.

Relics fire left to right within each step, and the screen shows each one as it lands. Adding
before multiplying is always better, so the game sorts relics into that order on its own and you
only arrange them within a step. Resonance pays the Tithe. Coin is separate: you earn it from
trading profit, from the Tithe reward, from visits you didn't need, and from interest.

## Relics

Relics come in 15 **schools**. A school is a family of relics that work together and point at
one way to play, often at one kind of real place. Most builds lean on one or two schools.

How to read a relic: **+Mult** relics add, and **×Mult** relics multiply. One ×2 relic does more
than any +Mult relic once your Mult passes 2, and two ×2 relics make ×4. That's why the rare
relics are mostly ×Mult.

<!-- BEGIN relics -->
### Throng

Busy places. Downtown, offices, apartment blocks.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Crowd feeder**<br>*It eats what everyone else ignores.* | Common | +0.1 Amp for every well in range (up to +4) | Start |
| **Rush hour**<br>*Six o'clock, and the currents run hot.* | Common | +3 Amp in a Nexus ward (26+ wells) | Visit a Nexus ward |
| **Lanyard**<br>*Still opens the side door on Fridays.* | Common | +5 Thirst at Syndicate wells (office networks) | Start |
| **Choir of eleven**<br>*They all sing one note. It carries.* | Uncommon | +0.5 Amp for each well on channel 11 in range | Start |
| **Packed house**<br>*Standing room only.* | Uncommon | ×1.5 Amp with 30+ wells in range | Offer with 40+ wells in range |
| **Gridlock**<br>*One long knot, and you hold the end.* | Rare | ×(1 + 0.05 per well in range), up to ×3 | Make a 5,000-Resonance offering in a Nexus ward |

### Hollow

Empty places. Parks, back roads, the edge of town.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Hollow saint**<br>*Patron of the places the signal forgot.* | Common | +4 Amp in Hollow wards (0-3 wells) | Start |
| **Hermit's lamp**<br>*Fewer wells, deeper thirst.* | Common | +3 Thirst with 5 or fewer wells in range | Make 20 offerings in Hollow wards |
| **Picnic blanket**<br>*Spread it wherever nobody else is.* | Common | +3 Units in a Hollow ward (0-3 wells) | Start |
| **Quiet hours**<br>*Listen. Nothing. Good.* | Uncommon | ×2 Amp in a Hollow ward | Start |
| **Lantern-bearer**<br>*Light one. Then another. Then another.* | Uncommon | +1 Amp for each Hollow ward you've visited tonight | Visit 10 Hollow wards in one week |
| **Lost signal**<br>*One voice in the dark is worth a thousand in a crowd.* | Rare | ×3 Amp with 0-2 wells in range | Start |

### Fringe

In-between places. Suburbs, quiet streets, home.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Borderwalker**<br>*Not the city, not the woods. Yours.* | Common | +2.5 Amp with 4-15 wells in range | Start |
| **Porch light**<br>*Someone left it on for you.* | Common | +4 Thirst at Hearth wells (home networks) | Start |
| **Spare key**<br>*Under the mat, where it always is.* | Common | +1 Amp for every 10 standing with the Hearth | Start |
| **Threshold**<br>*Every doorway is a little spell.* | Uncommon | ×1.5 Amp if you've changed wards since your last visit | Change wards 8 times in one Night |
| **Picket fence**<br>*Every house on the street knows you.* | Uncommon | Gains +0.75 Amp for every offering at a Hearth well | Make offerings at 30 Hearth wells |
| **Garden gate**<br>*Never quite shut. Never quite open.* | Uncommon | x1.5 Amp in a Fringe ward, x2 if you just changed wards | Start |

### Wayfaring

Finding wells you've never visited.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Long walk**<br>*The legs remember what the map forgets.* | Common | +0.25 Amp per ward entered tonight | Start |
| **First light**<br>*New wells are always the sweetest.* | Common | +4 Amp at a well you've never offered at | Find 25 new wells in one week |
| **Postcard**<br>*Wish you were here. You were, for a minute.* | Common | +1 Amp for each new well you've found tonight | Find 5 new wells in one Night |
| **Cartographer**<br>*Ink, mostly. And a little blood.* | Uncommon | +1 Thirst for every 6 new wells found this week | Start |
| **Wanderlust**<br>*Somewhere else is always louder.* | Uncommon | ×1.5 Amp at a well you've never offered at | Find 8 new wells in one Night |
| **Pilgrim's staff**<br>*Worn smooth by a hundred doorways.* | Rare | Gains ×0.15 Amp for every new well you find. Never resets. | Find 100 different wells, across all weeks |

### Refrain

Going back to the same wells, day after day.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Hearthstone**<br>*Warm from your hand, every morning.* | Common | +3 Units at a well you offered at on an earlier Night | Start |
| **Commuter's charm**<br>*Same train, same well, same luck.* | Uncommon | ×1.5 Amp at a well you've visited on 3+ Nights (×2 from Night 5) | Start |
| **Well-worn path**<br>*Your footprints are the floor now.* | Uncommon | +1 Amp for each Night you've visited this well | Visit the same well on all 7 Nights |
| **Chalk stub**<br>*Worn to a nub on other people's walls.* | Uncommon | x1.5 Amp at a well you've carved a glyph into | Start |
| **Bus pass**<br>*Punched every morning. Never missed one.* | Uncommon | Offerings count twice at wells you've visited every Night | Hold 2 weeks |
| **Keystone**<br>*Carve it deep enough and the city carves back.* | Rare | Your glyphs are twice as strong | Carve 10 glyphs |
| **Key ring**<br>*A key for every door that ever let you in.* | Rare | At wells you offered at on an earlier Night: x(1 + 0.25 per Night there) | Hold a week with Hearthstone |

### Elemental

One energy, pushed as far as it goes.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Ember-caller**<br>*Blow on your hands. Feel that?* | Common | +4 Thirst on Ember | Start |
| **Tide-caller**<br>*It comes in, it goes out, you skim the middle.* | Common | +4 Thirst on Tide | Start |
| **Gale-caller**<br>*Catch it before it's gone.* | Common | +4 Thirst on Gale | Start |
| **Hush-caller**<br>*Say nothing and it comes to you.* | Common | +4 Thirst on Hush | Offer Hush 30 times |
| **Static-caller**<br>*It bites. That's how you know it's good.* | Common | +4 Thirst on Static | Offer Static 30 times |
| **Resonant fork**<br>*Strike it once. It rings for a week.* | Common | +1 Amp for each Attunement level of the energy you offer | Buy 5 Attunements in one week |
| **Ascetic**<br>*One thing, done perfectly.* | Uncommon | ×2 Amp when your hold carries only one kind of energy | Start |
| **Mirror glass**<br>*Again. Again. Again.* | Uncommon | ×1.5 Amp when you offer the same energy as last time | Start |
| **Purist**<br>*Refined to nothing. Worth a fortune.* | Rare | ×(1 + 0.2 per Attunement level) on the energy you offer | Get one energy to Attunement level 5 |

### Variety

A bit of every energy.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Mixed blessing**<br>*A little of everything, a lot of luck.* | Common | +3 Thirst when your hold carries 3+ kinds of energy | Start |
| **Odd socks**<br>*They match. Just not each other.* | Common | +4 Amp when you offer a different energy than last time | Start |
| **Mixtape**<br>*Side A, side B, side A.* | Common | Gains +3 Amp whenever you switch energy. Never resets. | Start |
| **Prism wheel**<br>*Turn it and the light changes color.* | Uncommon | +1.5 Amp for each kind of energy you've offered tonight | Offer all five energies in one Night |
| **Tasting spoon**<br>*Just a little of each. To be sure.* | Uncommon | x2 Amp on the first offering of each energy tonight | Start |
| **Prism jar**<br>*Five colors under one lid.* | Uncommon | Gains ×0.3 Amp per energy you offer each Night. Never resets. | Start |
| **Spectrum**<br>*All five colors. The world goes white.* | Rare | ×(1 + 0.3 for each kind of energy offered tonight beyond the first) | Hold a week with Prism wheel |
| **Charm bracelet**<br>*One charm for every town she left.* | Rare | x(1 + 0.4 per kind of energy in your hold beyond the first) | Hold a week with Mixed blessing |

### Smuggling

Static, Heat and the Wardens.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Static eater**<br>*It hums when it's hungry.* | Common | Static turns up more often and gets +5 Thirst. It never leaks or raises Heat. | Start |
| **Smuggler's lining**<br>*The coat is bigger on the inside. Don't ask.* | Uncommon | +5 Units on Static, and Static turns up more often | Offer Static 25 times |
| **Devil's meter**<br>*The needle only moves one way.* | Uncommon | ×(1 + Heat) Amp. Wardens show up more often. | Start |
| **Film canister**<br>*Nothing in here, officer. Just film.* | Uncommon | x2 Amp on Static while Heat is under 1 | Hold a week with Static eater |
| **Black ledger**<br>*Each line a debt, each debt a promise.* | Rare | Static turns up more often. Gains ×0.6 Amp per Static offering. Never resets. | Offer Static 50 times |
| **Wanted poster**<br>*Your face, but younger. They used an old photo.* | Rare | ×3 Amp while Heat is 3 or more, and Static turns up more often | Offer Static 75 times |

### Hidden

Hush and hidden networks.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Hushed step**<br>*Walk like you're not there.* | Common | +3 Amp at a Hush well or when offering Hush | Find 10 Hush wells |
| **Thermos**<br>*Keeps cold things cold, and quiet things quiet.* | Common | +4 Units on Hush | Pay 5 Tithes |
| **Ghost key**<br>*It opens doors that don't have names.* | Uncommon | +5 Amp at Hush wells, which count as all five energies | Start |
| **Hush lantern**<br>*Its light makes no sound.* | Rare | ×2 Amp on Hush, and Hush turns up three times as often | Start |
| **Unseen hand**<br>*Nobody saw. Nobody ever sees.* | Rare | ×2.5 Amp at a Hush well | Find 30 Hush wells |
| **Locked diary**<br>*The lock is tiny. That's not the point.* | Rare | Gains x0.45 Amp at every Hush well. Never resets. | Hold a week with Hush lantern |

### Machine

The physics of WiFi: mesh, channels, signal strength.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Close call**<br>*Close enough to feel it on your teeth.* | Common | +3 Amp when the well's signal is very strong (stand right next to it) | Start |
| **Rain gauge**<br>*It fills whether anyone checks it or not.* | Common | Gains +1.5 Thirst every time you draw. Never resets. | Start |
| **Leyline**<br>*The old roads still run under the new ones.* | Uncommon | Units and Thirst ×1.2 | Start |
| **Channel surfer**<br>*Off the beaten band.* | Uncommon | ×1.75 Amp at a well on an odd channel (not 1, 6 or 11) | Find wells on all 13 channels |
| **Overclock**<br>*Run it hot. Apologize later.* | Uncommon | ×1.5 Amp on every offering, but 2 fewer visits per Night | Start |
| **Twin wells**<br>*Same water, many buckets.* | Rare | At a mesh WiFi group, the offering repeats once for each extra router | Start |

### Tempo

When in the Night you offer, and how long the week has gone on.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **First bell**<br>*Whoever rings it first gets heard.* | Common | +8 Amp on the first visit of each Night | Pay a Tithe in a single visit |
| **Tally stick**<br>*A notch a night. It's getting crowded.* | Common | Gains +3 Amp every time you pay a Tithe. Never resets. | Start |
| **Pedometer**<br>*It counts the walk, not the reason.* | Common | Gains +0.9 Amp for every offering you make. Never resets. | Start |
| **Night shift**<br>*The later it gets, the better you get.* | Uncommon | +0.5 Amp for each visit already used tonight | Start |
| **Witching hour**<br>*Midnight is a door. Walk out rich.* | Uncommon | On the visit that closes the Night, ×3 Amp | Start |
| **Ritual candle**<br>*It burns longer each night. Don't ask.* | Uncommon | Gains ×0.45 Amp every time you pay a Tithe. Never resets. | Pay 20 Tithes |
| **Patience**<br>*The Hum pays those who don't need it.* | Rare | Gains +3 Amp per visit left unused when a Night closes. Never resets. | Pay 3 Tithes in a row with 6+ visits to spare |

### Echo

Offerings that count twice, and relics that copy relics.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Doppel**<br>*Two of you at the well. One pays.* | Uncommon | The first offering of each Night counts twice | Make 150 offerings |
| **Tote bag**<br>*Holds more than it should. Says so on the side.* | Uncommon | Offerings of 12 or more Units count twice | Offer 12 units at once |
| **Echo chamber**<br>*Say it once, hear it twice, get paid for both.* | Rare | Every third offering counts twice | Start |
| **Copycat sigil**<br>*It doesn't have ideas. It has yours.* | Rare | Copies your rarest other relic | Start |
| **Overtone**<br>*Hit the right weight. The Deck rings.* | Rare | On an offering of 10+ units, every relic fires twice | Offer 15 units at once |
| **Seashell**<br>*Hold it to your ear. Someone else's sea.* | Rare | Counts twice if you've changed wards since your last visit | Make a 10,000-Resonance offering in a Nexus ward |

### Deck

The Cardputer's own sensors: microphone, gyro, battery.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Din**<br>*The Deck likes a noisy room.* | Common | +0 to +4 Amp, rising with how loud it is around you (microphone) | Start |
| **Steady hand**<br>*Breathe out. Now.* | Common | +3 Amp if you hold the Deck still while you offer (ADV only) | Start |
| **Earplugs**<br>*Everything goes quiet except the part you want.* | Common | +0 to +4 Amp, rising the quieter it is around you (microphone) | Start |
| **Dowser's knot**<br>*The string pulls. Follow it.* | Uncommon | ×1.5 Amp at a well you found by dowsing | Dowse 30 times |
| **Last ember**<br>*Spend it all. That's what it's for.* | Uncommon | ×2.5 Amp while the Deck's battery is under 20% | Pay a Tithe with the battery under 10% |
| **Spirit level**<br>*Wait for the bubble to stop.* | Uncommon | x1.5 Amp if you hold the Deck still at a very strong signal (ADV) | Start |

### Tokens

Coin turned into Resonance.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Laundromat token**<br>*Good for one load. Never once used for laundry.* | Common | +1 Amp for every 5 coin you hold (up to +4) | Start |
| **Bus fare**<br>*Exact change, and not a token more.* | Common | +5 Amp while you hold 4 coin or less | Leave a stall with 0 coin |
| **Hoard**<br>*It isn't about spending it. It never was.* | Uncommon | ×(1 + 0.02 per coin held), up to ×2 | Hold 40 coin at once |
| **Piggy bank**<br>*Shake it. It sounds like next week.* | Uncommon | Gains +1.5 Amp per coin of interest you earn. Never resets. | Hold 25 coin at once |
| **Seed vault**<br>*Nothing grows in there while you keep opening it.* | Uncommon | Gains ×0.3 Amp per coin of interest you earn. Never resets. | Start |
| **Long receipt**<br>*Itemized, folded twice, kept for no reason.* | Rare | x(1 + 0.05 per coin spent at the last stall, minus sales), up to x3 | Buy 3 rare relics |

### Cursed

Big upside, with a catch.

| Relic | Rarity | Effect | Unlock |
|---|---|---|---|
| **Hungry coin**<br>*Feed it and it feeds you. Mostly.* | Uncommon | +6 Amp, but it eats 2 coin every visit | Leave a stall with 0 coin |
| **One-way ticket**<br>*Nobody buys these by accident.* | Uncommon | x2.5 Amp, but wells you offered at on an earlier Night earn nothing | Find 40 new wells in one week |
| **Glass crown**<br>*Heavy is the head, until it shatters.* | Rare | ×3 Amp on every offering, but one energy is worthless each Night | Hold a week |
| **Blood moon**<br>*The Hum notices when you get greedy. It gets greedy back.* | Rare | ×3 Amp, but every Tithe is 35% higher | Hold a week on the Iron Pact |
| **Empty frame**<br>*Nobody in it yet. Keep it that way.* | Rare | x(1 + 0.5 for each empty relic slot) | Pay 5 Tithes at 3x or more |

### Legendary

Only from the SUMMON spell or a Conjunction Portent.

| Relic | Effect | Unlock |
|---|---|---|
| **The Grid Saint**<br>*She was a switchboard once. Now she is the switchboard.* | ×(1 + 0.1 per well in range) | Hold a week with 2 Throng relics |
| **The Unheard**<br>*...* | Every well counts as hidden. ×2 Amp on Hush. | Hold a week with 2 Hidden relics |
| **Seven-league boots**<br>*One step, one ward. Mind the curb.* | ×0.1 Amp for every new well found this week | Hold a week with 2 Wayfaring relics |
| **The Old Keeper**<br>*He has watched this street for a hundred years. He likes you.* | ×3 Amp at wells you offered at on an earlier Night | Hold a week with 2 Refrain relics |
| **The Midnight Engine**<br>*Tick. Tick. Tick. Tick.* | Every offering counts twice | Hold a week with 2 Tempo relics |
<!-- END relics -->

### Builds the schools make

| Build | Relics it wants | Where you walk |
|---|---|---|
| Core trader | Crowd feeder, Choir of eleven, Gridlock, Night shift, Witching hour | Downtown, offices, big apartment blocks |
| Hollow wanderer | Hollow saint, Quiet hours, Lost signal, Hermit's lamp | Parks, back roads, the edge of town |
| Porch to porch | Borderwalker, Porch light, Picket fence, Threshold | Suburban streets, around home |
| Pilgrim | First light, Wanderlust, Cartographer, Pilgrim's staff | Somewhere new every day |
| Commuter | Commuter's charm, Hearthstone, Well-worn path, Keystone + glyphs | The same route, squeezed harder each day |
| One energy | A Caller, Ascetic, Mirror glass, Purist, Resonant fork + Attunements | Wherever your energy's channel is busiest |
| Rainbow | Mixed blessing, Prism wheel, Spectrum | Mixed areas with every kind of network |
| Smuggler | Static eater, Smuggler's lining, Black ledger, Wanted poster | Cafés, transit, anywhere with open WiFi |
| Ghost | Hushed step, Ghost key, Hush lantern, Unseen hand | Places with hidden networks |
| Mesh hunter | Twin wells, Overtone, Leyline | Big houses and offices with mesh WiFi |
| Night owl | Night shift, Witching hour, Patience, Tally stick, Ritual candle | Anywhere. It's about timing. |
| Echo | Doppel, Echo chamber, Copycat sigil, Overtone | Anywhere. It doubles whatever else you have. |
| Battery gambler | Last ember, Din, Steady hand | Loud places, with the battery running low |
| Hoarder | Hoard, Miser's purse, Tithe-skimmer | Anywhere, with a fat purse |

## Polish

Polish is how coin keeps turning into power once your relic slots are full. At the Collector you
can Polish any relic you own: **tier I costs 6 coin, tier II costs 12, tier III costs 20**. Each
tier makes the relic's effect 50% bigger. A +4 Mult relic becomes +6, +8, then +10. A ×2 relic
becomes ×2.5, ×3, then ×3.5.

Polish rewards builds that are already working, because a polished ×Mult relic grows the whole
sale, and tier III is priced for builds that are already rich. That's what keeps the best builds
pulling ahead late in the week. Without Polish, the simulator showed the best builds stalling once their
slots filled, with coin piling up unspent. Swapping a relic out
loses its Polish, so a polished relic is a commitment.

## Editions

A shop relic can come with an edition. Editions get more common as the week goes on.

<!-- BEGIN editions -->
| Edition | Effect | Chance on a shop relic |
|---|---|---|
| **Gilt** | +2 coin at the end of each Night | 4.0% |
| **Lit** | +3 Amp | 3.0% |
| **Prismatic** | ×1.5 Amp | 1.5% |
| **Phantom** | Takes no relic slot | 0.5% |
<!-- END editions -->

## Trinkets

Trinkets live in their own 2 slots. Money pieces used to share slots with relics, and runs that
bought them early ran out of room for multipliers and failed more often, which is why they were
split off.

<!-- BEGIN trinkets -->
| Trinket | Effect | Flavor | Unlock |
|---|---|---|---|
| **Miser's purse** | Interest cap +5 | *Count it twice. It grows when you're not looking.* | Start |
| **Haggler** | One free reroll at each stall | *Everything's negotiable.* | Start |
| **Pickpocket** | +1 coin every visit | *Light fingers, heavy pockets.* | Start |
| **Fence** | Offer any energy at any well, ignoring faction bans | *No questions asked. Fine.* | Start |
| **Tithe-skimmer** | +1 coin for every 50% you beat a Tithe by (up to +10) | *The Hum never counts the change.* | Pay 3 Tithes at 3x or more |
| **Moneylender** | You can take two Syndicate loans per week, at 1.1x interest | *Friendly rates, for a friend.* | Take a Syndicate loan |
| **Lucky coin** | Rare relics show up twice as often at the stall | *Heads, you win. Tails, flip again.* | Buy 5 rare relics |
| **Collector's eye** | Shines show up three times as often | *You can tell the real ones by the shine.* | Own 3 relics with shines |
| **Golden thread** | +4 coin when you pay a Tithe with 4+ visits to spare | *Pull it and the night unravels early.* | Pay a Tithe with 6+ visits to spare |
| **Pawn ticket** | Sell relics back for their full price | *Redeemable. Eventually.* | Sell 5 relics back |
| **Chalk pouch** | Start each Night with a CHALK spell if you have a free spell slot | *Dusty fingers, sigils on the walls.* | Carve 5 glyphs |
<!-- END trinkets -->

## Spells

You hold 2 (3 with Fifth pocket) and cast them by typing the word on the Deck's keyboard. You can
also pick them from a menu. You start each run with GLAMOUR.

<!-- BEGIN spells -->
| Spell | Effect | Flavor | Unlock |
|---|---|---|---|
| **GLAMOUR** | This offering counts double | *Make it shine. Just for a moment.* | Start |
| **SURGE** | Your next draw takes 2.5x the units | *Pull. Harder.* | Start |
| **TRANSMUTE** | Hold becomes your focus for 4 offerings | *Lead into gold. Ember into Tide.* | Start |
| **HASTE** | +2 visits tonight | *Run.* | Start |
| **ECHO** | Offer to a visited well from anywhere | *It remembers you.* | Start |
| **HEX** | +8 coin | *A little curse, a big discount.* | Start |
| **VEIL** | Static: no Heat and +3 Thirst tonight | *Nobody saw anything.* | Offer Static 10 times |
| **CHALK** | Carve a random glyph into this well | *Your mark, their wall.* | Start |
| **KINDLE** | Give a random relic a shine | *A little fire in the glass.* | Own a relic with a shine |
| **AUGUR** | Clear tonight's Omen and undo what it took. Not a Wardens walk. | *Read the signs again. Squint.* | See 10 Omens |
| **RECALL** | Get back the last spell you used | *Again, with feeling.* | Cast 20 spells |
| **BARGAIN** | Relics and wares at the next stall are half price | *Closing-down sale. Everything must go.* | Leave a stall with 0 coin |
| **SUMMON** | A legendary relic, into a free slot | *Something answers. Be polite.* | Hold 3 weeks |
<!-- END spells -->

## Attunements

Each Attunement adds a level to one energy. Each level gives **+1 Mult** on every sale of that
energy, forever, for the run. Shops offer Attunements for the energies you've actually been
selling, so a focused build levels its main energy steadily. Five levels on one energy unlocks
the rare relic Purist.

## Glyphs

A glyph is carved into a real well, the router on your street, café or office, and stays there
for the rest of the run. Every return visit to that well gets the glyph's bonus. Glyphs are the
routine player's deck-building. The more often you pass a well, the more it's worth.

<!-- BEGIN glyphs -->
| Glyph | Effect | Flavor |
|---|---|---|
| **Minted** | +3 coin on every offering here | *A gold thumbprint on the router.* |
| **Ringing** | +4 Amp on every offering here | *It hums faintly, if you know how to listen.* |
| **Echoing** | Offerings here count twice | *Knock once and it knocks back.* |
| **Deep** | +5 Units on every offering here | *The well goes further down than it should.* |
| **Bound** | ×1.5 Amp on every offering here | *Tied to you with a thread nobody else can see.* |
| **Beacon** | This well counts as new every time | *It never quite recognizes you.* |
<!-- END glyphs -->

## Deck mods

One mod is on offer in every shop. The second tier only shows up once you own the first.

<!-- BEGIN mods -->
| Mod | Effect | Needs |
|---|---|---|
| **Extra breath** | +1 visit per Night | - |
| **Second wind** | +1 more visit per Night | Extra breath |
| **Satchel** | +2 Units on every draw | - |
| **Bottomless satchel** | +3 more Units on every draw | Satchel |
| **Lodestone** | Your focus energy turns up more often | - |
| **True north** | Dowsing finds a well on a third of your visits | Lodestone |
| **Overstock** | +1 relic at every stall | - |
| **Night market** | +1 spell, glyph or Attunement at every stall | Overstock |
| **Clearance** | Relics and wares at the stall cost 1 less | - |
| **Liquidation** | Rerolls cost 2 less | Clearance |
| **Fifth pocket** | +1 spell slot | - |
| **Sixth socket** | +1 relic slot | Fifth pocket |
| **Charm ward** | Omens only take effect half the time | - |
| **Eclipse ward** | The Eclipse only takes effect half the time | Charm ward |
| **Attuner's fork** | Attunements sometimes give 2 levels | - |
| **Tuning core** | Each Attunement level also gives +1 more Amp | Attuner's fork |
<!-- END mods -->

## Portents

Pay a Tithe with 6 or more visits to spare and you get a Portent, 2 with 9 spare and 3 with 11 spare.
Portents reward a strong build with more strength, which is part of what makes the ceiling climb.
They also save real-world walking: a Night that ends early is a shorter walk.

<!-- BEGIN portents -->
| Portent | Reward |
|---|---|
| **Rare sight** | A sure rare relic at the next stall |
| **Gift** | A free relic (a rare from Night 4) |
| **Windfall** | +10 coin |
| **Doubled interest** | Interest pays double tonight |
| **Free rerolls** | Two free rerolls at the next stall |
| **Shine** | A relic at the next stall comes with a shine |
| **Tuning** | +1 Attunement on your focus energy |
| **Scroll** | A free spell |
| **Chalk mark** | A free glyph to carve into any well |
| **Conjunction** | A free legendary relic (if any open) |
<!-- END portents -->

## Faction stalls

Every sale at a faction's well raises your standing with it. Selling Static lowers your standing
with the Warded and the Syndicate. At +30 standing, that faction opens a stall in the Collector's
shop, an extra relic slot stocked from its own kind of relic:

| Faction | Its networks | Its stall stocks |
|---|---|---|
| The Hedge | Open networks | Smuggling and Variety |
| The Hearth | Home networks | Fringe and Routine |
| The Warded | Newer locked networks (WPA3) | Hidden and Elemental |
| The Syndicate | Company networks | Throng and Machine |

Standing also moves prices at that faction's wells, by up to 20% either way.

## The Collector's shop

| Rule | Value |
|---|---|
| Relics on offer | 3 (4 with Overstock, +1 for each open faction stall) |
| Guarantees | The first shop always has a Caller for the energy you sell most. From the second shop on, at least one relic is a ×Mult relic. |
| Stocked for you | About a third of relic offers lean toward schools you already own, and half lean toward relics that suit the kinds of ward you've been playing in. A growing pool of unlocks never buries the relics your build needs. |
| Rarity | Rares are 6% of relics after Night 1 and climb 3 points a Night, to 21% after Night 6. Uncommons climb 1 point a Night. |
| Spells, Attunements, glyphs | 3 on offer |
| Trinket | One, 40% of the time |
| Deck mod | One |
| Prices | Common relic 4, uncommon 7, rare 10, +3 with an edition. Trinket 5. Spell, Attunement or glyph 3. Deck mod 10. |
| Rerolls | 5 coin, then 1 more each time in the same shop |
| Selling back | Half price (full with Pawn ticket) |
| Interest | +1 coin for every 5 you hold when a Night closes, up to +5 |
| Mercy | After a close call (1 visit or less to spare, or a candle or loan), the next shop adds a half-price ×Mult relic, a free reroll and a free Attunement level |
| Hint star | The Deck marks the offer that fits your build best, and when your slots are full it suggests which relic to swap out. It's a hint, not always right. |

## Rules for adding upgrades

The simulator found these, and new upgrades should follow them:

1. **Never make a must-buy.** If a relic ends up in more than a third of all runs and the runs
   holding it break open 80% of the time or more, it's too general. Give it a condition.
2. **Every relic needs somewhere to shine.** If it's kept in under 2% of runs, it's a trap.
   Loosen its condition or raise its number.
3. **Money pieces don't take relic slots.** That's what Trinkets are for.
4. **Conditions come from real places.** The best relics change where you walk. A relic that
   works the same everywhere is a missed chance.
5. **Every place gets its builds.** A rural player and a downtown player should each find two or
   three strong builds. The simulator checks this for each kind of player.
6. **Unlocks must not dilute the shop.** Before the shop leaned toward your build, unlocking the
   full catalog doubled the failure rate. The game got harder, not richer. New relics need a clear
   home school so the shop can offer them to the right builds.
7. **Spikes come from relics, steady growth comes from Attunements and Polish.** A new steady
   source makes every build stronger by the same amount, which squeezes the gap between good and
   great builds.
