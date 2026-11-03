"""Balance simulator for the signal-economy roguelike. Plays the catalog in catalog.py.

  python3 tools/balance_sim.py run        [runs]     one Binding at a time: outcome mix, Nights, ceiling
  python3 tools/balance_sim.py relics     [runs]     every relic: how often kept, how much it lifts wins
  python3 tools/balance_sim.py callings   [runs]     each Calling's outcome mix
  python3 tools/balance_sim.py pacts      [runs]     win rate on each Pact, by skill
  python3 tools/balance_sim.py career     [players]  many weeks of play: unlock pace, win rate over time
  python3 tools/balance_sim.py all                   everything, at report size

A "run" is one Binding of 7 Nights. Resonance pays the Tithe; coin buys things.
"""
import copy
import json
import os
import random
import sys
from collections import Counter, defaultdict
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import catalog as K  # noqa: E402

# ================================================================== tuning knobs
NIGHTS = 7
VISITS = 12
TITHES = [410, 900, 2200, 4700, 9000, 17500, 26000]
if os.environ.get("TITHES"):          # set by tune_tithes.py (worker processes read it on import)
    TITHES = [float(x) for x in os.environ["TITHES"].split(",")]
OVERNIGHT_GROWTH = 2.0
OVERNIGHT_ACCEL = 0.5      # each Overnight Night grows 0.5 more than the last
OP_RATIO = 3.0             # a win is "overpowered" if the build reached this many times the Tithe...
OP_HOLD = 2.0              # ...and never dropped below this many times it for the rest of the week
START_COIN = 4
START_SPELL = "glamour"
NIGHT_REWARD = 5
COIN_PER_BASE = 1 / 30     # trading profit: coin per point of un-multiplied sale value
INTEREST_PER, INTEREST_CAP = 5, 5
RELIC_SLOTS, TRINKET_SLOTS, SPELL_SLOTS = 5, 2, 2
PRICE = {"common": 4, "uncommon": 7, "rare": 10, "legendary": 0, "trinket": 5, "spell": 3,
         "attune": 3, "glyph": 3, "mod": 10, "edition": 3}
RARITY_W = {"common": 0.67, "uncommon": 0.27, "rare": 0.06}
REROLL, REROLL_STEP = 3, 2   # the first reroll at a stall, and what each one after adds
SHOP_RELICS, SHOP_CONSUMABLES, TRINKET_OFFER_P = 4, 3, 0.4
CONSUMABLE_W = {"attune": 0.45, "spell": 0.4, "glyph": 0.15}
BUILD_BIAS = 0.35          # chance a relic slot only offers relics from schools you already own
LOCALE_BIAS = 0.5          # chance a relic slot only offers relics that suit the wards you've played in
UNLOCKS_PER_RUN = int(os.environ.get("UNLOCKS_PER_RUN", 2))
LOAN_FLOOR, LOAN_INTEREST = 0.6, 1.25
PORTENT_AT = (6, 9, 11)    # visits to spare for 1 / 2 / 3 Portents
PORTENT_CHOICE_BELOW = 3   # below this Pact the dawn offers two Portents and the player keeps one
ATTUNE_PRICE = float(os.environ.get("ATTUNE_PRICE", 0))   # Price per Attunement level
ATTUNE_MULT = float(os.environ.get("ATTUNE_MULT", 1))     # Mult per Attunement level
RARE_GROWTH, UNCOMMON_GROWTH, EDITION_GROWTH = 0.03, 0.01, 0.25   # per Night: better stock as the week goes on
IRON_STEEP = (1.25, 1.5, 1.75)   # Iron: Nights 5, 6 and 7 (and Overnight) ask this many times more
ASH_TIRE = 0.7                   # Ash: the same energy as the offering before, tonight, pays this × Thirst
MULT_SLOT = False                # from the second stall, one shelf relic always multiplies (before Nov 2026)
MULT_KEEP = 0.35                 # a multiplier rolled for a plain slot stays with this chance
GROWER_PULL = 0.25               # chance a plain shelf roll becomes a growing relic of its rarity
GROWING_RELICS = {"tally_stick", "ritual_candle", "patience", "pilgrims_staff", "black_ledger", "picket_fence",
                  "piggy_bank", "locked_diary", "pedometer", "mixtape", "rain_gauge", "prism_jar", "seed_vault"}
WARDEN_BLOCK = 0.15
MERCY = os.environ.get("MERCY", "1") == "1"   # the Collector's mercy after a close call
CANDLES = [4, 6, 8]        # coin for each extra visit when a Night runs out short of the Tithe

BASE_PRICE = {"ember": 5, "tide": 5, "gale": 5, "hush": 9, "static": 8}
PROFILES = {   # how a kind of player moves through the world
    "city":     dict(w=0.30, wards=[0.03, 0.12, 0.40, 0.45], mobility=0.50, routine=0.8),
    "suburb":   dict(w=0.35, wards=[0.15, 0.40, 0.35, 0.10], mobility=0.35, routine=1.0),
    "rural":    dict(w=0.10, wards=[0.58, 0.30, 0.10, 0.02], mobility=0.30, routine=1.1),
    "commuter": dict(w=0.25, wards=[0.10, 0.35, 0.20, 0.35], mobility=0.45, routine=1.3),
}
SKILLS = {
    # novices get the shop's hint star, so their picks are noisy but not random
    "novice":  dict(noise=0.6, rerolls=1, keep=10, margin=1.0, random_buy=0.0, smart_swap=False, samples=1, pick=0.75),
    "average": dict(noise=0.35, rerolls=4, keep=20, margin=1.15, random_buy=0.0, smart_swap=True, samples=2, pick=0.9),
    "expert":  dict(noise=0.1, rerolls=8, keep=25, margin=1.25, random_buy=0.0, smart_swap=True, samples=4, pick=1.0),
}
CHOICES = {"hollow": 2, "fringe": 2, "ward": 3, "nexus": 3}   # wells you can pick between on the scanner
CHANNELS = [1] * 30 + [6] * 30 + [11] * 25 + [2, 3, 4, 5, 7, 8, 9, 10, 12, 13] + [3, 8, 13, 4, 9]

# The real world, as guessed. WORLD=world.json (from tools/walk_report.py --json) swaps in what
# walks with the Deck measured, and plays every run as the person who walked them.
WORLD = dict(hidden=0.10, hedge=0.15, warded=0.12, hearth_base=0.73, strong=0.35,
             syndicate={"hollow": 0.03, "fringe": 0.05, "ward": 0.15, "nexus": 0.30},
             mesh={"hollow": .03, "fringe": .08, "ward": .12, "nexus": .18})
if os.environ.get("WORLD"):
    with open(os.environ["WORLD"]) as _f:
        _w = json.load(_f)
    WORLD.update(hidden=_w["hidden"], hedge=_w["faction"]["hedge"], warded=_w["faction"]["warded"],
                 strong=_w["strong"])
    WORLD["hearth_base"] = 1 - WORLD["hedge"] - WORLD["warded"]
    WORLD["syndicate"].update(_w["syndicate_by_ward"])
    WORLD["mesh"].update(_w["mesh_by_ward"])
    CHANNELS = [int(c) for c, share in _w["channels"].items() for _ in range(round(share * 1000))]
    PROFILES = {"measured": dict(w=1.0, wards=_w["ward_mix"], mobility=_w.get("ward_change", 0.4),
                                 routine=_w.get("routine", 1.0))}
STARTERS = {k for k, v in K.RELICS.items() if v["unlock"] is None and not v["legendary"]}
RANK = {"common": 1, "uncommon": 2, "rare": 3, "legendary": 4}


# ================================================================== the run
def pact_on(r, level):
    return r["pact"] >= level


def rel(id, ed=None, withered=None):
    return {"id": id, "ed": ed, "withered": withered, "tier": 0}


POLISH_COST = [6, 12, 20]  # coin to Polish a relic to tier I, II, then III
POLISH_STEP = 0.5          # each tier: the relic's effect is 50% bigger
POLISH = os.environ.get("POLISH", "1") == "1"


