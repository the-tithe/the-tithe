#!/usr/bin/env python3
"""Read the Deck's walk logs and measure the real world the game runs on.

Copy /deck/walks from the SD card, then:

    python3 tools/walk_report.py path/to/walks              report on screen
    python3 tools/walk_report.py path/to/walks --json world.json
                                                             also save the numbers
    WORLD=world.json python3 tools/balance_sim.py run 2000   balance on them

The report puts each number the balance simulator guessed next to what
the walks measured. The log format is in firmware/src/WalkFormat.h.
"""
import argparse, collections, glob, json, os, statistics, sys

PLACES = ["home", "street", "transit", "shop", "work", "park", "none"]
WARDS = ["hollow", "fringe", "ward", "nexus"]
FACTIONS = ["hedge", "hearth", "warded", "syndicate"]
ENERGIES = ["ember", "tide", "gale", "hush", "static"]
GUARD = {"hedge": 0, "hearth": 1, "warded": 2, "syndicate": 3}

# What tools/balance_sim.py assumes today (make_visit and PROFILES).
SIM = {
    "hidden": 0.10,
    "faction": {"hedge": 0.15, "warded": 0.12},
    "syndicate_by_ward": {"hollow": 0.03, "fringe": 0.05, "ward": 0.15, "nexus": 0.30},
    "channels": {1: 0.30, 6: 0.30, 11: 0.25},
    "mesh_by_ward": {"hollow": 0.03, "fringe": 0.08, "ward": 0.12, "nexus": 0.18},
    "strong": 0.35,
    "mobility": 0.41,       # PROFILES' mobility, weighted by how common each player is
    "revisit": 0.49,        # average chance a visit is to a well from earlier in the week
    "profiles": {
        "city": [0.03, 0.12, 0.40, 0.45], "suburb": [0.15, 0.40, 0.35, 0.10],
        "rural": [0.58, 0.30, 0.10, 0.02], "commuter": [0.10, 0.35, 0.20, 0.35],
    },
}


def faction_of(lock):
    if lock in ("open", "owe"):
        return "hedge"
    if lock in ("wpa3", "wpa23"):
        return "warded"
    if lock == "ent":
        return "syndicate"
    return "hearth"


def ward_of(n):
    return "hollow" if n <= 3 else "fringe" if n <= 10 else "ward" if n <= 25 else "nexus"


def router_faction(guard, guest, most_nets):
    """A router's faction the way the Deck decides it (Well.h routerFaction): three or more networks
    is a business box (Syndicate), a guest-named network is the Hedge's."""
    if guard >= 3 or most_nets >= 3:
        return "syndicate"
    if guard == 2:
        return "warded"
    if guest or guard <= 0:
        return "hedge"
    return "hearth"


def energy_of(all_hidden, faction, channel):
    if all_hidden:
        return "hush"
    if faction == "hedge":
        return "static"
    return "ember" if channel <= 4 else "tide" if channel <= 9 else "gale"


# ---------------------------------------------------------------- loading
class Scan:
    __slots__ = ("ms", "no", "place", "nets", "routers")

    def __init__(self, ms, no, place):
        self.ms, self.no, self.place, self.nets = ms, no, place, []
        self.routers = {}


