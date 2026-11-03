"""The upgrade catalog: every relic, trinket, spell, glyph, edition, Deck mod, Portent, Omen,
Calling and Pact, with its numbers, flavor line and unlock condition.

This file is the source of truth. balance_sim.py plays it, and make_tables.py prints the
tables in UPGRADES.md and UNLOCKS.md from it, so the docs always match what was tested.

Effects get a sale context `c` and the run `r`:
  c["units"], c["price"]   phase 0 (add units / price)
  c["mult"]                phase 1 (add Mult)
  c["x"]                   phase 2 (multiply Mult)
  c["repeats"]             phase 3 (the sale counts again)
"""

ENERGIES = ["ember", "tide", "gale", "hush", "static"]
WARDS = ["hollow", "fringe", "ward", "nexus"]          # 0-3, 4-10, 11-25, 26+ networks

# ------------------------------------------------------------------ relics
RELICS = {}


def relic(id, name, rarity, school, phase, text, flavor, unlock=None, suits="all", fn=None,
          cursed=False, legendary=False):
    RELICS[id] = dict(id=id, name=name, rarity=rarity, school=school, phase=phase, text=text,
                      flavor=flavor, unlock=unlock, suits=suits, fn=fn, cursed=cursed,
                      legendary=legendary)


def st(r, id):
    return r["state"].setdefault(id, 0.0)


# --- Throng: busy places --------------------------------------------------------------
relic("crowd_feeder", "Crowd feeder", "common", "throng", 1,
      "+0.1 Amp for every well in range (up to +4)",
      "It eats what everyone else ignores.",
      suits="ward nexus", fn=lambda c, r: c.__setitem__("mult", c["mult"] + 0.1 * min(c["n"], 40)))
relic("rush_hour", "Rush hour", "common", "throng", 1,
      "+3 Amp in a Nexus ward (26+ wells)",
      "Six o'clock, and the currents run hot.",
      unlock=("visit_nexus", 1, "Visit a Nexus ward"), suits="nexus",
      fn=lambda c, r: c["ward"] == "nexus" and c.__setitem__("mult", c["mult"] + 3))
relic("choir_of_eleven", "Choir of eleven", "uncommon", "throng", 1,
      "+0.5 Amp for each well on channel 11 in range",
      "They all sing one note. It carries.",
      suits="ward nexus", fn=lambda c, r: c.__setitem__("mult", c["mult"] + 0.5 * c["ch11"]))
relic("packed_house", "Packed house", "uncommon", "throng", 2,
      "×1.5 Amp with 30+ wells in range",
      "Standing room only.",
      unlock=("max_networks", 40, "Offer with 40+ wells in range"), suits="nexus",
      fn=lambda c, r: c["n"] >= 30 and c.__setitem__("x", c["x"] * 1.5))
relic("gridlock", "Gridlock", "rare", "throng", 2,
      "×(1 + 0.05 per well in range), up to ×3",
      "One long knot, and you hold the end.",
      unlock=("big_sale_nexus", 5000, "Make a 5,000-Resonance offering in a Nexus ward"), suits="nexus",
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.05 * min(c["n"], 40))))

# --- Hollow: empty places --------------------------------------------------------------
relic("hollow_saint", "Hollow saint", "common", "hollow", 1,
      "+4 Amp in Hollow wards (0-3 wells)",
      "Patron of the places the signal forgot.",
      suits="hollow", fn=lambda c, r: c["ward"] == "hollow" and c.__setitem__("mult", c["mult"] + 4))
relic("hermits_lamp", "Hermit's lamp", "common", "hollow", 0,
      "+3 Thirst with 5 or fewer wells in range",
      "Fewer wells, deeper thirst.",
      unlock=("hollow_sales", 20, "Make 20 offerings in Hollow wards"), suits="hollow fringe",
      fn=lambda c, r: c["n"] <= 5 and c.__setitem__("price", c["price"] + 3))
relic("quiet_hours", "Quiet hours", "uncommon", "hollow", 2,
      "×2 Amp in a Hollow ward",
      "Listen. Nothing. Good.",
      suits="hollow", fn=lambda c, r: c["ward"] == "hollow" and c.__setitem__("x", c["x"] * 2))
relic("lantern_bearer", "Lantern-bearer", "uncommon", "hollow", 1,
      "+1 Amp for each Hollow ward you've visited tonight",
      "Light one. Then another. Then another.",
      unlock=("hollows_one_run", 10, "Visit 10 Hollow wards in one week"), suits="hollow fringe",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + r["hollows_tonight"]))
relic("lost_signal", "Lost signal", "rare", "hollow", 2,
      "×3 Amp with 0-2 wells in range",
      "One voice in the dark is worth a thousand in a crowd.",
      suits="hollow", fn=lambda c, r: c["n"] <= 2 and c.__setitem__("x", c["x"] * 3))

# --- Fringe: in-between places ----------------------------------------------------------
relic("borderwalker", "Borderwalker", "common", "fringe", 1,
      "+2.5 Amp with 4-15 wells in range",
      "Not the city, not the woods. Yours.",
      suits="fringe ward", fn=lambda c, r: 4 <= c["n"] <= 15 and c.__setitem__("mult", c["mult"] + 2.5))
relic("porch_light", "Porch light", "common", "fringe", 0,
      "+4 Thirst at Hearth wells (home networks)",
      "Someone left it on for you.",
      fn=lambda c, r: c["faction"] == "hearth" and c.__setitem__("price", c["price"] + 4))
relic("threshold", "Threshold", "uncommon", "fringe", 2,
      "×1.5 Amp if you've changed wards since your last visit",
      "Every doorway is a little spell.",
      unlock=("ward_changes_night", 8, "Change wards 8 times in one Night"),
      fn=lambda c, r: c["ward_changed"] and c.__setitem__("x", c["x"] * 1.5))
