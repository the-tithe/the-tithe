#!/usr/bin/env python3
"""Turn the PPM screens in shots/ into PNGs at 1x and 3x (the 3x ones are easier to inspect)."""
import glob, os
from PIL import Image

for p in sorted(glob.glob("shots/*.ppm")):
    img = Image.open(p)
    base = p[:-4]
    img.save(base + ".png")
    img.resize((img.width * 3, img.height * 3), Image.NEAREST).save(base + "@3x.png")
    os.remove(p)
    print(base + ".png")
