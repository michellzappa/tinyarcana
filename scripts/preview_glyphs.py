"""Render every glyph in src/glyphs.cpp to one PNG, the way the firmware does.

    .venv/bin/python scripts/preview_glyphs.py out.png [size]
"""
import math
import re
import sys

from PIL import Image

HERE = __import__("os").path.dirname(__import__("os").path.abspath(__file__))


def load_glyphs():
    """Returns (names, glyphs) parsed from src/glyphs.cpp."""
    src = open(HERE + "/../src/glyphs.cpp").read()
    table = src[src.index("GLYPH_TABLE_BEGIN"):src.index("GLYPH_TABLE_END")]
    names = re.findall(r"// (G_[A-Z]+)", table)
    tokens = re.findall(r"(SEG|ARC|CIRC|DISC|END)(?:\(([^)]*)\))?", table)
    glyphs, cur = [], []
    for kind, args in tokens:
        if kind == "END":
            glyphs.append(cur)
            cur = []
            continue
        v = [float(x.strip().rstrip("f")) for x in args.split(",")]
        if kind == "CIRC":
            cur.append(("ARC", v[0], v[1], v[2], 0, 360))
        elif kind == "SEG":
            cur.append(("SEG", *v))
        elif kind == "ARC":
            cur.append(("ARC", *v))
        else:
            cur.append(("DISC", *v))
    return names, glyphs


def seg_dist(px, py, x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    l2 = dx * dx + dy * dy
    t = ((px - x0) * dx + (py - y0) * dy) / l2 if l2 else 0
    t = max(0, min(1, t))
    return math.hypot(px - (x0 + t * dx), py - (y0 + t * dy))


def arc_dist(px, py, cx, cy, r, a0, a1):
    dx, dy = px - cx, py - cy
    d = math.hypot(dx, dy)
    if a1 - a0 >= 360:
        return abs(d - r)
    ang = math.degrees(math.atan2(dy, dx))
    while ang < a0:
        ang += 360
    while ang >= a0 + 360:
        ang -= 360
    if ang <= a1:
        return abs(d - r)
    e0 = (cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0)))
    e1 = (cx + r * math.cos(math.radians(a1)), cy + r * math.sin(math.radians(a1)))
    return min(math.hypot(px - e0[0], py - e0[1]), math.hypot(px - e1[0], py - e1[1]))


def glyph_alpha(strokes, size):
    """Alpha mask (size x size list of rows) exactly as glyphDraw() computes it."""
    hw = size / 32.0
    rows = []
    for j in range(size):
        uy = (j + 0.5) / size
        row = []
        for i in range(size):
            ux = (i + 0.5) / size
            best = 1e9
            for s in strokes:
                if s[0] == "SEG":
                    d = seg_dist(ux, uy, *s[1:5]) * size
                elif s[0] == "ARC":
                    d = arc_dist(ux, uy, *s[1:6]) * size
                else:
                    d = max(0, math.hypot(ux - s[1], uy - s[2]) - s[3]) * size - hw
                best = min(best, d)
            row.append(max(0, min(1, hw + 0.5 - best)))
        rows.append(row)
    return rows


def main():
    names, glyphs = load_glyphs()
    size = int(sys.argv[2]) if len(sys.argv) > 2 else 48
    pad = 12
    cols = 6
    nrows = (len(glyphs) + cols - 1) // cols
    img = Image.new("L", (cols * (size + pad) + pad, nrows * (size + pad + 14) + pad), 0)
    px = img.load()
    for n, strokes in enumerate(glyphs):
        gx = pad + (n % cols) * (size + pad)
        gy = pad + (n // cols) * (size + pad + 14)
        for j, row in enumerate(glyph_alpha(strokes, size)):
            for i, a in enumerate(row):
                px[gx + i, gy + j] = int(a * 255)
    img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
    img.save(sys.argv[1])
    print("%d glyphs: %s" % (len(glyphs), " ".join(names)))


if __name__ == "__main__":
    main()