relic("picket_fence", "Picket fence", "uncommon", "fringe", 1,
      "Gains +0.75 Amp for every offering at a Hearth well",
      "Every house on the street knows you.",
      unlock=("hearth_sales", 30, "Make offerings at 30 Hearth wells"),
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + st(r, "picket_fence")))

# --- Wayfaring: finding new wells -------------------------------------------------------
relic("long_walk", "Long walk", "common", "wayfaring", 1,
      "+0.25 Amp per ward entered tonight",
      "The legs remember what the map forgets.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + 0.25 * c["wards_tonight"]))
relic("first_light", "First light", "common", "wayfaring", 1,
      "+4 Amp at a well you've never offered at",
      "New wells are always the sweetest.",
      unlock=("new_wells_run", 25, "Find 25 new wells in one week"),
      fn=lambda c, r: c["new"] and c.__setitem__("mult", c["mult"] + 4))
relic("cartographer", "Cartographer", "uncommon", "wayfaring", 0,
      "+1 Thirst for every 6 new wells found this week",
      "Ink, mostly. And a little blood.",
      fn=lambda c, r: c.__setitem__("price", c["price"] + int(r["new_wells"] / 6)))
relic("wanderlust", "Wanderlust", "uncommon", "wayfaring", 2,
      "×1.5 Amp at a well you've never offered at",
      "Somewhere else is always louder.",
      unlock=("new_wells_night", 8, "Find 8 new wells in one Night"),
      fn=lambda c, r: c["new"] and c.__setitem__("x", c["x"] * 1.5))
relic("pilgrims_staff", "Pilgrim's staff", "rare", "wayfaring", 2,
      "Gains ×0.15 Amp for every new well you find. Never resets.",
      "Worn smooth by a hundred doorways.",
      unlock=("unique_wells", 100, "Find 100 different wells, across all weeks"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "pilgrims_staff"))))

# --- Routine: the same wells, again ----------------------------------------------------
relic("hearthstone", "Hearthstone", "common", "routine", 0,
      "+3 Units at a well you offered at on an earlier Night",
      "Warm from your hand, every morning.",
      fn=lambda c, r: c["revisit"] and c.__setitem__("units", c["units"] + 3))
relic("commuters_charm", "Commuter's charm", "uncommon", "routine", 2,
      "×1.5 Amp at a well you've visited on 3+ Nights (×2 from Night 5)",
      "Same train, same well, same luck.",
      fn=lambda c, r: c["revisit"] and c["nights_seen"] >= 3 and
      c.__setitem__("x", c["x"] * (2 if r["night"] >= 4 else 1.5)))
relic("well_worn_path", "Well-worn path", "uncommon", "routine", 1,
      "+1 Amp for each Night you've visited this well",
      "Your footprints are the floor now.",
      unlock=("same_well_nights", 7, "Visit the same well on all 7 Nights"),
      fn=lambda c, r: c["revisit"] and c.__setitem__("mult", c["mult"] + c["nights_seen"]))
relic("keystone", "Keystone", "rare", "routine", 9,
      "Your glyphs are twice as strong",
      "Carve it deep enough and the city carves back.",
      unlock=("glyphs_total", 10, "Carve 10 glyphs"), fn=None)

# --- Elemental: one energy, pushed hard ----------------------------------------------------
for e, name, flavor in [
    ("ember", "Ember-caller", "Blow on your hands. Feel that?"),
    ("tide", "Tide-caller", "It comes in, it goes out, you skim the middle."),
    ("gale", "Gale-caller", "Catch it before it's gone."),
    ("hush", "Hush-caller", "Say nothing and it comes to you."),
    ("static", "Static-caller", "It bites. That's how you know it's good."),
]:
    relic(f"{e}_caller", name, "common", "elemental", 0,
          f"+4 Thirst on {e.capitalize()}", flavor,
          unlock=None if e in ("ember", "tide", "gale") else
          (f"{e}_sold", 30, f"Offer {e.capitalize()} 30 times"),
          fn=(lambda en: lambda c, r: (c["e"] == en or c["ghost"]) and
              c.__setitem__("price", c["price"] + 4))(e))
relic("resonant_fork", "Resonant fork", "common", "elemental", 1,
      "+1 Amp for each Attunement level of the energy you offer",
      "Strike it once. It rings for a week.",
      unlock=("attunes_run", 5, "Buy 5 Attunements in one week"),
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + c["lvl"]))
relic("ascetic", "Ascetic", "uncommon", "elemental", 2,
      "×2 Amp when your hold carries only one kind of energy",
      "One thing, done perfectly.",
      fn=lambda c, r: c["one_kind"] and c.__setitem__("x", c["x"] * 2))
relic("mirror_glass", "Mirror glass", "uncommon", "elemental", 2,
      "×1.5 Amp when you offer the same energy as last time",
      "Again. Again. Again.",
      fn=lambda c, r: c["e"] == c["last_e"] and c.__setitem__("x", c["x"] * 1.5))
relic("purist", "Purist", "rare", "elemental", 2,
      "×(1 + 0.2 per Attunement level) on the energy you offer",
      "Refined to nothing. Worth a fortune.",
      unlock=("attune_level", 5, "Get one energy to Attunement level 5"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.2 * c["lvl"])))

# --- Variety: a bit of everything -----------------------------------------------------------
relic("mixed_blessing", "Mixed blessing", "common", "variety", 0,
      "+3 Thirst when your hold carries 3+ kinds of energy",
      "A little of everything, a lot of luck.",
      fn=lambda c, r: c["hold_kinds"] >= 3 and c.__setitem__("price", c["price"] + 3))
relic("prism_wheel", "Prism wheel", "uncommon", "variety", 1,
      "+1.5 Amp for each kind of energy you've offered tonight",
      "Turn it and the light changes color.",
      unlock=("kinds_night", 5, "Offer all five energies in one Night"),
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + 1.5 * len(r["kinds_tonight"] | {c["e"]})))
relic("spectrum", "Spectrum", "rare", "variety", 2,
      "×(1 + 0.3 for each kind of energy offered tonight beyond the first)",
      "All five colors. The world goes white.",
      unlock=("win_with", "prism_wheel", "Hold a week with Prism wheel"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.3 * (len(r["kinds_tonight"] | {c["e"]}) - 1))))