def load(path):
    """One walk file -> (header info, [Scan])."""
    info, scans, by_no = {"file": os.path.basename(path)}, [], {}
    with open(path, errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith(("# tithe walk log", "# deck walk log")):   # "deck" before 0.6.0
                for part in line[2:].split(","):
                    k, _, v = part.strip().rpartition(" ")
                    info[k or "log"] = v
                continue
            if not line or line.startswith("#"):
                continue
            p = line.split(",")
            if p[0] == "S" and len(p) >= 7:
                s = Scan(int(p[1]), int(p[2]), p[3])
                scans.append(s)
                by_no[s.no] = s
            elif p[0] == "N" and len(p) >= 9 and int(p[1]) in by_no:
                net = dict(net=p[2], router=p[3], name=p[4], rssi=int(p[5]), ch=int(p[6]), lock=p[7],
                           hidden=p[8] == "1", guest=len(p) >= 10 and p[9] == "1")   # guest: log version 2
                by_no[int(p[1])].nets.append(net)
    for s in scans:   # fold networks into routers, the way Sightings.h does
        for n in s.nets:
            r = s.routers.setdefault(n["router"], dict(rssi=-127, ch=0, guard=-1, visible=False, nets=0,
                                                       names=set(), guest=False))
            r["nets"] += 1
            r["guest"] = r["guest"] or n["guest"]
            if n["rssi"] > r["rssi"]:
                r["rssi"], r["ch"] = n["rssi"], n["ch"]
            r["guard"] = max(r["guard"], GUARD[faction_of(n["lock"])])
            r["visible"] = r["visible"] or not n["hidden"]
            if n["name"] != "-":
                r["names"].add(n["name"])
    return info, scans


def pct(x):
    return "%d%%" % round(100 * x)


def quantiles(xs):
    if not xs:
        return (0, 0, 0)
    xs = sorted(xs)
    q = lambda f: xs[min(len(xs) - 1, int(f * len(xs)))]
    return (q(0.1), statistics.median(xs), q(0.9))


# ---------------------------------------------------------------- measuring
def measure(walks):
    m = collections.defaultdict(collections.Counter)
    per_place = collections.defaultdict(lambda: dict(scans=0, ms=0, near=[], wards=collections.Counter(),
                                                      new=0, revisit=0, heard=0, dwell=[]))
    sightings = collections.Counter()          # router-sightings by ward
    seen_before = set()
    pairs = changes = 0
    for info, scans in walks:
        seen_this_walk, prev, last_ms = set(), set(), None
        timeline = []
        # Per router this walk, kept the way the Deck keeps it (Sightings.h):
        # faction only moves up, Hush only if it never showed a name, and
        # signal is smoothed.
        guard, visible, sig, last_scan, guest, most = {}, {}, {}, {}, {}, {}
        run = {}                               # router -> consecutive scans heard
        held, pending, agree = None, None, 0    # the ward the Deck would show
        for s in scans:
            P = per_place[s.place]
            P["scans"] += 1
            if last_ms is not None and 0 < s.ms - last_ms < 15000:
                P["ms"] += s.ms - last_ms
            last_ms = s.ms
            now = set(s.routers)
            # Like the Deck (Sightings.h): heard this scan or the last and not faint
            # (smoothed signal at or above -78) count; the ward moves once two scans agree.
            for rid, r in s.routers.items():
                gap = s.no - last_scan.get(rid, -99)
                sig[rid] = r["rssi"] if gap > 3 else sig[rid] * 0.6 + r["rssi"] * 0.4
            near = sum(1 for rid in now | prev if sig.get(rid, -99) >= -78)
            want = ward_of(near)
            if held is None: held, pending, agree = want, want, 0
            elif want == pending:
                agree += 1
                if agree >= 2: held = want
            else: pending, agree = want, 1
            ward = held
            P["near"].append(near)
            P["wards"][ward] += 1
            timeline.append((s.ms, near))
            m["ward_scans"][ward] += 1
            for rid in now - prev:
                run[rid] = 0
            for rid in prev - now:
                if rid in run:
                    P["dwell"].append(run.pop(rid))
            for rid in now:
                run[rid] = run.get(rid, 0) + 1
            # Mesh: a network name broadcast by more than one router in this scan.
            name_routers = collections.Counter()
            for r in s.routers.values():
                for nm in r["names"]:
                    name_routers[nm] += 1
            for rid, r in s.routers.items():
                guard[rid] = max(guard.get(rid, -1), r["guard"])
                guest[rid] = guest.get(rid, False) or r["guest"]
                most[rid] = max(most.get(rid, 0), r["nets"])
                visible[rid] = visible.get(rid, False) or r["visible"]
                last_scan[rid] = s.no
                fac = router_faction(guard[rid], guest[rid], most[rid])
                sightings[ward] += 1
                m["faction"][fac] += 1
                m["faction_by_ward_" + ward][fac] += 1
                m["energy"][energy_of(not visible[rid], fac, r["ch"])] += 1
                m["hidden"]["yes" if not visible[rid] else "no"] += 1
                m["channel"][r["ch"]] += 1
                m["strong"]["yes" if sig[rid] >= -55 else "no"] += 1
                m["multi_net"]["yes" if r["nets"] > 1 else "no"] += 1
                meshed = any(name_routers[nm] > 1 for nm in r["names"])
                m["mesh_by_ward_" + ward]["yes" if meshed else "no"] += 1
                if rid not in seen_this_walk:
                    seen_this_walk.add(rid)
                    P["new"] += 1
                    P["heard"] += 1
                    if rid in seen_before:
                        P["revisit"] += 1
            for n in s.nets:
                m["net_faction"][faction_of(n["lock"])] += 1
                m["net_hidden"]["yes" if n["hidden"] else "no"] += 1
            prev = now
        seen_before |= seen_this_walk
        # How often the ward is different 3 minutes later: a stand-in for how
        # often a player is somewhere new between two visits. The ward here
        # comes from the median of 5 scans, so a count hovering at a
        # threshold (10 or 11 wells, say) doesn't count as moving.
        counts = [n for _, n in timeline]
        timeline = [(timeline[i][0], ward_of(statistics.median(counts[max(0, i - 2):i + 3])))
                    for i in range(len(timeline))]
        j = 0
        for ms, ward in timeline:
            while j < len(timeline) and timeline[j][0] < ms + 180000:
                j += 1
            if j == len(timeline):
                break
            if timeline[j][0] < ms + 240000:
                pairs += 1
                changes += timeline[j][1] != ward
    m["ward_change"]["pairs"], m["ward_change"]["changes"] = pairs, changes
    return m, per_place, sightings


def share(counter, key):
    t = sum(counter.values())
    return counter[key] / t if t else 0.0


def build_world(m, per_place):
    total = sum(m["ward_scans"].values()) or 1
    world = {
        "ward_mix": [m["ward_scans"][w] / total for w in WARDS],
        "hidden": share(m["hidden"], "yes"),
        "faction": {f: share(m["faction"], f) for f in FACTIONS},
        "syndicate_by_ward": {w: share(m["faction_by_ward_" + w], "syndicate") for w in WARDS
                              if m["faction_by_ward_" + w]},
        "hedge_by_ward": {w: share(m["faction_by_ward_" + w], "hedge") for w in WARDS if m["faction_by_ward_" + w]},
        "warded_by_ward": {w: share(m["faction_by_ward_" + w], "warded") for w in WARDS if m["faction_by_ward_" + w]},
        "channels": {str(c): m["channel"][c] / max(1, sum(m["channel"].values())) for c in sorted(m["channel"])},
        "mesh_by_ward": {w: share(m["mesh_by_ward_" + w], "yes") for w in WARDS if m["mesh_by_ward_" + w]},
        "strong": share(m["strong"], "yes"),
        "multi_net": share(m["multi_net"], "yes"),
        "energy": {e: share(m["energy"], e) for e in ENERGIES},
        "places": {},
    }
    pairs = m["ward_change"]["pairs"]
    world["ward_change"] = m["ward_change"]["changes"] / pairs if pairs else SIM["mobility"]
    heard = sum(P["heard"] for P in per_place.values())
    world["revisit"] = sum(P["revisit"] for P in per_place.values()) / heard if heard else 0.0
    # balance_sim's revisit chance averages 0.48 x routine over a week.
    world["routine"] = min(1.8, max(0.5, world["revisit"] / 0.48))
    for p, P in per_place.items():
        lo, med, hi = quantiles(P["near"])
        mins = P["ms"] / 60000
        world["places"][p] = dict(
            minutes=round(mins, 1), scans=P["scans"], near_p10=lo, near_median=med, near_p90=hi,
            ward_mix=[P["wards"][w] / max(1, P["scans"]) for w in WARDS],
            new_per_minute=round(P["new"] / mins, 2) if mins else None,
            revisit=round(P["revisit"] / P["heard"], 3) if P["heard"] else None,
            dwell_median=statistics.median(P["dwell"]) if P["dwell"] else None,
        )
    return world


# ---------------------------------------------------------------- report
def report(walks, world, out=sys.stdout):
    w = lambda s="": print(s, file=out)
    scans = sum(len(s) for _, s in walks)
    minutes = sum(p["minutes"] for p in world["places"].values())
    decks = sorted({info.get("deck", "?") for info, _ in walks})
    w("# Walk report")
    w()
    w("%d walks, %d scans, %.0f minutes, from deck %s." % (len(walks), scans, minutes, ", ".join(decks)))
    if len(decks) > 1:
        w("Walks come from more than one Deck, so the same router has different IDs on each and revisits "
          "are undercounted.")
    w()
    w("## Places")
    w()
    w("| Place | Minutes | Wells in range (10th / median / 90th) | Hollow | Fringe | Ward | Nexus "
      "| New wells a minute | Already heard on an earlier walk | Scans a well stays in range |")
    w("|---|---|---|---|---|---|---|---|---|---|")
    for p in PLACES:
        P = world["places"].get(p)
        if not P:
            continue
        thin = " (thin)" if P["minutes"] < 20 else ""
        w("| %s | %.0f%s | %d / %g / %d | %s | %s | %s | %s | %s | %s | %s |" % (
            p, P["minutes"], thin, P["near_p10"], P["near_median"], P["near_p90"],
            *[pct(x) for x in P["ward_mix"]],
            "-" if P["new_per_minute"] is None else "%.1f" % P["new_per_minute"],
            "-" if P["revisit"] is None else pct(P["revisit"]),
            "-" if P["dwell_median"] is None else "%g" % P["dwell_median"]))
    w()
    w("A place with under 20 minutes is marked thin: its numbers can still move a lot.")
    w()
    w("## Measured against the simulator's guesses")
    w()
    w("| What | Simulator guessed | Walks measured |")
    w("|---|---|---|")
    w("| Hidden wells (Hush) | %s | %s |" % (pct(SIM["hidden"]), pct(world["hidden"])))
    w("| Open wells (Hedge, Static energy) | %s | %s |" % (pct(SIM["faction"]["hedge"]), pct(world["faction"]["hedge"])))
    w("| WPA3 wells (Warded) | %s | %s |" % (pct(SIM["faction"]["warded"]), pct(world["faction"]["warded"])))
    for ward in WARDS:
        got = world["syndicate_by_ward"].get(ward)
        w("| Company wells (Syndicate) in a %s | %s | %s |" % (
            ward, pct(SIM["syndicate_by_ward"][ward]), "-" if got is None else pct(got)))
    for ch in (1, 6, 11):
        w("| Wells on channel %d | %s | %s |" % (ch, pct(SIM["channels"][ch]), pct(world["channels"].get(str(ch), 0))))
    for ward in WARDS:
        got = world["mesh_by_ward"].get(ward)
        w("| Wells in a mesh, in a %s | %s | %s |" % (
            ward, pct(SIM["mesh_by_ward"][ward]), "-" if got is None else pct(got)))
    w("| Wells in reach (-55 dBm or stronger) | %s | %s |" % (pct(SIM["strong"]), pct(world["strong"])))
    w("| In a different ward 3 minutes later | %s | %s |" % (pct(SIM["mobility"]), pct(world["ward_change"])))
    w("| Wells already heard on an earlier walk | %s | %s |" % (pct(SIM["revisit"]), pct(world["revisit"])))
    if len(walks) < 3:
        w()
        w("Revisits need at least 3 walks on different days to mean much.")
    w()
    w("## Your ward mix")
    w()
    mix = world["ward_mix"]
    w("Time in each ward over all walks: %s." % ", ".join("%s %s" % (WARDS[i], pct(mix[i])) for i in range(4)))
    dist = {k: sum(abs(a - b) for a, b in zip(v, mix)) for k, v in SIM["profiles"].items()}
    best = min(dist, key=dist.get)
    w("Closest simulated player: **%s** (%s)." % (best, ", ".join(
        "%s %s" % (WARDS[i], pct(SIM["profiles"][best][i])) for i in range(4))))
    w()
    w("## Energy around you")
    w()
    w(", ".join("%s %s" % (e, pct(world["energy"][e])) for e in ENERGIES) + ".")
    w("%s of routers broadcast more than one network (a guest network, say)." % pct(world["multi_net"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("walks", help="folder holding W0001.CSV and so on, or one file")
    ap.add_argument("--json", help="also save the measured numbers here, for balance_sim.py")
    a = ap.parse_args()
    paths = [a.walks] if os.path.isfile(a.walks) else sorted(glob.glob(os.path.join(a.walks, "W*.CSV")) +
                                                             glob.glob(os.path.join(a.walks, "W*.csv")))
    if not paths:
        sys.exit("no walk logs (W0001.CSV ...) in %s" % a.walks)
    walks = [load(p) for p in paths]
    walks = [w for w in walks if w[1]]
    if not walks:
        sys.exit("the walk logs hold no scans yet")
    m, per_place, _ = measure(walks)
    world = build_world(m, per_place)
    report(walks, world)
    if a.json:
        with open(a.json, "w") as f:
            json.dump(world, f, indent=1)
        print("\nSaved %s" % a.json)


if __name__ == "__main__":
    main()
