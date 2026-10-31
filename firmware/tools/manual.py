#!/usr/bin/env python3
"""Typeset The Tithe's manual (manual/manual.md) as a phone-sized PDF, in the Deck's own font and
colors, with screenshots from the screen renderer.

    pio run -e render && .pio/build/render/program && python3 tools/png.py
    python3 tools/manual.py             # writes manual/Tithe-Manual.pdf

Pages are 240 pixels wide, the width of the Deck's screen, so screenshots sit edge to edge. The
font is the Deck's own, exported by the renderer to shots/font.png.

Markup, one block per paragraph:
    # Title                 a chapter: new page, big title
    ## Heading              a section heading
    text                    a paragraph (lines join until a blank line)
    1. item                 a numbered item, with a hanging indent
    - item                  a bullet
    > note                  a dim note
    ::: TITLE  ...  :::     the manual's own voice: a panel styled like a Deck screen (::: _ for no title)
    | left | right |        a two-column row; left is dim unless it starts with {color}
    ![shot](caption)        a screenshot from shots/<shot>.png
    ===                     a page break
    {p:Title}               the page a chapter (or else a panel) with that title starts on
    {version}               the app's version, from src/Config.h
    *word*                  accent color (\\* is a literal star);  {ember} {tide} {gale} {hush} {static}  energy glyphs
    {yellow}..{/} {dim}..{/} {red}..{/} and energy or faction names as colors
"""
import os
import re
import sys
import zlib

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
W, H, M = 240, 480, 6           # page size and side margin, in Deck pixels
SCALE = 4                       # pixels in the PDF per Deck pixel
LINE = 10                       # line height, as on the Deck's list screens
HEADER = "THE TITHE"            # the running title at the top of every page but the cover
CW = 6                          # character width


def q(r, g, b):
    """A color as the Deck's 16-bit screen shows it."""
    r5, g6, b5 = r >> 3, g >> 2, b >> 3
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))


C = {
    "bg": (0, 0, 0), "text": q(225, 228, 235), "dim": q(110, 116, 128), "faint": q(55, 60, 70),
    "sep": q(40, 70, 80), "selbg": q(22, 34, 48), "yellow": q(255, 225, 90), "accent": q(90, 220, 230),
    "red": q(255, 70, 70), "ember": q(255, 140, 40), "tide": q(60, 160, 255), "gale": q(140, 255, 170),
    "hush": q(190, 120, 255), "static": q(255, 230, 60), "hedge": q(110, 220, 100), "hearth": q(255, 180, 80),
    "warded": q(170, 200, 255), "syndicate": q(255, 90, 200), "hollow": q(140, 150, 165),
    "fringe": q(80, 200, 180), "quarter": q(240, 220, 150), "nexus": q(255, 80, 160),
    "panel": q(12, 20, 32),
    "units": q(255, 120, 210), "thirst": q(70, 220, 240), "amp": q(255, 75, 85),
}
GLYPHS = ["ember", "tide", "gale", "hush", "static"]

atlas = Image.open(os.path.join(ROOT, "shots", "font.png")).convert("L")
glyph_img = Image.open(os.path.join(ROOT, "shots", "glyphs.png")).convert("L")


