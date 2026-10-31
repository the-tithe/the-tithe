# The Tithe: design

A trading game for the Cardputer ADV. A secret world sits on top of the real one, and the WiFi
networks around you are its energy wells and markets. You walk around in real life to gather
energy where it's plentiful and sell it where it's scarce. It started from Hazke, a space trading
game for the same device, and borrows a few of its parts.

## Decisions so far

- **Device:** Cardputer ADV.
- **Home play:** playable but slow. Home gives you a few trades a day, and the good deals need you to go out.
- **Players:** solo first, built so two Cardputers can trade face to face later.
- **Runs:** one week each, and losing costs only the run. Difficulty levels are optional.
- **Theme:** urban magic in a cyberpunk city. The Cardputer is an object inside the story.

Runs, the Tithe and upgrades are in PROGRESSION.md.

## Theme: the Cardputer is the Deck

The city runs on a hidden current. Most people walk through it without noticing. A few people,
listeners, can hear it, and their tool is **the Deck**: a pocket terminal hacked to hear the
current. The game tells the rest of the story itself, in the Grimoire and the manual. The Cardputer in your hand *is* the Deck, so everything on screen
is the Deck's own software. The game never draws a separate interface around it.

How the real hardware fits the story:

| Cardputer | In the world |
|---|---|
| Turning it on | The Deck boots its own firmware, with a scrolling boot log and a sigil |
| WiFi scan | Listening to the currents |
| Screen | A scrying glass. Wells show as glyphs, with signal strength as a ripple. |
| Keyboard | Commands are typed like short spells (`DRAW`, `SELL TIDE 4`), with the menus as shortcuts |
| Turning the ADV around (gyro) | Dowsing. Turn slowly in a circle and a needle points toward the well. |
| Tilting and holding the ADV | The scrying glass: wells drift as it tips, and stillness clears it so Hush wells surface |
| Flicks and twists | Spells cast by hand: a flick is a pulse, a twist is a ward |
| Laying it face-down, picking it up | The Deck veils itself, and opens its eye when lifted |
| Speaker | The hum of the currents, which rises as you get closer |
| SD card | Your grimoire, which holds the atlas and saves |
| Serial console | The maker's back door (debug tools) |

Dowsing is real: your body blocks part of the signal from behind you, so turning in a circle and
fitting the pattern points at the source. In simulated sweeps with a 6 dB body shadow it lands
within 30 degrees 89–100% of the time. How big the shadow really is needs testing on the Cardputer.

Placeholder names in this theme:

- **Wells** are routers. **Wards** are the areas around you, named Hollow, Fringe, Quarter and Nexus from
  emptiest to busiest.
- **Energies:** Ember, Tide, Gale, Hush and Static, as in the table below.
- **Factions:** the Hedge (open networks, street mages), the Hearth (home networks, local
  covens), the Warded (newer locked networks, a careful new order) and the Syndicate (company
  networks, corporate magi).

## The world

**A well is one WiFi router.** Each router gives off a unique hardware ID. The game scrambles that
ID into a number and builds the well from it: a made-up name, a faction, how much energy it holds,
and what it likes to buy. The same router is always the same well, with the same name on every
Deck, but the game never shows or saves the router's real name or ID. A router that broadcasts
several networks (a home network plus a guest network, say) is one well.

**A ward is everything you can hear from where you stand.** Each scan (about every 2.5 seconds)
makes a new ward from the wells in range, so walking down a street changes the ward under you. How
many wells are around sets the kind of ward:

| Wells in range | Ward | Feel |
|---|---|---|
| 0–3 | Hollow | Little to trade, but rare energy turns up |
| 4–10 | Fringe | Quiet, steady prices |
| 11–25 | Quarter | Busy, plenty of common energy |
| 26+ | Nexus | Floods of common energy, hungry for rare kinds |

**Energy comes in five kinds.** Real WiFi decides which kind a well has (names are placeholders):

| Energy | Comes from | Why |
|---|---|---|
| Ember | Channels 1–4 | Most routers sit on channels 1, 6 or 11, so these three are common, and how crowded a channel is in real life sets the supply |
| Tide | Channels 5–9 | |
| Gale | Channels 10–13 | |
| Hush | Hidden networks, which don't broadcast a name | Rare and valuable |
| Static | Open networks with no password | Black-market energy. Some factions ban it. |

**There are four factions,** set by how a network is locked: open, home password, newer home
password (WPA3), and company login. Your standing with each moves prices by up to 20%.
A router with several networks takes its faction from the most guarded one, so a home router with an
open guest network is still Hearth.

## The economy

As built on the Deck. The numbers come from the balance simulator (BALANCE.md), which treats a
visit as one sale; these rules turn that into walking around.

- **Drawing.** At a well in reach (NEAR or closer on the scanner), you draw its energy into your
  hold: 4 units at the edge of reach, up to 8 right next to it. Hold the Deck still while it
  draws, or up to a quarter spills. Each well can be drawn once, then settles for 20 minutes of play before it can be drawn again.
- **Your focus energy.** Each run leans on one energy (Ember, Tide or Gale). Tonight about half of
  all wells will also give you your focus energy instead of their own, and a Lodestone raises that
  to 65%. Your Caller and Attunements make the focus energy pay best. This is the simulator's
  "focus" rule, made into a place.
- **The hold** carries 8 units (more with satchels). Stored energy leaks one unit every 15
  minutes of play, except Hush, so it pays to keep moving.