# --- Smuggling: Static and Heat ---------------------------------------------------------------
relic("static_eater", "Static eater", "common", "smuggling", 0,
      "Static turns up more often and gets +5 Thirst. It never leaks or raises Heat.",
      "It hums when it's hungry.",
      fn=lambda c, r: (c["e"] == "static" or c["ghost"]) and c.__setitem__("price", c["price"] + 5))
relic("smugglers_lining", "Smuggler's lining", "uncommon", "smuggling", 0,
      "+5 Units on Static, and Static turns up more often",
      "The coat is bigger on the inside. Don't ask.",
      unlock=("static_sold", 25, "Offer Static 25 times"),
      fn=lambda c, r: c["e"] == "static" and c.__setitem__("units", c["units"] + 5))
relic("devils_meter", "Devil's meter", "uncommon", "smuggling", 2,
      "×(1 + Heat) Amp. Wardens show up more often.",
      "The needle only moves one way.", cursed=True,
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + r["heat"])))
relic("black_ledger", "Black ledger", "rare", "smuggling", 2,
      "Static turns up more often. Gains ×0.6 Amp per Static offering. Never resets.",
      "Each line a debt, each debt a promise.",
      unlock=("static_sold", 50, "Offer Static 50 times"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "black_ledger"))))
relic("wanted_poster", "Wanted poster", "rare", "smuggling", 2,
      "×3 Amp while Heat is 3 or more, and Static turns up more often",
      "Your face, but younger. They used an old photo.",
      unlock=("static_sold", 75, "Offer Static 75 times"),
      fn=lambda c, r: r["heat"] >= 3 and c.__setitem__("x", c["x"] * 3))

# --- Hidden: Hush and hidden networks ---------------------------------------------------------------
relic("hushed_step", "Hushed step", "common", "hidden", 1,
      "+3 Amp at a Hush well or when offering Hush",
      "Walk like you're not there.",
      unlock=("hidden_found", 10, "Find 10 Hush wells"),
      fn=lambda c, r: (c["hidden"] or c["e"] == "hush") and c.__setitem__("mult", c["mult"] + 3))
relic("ghost_key", "Ghost key", "uncommon", "hidden", 1,
      "+5 Amp at Hush wells, which count as all five energies",
      "It opens doors that don't have names.",
      fn=lambda c, r: c["hidden"] and c.__setitem__("mult", c["mult"] + 5))
relic("hush_lantern", "Hush lantern", "rare", "hidden", 2,
      "×2 Amp on Hush, and Hush turns up three times as often",
      "Its light makes no sound.",
      fn=lambda c, r: (c["e"] == "hush" or c["ghost"]) and c.__setitem__("x", c["x"] * 2))
relic("unseen_hand", "Unseen hand", "rare", "hidden", 2,
      "×2.5 Amp at a Hush well",
      "Nobody saw. Nobody ever sees.",
      unlock=("hidden_found", 30, "Find 30 Hush wells"),
      fn=lambda c, r: c["hidden"] and c.__setitem__("x", c["x"] * 2.5))

# --- Machine: the physics of signal ---------------------------------------------------------------
relic("close_call", "Close call", "common", "machine", 1,
      "+3 Amp when the well's signal is very strong (stand right next to it)",
      "Close enough to feel it on your teeth.",
      fn=lambda c, r: c["strong"] and c.__setitem__("mult", c["mult"] + 3))
relic("leyline", "Leyline", "uncommon", "machine", 0,
      "Units and Thirst ×1.2",
      "The old roads still run under the new ones.",
      fn=lambda c, r: (c.__setitem__("units", c["units"] * 1.2), c.__setitem__("price", c["price"] * 1.2)))
relic("channel_surfer", "Channel surfer", "uncommon", "machine", 2,
      "×1.75 Amp at a well on an odd channel (not 1, 6 or 11)",
      "Off the beaten band.",
      unlock=("channels_seen", 13, "Find wells on all 13 channels"),
      fn=lambda c, r: c["channel"] not in (0, 1, 6, 11) and c.__setitem__("x", c["x"] * 1.75))   # 0: not known (ECHO)
relic("overclock", "Overclock", "uncommon", "machine", 2,
      "×1.5 Amp on every offering, but 2 fewer visits per Night",
      "Run it hot. Apologize later.",
      fn=lambda c, r: c.__setitem__("x", c["x"] * 1.5))
relic("twin_wells", "Twin wells", "rare", "machine", 3,
      "At a mesh WiFi group, the offering repeats once for each extra router",
      "Same water, many buckets.",
      suits="ward nexus", fn=lambda c, r: c["mesh"] and c.__setitem__("repeats", c["repeats"] + c["mesh"] - 1))

# --- Timing: when you sell -------------------------------------------------------------------------------
relic("first_bell", "First bell", "common", "timing", 1,
      "+8 Amp on the first visit of each Night",
      "Whoever rings it first gets heard.",
      unlock=("one_visit_close", 1, "Pay a Tithe in a single visit"),
      fn=lambda c, r: c["vi"] == 0 and c.__setitem__("mult", c["mult"] + 8))
relic("night_shift", "Night shift", "uncommon", "timing", 1,
      "+0.5 Amp for each visit already used tonight",
      "The later it gets, the better you get.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + 0.5 * c["vi"]))
relic("witching_hour", "Witching hour", "uncommon", "timing", 9,
      "On the visit that closes the Night, ×3 Amp",
      "Midnight is a door. Walk out rich.", fn=None)
relic("patience", "Patience", "rare", "timing", 1,
      "Gains +3 Amp per visit left unused when a Night closes. Never resets.",
      "The Hum pays those who don't need it.",
      unlock=("fast_closes", 3, "Pay 3 Tithes in a row with 6+ visits to spare"),
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + st(r, "patience")))

