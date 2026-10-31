#!/usr/bin/env python3
"""Make-believe walk logs in the Deck's format, for testing walk_report.py
before there are real ones. The numbers are guesses, not data.

usage: fake_walks.py OUTDIR [walks]
"""
import math, os, random, sys

rng = random.Random(7)


def hexid():
    return "%08x" % rng.getrandbits(32)


class Router:
    def __init__(self, where, pos=0.0, ent_share=0.05):
        self.router = hexid()
        self.pos = pos
        self.power = rng.uniform(-38, -28)            # dBm at 1 m
        self.channel = rng.choice([1] * 30 + [6] * 32 + [11] * 26 + [3, 4, 8, 9, 13, 2, 5, 7, 10, 12])
        r = rng.random()
        main = "ent" if r < ent_share else "open" if r < ent_share + 0.06 else \
            "wpa3" if r < ent_share + 0.09 else "wpa23" if r < ent_share + 0.17 else "wpa2"
        self.nets = [(hexid(), hexid(), main, rng.random() < 0.07)]
        if rng.random() < 0.22:                        # guest network on the same box
            self.nets.append((hexid(), hexid(), rng.choice(["open", "wpa2"]), False))
        if rng.random() < 0.05:
            self.nets.append((hexid(), "-", "wpa2", True))
        self.where = where


def heard(r, dist):
    rssi = r.power - 27 * math.log10(max(dist, 1)) + rng.gauss(0, 4)
    return int(rssi) if rssi > -92 and rng.random() < 0.93 else None


def walk_file(path, walk, plan, world):
    lines = ["# tithe walk log 1, firmware 0.1.0, deck 1f2e, walk %d" % walk,
             "# S,ms,scan,place,networks,routers,battery",
             "# N,scan,net,router,name,rssi,channel,lock,hidden",
             "# P,ms,place"]
    ms, scan, batt = 0, 0, 96
    for place, minutes in plan:
        lines.append("P,%d,%s" % (ms, place))
        routers = world[place]()
        x = 0.0
        speed = {"street": 1.3, "transit": 8.0, "park": 1.1}.get(place, 0.0)   # metres a second
        for _ in range(int(minutes * 60 / 2.4)):
            ms += 2400
            scan += 1
            x += speed * 2.4
            seen = []
            for r in routers:
                d = abs(r.pos - x) + r.offset if speed else r.offset
                rssi = heard(r, d)
                if rssi is not None:
                    seen.append((r, rssi))
            nets = []
            for r, rssi in seen:
                for k, (net, name, lock, hidden) in enumerate(r.nets):
                    nets.append("N,%d,%s,%s,%s,%d,%d,%s,%d" % (scan, net, r.router, name, rssi - k,
                                                               r.channel, lock, hidden))
            lines.append("S,%d,%d,%s,%d,%d,%d" % (ms, scan, place, len(nets), len(seen), batt))
            lines.extend(nets)
            if scan % 50 == 0:
                batt = max(5, batt - 1)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def make_world():
    def fixed(n, near, far, ent=0.05):
        rs = [Router("x", ent_share=ent) for _ in range(n)]
        for r in rs:
            r.offset = rng.uniform(near, far)
        return lambda: rs

    def street(n_per_100m, ent=0.08):
        rs = []     # the same street every day, so routers get passed again
        for i in range(int(n_per_100m * 30)):
            r = Router("street", pos=rng.uniform(-50, 3000), ent_share=ent)
            r.offset = rng.uniform(6, 30)
            rs.append(r)
        return lambda: rs

    return {"home": fixed(16, 3, 45), "work": fixed(40, 4, 60, ent=0.55), "shop": fixed(10, 3, 30, ent=0.15),
            "street": street(6), "transit": street(2, ent=0.2), "park": street(0.4)}


def main():
    out = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    os.makedirs(out, exist_ok=True)
    world = make_world()
    days = [
        [("home", 20), ("street", 12), ("transit", 15), ("street", 5), ("work", 30)],
        [("work", 20), ("street", 8), ("shop", 10), ("street", 8), ("home", 15)],
        [("home", 30), ("street", 20), ("park", 15), ("street", 20)],
    ]
    for w in range(1, n + 1):
        walk_file(os.path.join(out, "W%04d.CSV" % w), w, days[(w - 1) % len(days)], world)
        print(os.path.join(out, "W%04d.CSV" % w))


if __name__ == "__main__":
    main()
