#!/usr/bin/env python3
"""Lay several screens side by side at 2x, for checking them together.
usage: sheet.py OUT.png shot1 shot2 ...   (names in shots/, without .png)"""
import sys
from PIL import Image, ImageDraw

out, names = sys.argv[1], sys.argv[2:]
W, H, PAD = 480, 270, 8
cols = 2
rows = (len(names) + 1) // 2
sheet = Image.new("RGB", (cols * (W + PAD), rows * (H + PAD + 12)), (50, 50, 55))
d = ImageDraw.Draw(sheet)
for i, n in enumerate(names):
    im = Image.open("shots/%s.png" % n).resize((W, H), Image.NEAREST)
    x, y = (i % cols) * (W + PAD), (i // cols) * (H + PAD + 12)
    d.text((x + 2, y), n, fill=(200, 200, 200))
    sheet.paste(im, (x, y + 12))
sheet.save(out)