relic("tally_stick", "Tally stick", "common", "timing", 1,
      "Gains +3 Amp every time you pay a Tithe. Never resets.",
      "A notch a night. It's getting crowded.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + st(r, "tally_stick")))
relic("ritual_candle", "Ritual candle", "uncommon", "timing", 2,
      "Gains ×0.45 Amp every time you pay a Tithe. Never resets.",
      "It burns longer each night. Don't ask.",
      unlock=("nights_paid", 20, "Pay 20 Tithes"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "ritual_candle"))))
relic("hoard", "Hoard", "uncommon", "money", 2,
      "×(1 + 0.02 per coin held), up to ×2",
      "It isn't about spending it. It never was.",
      unlock=("max_coin", 40, "Hold 40 coin at once"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * min(2.0, 1 + 0.02 * r["coin"])))

# --- Echo: repeats and copies -------------------------------------------------------------------------------
relic("doppel", "Doppel", "uncommon", "echo", 3,
      "The first offering of each Night counts twice",
      "Two of you at the well. One pays.",
      unlock=("sales_total", 150, "Make 150 offerings"),
      fn=lambda c, r: c["vi"] == 0 and c.__setitem__("repeats", c["repeats"] + 1))
relic("echo_chamber", "Echo chamber", "rare", "echo", 3,
      "Every third offering counts twice",
      "Say it once, hear it twice, get paid for both.",
      fn=lambda c, r: r["sales"] % 3 == 2 and c.__setitem__("repeats", c["repeats"] + 1))
relic("copycat_sigil", "Copycat sigil", "rare", "echo", 9,
      "Copies your rarest other relic",
      "It doesn't have ideas. It has yours.", fn=None)
relic("overtone", "Overtone", "rare", "echo", 9,
      "On an offering of 10+ units, every relic fires twice",
      "Hit the right weight. The Deck rings.",
      unlock=("big_units", 15, "Offer 15 units at once"), fn=None)

# --- Deck: the Cardputer's own sensors -------------------------------------------------------------------------
relic("din", "Din", "common", "deck", 1,
      "+0 to +4 Amp, rising with how loud it is around you (microphone)",
      "The Deck likes a noisy room.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + 4 * c["noise"]))
relic("steady_hand", "Steady hand", "common", "deck", 1,
      "+3 Amp if you hold the Deck still while you offer (ADV only)",
      "Breathe out. Now.",
      fn=lambda c, r: c["still"] and c.__setitem__("mult", c["mult"] + 3))
relic("dowsers_knot", "Dowser's knot", "uncommon", "deck", 2,
      "×1.5 Amp at a well you found by dowsing",
      "The string pulls. Follow it.",
      unlock=("dowses", 30, "Dowse 30 times"),
      fn=lambda c, r: c["dowsed"] and c.__setitem__("x", c["x"] * 1.5))
relic("last_ember", "Last ember", "uncommon", "deck", 2,
      "×2.5 Amp while the Deck's battery is under 20%",
      "Spend it all. That's what it's for.",
      unlock=("low_battery_close", 1, "Pay a Tithe with the battery under 10%"),
      fn=lambda c, r: c["battery_low"] and c.__setitem__("x", c["x"] * 2.5))

# --- Cursed: big upside with a catch ------------------------------------------------------------------------------
relic("glass_crown", "Glass crown", "rare", "cursed", 2,
      "×3 Amp on every offering, but one energy is worthless each Night",
      "Heavy is the head, until it shatters.", cursed=True,
      unlock=("wins", 1, "Hold a week"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (0 if c["e"] == r["crown_ban"] else 3)))
relic("hungry_coin", "Hungry coin", "uncommon", "cursed", 1,
      "+6 Amp, but it eats 2 coin every visit",
      "Feed it and it feeds you. Mostly.", cursed=True,
      unlock=("broke_shop", 1, "Leave a stall with 0 coin"),
      fn=lambda c, r: (c.__setitem__("mult", c["mult"] + 6), c.__setitem__("coin", c["coin"] - 2)))
relic("blood_moon", "Blood moon", "rare", "cursed", 2,
      "×3 Amp, but every Tithe is 35% higher",
      "The Hum notices when you get greedy. It gets greedy back.", cursed=True,
      unlock=("pact_win", 3, "Hold a week on the Iron Pact"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * 3))

# --- Legendary: only from SUMMON or a Conjunction Portent ------------------------------------------------
relic("grid_saint", "The Grid Saint", "legendary", "throng", 2,
      "×(1 + 0.1 per well in range)",
      "She was a switchboard once. Now she is the switchboard.", legendary=True,
      unlock=("school_win", "throng", "Hold a week with 2 Throng relics"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.1 * min(c["n"], 40))))
relic("the_unheard", "The Unheard", "legendary", "hidden", 2,
      "Every well counts as hidden. ×2 Amp on Hush.", "...", legendary=True,
      unlock=("school_win", "hidden", "Hold a week with 2 Hidden relics"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (2 if c["e"] == "hush" else 1)))
relic("seven_league_boots", "Seven-league boots", "legendary", "wayfaring", 2,
      "×0.1 Amp for every new well found this week",
      "One step, one ward. Mind the curb.", legendary=True,
      unlock=("school_win", "wayfaring", "Hold a week with 2 Wayfaring relics"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.1 * r["new_wells"])))
relic("old_keeper", "The Old Keeper", "legendary", "routine", 2,
      "×3 Amp at wells you offered at on an earlier Night",
      "He has watched this street for a hundred years. He likes you.", legendary=True,
      unlock=("school_win", "routine", "Hold a week with 2 Refrain relics"),
      fn=lambda c, r: c["revisit"] and c.__setitem__("x", c["x"] * 3))
