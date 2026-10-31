"""Print the catalog as Markdown tables for UPGRADES.md and UNLOCKS.md.

  python3 tools/make_tables.py relics | trinkets | spells | glyphs | editions | mods | portents |
                               omens | callings | pacts | unlocks
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import catalog as K  # noqa: E402

SCHOOLS = [
    ("throng", "Throng", "Busy places. Downtown, offices, apartment blocks."),
    ("hollow", "Hollow", "Empty places. Parks, back roads, the edge of town."),
    ("fringe", "Fringe", "In-between places. Suburbs, quiet streets, home."),
    ("wayfaring", "Wayfaring", "Finding wells you've never visited."),
    ("routine", "Refrain", "Going back to the same wells, day after day."),
    ("elemental", "Elemental", "One energy, pushed as far as it goes."),
    ("variety", "Variety", "A bit of every energy."),
    ("smuggling", "Smuggling", "Static, Heat and the Wardens."),
    ("hidden", "Hidden", "Hush and hidden networks."),
    ("machine", "Machine", "The physics of WiFi: mesh, channels, signal strength."),
    ("timing", "Tempo", "When in the Night you offer, and how long the week has gone on."),
    ("echo", "Echo", "Offerings that count twice, and relics that copy relics."),
    ("deck", "Deck", "The Cardputer's own sensors: microphone, gyro, battery."),
    ("money", "Tokens", "Coin turned into Resonance."),
    ("cursed", "Cursed", "Big upside, with a catch."),
]
RARITY = {"common": "Common", "uncommon": "Uncommon", "rare": "Rare", "legendary": "Legendary"}


def unlock_text(u):
    return "Start" if u is None else u[2]


def relics():
    for key, title, blurb in SCHOOLS:
        rows = [d for d in K.RELICS.values() if d["school"] == key and not d["legendary"]]
        if not rows:
            continue
        print(f"\n### {title}\n\n{blurb}\n")
        print("| Relic | Rarity | Effect | Unlock |")
        print("|---|---|---|---|")
        order = {"common": 0, "uncommon": 1, "rare": 2}
        for d in sorted(rows, key=lambda d: order[d["rarity"]]):
            print(f"| **{d['name']}**<br>*{d['flavor']}* | {RARITY[d['rarity']]} | {d['text']} | {unlock_text(d['unlock'])} |")
    print("\n### Legendary\n\nOnly from the SUMMON spell or a Conjunction Portent.\n")
    print("| Relic | Effect | Unlock |")
    print("|---|---|---|")
    for d in K.RELICS.values():
        if d["legendary"]:
            print(f"| **{d['name']}**<br>*{d['flavor']}* | {d['text']} | {unlock_text(d['unlock'])} |")


def simple(table, cols, unlock_idx=None):
    print("| " + " | ".join(cols) + " |")
    print("|" + "---|" * len(cols))
    for k, v in table.items():
        cells = [f"**{v[0]}**", v[1]]
        if len(cols) > 2 and cols[2] == "Flavor":
            cells.append(f"*{v[2]}*")
        if unlock_idx is not None:
            cells.append(unlock_text(v[unlock_idx]))
        print("| " + " | ".join(cells) + " |")


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else "relics"
    if what == "relics":
        relics()
    elif what == "trinkets":
        simple(K.TRINKETS, ["Trinket", "Effect", "Flavor", "Unlock"], 3)
    elif what == "spells":
        simple(K.SPELLS, ["Spell", "Effect", "Flavor", "Unlock"], 3)
    elif what == "glyphs":
        simple(K.GLYPHS, ["Glyph", "Effect", "Flavor"])
    elif what == "editions":
        print("| Edition | Effect | Chance on a shop relic |")
        print("|---|---|---|")
        for v in K.EDITIONS.values():
            print(f"| **{v[0]}** | {v[1]} | {v[2]:.1%} |")
    elif what == "mods":
        print("| Mod | Effect | Needs |")
        print("|---|---|---|")
        for v in K.MODS.values():
            print(f"| **{v[0]}** | {v[1]} | {K.MODS[v[2]][0] if v[2] else '-'} |")
    elif what == "portents":
        simple(K.PORTENTS, ["Portent", "Reward"])
    elif what == "omens":
        simple(K.OMENS, ["Omen", "Rule"])
        print()
        simple(K.ECLIPSES, ["Eclipse", "Rule"])
    elif what == "callings":
        print("| Calling | Rule | Unlock |")
        print("|---|---|---|")
        for v in K.CALLINGS.values():
            print(f"| **{v[0]}** | {v[1]} | {unlock_text(v[2])} |")
    elif what == "pacts":
        print("| Pact | Adds |")
        print("|---|---|")
        for name, text in K.PACTS:
            print(f"| **{name}** | {text} |")
    elif what == "challenges":
        print("| Challenge | Rule |")
        print("|---|---|")
        for name, text in K.CHALLENGES:
            print(f"| **{name}** | {text} |")
    elif what == "grimoire":
        print("| Page | Unlock | Deck theme |")
        print("|---|---|---|")
        for name, how, theme in K.GRIMOIRE:
            print(f"| **{name}** | {how} | {theme} |")
    elif what == "tithes":
        import balance_sim as B
        print("| Night | " + " | ".join(str(i + 1) for i in range(B.NIGHTS)) + " |")
        print("|---|" + "---|" * B.NIGHTS)
        print("| Tithe (Resonance) | " + " | ".join(f"{t:,.0f}" for t in B.TITHES) + " |")
    elif what == "counts":
        starters = sum(1 for d in K.RELICS.values() if d["unlock"] is None and not d["legendary"])
        print(f"relics {len(K.RELICS)} (start {starters}, legendary {sum(d['legendary'] for d in K.RELICS.values())}), "
              f"trinkets {len(K.TRINKETS)}, spells {len(K.SPELLS)}, glyphs {len(K.GLYPHS)}, editions {len(K.EDITIONS)}, "
              f"mods {len(K.MODS)}, portents {len(K.PORTENTS)}, omens {len(K.OMENS)}, eclipses {len(K.ECLIPSES)}, "
              f"callings {len(K.CALLINGS)}, pacts {len(K.PACTS)}")


if __name__ == "__main__":
    main()
