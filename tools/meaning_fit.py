#!/usr/bin/env python3
"""Check that every card meaning fits the meaning page.

uiMeaning() wraps a position text with txtWrappedFn(..., maxLines 6) and the
meaning column's own chord (meaningWidthAt in src/ui.cpp). Past six lines the
firmware simply stops drawing, with no warning, so an over-long translation
loses its last sentence silently. This is the gate for that.

    python3 tools/meaning_fit.py                 # every language, every deck
    python3 tools/meaning_fit.py pt-BR waite-smith
"""
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import fit_check as fc  # noqa: E402

# Mirrors src/ui.cpp: R, CY, meaningWidthAt(), bodyY, lineH, maxLines.
R = CY = 233
BODY_Y, LINE_H, MAX_LINES, INSET = 204, 30, 6, 44


def width_at(baseline):
    v = R * R - (baseline - 6 - CY) ** 2
    h = max(40, (math.isqrt(v) if v > 0 else 0) - INSET)
    return 2 * h


def lines(font, text):
    out, s, y = 0, text.strip(), BODY_Y
    while s:
        _, s = fc.fit_line(font, s, width_at(y))
        out += 1
        y += LINE_H
        if out > 40:
            break
    return out


def main():
    font = fc.load_font("lora_meaning")
    langs = sys.argv[1:2] or sorted(os.listdir(os.path.join(ROOT, "assets", "lang")))
    bad = 0
    for lang in langs:
        d = os.path.join(ROOT, "assets", "lang", lang)
        decks = sys.argv[2:3] or sorted(
            f[:-5] for f in os.listdir(d) if f.endswith(".json") and f != "ui.json")
        for deck in decks:
            path = os.path.join(d, deck + ".json")
            if not os.path.isfile(path):
                continue
            worst = 0
            with open(path, encoding="utf8") as f:
                cards = json.load(f)["cards"]
            for c in cards:
                for pos in ("past", "present", "future"):
                    n = lines(font, c[pos])
                    worst = max(worst, n)
                    if n > MAX_LINES:
                        bad += 1
                        print("  OVER %d/%d  %s %s / %s: %s"
                              % (n, MAX_LINES, lang, deck, pos, c["name"]))
            print("%-6s %-10s worst %d of %d lines" % (lang, deck, worst, MAX_LINES))
    print("\n%d meaning(s) over the limit" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