relic("midnight_engine", "The Midnight Engine", "legendary", "timing", 3,
      "Every offering counts twice",
      "Tick. Tick. Tick. Tick.", legendary=True,
      unlock=("school_win", "timing", "Hold a week with 2 Tempo relics"),
      fn=lambda c, r: c.__setitem__("repeats", c["repeats"] + 1))


# ---- The second batch: more to find at the stall (Oct 2026) ----
relic("laundromat_token", "Laundromat token", "common", "money", 1,
      "+1 Amp for every 5 coin you hold (up to +4)", "Good for one load. Never once used for laundry.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + min(4, int(r["coin"] // 5))))
relic("bus_fare", "Bus fare", "common", "money", 1,
      "+5 Amp while you hold 4 coin or less", "Exact change, and not a token more.",
      unlock=("broke_shop", 1, "Leave a stall with 0 coin"),
      fn=lambda c, r: r["coin"] <= 4 and c.__setitem__("mult", c["mult"] + 5))
relic("piggy_bank", "Piggy bank", "uncommon", "money", 1,
      "Gains +1.5 Amp per coin of interest you earn. Never resets.", "Shake it. It sounds like next week.",
      unlock=("max_coin", 25, "Hold 25 coin at once"),
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + st(r, "piggy_bank")))
relic("long_receipt", "Long receipt", "rare", "money", 2,
      "x(1 + 0.05 per coin spent at the last stall, minus sales), up to x3", "Itemized, folded twice, kept for no reason.",
      unlock=("rares_bought", 3, "Buy 3 rare relics"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "long_receipt"))))
relic("odd_socks", "Odd socks", "common", "variety", 1,
      "+4 Amp when you offer a different energy than last time", "They match. Just not each other.",
      fn=lambda c, r: c["last_e"] is not None and c["e"] != c["last_e"] and c.__setitem__("mult", c["mult"] + 4))
relic("tasting_spoon", "Tasting spoon", "uncommon", "variety", 2,
      "x2 Amp on the first offering of each energy tonight", "Just a little of each. To be sure.",
      fn=lambda c, r: c["e"] not in r["kinds_tonight"] and c.__setitem__("x", c["x"] * 2))
relic("charm_bracelet", "Charm bracelet", "rare", "variety", 2,
      "x(1 + 0.4 per kind of energy in your hold beyond the first)", "One charm for every town she left.",
      unlock=("win_with", "mixed_blessing", "Hold a week with Mixed blessing"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.4 * (c["hold_kinds"] - 1))))
relic("one_way_ticket", "One-way ticket", "uncommon", "cursed", 2,
      "x2.5 Amp, but wells you offered at on an earlier Night earn nothing", "Nobody buys these by accident.", cursed=True,
      unlock=("new_wells_run", 40, "Find 40 new wells in one week"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (0 if c["revisit"] else 2.5)))
relic("empty_frame", "Empty frame", "rare", "cursed", 2,
      "x(1 + 0.5 for each empty relic slot)", "Nobody in it yet. Keep it that way.", cursed=True,
      unlock=("op_nights", 5, "Pay 5 Tithes at 3x or more"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + 0.5 * max(0, r["_slots"] - r["_used"]))))
relic("spare_key", "Spare key", "common", "fringe", 1,
      "+1 Amp for every 10 standing with the Hearth", "Under the mat, where it always is.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + int(max(0.0, r["standing"]["hearth"]) // 10)))
relic("garden_gate", "Garden gate", "uncommon", "fringe", 2,
      "x1.5 Amp in a Fringe ward, x2 if you just changed wards", "Never quite shut. Never quite open.",
      suits="fringe",
      fn=lambda c, r: c["ward"] == "fringe" and c.__setitem__("x", c["x"] * (2 if c["ward_changed"] else 1.5)))
relic("chalk_stub", "Chalk stub", "uncommon", "routine", 2,
      "x1.5 Amp at a well you've carved a glyph into", "Worn to a nub on other people's walls.",
      fn=lambda c, r: c["glyph"] is not None and c.__setitem__("x", c["x"] * 1.5))
relic("bus_pass", "Bus pass", "uncommon", "routine", 3,
      "Offerings count twice at wells you've visited every Night", "Punched every morning. Never missed one.",
      unlock=("wins", 2, "Hold 2 weeks"),
      fn=lambda c, r: c["revisit"] and r["night"] >= 1 and c["nights_seen"] >= r["night"] + 1
      and c.__setitem__("repeats", c["repeats"] + 1))
relic("key_ring", "Key ring", "rare", "routine", 2,
      "At wells you offered at on an earlier Night: x(1 + 0.25 per Night there)", "A key for every door that ever let you in.",
      unlock=("win_with", "hearthstone", "Hold a week with Hearthstone"),
      fn=lambda c, r: c["revisit"] and c.__setitem__("x", c["x"] * (1 + 0.25 * c["nights_seen"])))
relic("thermos", "Thermos", "common", "hidden", 0,
      "+4 Units on Hush", "Keeps cold things cold, and quiet things quiet.",
      unlock=("nights_paid", 5, "Pay 5 Tithes"),
      fn=lambda c, r: (c["e"] == "hush" or c["ghost"]) and c.__setitem__("units", c["units"] + 4))
relic("locked_diary", "Locked diary", "rare", "hidden", 2,
      "Gains x0.45 Amp at every Hush well. Never resets.", "The lock is tiny. That's not the point.",
      unlock=("win_with", "hush_lantern", "Hold a week with Hush lantern"),
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "locked_diary"))))
relic("tote_bag", "Tote bag", "uncommon", "echo", 3,
      "Offerings of 12 or more Units count twice", "Holds more than it should. Says so on the side.",
      unlock=("big_units", 12, "Offer 12 units at once"),
      fn=lambda c, r: c["units"] >= 12 and c.__setitem__("repeats", c["repeats"] + 1))
