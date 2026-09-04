"""Simulate the round board's meaning page with the same fonts, chord
wrapping and glyph strokes the firmware uses, for layout decisions.

    .venv/bin/python scripts/preview_read.py out.png
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from preview_glyphs import glyph_alpha, load_glyphs  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = HERE + "/../assets/fonts/"
W = H = 466
CX = CY = 233
R = 233
GOLD, GOLD_DIM, IVORY, DIM, RULE = (214, 176, 96), (120, 100, 60), (232, 224, 205), (122, 114, 104), (50, 44, 60)


def font(size, weight=400, italic=False):
    f = ImageFont.truetype(FONTS + ("Lora-Italic-VariableFont_wght.ttf" if italic else "Lora-VariableFont_wght.ttf"), size)
    try:
        f.set_variation_by_axes([weight])
    except Exception:
        pass
    return f


def chord_half(y):
    v = R * R - (y - CY) ** 2
    return math.sqrt(v) if v > 0 else 0


def draw_center(d, f, text, y, col, tracking=0):
    # Baseline anchor, like the firmware.
    if tracking:
        w = sum(f.getlength(c) + tracking for c in text) - tracking
        x = CX - w / 2
        for c in text:
            d.text((x, y), c, font=f, fill=col, anchor="ls")
            x += f.getlength(c) + tracking
    else:
        d.text((CX, y), text, font=f, fill=col, anchor="ms")


def wrap_chord(d, f, text, y0, line_h, inset, col, max_lines):
    words = text.split()
    y = y0
    lines = 0
    while words and lines < max_lines:
        maxw = 2 * (chord_half(y - 6) - inset)
        line = words[0]
        k = 1
        while k < len(words) and f.getlength(line + " " + words[k]) <= maxw:
            line += " " + words[k]
            k += 1
        words = words[k:]
        d.text((CX, y), line, font=f, fill=col, anchor="ms")
        y += line_h
        lines += 1
    return y


def draw_glyph(img, glyph, cy, size, col):
    px = img.load()
    x0, y0 = CX - size // 2, cy - size // 2
    for j, row in enumerate(glyph_alpha(glyph, size)):
        for i, a in enumerate(row):
            if a <= 0:
                continue
            bg = px[x0 + i, y0 + j]
            px[x0 + i, y0 + j] = tuple(int(bg[k] * (1 - a) + col[k] * a) for k in range(3))


def render(spec, card):
    img = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(img)
    # Glass edge for reference.
    d.ellipse((0, 0, W - 1, H - 1), outline=(40, 40, 40))
    draw_center(d, font(spec["label"], 600), card["pos"].upper(), spec["labelY"], GOLD, 3)
    draw_center(d, font(spec["name"], 500), card["name"], spec["nameY"], IVORY)
    draw_center(d, font(spec["keys"], 400, True), card["keys"], spec["keysY"], DIM)
    d.line((CX - 70, spec["ruleY"], CX + 70, spec["ruleY"]), fill=GOLD_DIM)
    y = wrap_chord(d, font(spec["body"]), card["text"], spec["bodyY"], spec["lineH"], spec.get("inset", 52), IVORY, spec["maxLines"])
    names, glyphs = load_glyphs()
    glyph_y = spec["glyphY"]
    if spec.get("flow"):
        glyph_y = min(y - spec["lineH"] + spec.get("gap", 44), spec["dotsY"] - spec.get("floor", 52))
    draw_glyph(img, glyphs[names.index(card["glyph"])], glyph_y, spec["glyph"], GOLD)
    d = ImageDraw.Draw(img)
    cap_y = glyph_y + spec.get("capGap", 38) if spec.get("flow") else spec["capY"]
    draw_center(d, font(spec["cap"], 600), card["cap"], cap_y, GOLD_DIM, 2)
    for i in range(3):
        x = CX - 12 + i * 12
        if i == 1:
            d.ellipse((x - 3, spec["dotsY"] - 3, x + 3, spec["dotsY"] + 3), fill=GOLD)
        else:
            d.ellipse((x - 2, spec["dotsY"] - 2, x + 2, spec["dotsY"] + 2), outline=GOLD_DIM)
    draw_center(d, font(12, 600), "TAP: SPREAD   BOOT: NEXT CARD", 426, DIM, 1)
    draw_center(d, font(12, 600), "PWR: INNER READING", 444, DIM, 1)
    return img


CURRENT = dict(label=12, labelY=84, name=26, nameY=118, keys=15, keysY=144, ruleY=160,
               body=17, lineH=24, bodyY=190, maxLines=7, glyph=34, glyphY=364, cap=12, capY=398, dotsY=408)
BUMPED = dict(label=14, labelY=80, name=30, nameY=120, keys=17, keysY=150, ruleY=166,
              body=19, lineH=27, bodyY=198, maxLines=6, glyph=40, glyphY=354, cap=14, capY=398, dotsY=408)
FLOW = dict(BUMPED, flow=True)
# One more notch. The body column widens a little (inset 44) so the longest
# meanings still hold five lines.
BUMPED2 = dict(label=16, labelY=78, name=34, nameY=122, keys=19, keysY=154, ruleY=170,
               body=21, lineH=30, bodyY=204, maxLines=6, inset=44, glyph=46, glyphY=0, cap=16, capY=0,
               dotsY=416, flow=True, gap=50, floor=64, capGap=44)


def longest_cards(n=3):
    """The n longest position texts in src/tarot_data.h, as card dicts."""
    import re
    src = open(HERE + "/../src/tarot_data.h").read()
    names, glyphs = load_glyphs()
    gsrc = open(HERE + "/../src/glyphs.cpp").read()
    card_glyphs = re.findall(r"  (G_[A-Z]+),\s*// \d+", gsrc)
    entries = re.findall(r'\{"([^"]+)", "([^"]+)", EL_([A-Z]+), "([^"]+)", "([^"]+)",\s*"[^"]*",\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",', src)
    out = []
    for i, (name, num, el, ruler, keys, past, present, future) in enumerate(entries):
        for pos, text in (("Past", past), ("Present", present), ("Future", future)):
            cap = "%s   %s   %s" % (num, ruler.upper(), el) if ruler.upper() != el else "%s   %s" % (num, el)
            out.append(dict(pos=pos, name=name, keys=keys, glyph=card_glyphs[i], cap=cap, text=text))
    out.sort(key=lambda c: -len(c["text"]))
    return out[:n]


CARDS = [
    dict(pos="Present", name="The High Priestess", keys="intuition, stillness, the unsaid", glyph="G_MOON",
         cap="II   THE MOON   WATER",
         text="Not everything here wants to be solved. Sit with what you already know. The answer is behind the veil, not in the questions you keep asking."),
    dict(pos="Future", name="The Hermit", keys="solitude, search, guidance", glyph="G_VIRGO",
         cap="IX   VIRGO   EARTH",
         text="A period of retreat approaches, or a guide who has already walked this. Either way, the answer is up the mountain, not in the village."),
]

if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[2] == "longest":
        specs = [("flashed (notch 1)", FLOW), ("notch 2", BUMPED2)]
        cards = longest_cards(3)
    else:
        specs = [("current", CURRENT), ("bumped", BUMPED), ("bumped + flow (firmware)", FLOW)]
        cards = CARDS
    sheet = Image.new("RGB", (len(specs) * (W + 20) + 20, len(cards) * (H + 40) + 20), (24, 24, 24))
    d = ImageDraw.Draw(sheet)
    for ci, card in enumerate(cards):
        for si, (label, spec) in enumerate(specs):
            x = 20 + si * (W + 20)
            y = 20 + ci * (H + 40)
            d.text((x, y), "%s  %s" % (label, card["name"]), fill=(200, 200, 200), font=font(16))
            sheet.paste(render(spec, card), (x, y + 24))
    sheet.save(sys.argv[1])
