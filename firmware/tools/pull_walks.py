#!/usr/bin/env python3
"""Copy the Deck's walk logs to this computer over USB, no card reader needed.

usage: pull_walks.py [folder]        (default: walks/ next to firmware/)

Only files that are new or have grown since the last copy are fetched.
Saved names carry the Deck's tag (W0001-1f2e.CSV), so two Decks' walks never mix.
Then: python3 ../tools/walk_report.py ../walks
"""
import os, sys

from deck_port import command, open_port, read_line

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "walks")
    out = os.path.normpath(out)
    os.makedirs(out, exist_ok=True)
    s = open_port()
    _, status = command(s, "status")
    words = status.split()
    deck = words[words.index("deck") + 1] if "deck" in words else "deck"
    lines, end = command(s, "files")
    if end.startswith("err"):
        raise SystemExit("The Deck says: " + end[4:])
    files = []
    for line in lines:
        name, _, size = line.rpartition(" ")
        if name.upper().startswith("W") and size.isdigit():
            files.append((name, int(size)))
    got = 0
    for name, size in files:
        stem, ext = os.path.splitext(name)
        dest = os.path.join(out, "%s-%s%s" % (stem, deck, ext))
        if os.path.exists(dest) and os.path.getsize(dest) >= size:
            continue
        s.write(("send %s\n" % name).encode())
        head = read_line(s)
        parts = head.split()
        if len(parts) != 3 or parts[0] != "FILE" or parts[1] != name:
            raise SystemExit("Lost step with the Deck (asked for %s, got %r). Run this again." % (name, head))
        n = int(parts[2])
        data = bytearray()
        while len(data) < n:
            chunk = s.read(n - len(data))
            if not chunk:
                raise TimeoutError("transfer of %s stalled" % name)
            data += chunk
        tail = read_line(s)
        while not tail:
            tail = read_line(s)
        if not tail.startswith("ok"):
            print("skipped %s: the Deck couldn't read all of it (%s)" % (name, tail))
            continue
        with open(dest + ".part", "wb") as f:
            f.write(data)
        os.replace(dest + ".part", dest)
        got += 1
        print("%s  %d KB" % (dest, n // 1024))
    s.close()
    print("%d of %d walk logs copied to %s" % (got, len(files), out))


if __name__ == "__main__":
    main()