relic("seashell", "Seashell", "rare", "echo", 3,
      "Counts twice if you've changed wards since your last visit", "Hold it to your ear. Someone else's sea.",
      unlock=("big_sale_nexus", 10000, "Make a 10,000-Resonance offering in a Nexus ward"),
      fn=lambda c, r: c["ward_changed"] and c.__setitem__("repeats", c["repeats"] + 1))
relic("earplugs", "Earplugs", "common", "deck", 1,
      "+0 to +4 Amp, rising the quieter it is around you (microphone)", "Everything goes quiet except the part you want.",
      suits="hollow fringe", fn=lambda c, r: c.__setitem__("mult", c["mult"] + 4 * (1 - c["noise"])))
relic("spirit_level", "Spirit level", "uncommon", "deck", 2,
      "x1.5 Amp if you hold the Deck still at a very strong signal (ADV)", "Wait for the bubble to stop.",
      fn=lambda c, r: c["still"] and c["strong"] and c.__setitem__("x", c["x"] * 1.5))
relic("lanyard", "Lanyard", "common", "throng", 0,
      "+5 Thirst at Syndicate wells (office networks)", "Still opens the side door on Fridays.",
      suits="ward nexus", fn=lambda c, r: c["faction"] == "syndicate" and c.__setitem__("price", c["price"] + 5))
relic("picnic_blanket", "Picnic blanket", "common", "hollow", 0,
      "+3 Units in a Hollow ward (0-3 wells)", "Spread it wherever nobody else is.",
      suits="hollow", fn=lambda c, r: c["ward"] == "hollow" and c.__setitem__("units", c["units"] + 3))
relic("postcard", "Postcard", "common", "wayfaring", 1,
      "+1 Amp for each new well you've found tonight", "Wish you were here. You were, for a minute.",
      unlock=("new_wells_night", 5, "Find 5 new wells in one Night"),
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + r.get("new_tonight", 0)))
relic("film_canister", "Film canister", "uncommon", "smuggling", 2,
      "x2 Amp on Static while Heat is under 1", "Nothing in here, officer. Just film.",
      unlock=("win_with", "static_eater", "Hold a week with Static eater"),
      fn=lambda c, r: (c["e"] == "static" or c["ghost"]) and r["heat"] < 1 and c.__setitem__("x", c["x"] * 2))

# ---- Growing relics: weak the Night you buy them, the reason you live past Night 5 (Nov 2026) ----
relic("pedometer", "Pedometer", "common", "timing", 1,
      "Gains +0.9 Amp for every offering you make. Never resets.",
      "It counts the walk, not the reason.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + st(r, "pedometer")))
relic("mixtape", "Mixtape", "common", "variety", 1,
      "Gains +3 Amp whenever you switch energy. Never resets.",
      "Side A, side B, side A.",
      fn=lambda c, r: c.__setitem__("mult", c["mult"] + st(r, "mixtape")))
relic("rain_gauge", "Rain gauge", "common", "machine", 0,
      "Gains +1.5 Thirst every time you draw. Never resets.",
      "It fills whether anyone checks it or not.",
      fn=lambda c, r: c.__setitem__("price", c["price"] + st(r, "rain_gauge")))
relic("prism_jar", "Prism jar", "uncommon", "variety", 2,
      "Gains ×0.3 Amp per energy you offer each Night. Never resets.",
      "Five colors under one lid.",
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "prism_jar"))))
relic("seed_vault", "Seed vault", "uncommon", "money", 2,
      "Gains ×0.3 Amp per coin of interest you earn. Never resets.",
      "Nothing grows in there while you keep opening it.",
      fn=lambda c, r: c.__setitem__("x", c["x"] * (1 + st(r, "seed_vault"))))

# ------------------------------------------------------------------ trinkets (2 slots, money and utility)
TRINKETS = {
    "misers_purse": ("Miser's purse", "Interest cap +5", "Count it twice. It grows when you're not looking.", None),
    "haggler": ("Haggler", "One free reroll at each stall", "Everything's negotiable.", None),
    "pickpocket": ("Pickpocket", "+1 coin every visit", "Light fingers, heavy pockets.", None),
    "fence": ("Fence", "Offer any energy at any well, ignoring faction bans", "No questions asked. Fine.", None),
    "tithe_skimmer": ("Tithe-skimmer", "+1 coin for every 50% you beat a Tithe by (up to +10)",
                      "The Hum never counts the change.", ("op_nights", 3, "Pay 3 Tithes at 3x or more")),
    "moneylender": ("Moneylender", "You can take two Syndicate loans per week, at 1.1x interest",
                    "Friendly rates, for a friend.", ("loans", 1, "Take a Syndicate loan")),
    "lucky_coin": ("Lucky coin", "Rare relics show up twice as often at the stall", "Heads, you win. Tails, flip again.",
                   ("rares_bought", 5, "Buy 5 rare relics")),
    "collectors_eye": ("Collector's eye", "Shines show up three times as often", "You can tell the real ones by the shine.",
                       ("editions_owned", 3, "Own 3 relics with shines")),
    "golden_thread": ("Golden thread", "+4 coin when you pay a Tithe with 4+ visits to spare",
                      "Pull it and the night unravels early.", ("fast_closes", 1, "Pay a Tithe with 6+ visits to spare")),
    "pawn_ticket": ("Pawn ticket", "Sell relics back for their full price", "Redeemable. Eventually.",
                    ("relics_sold", 5, "Sell 5 relics back")),
    "chalk_pouch": ("Chalk pouch", "Start each Night with a CHALK spell if you have a free spell slot",
                    "Dusty fingers, sigils on the walls.", ("glyphs_total", 5, "Carve 5 glyphs")),
}