def fire(fn, c, r, tier):
    """Run one relic effect, scaled up by its Polish tier."""
    if not tier:
        fn(c, r)
        return
    before = {k: c[k] for k in ("units", "price", "mult", "coin", "repeats", "x")}
    fn(c, r)
    k = 1 + POLISH_STEP * tier
    for f in ("units", "price", "mult", "coin", "repeats"):
        c[f] = before[f] + (c[f] - before[f]) * k
    if before["x"] and c["x"] != before["x"]:
        ratio = c["x"] / before["x"]
        c["x"] = before["x"] * max(0.0, 1 + (ratio - 1) * k) if ratio > 1 else c["x"]


def new_run(rng, profile=None, skill="average", calling="wayfarer", pact=0, unlocked=None):
    profile = profile or rng.choices(list(PROFILES), [p["w"] for p in PROFILES.values()])[0]
    unlocked = unlocked if unlocked is not None else set(STARTERS) | set(K.TRINKETS) | set(K.SPELLS)
    r = dict(profile=profile, skill=skill, calling=calling, pact=pact, unlocked=unlocked,
             focus=rng.choice(["ember", "tide", "gale"]),
             relics=[], trinkets=[], spells=[START_SPELL], mods=set(), attune={}, glyphs=[],
             coin=START_COIN, night=0, state={}, heat=0.0, standing=defaultdict(float),
             sales=0, new_wells=0.0, last_e=None, kinds_tonight=set(), hollows_tonight=0,
             wards_tonight=1, crown_ban=None, owed=0.0, loans_left=1, ward_seen=Counter(),
             transmute_left=0, veil=False, last_spell=None, bargain=False,
             stats=Counter(), best=Counter(), shop_flags=set(), streak=0, channels=set())
    if calling == "hedge_witch":
        r["relics"] += [rel("static_eater"), rel("static_caller")]
    elif calling == "hermit":
        r["relics"].append(rel("hearthstone"))
    elif calling == "diviner":
        r["mods"] |= {"lodestone", "true_north"}
    elif calling == "cartomancer":
        r["spells"] = ["chalk", "chalk"]
    elif calling == "contractor":
        r["coin"] = 20
        r["owed_n2"] = 0.2 * TITHES[1]
    elif calling == "glitch":
        rares = sorted(k for k in unlocked if k in K.RELICS and K.RELICS[k]["rarity"] == "rare")
        r["relics"] += [rel(k) for k in rng.sample(rares, 2)]
    return r


def owned(r, id):
    ids = r.get("_ids")
    if ids is not None:
        return id in ids
    return any(x["id"] == id for x in r["relics"])


def relic_slots(r):
    if r["calling"] == "pilgrim":
        return 4 - pact_on(r, 7)
    return RELIC_SLOTS + ("sixth_socket" in r["mods"]) + (r["calling"] == "chorus") - pact_on(r, 7)


def slots_used(r):
    return sum(1 for x in r["relics"] if x["ed"] != "phantom")


def spell_slots(r):
    return SPELL_SLOTS + ("fifth_pocket" in r["mods"])


# ================================================================== one visit
def where(r, rng):
    ward = rng.choices(K.WARDS, PROFILES[r["profile"]]["wards"])[0]
    if ward == "hollow":
        n = rng.choices([1, 2, 3], [0.45, 0.33, 0.22])[0]
    elif ward == "fringe":
        n = rng.randint(4, 10)
    elif ward == "ward":
        n = rng.randint(11, 25)
    else:
        n = rng.randint(26, 45)
    return ward, n


def make_visit(r, rng, vi, nx, loc=None):
    prof = PROFILES[r["profile"]]
    ward, n = loc or where(r, rng)
    synd = WORLD["syndicate"][ward]
    faction = rng.choices(["hedge", "warded", "syndicate", "hearth"],
                          [WORLD["hedge"], WORLD["warded"], synd, max(0.0, WORLD["hearth_base"] - synd)])[0]
    hidden = rng.random() < WORLD["hidden"]
    channel = rng.choice(CHANNELS)
    focus = nx["focus"]
    focus_p = 0.5 + 0.15 * ("lodestone" in r["mods"]) + 0.15 * ("fence" in r["trinkets"])
    well_e = "hush" if hidden else "static" if faction == "hedge" else \
        "ember" if channel <= 4 else "tide" if channel <= 9 else "gale"
    transmute = r["transmute_left"] > 0
    if r["calling"] == "pilgrim" or transmute:
        e = focus
    elif owned(r, "hush_lantern") and rng.random() < 0.14:
        e = "hush"
    elif (owned(r, "static_eater") or owned(r, "black_ledger") or owned(r, "smugglers_lining")
          or owned(r, "wanted_poster")) and rng.random() < 0.15:
        e = "static"
    else:
        e = focus if rng.random() < focus_p else well_e
    revisit_p = min(0.85, 0.12 * (r["night"] + 1) * prof["routine"] * (1.25 if r["calling"] == "hermit" else 1))
    revisit = rng.random() < revisit_p
    glyph = None
    if revisit and r["glyphs"] and rng.random() < min(1.0, len(r["glyphs"]) / 5):
        glyph = rng.choice(r["glyphs"])
    new = (not revisit and rng.random() < 0.8) or glyph == "beacon"
    changed = vi > 0 and rng.random() < prof["mobility"]
    one_kind = r["calling"] == "pilgrim" or transmute or rng.random() < focus_p * 0.7
    units = rng.randint(4, 8) + 2 * ("satchel" in r["mods"]) + 3 * ("bottomless_satchel" in r["mods"])
    price = BASE_PRICE[e] * rng.uniform(0.8, 1.25)
    if ward == "hollow":       # scarce wells pay more, so empty places keep up with busy ones
        price *= 1.4
        units *= 0.85
    elif ward == "fringe":
        price *= 1.1
    elif ward == "nexus":
        price *= 0.85
        units *= 1.2
    if r["calling"] == "courier":
        units *= 1.4
    if pact_on(r, 3) and r["kinds_tonight"] and e == r["last_e"]:   # Ash: the same energy twice in a row
        price *= ASH_TIRE
    om = nx["omen"]
    if om == "low_tide":
        units *= 0.75
    if om == "crowded_sky" and ward in ("ward", "nexus"):
        price *= 0.7
    if om == "empty_roads" and ward in ("hollow", "fringe"):
        price *= 0.7
    price *= 1 + 0.2 * r["standing"][faction] / 100
    lvl = r["attune"].get(e, 0)
    noise = {"hollow": (0, 0.3), "fringe": (0, 0.5), "ward": (0.2, 0.8), "nexus": (0.4, 1.0)}[ward]
    dowse_p = 0.33 if "true_north" in r["mods"] else 0.12
    rel_n = n // 2 if om == "fog" else n
    mesh_p = WORLD["mesh"][ward]
    return dict(
        e=e, n=rel_n, real_n=n, ward=ward, faction=faction, hidden=hidden or owned(r, "the_unheard"),
        channel=channel, ch11=max(0, round(rng.gauss(0.25 * min(rel_n, 40), (0.1875 * min(rel_n, 40)) ** 0.5))),
        strong=rng.random() < WORLD["strong"], mesh=rng.randint(2, 4) if rng.random() < mesh_p else 0,
        revisit=revisit, nights_seen=rng.randint(1, r["night"] + 1) if revisit else 1, new=new,
        ward_changed=changed, glyph=glyph, noise=rng.uniform(*noise), still=rng.random() < 0.7,
        battery_low=rng.random() < 0.06, dowsed=rng.random() < dowse_p, one_kind=one_kind,
        hold_kinds=1 if one_kind else rng.randint(2, 4), units=units, price=price + ATTUNE_PRICE * lvl,
        lvl=lvl, mult=1 + ATTUNE_MULT * lvl + lvl * ("tuning_core" in r["mods"]), x=1.0, repeats=0, coin=0.0, vi=vi,
        last_e=r["last_e"], ghost=hidden and owned(r, "ghost_key"),
        wards_tonight=r["wards_tonight"] + (1 if changed else 0),
    )