- **Selling** everything you carry of one energy is one visit, and each well takes one offering, then settles for 20 minutes. A settled well offered to again the same Night wants x0.75 Thirst in a Quarter or Nexus, full Thirst in a Hollow or Fringe, so a street with few wells can still carry a Night.
  Price = base price (5, or 8 for Static and 9 for Hush) × the well's taste for that energy
  (0.8 to 1.25, fixed per well, the same on every Deck) × the ward (Hollow 1.4, Fringe 1.1,
  Nexus 0.85) × your standing with its faction (±20%). Hollow wards pay more per unit, and busy
  ones give more units, so empty places keep up with crowded ones.
- **Factions.** The Warded refuse Static unless you carry a Fence. Selling raises your standing
  with the well's faction, and Static lowers it with the Warded and the Syndicate. About a third
  of wells offer a small faction job for the Night (sell some Tide at a Hearth well, sell in
  three kinds of ward) that pays coin and standing.
- **Heat and the Wardens.** Static raises Heat. From Heat 3, each sale has a 15% chance of being
  taken. A twist of the wrist casts a ward that keeps the Wardens off for a minute, once a Night.
- **Time:** the game never goes online, so it has no real clock. Nights count visits, and leaks
  run on play time.

## Loop

Open the scanner → the star marks the well where your build makes the most → walk toward it until
it's in reach → draw, then sell → pay the Night's Tithe → shop at the Collector → the next Night.

Over time you build the **atlas**, a log of every well you've visited, which is your private map
of the secret world. Relics care about it: some pay at wells you know, others at new ones.

## Screens

As built. Pictures of each are in firmware/README.md.

| Screen | What it's for |
|---|---|
| Scanner | The wells around you, strongest first, with the hint star and the Night's progress |
| Scrying glass | The same wells floating in a bowl; dowsed wells at their real direction |
| Well | One well's signal over time, for walking toward it; dowsing from here |
| Visit | Draw, sell, cast, carve a glyph, take a job |
| Sale | The sale playing out relic by relic: Units × Price × Mult |
| Night start and end | The Omen and Tithe; the payout and Portents |
| The Collector | Relics, wares, Polish, rerolls and selling back |
| Build, spells, atlas, Grimoire, menu | Your relics and the rest of your build, your spells, wells you've visited, unlocks and lore |
| Bind, won, lost, reveal | Starting a week, its end, and what it unlocked |

## What happens to Hazke's code

The Tithe is new firmware rather than a stripped-down Hazke, because a game with no flight needs
little of it. It borrows Hazke's frame capture and `tools/capture.py`, its SD card setup, its
keyboard handling and its console commands, all credited in firmware/CREDITS.md. None of the
*Elite* names come along.

## Rules for real-world signals

- Only listen to what networks broadcast anyway, the way a phone's WiFi list does (a passive
  scan, so the Deck sends nothing). Never connect, never capture other traffic.
- Keep only scrambled IDs, never real network names or hardware IDs, and never upload anything.
- Two keys do the scrambling. Each Deck makes its own random key on first boot and keeps it in its
  flash, and everything stored or logged uses it, so a lost SD card can't be matched to real
  networks. A second key, the same on every Deck, makes the names, so players see the same well
  under the same name. Nothing made with the shared key is stored.

## Risks to check early

- **Memory:** the Deck uses one 65 KB screen buffer and about 115 KB of other memory, which
  leaves room for WiFi on the Cardputer's 320 KB. Not yet measured on the device.
- **Scan time:** a full scan takes about 2 seconds, so it runs in the background and the screen
  never freezes.
- **Mesh WiFi:** a house with mesh WiFi shows up as several routers. The scanner folds a router's
  own networks into one well, and routers that share a network name count as a mesh group (the
  Twin wells relic pays on them). The walk report measures how common they really are.
- **Repetition:** whether home, commute and office give enough variety. Step 2 below answers this
  with real data before we tune anything.

## Build steps

1. **Fork and strip.** Cut flight and combat, rename *Elite* names. Changed: the scanner was built
   as new firmware in `firmware/`, borrowing only the Hazke parts it needs (capture, SD card,
   keyboard, console). The game screens and saves turned out simpler to write new.
2. **Scanner + calibration walk.** Built, in `firmware/`. The Deck's boot screen, a live list of
   nearby wells with names, energy and signal, a screen for walking toward one well, a walk
   summary, and an SD log of scrambled scans. `tools/walk_report.py` turns the logs into the
   numbers the balance simulator guesses today. Still to do: flash it to a real Cardputer, and walk.
3. **Economy.** Built: drawing, the hold, selling, prices, factions and Heat, as above. The numbers
   are the simulator's guesses until real walks come in (step 2).
4. **Atlas and saves.** Built: every well you visit, across runs, with its Nights and glyphs. The
   whole game saves to the Deck's flash after every change, with a backup on the SD card and a
   checksum that catches a damaged save.
5. **Factions, jobs, rank.** Built: faction standing and stalls, faction jobs, and a career rank
   from Tithes paid. Hazke's job code didn't fit a game without travel, so jobs are new.
6. **Roguelike layer.** Built: Bindings, Nights, Tithes, Omens and the Eclipse, the Collector's
   shop, every relic, trinket, spell, glyph, edition, Deck mod and Portent, candles, the loan,
   mercy, Overnight, Callings, Pacts, Challenges, unlocks and the Grimoire. The rules are a port of
   the simulator's, checked against it (BALANCE.md).
7. **Dowsing** with the gyro. Built early, with the other motion features (see firmware/README.md).
   Still to check on the device: how strong the body's shadow really is.
8. **Later:** Bluetooth devices passing by (phones, earbuds) as wandering traders with one-off
   deals. Two Cardputers trading with each other directly (ESP32s can message each other without
   a router). Sound and look polish.

## Open questions

- The game's name, and whether to keep the placeholder names above.