# ------------------------------------------------------------------ spells (2 slots, typed on the keyboard)
SPELLS = {
    "glamour": ("GLAMOUR", "This offering counts double", "Make it shine. Just for a moment.", None, 1.0),
    "surge": ("SURGE", "Your next draw takes 2.5x the units", "Pull. Harder.", None, 1.0),
    "transmute": ("TRANSMUTE", "Hold becomes your focus for 4 offerings", "Lead into gold. Ember into Tide.", None, 1.0),
    "haste": ("HASTE", "+2 visits tonight", "Run.", None, 1.0),
    "echo": ("ECHO", "Offer to a visited well from anywhere", "It remembers you.", None, 1.0),
    "hex": ("HEX", "+8 coin", "A little curse, a big discount.", None, 1.0),
    "veil": ("VEIL", "Static: no Heat and +3 Thirst tonight", "Nobody saw anything.", ("static_sold", 10, "Offer Static 10 times"), 1.0),
    "chalk": ("CHALK", "Carve a random glyph into this well", "Your mark, their wall.", None, 1.0),
    "kindle": ("KINDLE", "Give a random relic a shine", "A little fire in the glass.", ("editions_owned", 1, "Own a relic with a shine"), 0.6),
    "augur": ("AUGUR", "Clear tonight's Omen and undo what it took. Not a Wardens walk.", "Read the signs again. Squint.", ("omens_seen", 10, "See 10 Omens"), 0.8),
    "recall": ("RECALL", "Get back the last spell you used", "Again, with feeling.", ("spells_cast", 20, "Cast 20 spells"), 0.6),
    "bargain": ("BARGAIN", "Relics and wares at the next stall are half price", "Closing-down sale. Everything must go.", ("broke_shop", 1, "Leave a stall with 0 coin"), 0.6),
    "summon": ("SUMMON", "A legendary relic, into a free slot", "Something answers. Be polite.", ("wins", 3, "Hold 3 weeks"), 0.08),
}

# ------------------------------------------------------------------ glyphs (carved into a real well for the run)
GLYPHS = {
    "gilded": ("Minted", "+3 coin on every offering here", "A gold thumbprint on the router."),
    "lit": ("Ringing", "+4 Amp on every offering here", "It hums faintly, if you know how to listen."),
    "echoing": ("Echoing", "Offerings here count twice", "Knock once and it knocks back."),
    "deep": ("Deep", "+5 Units on every offering here", "The well goes further down than it should."),
    "bound": ("Bound", "×1.5 Amp on every offering here", "Tied to you with a thread nobody else can see."),
    "beacon": ("Beacon", "This well counts as new every time", "It never quite recognizes you."),
}

# ------------------------------------------------------------------ editions (a shine on a relic)
EDITIONS = {
    "gilt": ("Gilt", "+2 coin at the end of each Night", 0.04),
    "lit": ("Lit", "+3 Amp", 0.03),
    "prismatic": ("Prismatic", "×1.5 Amp", 0.015),
    "phantom": ("Phantom", "Takes no relic slot", 0.005),
}

# ------------------------------------------------------------------ Deck mods (permanent for the run; tier 2 needs tier 1)
MODS = {
    "extra_breath": ("Extra breath", "+1 visit per Night", None),
    "second_wind": ("Second wind", "+1 more visit per Night", "extra_breath"),
    "satchel": ("Satchel", "+2 Units on every draw", None),
    "bottomless_satchel": ("Bottomless satchel", "+3 more Units on every draw", "satchel"),
    "lodestone": ("Lodestone", "Your focus energy turns up more often", None),
    "true_north": ("True north", "Dowsing finds a well on a third of your visits", "lodestone"),
    "overstock": ("Overstock", "+1 relic at every stall", None),
    "night_market": ("Night market", "+1 spell, glyph or Attunement at every stall", "overstock"),
    "clearance": ("Clearance", "Relics and wares at the stall cost 1 less", None),
    "liquidation": ("Liquidation", "Rerolls cost 2 less", "clearance"),
    "fifth_pocket": ("Fifth pocket", "+1 spell slot", None),
    "sixth_socket": ("Sixth socket", "+1 relic slot", "fifth_pocket"),
    "charm_ward": ("Charm ward", "Omens only take effect half the time", None),
    "eclipse_ward": ("Eclipse ward", "The Eclipse only takes effect half the time", "charm_ward"),
    "attuners_fork": ("Attuner's fork", "Attunements sometimes give 2 levels", None),
    "tuning_core": ("Tuning core", "Each Attunement level also gives +1 more Amp", "attuners_fork"),
}

# ------------------------------------------------------------------ Portents (rewards for paying a Tithe with visits to spare)
PORTENTS = {
    "rare_sight": ("Rare sight", "A sure rare relic at the next stall"),
    "gift": ("Gift", "A free relic (a rare from Night 4)"),
    "windfall": ("Windfall", "+10 coin"),
    "doubled_interest": ("Doubled interest", "Interest pays double tonight"),
    "free_rerolls": ("Free rerolls", "Two free rerolls at the next stall"),
    "shine": ("Shine", "A relic at the next stall comes with a shine"),
    "tuning": ("Tuning", "+1 Attunement on your focus energy"),
    "scroll": ("Scroll", "A free spell"),
    "chalk_mark": ("Chalk mark", "A free glyph to carve into any well"),
    "conjunction": ("Conjunction", "A free legendary relic (if any open)"),
}

