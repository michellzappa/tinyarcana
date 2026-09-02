"""Render a string from a generated glyph table to PNG, to eyeball the tables."""
import re, sys
from PIL import Image
name, text, out = sys.argv[1], sys.argv[2], sys.argv[3]
src = open("src/fonts/%s.h" % name).read()
alpha = bytes(int(v) for v in re.search(r"_alpha\[\] = \{(.*?)\};", src, re.S).group(1).replace("\n", "").split(",") if v.strip())
glyphs = [tuple(int(v) for v in g.split(",")) for g in re.findall(r"\{(-?\d+(?:, ?-?\d+){5})\}", src)]
first, last, lineh, asc = [int(v) for v in re.search(r"= \{\w+_alpha, \w+_glyphs, (\d+), (\d+), (\d+), (\d+)\}", src).groups()]
img = Image.new("L", (600, lineh + 8), 0); px = img.load(); x = 4
for ch in text:
    off, w, h, xo, yo, adv = glyphs[ord(ch) - first]
    for j in range(h):
        for i in range(w):
            X, Y = x + xo + i, 4 + asc + yo + j
            if 0 <= X < 600 and 0 <= Y < img.height: px[X, Y] = max(px[X, Y], alpha[off + j * w + i])
    x += adv
img.save(out)