def score(c, r, nx):
    """Resonance and coin for one sale."""
    om, ecl = nx["omen"], nx["eclipse"]
    if om == "fade" and c["e"] == nx["fade_e"]:
        c["price"] *= 0.5
    if om == "silent_bell" and c["vi"] < 2:
        return 0.0, 0.0
    if r["veil"] and c["e"] == "static":
        c["price"] += 3
    glyph_k = (2 if owned(r, "keystone") else 1) * (1.5 if r["calling"] == "cartomancer" else 1)
    g = c["glyph"]
    if g == "deep":
        c["units"] += 5 * glyph_k
    relics = [x for x in r["relics"] if x["id"] != nx["sleeping"]]
    ids = [x["id"] for x in relics]
    copy_of = None
    if "copycat_sigil" in ids:
        others = [x for x in ids if K.RELICS[x]["fn"] is not None]
        if others:
            copy_of = max(others, key=lambda k: (RANK[K.RELICS[k]["rarity"]], K.RELICS[k]["phase"]))
    price_before = c["price"]
    for x in relics:
        d = K.RELICS[x["id"]]
        if d["phase"] == 0 and d["fn"]:
            tier = x.get("tier", 0) // 2 if nx.get("no_polish") else x.get("tier", 0)
            fire(d["fn"], c, r, tier)
            if x["id"] == copy_of:
                fire(d["fn"], c, r, tier)
    if om == "iron_price" and c["price"] > price_before:
        c["price"] = price_before + (c["price"] - price_before) / 2
    base_mult = c["mult"]
    if g == "lit":
        c["mult"] += 4 * glyph_k
    if g == "bound":
        c["x"] *= 1 + 0.5 * glyph_k
    if g == "echoing":
        c["repeats"] += int(glyph_k)
    times = 2 if ("overtone" in ids and c["units"] >= 10) else 1
    dim = ecl == "dim"
    for phase in (1, 2, 3):
        for x in relics:
            d = K.RELICS[x["id"]]
            if phase == 1 and x["ed"] == "lit":
                c["mult"] += 3
            if phase == 2 and x["ed"] == "prismatic":
                c["x"] *= 1.375 if dim else 1.5
            if d["phase"] != phase or not d["fn"]:
                continue
            for _ in range(times * (2 if x["id"] == copy_of else 1)):
                xb = c["x"]
                fire(d["fn"], c, r, x.get("tier", 0) // 2 if nx.get("no_polish") else x.get("tier", 0))
                if dim and xb and c["x"] > xb:
                    c["x"] = xb * (1 + (c["x"] / xb - 1) * 0.75)
    if om == "tangled":
        mult = base_mult * c["x"] + (c["mult"] - base_mult)
        res = c["units"] * c["price"] * mult * (1 + c["repeats"])
    else:
        res = c["units"] * c["price"] * c["mult"] * c["x"] * (1 + c["repeats"])
    coin = c["units"] * c["price"] * COIN_PER_BASE + c["coin"] + ("pickpocket" in r["trinkets"])
    if g == "gilded":
        coin += 3 * glyph_k
    return max(0.0, res), coin


# ================================================================== one Night
def night_setup(r, rng, night, omen=None, eclipse=None):
    focus = r["focus"]
    if r["calling"] == "glitch":
        focus = rng.choice(K.ENERGIES[:3])
    nx = dict(rng=rng, omen=omen, eclipse=eclipse, fade_e=rng.choice(K.ENERGIES), sleeping=None, focus=focus)
    nx["no_polish"] = eclipse == "sleep"
    r["crown_ban"] = rng.choice(K.ENERGIES)
    if omen == "wardens_walk":
        r["heat"] = max(r["heat"], 3.0)
    return nx


def tithe_for(r, night, omen, eclipse):
    t = TITHES[night] if night < NIGHTS else TITHES[-1]
    for k in range(night - NIGHTS + 1):   # Overnight: x2, then x2.5, x3... (each Night steeper than the last)
        t *= OVERNIGHT_GROWTH + OVERNIGHT_ACCEL * k
    if pact_on(r, 2) and night >= 4:   # Iron: the last three Nights ask more
        t *= IRON_STEEP[min(night, 6) - 4]
    if owned(r, "blood_moon"):
        t *= 1.35
    if omen == "blood_price":
        t *= 1.2
    if eclipse == "hunger":
        t *= 1.25
    if night == 1 and r.get("owed_n2"):
        t += r["owed_n2"]
    return t + r["owed"]


def cast(r, sp, nx):
    """Use a spell. Returns the sale multiplier it gives (1 if none)."""
    r["spells"].remove(sp)
    r["stats"]["spells_cast"] += 1
    if sp != "recall":
        r["last_spell"] = sp
    rng = nx["rng"]
    if sp == "glamour":
        return 2.0
    if sp == "surge":
        return 2.5
    if sp == "transmute":
        r["transmute_left"] = 4
        return 1.6
    if sp == "echo":
        return 1.5
    if sp == "hex":
        r["coin"] += 8
    elif sp == "veil":
        r["veil"] = True
    elif sp == "chalk":
        r["glyphs"].append(rng.choice(sorted(K.GLYPHS)))
        r["stats"]["glyphs_total"] += 1
    elif sp == "kindle":
        plain = [x for x in r["relics"] if not x["ed"]]
        if plain:
            rng.choice(plain)["ed"] = rng.choice(["gilt", "lit", "prismatic"])
    elif sp == "augur":   # the Omen goes, with the visits and Tithe it took (play_night undoes them)
        nx["augured"] = nx["omen"]
        nx["omen"] = None
    elif sp == "recall" and r["last_spell"]:
        r["spells"].append(r["last_spell"])
    elif sp == "bargain":
        r["bargain"] = True
    elif sp == "summon":
        legends = sorted(k for k in r["unlocked"] if k in K.RELICS and K.RELICS[k]["legendary"] and not owned(r, k))
        if legends:
            if slots_used(r) >= relic_slots(r):
                drop_weakest(r)
            r["relics"].append(rel(rng.choice(legends)))
    return 1.0


SALE_SPELLS = ("surge", "glamour", "transmute", "echo")
START_SPELLS = ("chalk", "kindle", "hex", "summon", "recall", "bargain")


def play_night(r, rng, night, tithe, nx, full=False):
    """Play one Night. full=True keeps going after the Tithe is met (to measure the ceiling)."""
    r["night"] = night
    sk = SKILLS[r["skill"]]
    visits = (10 if r["calling"] == "courier" else VISITS) + ("extra_breath" in r["mods"]) + \
        ("second_wind" in r["mods"]) - 2 * owned(r, "overclock")
    if nx["omen"] == "short_night":
        visits -= 2
    if nx["eclipse"] == "dark":
        visits -= 2
    r.update(kinds_tonight=set(), hollows_tonight=0, wards_tonight=1, veil=False, transmute_left=0, new_tonight=0)
    if not full:
        if "chalk_pouch" in r["trinkets"] and len(r["spells"]) < spell_slots(r):
            r["spells"].append("chalk")
        for sp in list(r["spells"]):
            # AUGUR can't call off a Wardens walk: its Heat can't be put back
            if sp in START_SPELLS or (sp == "augur" and nx["omen"] and nx["omen"] != "wardens_walk") or \
                    (sp == "veil" and owned(r, "black_ledger")):
                cast(r, sp, nx)
        if nx.get("augured") == "short_night":
            visits += 2
        if nx.get("augured") == "blood_price":
            tithe = nx["tithe_unomened"]
    r["_ids"] = {x["id"] for x in r["relics"]}
    r["_slots"], r["_used"] = relic_slots(r), slots_used(r)
    total, coin, met_at, best_sale, vi, candles = 0.0, 0.0, None, 0.0, 0, 0
    while True:
        if vi >= visits:
            # out of visits and short: light a candle for one more, if the purse allows
            price = CANDLES[min(candles, len(CANDLES) - 1)] * (2 if pact_on(r, 6) else 1)
            if full or met_at is not None or candles >= len(CANDLES) or r["coin"] < price:
                break
            r["coin"] -= price
            candles += 1
            visits += 1
            r["stats"]["candles"] += 1
        loc = where(r, rng)
        k = min(loc[1], CHOICES[loc[0]])
        cands = []
        for _ in range(k):
            cc = make_visit(r, rng, vi, nx, loc)
            cands.append((score(dict(cc), r, nx)[0], cc))
        if full or rng.random() < sk["pick"]:
            c = max(cands, key=lambda t: t[0])[1]
        else:
            c = rng.choice(cands)[1]
        res, sale_coin = score(c, r, nx)
        if r["heat"] >= 3 and r["calling"] != "hedge_witch":
            if rng.random() < WARDEN_BLOCK * (1.7 if owned(r, "devils_meter") else 1):
                res, sale_coin = 0.0, 0.0
            elif not full:
                r["stats"]["warden_escapes"] += 1
        if not full and met_at is None:
            avg = total / vi if vi else res
            danger = total + avg * (visits - vi) < tithe * sk["margin"]
            if danger and "haste" in r["spells"]:
                cast(r, "haste", nx)
                visits += 2
            if danger and res > 0:
                for sp in SALE_SPELLS:
                    if sp in r["spells"]:
                        res *= cast(r, sp, nx)
                        break
        last = (met_at is None and total + 3 * res >= tithe) if not full else vi == visits - 1
        if last and owned(r, "witching_hour") and nx["sleeping"] != "witching_hour":
            res *= 3
        r["transmute_left"] = max(0, r["transmute_left"] - 1)
        # bookkeeping
        if owned(r, "mixtape") and r["last_e"] is not None and c["e"] != r["last_e"]:
            r["state"]["mixtape"] = r["state"].get("mixtape", 0) + 3.0 * GROW
        if owned(r, "prism_jar") and c["e"] not in r["kinds_tonight"]:
            r["state"]["prism_jar"] = r["state"].get("prism_jar", 0) + 0.3 * GROW
        if owned(r, "pedometer"):
            r["state"]["pedometer"] = r["state"].get("pedometer", 0) + 0.9 * GROW
        if owned(r, "rain_gauge"):   # the simulator's visit is a draw and an offering
            r["state"]["rain_gauge"] = r["state"].get("rain_gauge", 0) + 1.5 * GROW
        r["last_e"] = c["e"]
        r["sales"] += 1
        r["kinds_tonight"].add(c["e"])
        r["wards_tonight"] = c["wards_tonight"]
        r["hollows_tonight"] += c["ward"] == "hollow"
        r["ward_seen"][c["ward"]] += 1
        st, s, b = r["state"], r["stats"], r["best"]
        if c["new"]:
            r["new_wells"] += 1
            r["new_tonight"] = r.get("new_tonight", 0) + 1
            if owned(r, "pilgrims_staff"):
                st["pilgrims_staff"] = st.get("pilgrims_staff", 0) + 0.15
        if c["hidden"] and owned(r, "locked_diary"):
            st["locked_diary"] = st.get("locked_diary", 0) + 0.45
        if c["faction"] == "hearth" and owned(r, "picket_fence"):
            st["picket_fence"] = st.get("picket_fence", 0) + 0.75
        if c["e"] == "static":
            if owned(r, "black_ledger"):
                st["black_ledger"] = st.get("black_ledger", 0) + 0.6
            if not (owned(r, "static_eater") or r["veil"]):
                r["heat"] = min(4.0, r["heat"] + 0.5)
            r["standing"]["warded"] -= 2
            r["standing"]["syndicate"] -= 2
        r["heat"] = round(max(0.0, r["heat"] - 0.1), 1)   # whole tenths, as on the Deck
        r["standing"][c["faction"]] = min(100, r["standing"][c["faction"]] + 1)
        if not full:
            s["sales_total"] += 1
            s[f"{c['e']}_sold"] += 1
            s["hidden_found"] += c["hidden"]
            s["visit_nexus"] += c["ward"] == "nexus"
            s["hollow_sales"] += c["ward"] == "hollow"
            s["hearth_sales"] += c["faction"] == "hearth"
            s["dowses"] += c["dowsed"]
            s["new_wells"] += c["new"]
            b["max_networks"] = max(b["max_networks"], c["real_n"])
            b["max_heat"] = max(b["max_heat"], r["heat"])
            if c["ward"] == "nexus":
                b["big_sale_nexus"] = max(b["big_sale_nexus"], res)
            b["big_units"] = max(b["big_units"], c["units"])
            r["channels"].add(c["channel"])
        best_sale = max(best_sale, res)
        total += res
        if met_at is None:
            coin += sale_coin
            if total >= tithe:
                met_at = vi + 1
                if not full:
                    break
        vi += 1
    if not full:
        b = r["best"]
        b["kinds_night"] = max(b["kinds_night"], len(r["kinds_tonight"]))
        b["ward_changes_night"] = max(b["ward_changes_night"], r["wards_tonight"] - 1)
        b["hollows_one_run"] = max(b["hollows_one_run"], r["ward_seen"]["hollow"])
        b["new_wells_night"] = max(b["new_wells_night"], r.get("new_tonight", 0))
    r.pop("_ids", None)
    used = met_at or visits
    return dict(paid=met_at is not None, total=total, used=used, visits=visits, coin=coin, best=best_sale,
                candles=candles, tithe=tithe)


def snapshot(r):
    """A copy of the run that a full-night measurement can scribble on."""
    t = dict(r)
    t["relics"] = [dict(x) for x in r["relics"]]
    t["state"] = dict(r["state"])
    t["standing"] = defaultdict(float, r["standing"])
    t["ward_seen"] = Counter(r["ward_seen"])
    t["kinds_tonight"] = set()
    t["attune"] = dict(r["attune"])
    t["glyphs"] = list(r["glyphs"])
    t["mods"] = set(r["mods"])
    t["shop_flags"] = set(r["shop_flags"])
    t.pop("_ids", None)
    return t


def power(r, rng, night, omen=None, eclipse=None, samples=2, seed=None):
    """Average full-night Resonance with the build as it stands (the bot's eyes, and the ceiling stat).
    With `seed`, every call plays the same Nights, so two builds are compared fairly."""
    tot = 0.0
    for i in range(samples):
        if seed is not None:
            rng = random.Random(seed * 1000 + i)
        t = snapshot(r)
        t["spells"] = []
        nx = night_setup(t, rng, night, omen, eclipse)
        tot += play_night(t, rng, night, float("inf"), nx, full=True)["total"]
    return tot / samples


# ================================================================== the shop
def roll_relic(r, rng, rarity=None, multiplier=False):
    w = dict(RARITY_W)
    w["rare"] += RARE_GROWTH * r["night"]
    w["uncommon"] += UNCOMMON_GROWTH * r["night"]
    if "lucky_coin" in r["trinkets"]:
        w["rare"] *= 2
    rarity = rarity or rng.choices(list(w), list(w.values()))[0]
    have = {x["id"] for x in r["relics"]}
    schools = {K.RELICS[x["id"]]["school"] for x in r["relics"]}
    pool = sorted(k for k in r["unlocked"] if k in K.RELICS and K.RELICS[k]["rarity"] == rarity
                  and k not in have and not K.RELICS[k]["legendary"]
                  and (not multiplier or K.RELICS[k]["phase"] in (2, 3))
                  and not (r["calling"] == "pilgrim" and k.endswith("_caller") and k != r["focus"] + "_caller"))
    if multiplier and not pool:
        return roll_relic(r, rng, "uncommon" if rarity == "common" else None, True) if rarity == "common" else None
    if not pool:
        return None
    if schools and rng.random() < BUILD_BIAS:   # the Collector stocks what suits your build
        fit = [k for k in pool if K.RELICS[k]["school"] in schools]
        if fit:
            pool = fit
    seen = sum(r["ward_seen"].values())
    if seen and rng.random() < LOCALE_BIAS:
        mine = {wd for wd, n in r["ward_seen"].items() if n / seen >= 0.25}
        fit = [k for k in pool if K.RELICS[k]["suits"] == "all" or mine & set(K.RELICS[k]["suits"].split())]
        pool = fit or pool
    k = rng.choice(pool)
    if not multiplier and K.RELICS[k]["phase"] in (2, 3) and rng.random() > MULT_KEEP:
        plain = [x for x in pool if K.RELICS[x]["phase"] < 2]   # a multiplier turned away: something that adds
        if plain:
            k = rng.choice(plain)
    if not multiplier and rng.random() < GROWER_PULL:   # the Collector stocks what grows
        grow = [x for x in pool if x in GROWING_RELICS]
        if grow:
            k = rng.choice(grow)
    ed = None
    ed_mult = (3 if "collectors_eye" in r["trinkets"] else 1) * (3 if r["calling"] == "collector" else 1)
    for name, (_, _, p) in K.EDITIONS.items():   # a Shine Portent waits for the stall's shelf (shop)
        if rng.random() < p * ed_mult * (1 + EDITION_GROWTH * r["night"]):
            ed = name
            break
    return rel(k, ed, 3 if (pact_on(r, 4) and rng.random() < 0.3) else None)


def relic_price(r, item):
    """A relic's full price; selling one back pays half of this, whatever the stall's discounts."""
    return (PRICE[K.RELICS[item["id"]]["rarity"]] + (PRICE["edition"] if item["ed"] else 0)
            + (r["calling"] in ("collector", "chorus")))


def cost(r, kind, item=None):
    if kind == "relic":
        p = relic_price(r, item)
    else:
        p = PRICE[kind]
    if "clearance" in r["mods"]:
        p -= 1
    if r["bargain"] or (item is not None and item.get("half")):
        p //= 2
    return max(1, p)


GROW = float(os.environ.get("GROW", 1))   # the new growing relics' steps, times this (tuning)
GROWTH = {"pedometer": 4.5, "mixtape": 7.5, "rain_gauge": 6.0, "prism_jar": 0.75, "seed_vault": 1.2,
          "tally_stick": 3.0, "ritual_candle": 0.45, "patience": 18.0, "pilgrims_staff": 0.6,
          "black_ledger": 0.6, "picket_fence": 2.25, "piggy_bank": 3.0, "locked_diary": 0.45,
          "long_receipt": 0.2}   # expected gain per Night, for the bot's lookahead


def cost_polish(r, x):
    return round(POLISH_COST[x["tier"]] * (2 if pact_on(r, 5) else 1))


def drop_weakest(r):
    cands = [x for x in r["relics"] if x["ed"] != "phantom"]
    if not cands:
        return None
    x = min(cands, key=lambda x: (RANK[K.RELICS[x["id"]]["rarity"]], x["ed"] is not None))
    r["relics"].remove(x)
    return x


def shop(r, rng, night):
    """The Collector's shop after Night `night` (0-based)."""
    sk = SKILLS[r["skill"]]
    _coin_in = r["coin"]
    n_rel = SHOP_RELICS + ("overstock" in r["mods"])
    if max(r["standing"].values(), default=0) >= 30:
        n_rel += 1    # a faction stall opens
    rerolls_left = sk["rerolls"]
    free = ("haggler" in r["trinkets"]) + 2 * ("free_rerolls" in r["shop_flags"])
    r["shop_flags"].discard("free_rerolls")
    reroll_cost = (8 if pact_on(r, 5) else REROLL) - 2 * ("liquidation" in r["mods"]) + 2 * (r["calling"] == "night_clerk")

    def offers_now(first):
        caller = night == 0 and first   # the first shop always has a Caller for your focus energy
        offs = [rel(r["focus"] + "_caller")] if caller else []
        offs += [roll_relic(r, rng) for _ in range(n_rel - 1 - len(offs))]
        offs.append(roll_relic(r, rng, multiplier=True) if night >= 1 and MULT_SLOT else roll_relic(r, rng))
        if "rare_sight" in r["shop_flags"]:   # a rare on top of the usual shelf
            offs.append(roll_relic(r, rng, "rare"))
            r["shop_flags"].discard("rare_sight")
        seen, out = set(), []
        for o in offs:
            if o and not owned(r, o["id"]) and o["id"] not in seen:
                seen.add(o["id"])
                out.append(o)
        if first and "shine" in r["shop_flags"]:   # the Shine Portent: on a relic that stays, never the Caller
            r["shop_flags"].discard("shine")
            cands = [o for o in out if not (caller and o["id"] == r["focus"] + "_caller")]
            plain = [o for o in cands if not o["ed"]]
            if cands:
                rng.choice(plain or cands)["ed"] = rng.choice(["gilt", "lit", "prismatic"])
        return out

    offers = offers_now(True)
    mercy = "mercy" in r["shop_flags"]
    if mercy:
        r["shop_flags"].discard("mercy")
        m = roll_relic(r, rng, multiplier=True)
        if m and not owned(r, m["id"]) and all(o["id"] != m["id"] for o in offers):
            m["half"] = True
            offers.append(m)
        free += 1
        r["attune"][r["focus"]] = r["attune"].get(r["focus"], 0) + 1
    cons = rng.choices(list(CONSUMABLE_W), list(CONSUMABLE_W.values()),
                       k=SHOP_CONSUMABLES + ("night_market" in r["mods"]))
    tpool = sorted(t for t in K.TRINKETS if t in r["unlocked"] and t not in r["trinkets"]
                   and not (t == "moneylender" and pact_on(r, 6)))   # no loans from OBSIDIAN up
    trinket = rng.choice(tpool) if tpool and rng.random() < TRINKET_OFFER_P else None
    useless = {"sixth_socket", "lodestone"} if r["calling"] == "pilgrim" else set()   # 4 slots, and all focus
    tier2 = sorted(m for m, v in K.MODS.items() if v[2] and (v[2] in r["mods"] or v[2] in useless)
                   and m not in r["mods"] and m not in useless)
    tier1 = sorted(m for m, v in K.MODS.items() if not v[2] and m not in r["mods"] and m not in useless)
    mod = rng.choice(tier2 + tier1) if (tier2 or tier1) else None

    smp = sk["samples"]
    while True:
        ev = rng.randrange(1 << 30)
        base = power(r, rng, night + 1, samples=smp, seed=ev)
        while True:   # buy the best relic while it's worth it
            best, best_gain = None, 0.0
            best_drop = None
            for o in offers:
                if cost(r, "relic", o) > r["coin"]:
                    continue
                full = o["ed"] != "phantom" and slots_used(r) >= relic_slots(r)
                if not full:
                    drops = [None]
                elif sk["smart_swap"]:
                    drops = [x for x in r["relics"] if x["ed"] != "phantom"]
                else:
                    t = dict(r, relics=list(r["relics"]))
                    drops = [drop_weakest(t)]
                for d in drops:
                    trial = dict(r, relics=[x for x in r["relics"] if x is not d] + [o])
                    grow = GROWTH.get(o["id"])
                    if grow:   # people buy growing relics for what they'll become, not what they are
                        trial["state"] = dict(r["state"])
                        trial["state"][o["id"]] = trial["state"].get(o["id"], 0) + grow * (NIGHTS - 1 - night) / 2
                    gain = power(trial, rng, night + 1, samples=smp, seed=ev) / max(base, 1) - 1
                    gain *= rng.uniform(1 - sk["noise"], 1 + sk["noise"])
                    if rng.random() < sk["random_buy"]:
                        gain = rng.uniform(0, 0.3)
                    if gain > best_gain:
                        best, best_gain, best_drop = o, gain, d
            if not best or best_gain < 0.05:
                break
            r["coin"] -= cost(r, "relic", best)
            if best_drop is not None:
                r["relics"].remove(best_drop)
                gone = best_drop
                back = relic_price(r, gone)
                r["coin"] += back if "pawn_ticket" in r["trinkets"] else back // 2
                r["stats"]["relics_sold"] += 1
            r["relics"].append(best)
            offers.remove(best)
            r["stats"]["rares_bought"] += K.RELICS[best["id"]]["rarity"] == "rare"
            base = power(r, rng, night + 1, samples=smp, seed=ev)
        while POLISH:   # Polish the relic that gains the most per coin
            cands = [x for x in r["relics"] if K.RELICS[x["id"]]["fn"] and x["tier"] < len(POLISH_COST)
                     and r["coin"] - cost_polish(r, x) >= (sk["keep"] if night >= 1 else 0)]
            if not cands:
                break
            best, best_val = None, 0.0
            for x in cands:
                x["tier"] += 1
                gain = power(r, rng, night + 1, samples=smp, seed=ev) / max(base, 1) - 1
                x["tier"] -= 1
                gain *= rng.uniform(1 - sk["noise"], 1 + sk["noise"])
                if gain / cost_polish(r, x) > best_val:
                    best, best_val = x, gain / cost_polish(r, x)
            if not best or best_val * cost_polish(r, best) < 0.04:
                break
            r["coin"] -= cost_polish(r, best)
            best["tier"] += 1
            r["stats"]["polishes"] += 1
            base = power(r, rng, night + 1, samples=smp, seed=ev)
        if mod and r["coin"] >= cost(r, "mod") + 2 and night < 5:
            r["coin"] -= cost(r, "mod")
            r["mods"].add(mod)
            mod = None
        if trinket and len(r["trinkets"]) < TRINKET_SLOTS and r["coin"] >= cost(r, "trinket") + 2 and night < 5:
            r["coin"] -= cost(r, "trinket")
            r["trinkets"].append(trinket)
            if trinket == "moneylender":
                r["loans_left"] += 1
            trinket = None
        for item in list(cons):
            p = cost(r, item)
            if r["coin"] < p + 1:
                continue
            if item == "attune":
                sold = [r["stats"][f"{en}_sold"] + 1 for en in K.ENERGIES]
                e = rng.choices(K.ENERGIES, sold)[0]
                if e == r["focus"] or r["calling"] == "glitch" or rng.random() < 0.15:
                    r["coin"] -= p
                    lv = 2 if ("attuners_fork" in r["mods"] and rng.random() < 0.33) else 1
                    tgt = r["focus"] if r["calling"] != "glitch" else rng.choice(K.ENERGIES[:3])
                    r["attune"][tgt] = r["attune"].get(tgt, 0) + lv
                    r["stats"]["attunes_run"] += 1
                cons.remove(item)
            elif item == "spell" and len(r["spells"]) < spell_slots(r):
                pool = sorted(s for s in K.SPELLS if s in r["unlocked"] and not (s == "transmute" and r["calling"] == "pilgrim"))
                r["coin"] -= p
                r["spells"].append(rng.choices(pool, [K.SPELLS[s][4] for s in pool])[0])
                cons.remove(item)
            elif item == "glyph" and 1 <= night < 6:
                r["coin"] -= p
                r["glyphs"].append(rng.choice(sorted(K.GLYPHS)))
                r["stats"]["glyphs_total"] += 1
                cons.remove(item)
        if r["coin"] == 0:
            r["stats"]["broke_shop"] += 1
        if free > 0:
            free -= 1
        elif rerolls_left > 0 and r["coin"] - reroll_cost >= (sk["keep"] if night >= 1 else 6):
            r["coin"] -= reroll_cost
            reroll_cost += REROLL_STEP
            rerolls_left -= 1
        else:
            break
        offers = offers_now(False)
    r["bargain"] = False
    r["best"]["max_coin"] = max(r["best"]["max_coin"], r["coin"])
    if owned(r, "long_receipt"):   # every coin spent at this stall
        r["state"]["long_receipt"] = min(2.0, 0.05 * max(0, _coin_in - r["coin"]))


PORTENT_W = [3, 2, 3, 2, 2, 2, 2, 2, 2, 0.3]
FLAG_PORTENTS = ("rare_sight", "free_rerolls", "shine", "doubled_interest")   # a second at one dawn would do nothing


def portent_value(r, p):
    """What a Portent is worth to this build, for keeping one of two (Game.h choosePortent; the bot's ranking)."""
    if p == "conjunction":
        legends = [k for k in r["unlocked"] if k in K.RELICS and K.RELICS[k]["legendary"] and not owned(r, k)]
        return (100 if slots_used(r) < relic_slots(r) else 20) if legends else 0
    return {"gift": 60 if slots_used(r) < relic_slots(r) else 20, "rare_sight": 40 if r["coin"] >= 10 else 15,
            "windfall": 35, "tuning": 30, "shine": 25, "free_rerolls": 20,
            "doubled_interest": 3 * min(INTEREST_CAP, int(r["coin"] // INTEREST_PER)),
            "scroll": 10 if len(r["spells"]) < spell_slots(r) else 0, "chalk_mark": 3}[p]


def roll_portent(r, rng, rolled, avoid=()):
    """A Portent at random (Game.h rollPortent): never a stall Portent already rolled at this
    dawn, nor a Conjunction with no legendary left to give."""
    names = list(K.PORTENTS)
    legends = [k for k in r["unlocked"] if k in K.RELICS and K.RELICS[k]["legendary"] and not owned(r, k)]
    w = [0 if p in avoid or (p in FLAG_PORTENTS and p in rolled)
         or (p == "conjunction" and len(legends) <= rolled.count("conjunction")) else wt
         for p, wt in zip(names, PORTENT_W)]
    p = rng.choices(names, w if sum(w) > 0 else PORTENT_W)[0]
    rolled.append(p)
    return p


def apply_portent(r, rng, choose=False, rolled=None):
    rolled = [] if rolled is None else rolled
    p = roll_portent(r, rng, rolled)
    if choose:   # one of two: the dawn offers another and the player keeps the one worth more
        q = roll_portent(r, rng, rolled, avoid=(p,))
        if portent_value(r, q) > portent_value(r, p):
            p = q
    if p == "windfall":
        r["coin"] += 10
    elif p == "doubled_interest":
        r["shop_flags"].add("double_interest")
    elif p in ("rare_sight", "free_rerolls", "shine"):
        r["shop_flags"].add(p)
    elif p == "tuning":
        r["attune"][r["focus"]] = r["attune"].get(r["focus"], 0) + 1
    elif p == "scroll" and len(r["spells"]) < spell_slots(r):
        r["spells"].append(rng.choice(sorted(s for s in K.SPELLS if s in r["unlocked"] and s != "summon")))
    elif p == "chalk_mark":
        r["glyphs"].append(rng.choice(sorted(K.GLYPHS)))
        r["stats"]["glyphs_total"] += 1
    elif p == "gift":
        o = roll_relic(r, rng, "rare" if r["night"] >= 3 else "uncommon")
        if o:
            if slots_used(r) >= relic_slots(r):
                drop_weakest(r)
            r["relics"].append(o)
    elif p == "conjunction":
        legends = sorted(k for k in r["unlocked"] if k in K.RELICS and K.RELICS[k]["legendary"] and not owned(r, k))
        if legends:
            if slots_used(r) >= relic_slots(r):
                drop_weakest(r)
            r["relics"].append(rel(rng.choice(legends)))
    r["stats"]["portents"] += 1


# ================================================================== a whole Binding
def play_run(args):
    seed, kw = args
    rng = random.Random(seed)
    r = new_run(rng, **kw)
    omens = [None] + [rng.choice(sorted(K.OMENS)) for _ in range(NIGHTS - 2)]
    eclipse = rng.choice(sorted(K.ECLIPSES))
    out = dict(profile=r["profile"], calling=r["calling"], pact=r["pact"], ratios=[], used=[], fails_at=None,
               overnight=0, omens=omens, eclipse=eclipse, loan=False, coin_at=[], bought=0)
    night = 0
    while True:
        in_week = night < NIGHTS
        if in_week:
            omen = omens[night] if night < NIGHTS - 1 else None
            ecl = eclipse if night == NIGHTS - 1 else None
        else:
            omen = rng.choice(sorted(K.OMENS))
            ecl = rng.choice(sorted(K.ECLIPSES)) if (night - NIGHTS) % 3 == 2 else None
        if omen and "charm_ward" in r["mods"] and rng.random() < 0.5:
            omen = None
        if omen and r["calling"] == "diviner" and rng.random() < 0.3:
            omen = None
        if ecl and "eclipse_ward" in r["mods"] and rng.random() < 0.5:
            ecl = None
        r["stats"]["omens_seen"] += omen is not None
        tithe = tithe_for(r, night, omen, ecl)
        unomened = tithe_for(r, night, None, ecl)   # for an AUGUR cast on a Blood price
        r["owed"] = 0.0
        if in_week:
            out["ratios"].append(power(r, rng, night, omen, ecl) / tithe)
        nx = night_setup(r, rng, night, omen, ecl)
        nx["tithe_unomened"] = unomened
        res = play_night(r, rng, night, tithe, nx)
        tithe = res["tithe"]   # AUGUR may have cleared a Blood price
        if in_week:
            out["used"].append(res["used"])
        paid = res["paid"]
        if paid and owned(r, "patience"):
            r["state"]["patience"] = r["state"].get("patience", 0) + 3 * (res["visits"] - res["used"])
        if not paid and in_week and night < NIGHTS - 1 and r["loans_left"] > 0 and not pact_on(r, 6) \
                and res["total"] >= tithe * LOAN_FLOOR:
            r["loans_left"] -= 1
            paid = True
            out["loan"] = True
            r["owed"] = (tithe - res["total"]) * (1.1 if "moneylender" in r["trinkets"] else LOAN_INTEREST)
            r["stats"]["loans"] += 1
            res["coin"], res["used"] = 0, res["visits"]
        elif paid and r.get("had_loan"):
            r["stats"]["loans_repaid"] += 1
        r["had_loan"] = r["owed"] > 0
        if not paid:
            if in_week:
                out["fails_at"] = night + 1
            break
        r["stats"]["nights_paid"] += 1
        for k, step in (("tally_stick", 3.0), ("ritual_candle", 0.45)):
            if owned(r, k):
                r["state"][k] = r["state"].get(k, 0) + step
        spare = res["visits"] - res["used"]
        if MERCY and (spare <= 1 or res.get("candles") or out["loan"] and r["owed"] > 0):
            r["shop_flags"].add("mercy")
            r["stats"]["mercies"] += 1
        r["streak"] = r["streak"] + 1 if spare >= 6 else 0
        r["best"]["fast_closes"] = max(r["best"]["fast_closes"], r["streak"])
        r["stats"]["one_visit_close"] += res["used"] == 1
        r["stats"]["op_nights"] += res["total"] >= 3 * tithe or spare >= 8
        if not in_week:
            out["overnight"] += 1
            if out["overnight"] >= 10:
                break
        cap = INTEREST_CAP + 5 * ("misers_purse" in r["trinkets"])
        if r["calling"] == "night_clerk":
            cap *= 2
        interest = min(cap, int(r["coin"] // INTEREST_PER))
        if owned(r, "piggy_bank"):
            r["state"]["piggy_bank"] = r["state"].get("piggy_bank", 0) + 1.5 * interest
        if owned(r, "seed_vault"):
            r["state"]["seed_vault"] = r["state"].get("seed_vault", 0) + 0.3 * interest * GROW
        gilt = 2 * sum(1 for x in r["relics"] if x["ed"] == "gilt")
        thread = 4 if ("golden_thread" in r["trinkets"] and spare >= 4) else 0
        skim = min(10, max(0, int((res["total"] / tithe - 1) / 0.5))) if "tithe_skimmer" in r["trinkets"] else 0
        reward = 0 if pact_on(r, 1) else NIGHT_REWARD   # Lead: the Tithe pays no coin
        r["coin"] = max(0, r["coin"] + int(res["coin"]) + spare + reward + interest + gilt + thread + skim)
        for x in list(r["relics"]):   # withered relics count down before the dawn's own relics arrive
            if x["withered"] is not None:
                x["withered"] -= 1
                if x["withered"] <= 0:
                    r["relics"].remove(x)
        rolled = []
        for _ in range(sum(spare >= a for a in PORTENT_AT)):
            apply_portent(r, rng, choose=r["pact"] < PORTENT_CHOICE_BELOW, rolled=rolled)
        if "double_interest" in r["shop_flags"]:    # Doubled interest: tonight's interest pays again
            r["shop_flags"].discard("double_interest")
            r["coin"] += interest
            if owned(r, "piggy_bank"):   # interest earned, doubled or not
                r["state"]["piggy_bank"] = r["state"].get("piggy_bank", 0) + 1.5 * interest
        if night == NIGHTS - 1:
            out["won"] = True
            rs = out["ratios"]
            broke = any(rs[k] >= OP_RATIO and min(rs[k:]) >= OP_HOLD for k in range(2, NIGHTS))
            out["result"] = "op" if broke else "scraped"
            out["relics_at_win"] = [x["id"] for x in r["relics"]]
        if in_week:
            out["coin_at"].append(r["coin"])
        night += 1
        before = len(r["relics"]) + len(r["mods"]) + len(r["trinkets"])
        shop(r, rng, night - 1)
        if in_week:
            out["bought"] += max(0, len(r["relics"]) + len(r["mods"]) + len(r["trinkets"]) - before)
    if not out.get("won"):
        out["result"] = "fail"
    out["relics"] = out.get("relics_at_win") or [x["id"] for x in r["relics"]]
    out["editions"] = sum(1 for x in r["relics"] if x["ed"])
    out["stats"], out["best"] = dict(r["stats"]), dict(r["best"])
    out["best"]["attune_level"] = max(r["attune"].values(), default=0)
    out["best"]["editions_owned"] = out["editions"]
    out["best"]["rares_one_run"] = sum(1 for x in r["relics"] if K.RELICS[x["id"]]["rarity"] == "rare")
    out["best"]["new_wells_run"] = r["new_wells"]
    out["best"]["attunes_run"] = r["stats"]["attunes_run"]
    out["channels"] = sorted(r["channels"])
    return out


def batch(n, seed=1, **kw):
    with Pool() as p:
        return p.map(play_run, [(seed * 1_000_003 + i, kw) for i in range(n)], chunksize=8)


# ================================================================== reports
def pct(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p / 100 * len(xs)))] if xs else 0


def mix(outs):
    n = len(outs) or 1
    c = Counter(o["result"] for o in outs)
    return c["op"] / n, c["scraped"] / n, c["fail"] / n


def fmt_mix(outs):
    op, sc, fa = mix(outs)
    return f"overpowered {op:4.0%}   scraped by {sc:4.0%}   failed {fa:4.0%}"


FULL = None


def full_pool():
    return set(K.RELICS) | set(K.TRINKETS) | set(K.SPELLS)


def report_run(n):
    outs = batch(n, 11)
    print(f"== {n} runs: average skill, Wayfarer, Chalk Pact, starting pool")
    print("  ", fmt_mix(outs))
    fails = Counter(o["fails_at"] for o in outs if o["fails_at"])
    print("   fails by Night:", "  ".join(f"N{k} {fails[k]/n:.1%}" for k in range(1, NIGHTS + 1)))
    print(f"   runs a Syndicate loan kept alive: {sum(o['loan'] and o['result'] != 'fail' for o in outs)/n:.0%}")
    for prof in PROFILES:
        sub = [o for o in outs if o["profile"] == prof]
        print(f"   {prof:9} ({len(sub):4}) {fmt_mix(sub)}")
    print("   Night   power ÷ Tithe:  median  top 10%  top 1%     visits used: median  fastest 10%   coin after: median")
    for k in range(NIGHTS):
        xs = [o["ratios"][k] for o in outs if len(o["ratios"]) > k]
        us = [o["used"][k] for o in outs if len(o["used"]) > k]
        cs = [o["coin_at"][k] for o in outs if len(o["coin_at"]) > k]
        print(f"   N{k+1}                  {pct(xs,50):6.2f}  {pct(xs,90):6.2f} {pct(xs,99):7.2f}"
              f"                  {pct(us,50):3}          {pct(us,10):3}               {pct(cs,50):3}")
    print(f"   things bought per run (relics, mods, trinkets): median {pct([o['bought'] for o in outs], 50)}")
    for kind in ("op", "scraped"):
        sub = [o["overnight"] for o in outs if o["result"] == kind]
        if sub:
            print(f"   Overnight Nights survived after a win ({kind}): median {pct(sub,50)}, top 10% {pct(sub,90)}")
    om = defaultdict(list)
    for o in outs:
        for k, name in enumerate(o["omens"]):
            if name and len(o["used"]) > k:
                om[name].append(o["fails_at"] == k + 1)
    print("   fail rate on a Night with this Omen:",
          ", ".join(f"{k} {sum(v)/len(v):.1%}" for k, v in sorted(om.items(), key=lambda kv: -sum(kv[1]) / len(kv[1]))))
    ec = defaultdict(list)
    for o in outs:
        if len(o["used"]) == NIGHTS:
            ec[o["eclipse"]].append(o["fails_at"] == NIGHTS)
    print("   Night 7 fail rate by Eclipse:", ", ".join(f"{k} {sum(v)/len(v):.1%}" for k, v in sorted(ec.items())))
    for sk in ("novice", "expert"):
        print(f"   {sk:8} players: {fmt_mix(batch(n // 2, 12, skill=sk))}")
    return outs


def report_relics(n):
    outs = batch(n, 21, unlocked=full_pool())
    base = 1 - mix(outs)[2]
    print(f"== relics: {n} runs with everything unlocked (win rate {base:.0%})")
    print(f"   {'relic':20} {'kept':>6} {'win when kept':>14} {'overpowered when kept':>22}")
    rows = []
    for k in K.RELICS:
        held = [o for o in outs if k in o["relics"]]
        op, _, fa = mix(held) if held else (0, 0, 1)
        rows.append((k, len(held) / n, 1 - fa if held else 0, op))
    for k, h, w, op in sorted(rows, key=lambda x: -x[3]):
        flag = ""
        if h < 0.02 and not K.RELICS[k]["legendary"]:
            flag = "  <- rarely kept"
        elif h > 0.35 and op > 0.8:
            flag = "  <- dominant"
        print(f"   {k:20} {h:6.0%} {w:14.0%} {op:22.0%}{flag}")
    pairs = Counter()
    for o in outs:
        if o["result"] == "op":
            rs = sorted(set(o["relics"]))
            for i in range(len(rs)):
                for j in range(i + 1, len(rs)):
                    pairs[(rs[i], rs[j])] += 1
    print("   commonest pairs in overpowered wins:", "; ".join(f"{a}+{b} {v}" for (a, b), v in pairs.most_common(8)))
    schools = Counter()
    for o in outs:
        if o["result"] == "op":
            sc = Counter(K.RELICS[k]["school"] for k in o["relics"]).most_common(1)
            if sc:
                schools[sc[0][0]] += 1
    tot = sum(schools.values()) or 1
    print("   main school of overpowered builds:", ", ".join(f"{k} {v/tot:.0%}" for k, v in schools.most_common()))


def report_callings(n):
    print(f"== Callings: {n} runs each, everything unlocked")
    for cl in K.CALLINGS:
        outs = batch(n, 31, calling=cl, unlocked=full_pool())
        top = Counter(k for o in outs if o["result"] == "op" for k in o["relics"]).most_common(3)
        print(f"   {cl:12} {fmt_mix(outs)}   top relics: {', '.join(k for k, _ in top)}")


def report_pacts(n):
    print(f"== Pacts: {n} runs each, everything unlocked, Wayfarer (win rate)")
    for i, (name, _) in enumerate(K.PACTS):
        line = [f"{sk} {1 - mix(batch(n, 41 + i, pact=i, skill=sk, unlocked=full_pool()))[2]:4.0%}"
                for sk in ("novice", "average", "expert")]
        print(f"   {i} {name:9} " + "   ".join(line))


# ------------------------------------------------------------------ career: many weeks of play
def unlocked_by(career, cond):
    key, need = cond[0], cond[1]
    if key == "win_with":
        return need in career["won_with"]
    if key == "school_win":
        return need in career["school_wins"]
    if key == "callings_won":
        return len(career["callings_won"]) >= need
    if key == "pact_win":
        return career["best_pact"] >= need - 1
    if key == "channels_seen":
        return len(career["channels"]) >= need
    if key == "unique_wells":
        return career["stats"]["new_wells"] * 0.5 >= need
    if key == "same_well_nights":
        return career["same_well"] >= 1
    if key == "low_battery_close":
        return career["low_battery"] >= 1
    if key == "wins":
        return career["wins"] >= need
    return max(career["stats"][key], career["best"][key]) >= need


def all_unlockables():
    items = [("relic", k, d["unlock"]) for k, d in K.RELICS.items() if d["unlock"]]
    items += [("trinket", k, v[3]) for k, v in K.TRINKETS.items() if v[3]]
    items += [("spell", k, v[3]) for k, v in K.SPELLS.items() if v[3]]
    items += [("calling", k, v[2]) for k, v in K.CALLINGS.items() if v[2]]
    return items


def play_career(args):
    seed, runs = args
    rng = random.Random(seed)
    unl = set(STARTERS) | {k for k, v in K.TRINKETS.items() if not v[3]} | {k for k, v in K.SPELLS.items() if not v[3]}
    callings = {k for k, v in K.CALLINGS.items() if not v[2]}
    career = dict(stats=Counter(), best=Counter(), won_with=set(), school_wins=set(), callings_won=set(),
                  best_pact=-1, channels=set(), same_well=0, low_battery=0, wins=0, got=set())
    seals = defaultdict(lambda: -1)
    items = all_unlockables()
    log, fresh, queue = [], [], []
    profile = rng.choices(list(PROFILES), [p["w"] for p in PROFILES.values()])[0]
    for i in range(runs):
        calling = fresh.pop(0) if fresh else rng.choice(sorted(callings))
        pact = min(7, seals[calling] + 1) if rng.random() < 0.6 else 0
        o = play_run((seed * 7919 + i, dict(profile=profile, calling=calling, pact=pact, unlocked=set(unl))))
        win = o["result"] != "fail"
        career["stats"].update(o["stats"])
        for k, v in o["best"].items():
            career["best"][k] = max(career["best"][k], v)
        career["channels"] |= set(o["channels"])
        if PROFILES[profile]["routine"] >= 1.0 and len(o["used"]) == NIGHTS and rng.random() < 0.35:
            career["same_well"] += 1
        career["low_battery"] += rng.random() < 0.03
        if win:
            career["wins"] += 1
            career["won_with"] |= set(o["relics"])
            schools = Counter(K.RELICS[k]["school"] for k in o["relics"])
            career["school_wins"] |= {s for s, c in schools.items() if c >= 2}
            career["callings_won"].add(calling)
            seals[calling] = max(seals[calling], pact)
            career["best_pact"] = max(career["best_pact"], pact)
        for kind, k, cond in items:   # earned this run: waits in the queue
            if (kind, k) not in career["got"] and (kind, k) not in queue and unlocked_by(career, cond):
                queue.append((kind, k))
        new = 0
        while queue and new < UNLOCKS_PER_RUN:   # the Grimoire reveals up to 2 a Binding
            kind, k = queue.pop(0)
            career["got"].add((kind, k))
            new += 1
            if kind == "calling":
                callings.add(k)
                fresh.append(k)
            else:
                unl.add(k)
        log.append(dict(win=win, op=o["result"] == "op", new=new, got=len(career["got"]),
                        seals=sum(v + 1 for v in seals.values()), have=set(career["got"]) if i == runs - 1 else None))
    return log


def report_career(players, runs=40):
    with Pool() as p:
        logs = p.map(play_career, [(s, runs) for s in range(players)])
    total = len(all_unlockables())
    seal_total = len(K.CALLINGS) * len(K.PACTS)
    print(f"== career: {players} players x {runs} runs ({total} unlockables, {seal_total} Pact seals)")
    print("   runs     win rate  overpowered  runs with an unlock   unlocked so far   seals (median)")
    for a in range(0, runs, 5):
        seg = [log[i] for log in logs for i in range(a, min(a + 5, runs))]
        got = [log[min(a + 4, runs - 1)]["got"] for log in logs]
        seals = [log[min(a + 4, runs - 1)]["seals"] for log in logs]
        print(f"   {a+1:2}-{a+5:<4} {sum(s['win'] for s in seg)/len(seg):7.0%}   {sum(s['op'] for s in seg)/len(seg):8.0%}"
              f"   {sum(s['new'] > 0 for s in seg)/len(seg):14.0%}      {pct(got,50)/total:9.0%}"
              f"        {pct(seals,50)}/{seal_total}")
    missing = Counter()
    for log in logs:
        for kind, k, _ in all_unlockables():
            if (kind, k) not in log[-1]["have"]:
                missing[k] += 1
    print("   still locked after", runs, "runs (share of players):",
          ", ".join(f"{k} {v/len(logs):.0%}" for k, v in missing.most_common(14)))
    for share in (0.25, 0.5, 0.75, 0.9, 1.0):
        when = [next((i + 1 for i, s in enumerate(log) if s["got"] >= share * total), 999) for log in logs]
        med = pct(when, 50)
        print(f"   {share:4.0%} unlocked by run {med if med < 999 else '>' + str(runs)} (median player)")


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else "run"
    n = int(sys.argv[2]) if len(sys.argv) > 2 else None
    if what in ("run", "all"):
        report_run(n or 3000)
    if what in ("relics", "all"):
        report_relics(n or 4000)
    if what in ("callings", "all"):
        report_callings(n or 800)
    if what in ("pacts", "all"):
        report_pacts(n or 600)
    if what in ("career", "all"):
        report_career(n or 120)


if __name__ == "__main__":
    main()
