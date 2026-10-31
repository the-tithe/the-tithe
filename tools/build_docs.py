"""Refresh the generated tables in the docs from catalog.py.

Each table sits between <!-- BEGIN name --> and <!-- END name --> markers; this replaces what's
between them with fresh output from make_tables.py.   python3 tools/build_docs.py
"""
import os
import re
import subprocess
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
docs = ["UPGRADES.md", "UNLOCKS.md", "PROGRESSION.md"]
pat = re.compile(r"(<!-- BEGIN (\w+) -->\n)(.*?)(<!-- END \2 -->)", re.S)


def table(name):
    out = subprocess.run([sys.executable, os.path.join(root, "tools", "make_tables.py"), name],
                         capture_output=True, text=True, check=True).stdout
    return out.strip("\n") + "\n"


for doc in docs:
    path = os.path.join(root, doc)
    if not os.path.exists(path):
        continue
    text = open(path).read()
    new = pat.sub(lambda m: m.group(1) + table(m.group(2)) + m.group(4), text)
    if new != text:
        open(path, "w").write(new)
        print("updated", doc)