def char_mask(ch):
    i = ord(ch) - 32
    if i < 0 or i > 94:
        raise SystemExit("not in the Deck's font: %r" % ch)
    return atlas.crop(((i % 16) * 6, (i // 16) * 8, (i % 16) * 6 + 6, (i // 16) * 8 + 8))


def draw_text(page, x, y, s, color, size=1):
    for ch in s:
        m = char_mask(ch)
        if size != 1:
            m = m.resize((6 * size, 8 * size), Image.NEAREST)
        page.paste(Image.new("RGB", m.size, color), (x, y), m)
        x += CW * size
    return x


def draw_glyph(page, x, y, name):
    i = GLYPHS.index(name)
    m = glyph_img.crop((i * 8, 0, i * 8 + 7, 7))
    page.paste(Image.new("RGB", m.size, C[name]), (x, y), m)


def hline(page, y, color):
    page.paste(Image.new("RGB", (W, 1), color), (0, y))


# ---- Words with colors -------------------------------------------------------------

TOKEN = re.compile(r"\{(/|[a-z]+)\}|\*")


def words(text, base):
    """Split text into (word, color, glyph) runs, following the color markup."""
    out, stack, word, color = [], [base], "", base
    i = 0

    def flush():
        nonlocal word
        if word:
            out.append((word, stack[-1], None))
            word = ""

    while i < len(text):
        if text[i] == "\\" and i + 1 < len(text):   # \* is a literal star
            word += text[i + 1]
            i += 2
            continue
        m = TOKEN.match(text, i)
        if m:
            tok = m.group(0)
            if tok == "*":
                flush()
                if stack[-1] == C["accent"] and len(stack) > 1:
                    stack.pop()
                else:
                    stack.append(C["accent"])
            elif m.group(1) == "/":
                flush()
                if len(stack) > 1:
                    stack.pop()
            elif m.group(1) in GLYPHS and text[m.end():m.end() + 1] in (" ", "") and (
                    i == 0 or text[i - 1] == " "):
                flush()
                out.append(("", m.group(1), "glyph"))
            elif m.group(1) in C:
                flush()
                stack.append(C[m.group(1)])
            else:
                raise SystemExit("unknown markup %s in: %s" % (tok, text[:60]))
            i = m.end()
            continue
        ch = text[i]
        if ch == " ":
            flush()
            out.append((" ", stack[-1], None))
        else:
            word += ch
        i += 1
    flush()
    return out


def wrap(text, base, width_chars):
    """Lines of runs, each line at most width_chars wide."""
    runs = words(text, base)
    lines, line, n = [], [], 0
    # Glue: a glyph and the word after it stay together.
    tokens = []
    for r in runs:
        if r[0] == " ":
            tokens.append(("space", r))
        elif tokens and tokens[-1][0] == "word" and tokens[-1][1][-1][2] == "glyph":
            tokens[-1][1].append(r)
        elif tokens and tokens[-1][0] == "word" and not (tokens[-1][1][-1][0] == " "):
            tokens[-1][1].append(r)   # a color change mid-word
        else:
            tokens.append(("word", [r]))
    def tw(rs):
        return sum(2 if r[2] == "glyph" else len(r[0]) for r in rs)
    for kind, rs in tokens:
        if kind == "space":
            if line:
                line.append(rs)
                n += 1
            continue
        w = tw(rs)
        if n + w > width_chars and line:
            while line and line[-1][0] == " ":
                line.pop()
            lines.append(line)
            line, n = [], 0
        line.extend(rs)
        n += w
    while line and line[-1][0] == " ":
        line.pop()
    if line:
        lines.append(line)
    return lines


def draw_line(page, x, y, runs, size=1):
    for text, color, kind in runs:
        if kind == "glyph":
            draw_glyph(page, x, y, color)
            x += 2 * CW
        else:
            x = draw_text(page, x, y, text, color, size)
    return x


def line_width(runs):
    return sum(2 * CW if k == "glyph" else len(t) * CW for t, _, k in runs)


# ---- Pages ---------------------------------------------------------------------------

class Book:
    def __init__(self):
        self.pages, self.page, self.y, self.chapter = [], None, 0, ""
        self.covers = set()
        self.chapters = {}     # chapter title -> its page number
        self.panels = {}       # panel title -> the page it starts on (the first, if the title repeats)
        self.top = 19
        self.panel = False
        self.regions = []      # (page, top, bottom) of the manual's panels
        self.panel_shots = []  # screenshots inside panels, put back after the panel is tinted

    def new_page(self):
        if self.panel:
            self.regions.append((self.page, self.panel_top, H - 16))
        self.page = Image.new("RGB", (W, H), C["bg"])
        self.pages.append((self.page, self.chapter))
        self.y = self.top
        if self.panel:
            self.panel_top = self.y - 4
            self.y += 2

    # The manual's voice: a panel like one of the Deck's screens, with a title bar.
    def panel_open(self, title, keep=LINE * 3, shot_next=False):
        # Keep the title bar with what follows it (a whole screenshot, if that comes first).
        self.room(4 + (4 + LINE + 5 if title else 0) + keep)
        fresh = self.y == self.top
        self.y += 4
        self.panel_top = self.top - 4 if fresh else self.y   # every panel starts just under the header rule
        self.panel = True
        if title:
            self.panels.setdefault(title.upper(), len(self.pages))
            self.y += 4
            draw_text(self.page, M, self.y, title.upper(), C["yellow"])
            self.y += LINE
            if shot_next:
                self.y -= 1   # the screenshot's own top rule is the title's rule
            else:
                hline(self.page, self.y, C["sep"])
                self.y += 5
        else:
            self.y += 4

    def panel_close(self):
        self.y += 2
        self.regions.append((self.page, self.panel_top, self.y))
        self.panel = False
        self.y += 8

    def tint_panels(self):
        for page, top, bottom in self.regions:
            box = (0, top, W, bottom)
            region = page.crop(box)
            mask = region.convert("L").point(lambda v: 255 if v == 0 else 0)
            page.paste(Image.new("RGB", region.size, C["panel"]), (0, top), mask)
        for page, img, pos in self.panel_shots:
            page.paste(img, pos)

    def room(self, h):
        if self.page is None or self.y + h > H - 16:
            self.new_page()

    def chrome(self):
        total = len(self.pages)
        for i, (p, chapter) in enumerate(self.pages):
            if i in self.covers:
                continue   # the covers have none
            draw_text(p, 3, 3, HEADER, C["yellow"])
            room = (W - 6) // CW - len(HEADER + " ")
            if len(chapter) > room:   # long chapter names would run into the title
                chapter = chapter[:room - 2].rstrip(" ,") + ".."
            draw_text(p, W - 3 - len(chapter) * CW, 3, chapter, C["dim"])
            hline(p, 13, C["sep"])
            hline(p, H - 12, C["sep"])
            label = "%d / %d" % (i + 1, total)
            draw_text(p, W - 3 - len(label) * CW, H - 9, label, C["dim"])

    # Blocks
    def chapter_title(self, title):
        self.chapter = title.upper()
        self.new_page()
        self.chapters[title.upper()] = len(self.pages)
        lines = wrap(title.upper(), C["yellow"], (W - 2 * M) // (CW * 2))
        for ln in lines:
            draw_line(self.page, M, self.y, ln, 2)
            self.y += 18
        self.y += 6

    def heading(self, text, keep=LINE * 3):
        # Keep the heading with the start of what follows it.
        self.room(LINE * 2 + 9 + keep)
        self.y += 6
        for ln in wrap(text.upper(), C["accent"], (W - 2 * M) // CW):
            draw_line(self.page, M, self.y, ln)
            self.y += LINE
        self.y += 3

    def paragraph(self, text, color, x=M, indent_first=None, gap=5):
        lines = wrap(text, color, (W - x - M) // CW)
        if len(lines) <= 4:
            self.room(len(lines) * LINE)   # short paragraphs stay whole
        else:
            self.room(2 * LINE)   # never leave a paragraph's first line alone at the foot of a page
        for i, ln in enumerate(lines):
            if i == len(lines) - 2:
                self.room(2 * LINE)   # never leave a paragraph's last line alone on a new page
            self.room(LINE)
            draw_line(self.page, x, self.y, ln)
            self.y += LINE
        self.y += gap

    def bullet(self, text):
        lines = wrap(text, C["text"], (W - M - 12 - M) // CW)
        if len(lines) <= 3:
            self.room(len(lines) * LINE)   # short items stay whole
        else:
            self.room(2 * LINE)
        for i, ln in enumerate(lines):
            if i == len(lines) - 2:
                self.room(2 * LINE)
            self.room(LINE)
            if i == 0:
                draw_text(self.page, M, self.y, "-", C["dim"])
            draw_line(self.page, M + 12, self.y, ln)
            self.y += LINE
        self.y += 2

    def numbered(self, num, text):
        x = M + 4 * CW   # hanging indent past "10. "
        lines = wrap(text, C["text"], (W - x - M) // CW)
        if len(lines) <= 3:
            self.room(len(lines) * LINE)
        else:
            self.room(2 * LINE)
        for i, ln in enumerate(lines):
            if i == len(lines) - 2:
                self.room(2 * LINE)
            self.room(LINE)
            if i == 0:
                label = num + "."
                draw_text(self.page, M + 3 * CW - len(label) * CW + CW, self.y, label, C["dim"])
            draw_line(self.page, x, self.y, ln)
            self.y += LINE
        self.y += 3

    def row(self, left, right, left_chars):
        lw = left_chars * CW
        llines = wrap(left, C["dim"], left_chars)
        rlines = wrap(right, C["text"], (W - 2 * M - lw - CW) // CW)
        n = max(len(llines), len(rlines))
        self.room(min(n, 3) * LINE)
        for i in range(n):
            self.room(LINE)
            if i < len(llines):
                draw_line(self.page, M, self.y, llines[i])
            if i < len(rlines):
                draw_line(self.page, M + lw + CW, self.y, rlines[i])
            self.y += LINE
        self.y += 3

    def shot(self, name, caption):
        img = Image.open(os.path.join(ROOT, "shots", name + ".png")).convert("RGB")
        caps = wrap(caption, C["dim"], (W - 2 * M) // CW) if caption else []
        self.room(img.height + 6 + LINE * len(caps) + 4)
        self.y += 2
        self.page.paste(img, (0, self.y))
        if self.panel:
            self.panel_shots.append((self.page, img, (0, self.y)))
        hline(self.page, self.y - 1, C["sep"])
        hline(self.page, self.y + img.height, C["sep"])
        self.y += img.height + 4
        for ln in caps:
            draw_line(self.page, (W - line_width(ln)) // 2, self.y, ln)
            self.y += LINE
        self.y += 6

    def cover(self, lines):
        self.chapter = ""
        self.new_page()
        self.covers.add(len(self.pages) - 1)
        boot = Image.open(os.path.join(ROOT, "shots", "boot_3.png")).convert("RGB")
        sigil = boot.crop((0, 10, 112, 125)).resize((224, 230), Image.NEAREST)
        self.page.paste(sigil, ((W - 224) // 2, 40))
        y = 300
        for text, color, size in lines:
            for ln in wrap(text, C[color], (W - 2 * M) // (CW * size)):
                draw_line(self.page, (W - line_width(ln) * size) // 2, y, ln, size)
                y += 8 * size + 4
            y += 6


def estimate(blocks, left_chars):
    """Roughly how tall a run of blocks will set, in Deck pixels."""
    h = 0
    for b in blocks:
        b = b.strip()
        if b.startswith("@cols "):
            left_chars = int(b.split()[1])
        elif b.startswith("!["):
            m = re.match(r"!\[(.+?)\]\((.*)\)", b, re.S)
            img = Image.open(os.path.join(ROOT, "shots", m.group(1) + ".png"))
            h += img.height + 16 + LINE * len(wrap(" ".join(m.group(2).split()), C["dim"], (W - 2 * M) // CW))
        elif b.startswith("|"):
            rw = (W - 2 * M - left_chars * CW - CW) // CW
            for ln in b.splitlines():
                c = [x.strip() for x in ln.strip().strip("|").split("|")]
                h += max(len(wrap(c[0], C["dim"], left_chars)),
                         len(wrap(c[1] if len(c) > 1 else "", C["text"], rw))) * LINE + 3
            h += 2
        elif b.startswith("- "):
            for it in re.split(r"\n- ", "\n" + b)[1:]:
                h += len(wrap(" ".join(it.split()), C["text"], (W - M - 12 - M) // CW)) * LINE + 2
        elif re.match(r"^\d+\.\s", b):
            for it in re.split(r"\n(?=\d+\.\s)", b):
                h += len(wrap(" ".join(it.split()), C["text"], (W - 2 * M - 4 * CW) // CW)) * LINE + 3
            h += 2
        else:
            h += len(wrap(" ".join(b.split()), C["text"], (W - 2 * M) // CW)) * LINE + 5
    return h


def deck_version():
    """The app's version, from firmware/src/Config.h, so the manual always says which build it's for."""
    cfg = open(os.path.join(ROOT, "src", "Config.h")).read()
    return re.search(r'Version = "([^"]+)"', cfg).group(1)


def build(src, pages_of=None):
    book = Book()
    text = re.sub(r"\{p:([^}]+)\}", lambda m: str((pages_of or {}).get(m.group(1).upper(), 99)), open(src).read())
    text = text.replace("{version}", deck_version())
    blocks = re.split(r"\n\s*\n", text.strip())
    left_chars = 11
    for i, b in enumerate(blocks):
        nxt = blocks[i + 1].strip() if i + 1 < len(blocks) else ""
        b = b.strip()
        if b.startswith("[cover]"):
            lines = []
            for ln in b.splitlines()[1:]:
                size = 2 if ln.startswith("!") else 1
                color = "yellow" if size == 2 else "dim" if ln.startswith("~") else "text"
                lines.append((ln.lstrip("!~").strip(), color, size))
            book.cover(lines)
        elif b.startswith(":::") and b.strip() != ":::":
            keep, shot_next = LINE * 3, nxt.startswith("![")
            if shot_next:
                m = re.match(r"!\[(.+?)\]\((.*)\)", nxt, re.S)
                img = Image.open(os.path.join(ROOT, "shots", m.group(1) + ".png"))
                caps = wrap(" ".join(m.group(2).split()), C["dim"], (W - 2 * M) // CW) if m.group(2).strip() else []
                keep = img.height + 6 + LINE * len(caps) + 4
            title = b[3:].strip()
            body = []
            for nb in blocks[i + 1:]:
                if nb.strip() == ":::":
                    break
                body.append(nb)
            whole = estimate(body, left_chars) + 12
            if whole < H * 0.55:
                keep = max(keep, whole)   # a short panel stays on one page
            book.panel_open("" if title == "_" else title, keep, shot_next)
        elif b.strip() == ":::":
            book.panel_close()
        elif b == "===":
            book.new_page()
        elif b.startswith("@cols "):
            left_chars = int(b.split()[1])
        elif b.startswith("# "):
            book.chapter_title(b[2:].strip())
        elif b.startswith("## "):
            book.heading(b[3:].strip(), keep=150 if nxt.startswith("![") else LINE * 3)
        elif b.startswith("!["):
            m = re.match(r"!\[(.+?)\]\((.*)\)", b, re.S)
            book.shot(m.group(1), " ".join(m.group(2).split()))
        elif re.match(r"^\d+\.\s", b):
            for item in re.split(r"\n(?=\d+\.\s)", b):
                m = re.match(r"^(\d+)\.\s+(.*)", item, re.S)
                book.numbered(m.group(1), " ".join(m.group(2).split()))
            book.y += 2
        elif b.startswith("- "):
            items = [" ".join(item.split()) for item in re.split(r"\n- ", "\n" + b)[1:]]
            th = sum(len(wrap(it, C["text"], (W - M - 12 - M) // CW)) * LINE + 2 for it in items)
            if th < H // 2:
                book.room(th)   # short lists stay on one page
            for it in items:
                book.bullet(it)
        elif b.startswith("|"):
            rw = (W - 2 * M - left_chars * CW - CW) // CW
            cells = [[x.strip() for x in ln.strip().strip("|").split("|")] for ln in b.splitlines()]
            th = sum(max(len(wrap(c[0], C["dim"], left_chars)),
                         len(wrap(c[1] if len(c) > 1 else "", C["text"], rw))) * LINE + 3 for c in cells)
            if th < H // 2:
                book.room(th)   # short tables stay on one page
            for ln in b.splitlines():
                cells = [c.strip() for c in ln.strip().strip("|").split("|")]
                book.row(cells[0], cells[1] if len(cells) > 1 else "", left_chars)
            book.y += 2
        elif b.startswith(">"):
            book.paragraph(" ".join(l.lstrip("> ").strip() for l in b.splitlines()), C["dim"])
        else:
            book.paragraph(" ".join(b.split()), C["text"])
    book.tint_panels()
    book.chrome()
    return [p for p, _ in book.pages], {**book.panels, **book.chapters}


# ---- A small lossless PDF writer ----------------------------------------------------

def write_pdf(pages, path, page_w_pt=270):
    page_h_pt = page_w_pt * H / W
    objs = []

    def obj(data):
        objs.append(data)
        return len(objs)

    kids = []
    catalog = obj(None)
    pages_obj = obj(None)
    for img in pages:
        big = img.resize((W * SCALE, H * SCALE), Image.NEAREST)
        raw = zlib.compress(big.tobytes(), 9)
        im = obj(b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceRGB "
                 b"/BitsPerComponent 8 /Filter /FlateDecode /Length %d >>\nstream\n" % (big.width, big.height, len(raw))
                 + raw + b"\nendstream")
        content = b"q %.2f 0 0 %.2f 0 0 cm /Im0 Do Q" % (page_w_pt, page_h_pt)
        cs = obj(b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream")
        pg = obj(b"<< /Type /Page /Parent %d 0 R /MediaBox [0 0 %.2f %.2f] /Resources << /XObject << /Im0 %d 0 R >> >> "
                 b"/Contents %d 0 R >>" % (pages_obj, page_w_pt, page_h_pt, im, cs))
        kids.append(pg)
    objs[catalog - 1] = b"<< /Type /Catalog /Pages %d 0 R >>" % pages_obj
    objs[pages_obj - 1] = b"<< /Type /Pages /Kids [%s] /Count %d >>" % (
        b" ".join(b"%d 0 R" % k for k in kids), len(kids))
    out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for i, data in enumerate(objs, 1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % i + data + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for off in offsets:
        out += b"%010d 00000 n \n" % off
    out += b"trailer\n<< /Size %d /Root %d 0 R /Info << /Title (The Tithe: manual and startup guide) >> >>\nstartxref\n%d\n%%%%EOF\n" % (
        len(objs) + 1, catalog, xref)
    with open(path, "wb") as f:
        f.write(out)


def main():
    src = os.path.join(ROOT, "manual", "manual.md")
    pages, found = build(src)
    for _ in range(3):   # page references can move pages, so set them until they settle
        pages, again = build(src, found)
        if again == found:
            break
        found = again
    missing = [m for m in re.findall(r"\{p:([^}]+)\}", open(src).read()) if m.upper() not in found]
    if missing:
        sys.exit("no chapter called: " + ", ".join(missing))
    out = os.path.join(ROOT, "manual", "Tithe-Manual.pdf")
    write_pdf(pages, out)
    os.makedirs(os.path.join(ROOT, "manual", "pages"), exist_ok=True)
    for i, p in enumerate(pages):
        p.resize((W * 2, H * 2), Image.NEAREST).save(os.path.join(ROOT, "manual", "pages", "%02d.png" % (i + 1)))
    print("%d pages: %s (%d KB)" % (len(pages), out, os.path.getsize(out) // 1024))


if __name__ == "__main__":
    main()
