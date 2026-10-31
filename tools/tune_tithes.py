"""Fit each Night's Tithe to what builds can actually do.

Repeats: play a batch of runs, measure the median build's power ÷ Tithe on each Night, and nudge
each Tithe toward a target ratio. Prints the curve and the outcome mix at the end.

  python3 tools/tune_tithes.py [runs] [rounds] [targets for N1..N7, comma-separated]
"""
import os
import sys
import importlib

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)


def main():
    runs = int(sys.argv[1]) if len(sys.argv) > 1 else 1500
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    targets = [float(x) for x in (sys.argv[3] if len(sys.argv) > 3 else "1.25,2.0,2.1,2.2,2.4,2.7,3.2").split(",")]
    import balance_sim as B
    tithes = list(B.TITHES)
    for k in range(rounds):
        os.environ["TITHES"] = ",".join(f"{t:.0f}" for t in tithes)
        importlib.reload(B)
        outs = B.batch(runs, 50 + k)
        meds = []
        for n in range(B.NIGHTS):
            xs = [o["ratios"][n] for o in outs if len(o["ratios"]) > n]
            meds.append(B.pct(xs, 50))
        op, sc, fa = B.mix(outs)
        print(f"round {k}: tithes {[round(t) for t in tithes]}")
        print(f"         medians {[round(m, 2) for m in meds]}  ->  op {op:.0%} scraped {sc:.0%} fail {fa:.0%}")
        tithes = [t * (m / tg) ** 0.8 for t, m, tg in zip(tithes, meds, targets)]
        # keep it rising and round to friendly numbers
        for i in range(1, len(tithes)):
            tithes[i] = max(tithes[i], tithes[i - 1] * 1.2)
        tithes = [round(t, -1) if t < 1000 else round(t, -2) for t in tithes]
    print("final:", ",".join(f"{t:.0f}" for t in tithes))


if __name__ == "__main__":
    main()
