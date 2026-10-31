#!/usr/bin/env python3
"""Flag catalog text that the Deck's shop and Book panels cut off.

The detail panel (details() in firmware/src/app/ShopScreens.h) gives rule text up to 2
lines and the flavor line whatever is left of 3, at 38 characters a line. This reads the
strings the Deck actually shows (firmware/src/game/CatalogData.h), wraps them the same
way UI::wrap does, and lists every entry that doesn't fit.

    python3 tools/fit_check.py
"""
import os
import re
import sys

HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware", "src", "game", "CatalogData.h")
PER_LINE = (240 - 8) // 6  # UI::W - 8 pixels at CharW 6
GLYPH_FLAVOR = "Carve it into a well you visit."  # the shop's fixed flavor line for glyphs
STR = re.compile(r'"((?:[^"\\]|\\.)*)"')


def wrap(s):
    """UI::wrap's line breaks, as a list of lines."""
    out = []
    while s:
        take = min(len(s), PER_LINE)
        if len(s) > PER_LINE:
            sp = take
            while sp > 0 and s[sp] != " ":
                sp -= 1
            if sp > 0:
                take = sp
        out.append(s[:take])
        s = s[take:].lstrip(" ")
    return out


def table(src, name):
    """Each row of `name[...] = {...};` as its list of string literals (key, name, text, flavor, ...)."""
    body = re.search(r"\b%s\[\w+\] = \{\n(.*?)\n\};" % name, src, re.S).group(1)
    return [[m.replace('\\"', '"') for m in STR.findall(line)] for line in body.splitlines() if '"' in line]


def check(kind, row, flavor):
    name, text = row[1], row[2]
    t = wrap(text)
    f = wrap(flavor) if flavor else []
    if len(t) > 2 or len(t) + len(f) > 3:
        return "%-8s %-20s rule %d + flavor %d\n    %s\n    %s" % (
            kind, name, len(t), len(f), " | ".join(t), " | ".join(f))
    return None


def main():
    src = open(HEADER).read()
    rows = []
    for kind, tab in (("relic", "Relics"), ("trinket", "Trinkets"), ("spell", "Spells")):
        rows += [check(kind, r, r[3]) for r in table(src, tab)]
    rows += [check("glyph", r, GLYPH_FLAVOR) for r in table(src, "Glyphs")]
    rows += [check("mod", r, None) for r in table(src, "Mods")]
    bad = [r for r in rows if r]
    print("\n".join(bad))
    print("%d of %d entries cut off" % (len(bad), len(rows)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
