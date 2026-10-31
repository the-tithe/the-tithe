"""Compare Tithe curves side by side: python3 tools/try_curves.py runs "c1" "c2" ..."""
import os
import subprocess
import sys

CODE = r'''
import sys; sys.path.insert(0, "tools")
import balance_sim as B
from collections import Counter
if __name__ == "__main__":
    n = int(sys.argv[1])
    outs = B.batch(n, 5)
    op, sc, fa = B.mix(outs)
    f = Counter(o["fails_at"] for o in outs if o["fails_at"])
    top = [B.pct([o["ratios"][k] for o in outs if len(o["ratios"]) > k], 90) for k in range(7)]
    med = [B.pct([o["ratios"][k] for o in outs if len(o["ratios"]) > k], 50) for k in range(7)]
    print(f"op {op:.0%} scraped {sc:.0%} fail {fa:.0%} | fails " + " ".join(f"{f[k]/n:.1%}" for k in range(1, 8)))
    print("    median " + " ".join(f"{x:.2f}" for x in med) + "   top10 " + " ".join(f"{x:.2f}" for x in top))
'''
runs = sys.argv[1]
for curve in sys.argv[2:]:
    env = dict(os.environ, TITHES=curve)
    print("==", curve, flush=True)
    subprocess.run([sys.executable, "-c", CODE, runs], env=env)