# ------------------------------------------------------------------ Omens (one rule per Night, shown at the start of it)
OMENS = {
    "fade": ("The fade", "One energy is worth half tonight"),
    "low_tide": ("Low tide", "Units ×0.75 tonight"),
    "short_night": ("Short night", "2 fewer visits tonight"),
    "silent_bell": ("Silent bell", "Your first 2 visits earn nothing"),
    "crowded_sky": ("Crowded sky", "Thirst ×0.7 in Quarter and Nexus wards"),
    "empty_roads": ("Empty roads", "Thirst ×0.7 in Hollow and Fringe wards"),
    "iron_price": ("Still water", "Relics that add Thirst give half as much tonight"),
    "wardens_walk": ("Wardens walk", "You start the Night at Heat 3"),
    "fog": ("Fog", "Relics see half as many wells as there are"),
    "blood_price": ("Blood price", "This Tithe is 20% higher"),
    "tangled": ("Tangled lines", "Relics fire in the wrong order: multiply first, then add"),
}
ECLIPSES = {
    "sleep": ("The Sleeper", "Polish counts for half tonight (tier III works as tier I)"),
    "hunger": ("The Hunger", "The Tithe is 25% higher"),
    "dark": ("The Dark", "2 fewer visits tonight"),
    "dim": ("The Dimming", "Every ×Amp effect gives a quarter less (×3 becomes ×2.5)"),
}

# ------------------------------------------------------------------ Callings (how each Binding starts)
CALLINGS = {
    "wayfarer": ("Wayfarer", "No twist. The Deck as it comes.", None),
    "hedge_witch": ("Hedge witch", "Starts with Static eater and Static-caller. Wardens never appear.", None),
    "hermit": ("Hermit", "Starts with Hearthstone. A well you offered at on an earlier Night can be drawn twice.", None),
    "courier": ("Courier", "Units ×1.4 on every draw, but only 10 visits per Night", ("wins", 1, "Hold a week")),
    "diviner": ("Diviner", "Starts with Lodestone and True north. You see each Omen two Nights early.",
                ("dowses", 30, "Dowse 30 times")),
    "night_clerk": ("Night clerk", "Interest cap doubled, but rerolls cost 2 more",
                    ("max_coin", 30, "Hold 30 coin at once")),
    "chorus": ("Chorus", "+1 relic slot, but relics cost 1 more", ("rares_one_run", 4, "Own 4 rare relics in one week")),
    "cartomancer": ("Cartomancer", "Starts with two CHALK spells. Glyphs are 50% stronger.",
                    ("glyphs_total", 10, "Carve 10 glyphs")),
    "pilgrim": ("Pilgrim", "Only your focus energy exists for you, but you have 4 relic slots",
                ("win_with", "ascetic", "Hold a week with Ascetic")),
    "collector": ("Magpie", "Shines are three times as common, but relics cost 1 more",
                  ("editions_owned", 3, "Own 3 relics with shines")),
    "contractor": ("Contractor", "Starts with 20 coin and a Syndicate loan (Night 2's Tithe +20%)",
                   ("loans_repaid", 1, "Pay off a Syndicate loan")),
    "glitch": ("Glitch", "The hard Calling. Energy types reshuffle every Night. Starts with two random rare relics.",
               ("callings_won", 3, "Hold weeks with 3 different Callings")),
}

# ------------------------------------------------------------------ Pacts (optional difficulty, each adds to the ones before)
# Each rung's text names the thing the player has been leaning on, the way a Slay the Spire
# Ascension says "One less potion slot". After a week lost on a Pact the Bind screen offers the
# Trial that teaches it (Game.h trialForPact).
PACTS = [
    ("Chalk", "Nothing added. The Deck as it comes."),
    ("Lead", "The Tithe pays no coin at dawn. Live on trade, favors and spare visits."),
    ("Iron", "Nights 5, 6 and 7 ask ×1.25, ×1.5, ×1.75. Buy what grows before you need it."),
    ("Ash", "The same energy twice in a row pays 30% less. Carry more than one."),
    ("Bone", "Relics don't all last: some on the stall are Withered and crumble after 3 Nights."),
    ("Silver", "Rerolls start at 8 coin, and Polish costs twice as much."),
    ("Obsidian", "No Syndicate loan, and candles cost twice as much."),
    ("Gold", "One fewer relic slot."),
]

# ------------------------------------------------------------------ Challenges (fixed-rule Bindings, for variety)
# The first opens with your first win, then one more opens with each win after that.
CHALLENGES = [
    ("Shoestring", "You can never hold more than 15 coin"),
    ("One note", "Pilgrim rules, and the stall only stocks Callers, Attunements and Polish"),
    ("Night owl", "8 visits per Night, but Portents start at 4 visits to spare"),
    ("Glass house", "Start with Glass crown. It can't be sold or swapped out, and Tithes are x2.5."),
    ("Static storm", "Every well is an open network. Heat never cools."),
    ("The long way", "Wells you offered at on an earlier Night earn nothing"),
    ("Homebody", "From Night 2, you can only offer where you offered on an earlier Night, but those wells refill all Night"),
    ("Empty pockets", "No stall. Each Night you pay brings a free relic and an Attunement instead."),
    ("Grand tour", "Visit all four kinds of ward each Night, or the Tithe doubles"),
    ("Redline", "Start with Overclock and Witching hour, and 2 extra visits a Night to run them. Tithes ×2.5."),
]

# ------------------------------------------------------------------ the Grimoire: lore pages and Deck themes
# Each page tells part of the secret world's story and unlocks a cosmetic theme for the Deck
# (boot sigil and screen colors). No page changes the odds.
GRIMOIRE = [
    ("The first Deck", "Bind your first week", "Ash"),
    ("What the Tithe feeds", "Pay 10 Tithes", "Ember"),
    ("The Collector", "Buy 25 things from the Collector", "Brass"),
    ("The Hedge", "Reach +80 standing with the Hedge in one week", "Moss"),
    ("The Hearth", "Reach +80 standing with the Hearth in one week", "Hearth"),
    ("The Warded", "Reach +80 standing with the Warded in one week", "Seal"),
    ("The Syndicate", "Pay off a Syndicate loan", "Chrome"),
    ("Wells", "Find 100 different wells", "Tide"),
    ("The Eclipse", "Survive all four Eclipses", "Umbra"),
    ("Overnight", "Survive 5 Overnight Nights in one week", "Midnight"),
    ("The old ones", "SUMMON a legendary relic", "Gold leaf"),
    ("The maker", "Hold a week on the Gold Pact", "White"),
]
