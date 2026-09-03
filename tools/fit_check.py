#!/usr/bin/env python3
"""Reports how a reading lays out on the device, without a board.

Mirrors txtLayout() in src/text.cpp exactly: the same greedy line fit, the same
8 px paragraph gap that costs no line, the same rule that keeps a heading with
the first line of its body, and the same per-line width from the chord of the
round face (readWidthAt() in src/ui.cpp). Glyph advances are read from the
generated font headers, so this stays correct when the fonts are rebuilt.

  python3 tools/fit_check.py reading.txt
  python3 tools/fit_check.py --board 18 --max-pages 2 corpus/*.txt
  ... | python3 tools/fit_check.py --jsonl --field text --max-pages 2 -

Exits non-zero if any input exceeds --max-pages, so it can gate a corpus.
"""
import argparse, json, math, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_font(name):
    """Glyph advances and the first/last codepoints from src/fonts/<name>.h."""
    src = open(os.path.join(ROOT, "src", "fonts", name + ".h"), encoding="utf8").read()
    body = src[src.index(name + "_glyphs[] = {"):]
    adv = [int(m) for m in re.findall(
        r"\{\s*\d+,\s*\d+,\s*\d+,\s*-?\d+,\s*-?\d+,\s*(\d+)\s*\}", body)]
    m = re.search(r"const AaFont " + name + r"\s*=\s*\{[^,]+,[^,]+,\s*(\d+),\s*(\d+),", src)
    first, last = (int(m.group(1)), int(m.group(2))) if m else (32, 126)
    return {"adv": adv, "first": first, "last": last}


def width(font, s, tracking=0):
    """txtWidth(): sum of advances plus tracking between glyphs."""
    if not s:
        return 0
    a, first, last = font["adv"], font["first"], font["last"]
    w = 0
    for ch in s:
        o = ord(ch)
        if o < first or o > last:
            o = ord("?")
        w += a[o - first] + tracking
    return w - tracking


def fit_line(font, s, max_w):
    """txtFitLine(): greedy, breaking at the last space that still fits.

    Tracking is not applied here, matching the firmware: headings break as if
    untracked and are only tracked when centered.
    """
    q, last_break = 0, None
    while q < len(s):
        word_end = s.find(" ", q)
        if word_end < 0:
            word_end = len(s)
        if width(font, s[:word_end]) > max_w and last_break is not None:
            break
        last_break = word_end
        q = word_end
        while q < len(s) and s[q] == " ":
            q += 1
        if word_end >= len(s):
            break
    end = last_break if last_break is not None else q
    if end == 0:                       # one oversized word: hard cut
        end = s.find(" ")
        if end < 0:
            end = len(s)
    nxt = end
    while nxt < len(s) and s[nxt] == " ":
        nxt += 1
    return s[:end], s[nxt:]


BOARDS = {
    # readWidthAt() plus the INNER_TOP/INNER_BOTTOM pair for each board.
    "round": {"top": 110, "bottom": 388, "round": True, "r": 233, "cy": 233,
              "cx": 233, "edge": 52, "label": "1.75 round 466x466"},
    "18":    {"top": 96,  "bottom": 404, "round": False, "w": 368 - 2 * 26,
              "label": "1.8 portrait 368x448"},
}
LINE_H, GAP_H, HEAD_H = 24, 8, 24


def line_width(b, y):
    if not b["round"]:
        return b["w"]
    dy = y - 6 - b["cy"]
    v = b["r"] * b["r"] - dy * dy
    h = (0 if v <= 0 else int(math.sqrt(v))) - b["edge"]
    return 2 * (h if h >= 40 else 40)


def layout(text, board, fonts):
    """Returns (lines, pages) the way txtLayout() would produce them."""
    b = BOARDS[board]
    body, italic, head = fonts["read"], fonts["read_italic"], fonts["small"]
    y, page, out = b["top"], 0, []
    for para in text.split("\n"):
        style, s = "body", para
        if not para.strip():
            style = "gap"
        elif para[0] == "#":
            style, s = "head", para[1:]
        elif para[0] == ">":
            style, s = "italic", para[1:]
        s = s.lstrip(" ")
        if style == "gap":
            if y != b["top"]:
                y += GAP_H
            continue
        f = head if style == "head" else (italic if style == "italic" else body)
        lh = HEAD_H if style == "head" else LINE_H
        if y > b["bottom"] or (style == "head" and y + LINE_H > b["bottom"]):
            page += 1
            y = b["top"]
        while s:
            if y > b["bottom"]:
                page += 1
                y = b["top"]
            ln, s = fit_line(f, s, line_width(b, y))
            out.append((page, style, ln))
            y += lh
    return out, page + 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="+", help="text files, or - for stdin")
    ap.add_argument("--board", choices=sorted(BOARDS), default="round")
    ap.add_argument("--max-pages", type=int, default=0, help="fail above this")
    ap.add_argument("--max-lines", type=int, default=0, help="fail above this")
    ap.add_argument("--jsonl", action="store_true", help="input is JSON Lines")
    ap.add_argument("--field", default="text", help="JSONL field holding the text")
    ap.add_argument("--show", action="store_true", help="print the wrapped lines")
    a = ap.parse_args()

    fonts = {k: load_font("lora_" + k) for k in ("read", "read_italic", "small")}
    items = []
    for path in a.files:
        raw = sys.stdin.read() if path == "-" else open(path, encoding="utf8").read()
        if a.jsonl:
            for i, line in enumerate(raw.splitlines()):
                if line.strip():
                    items.append((f"{path}:{i}", json.loads(line)[a.field]))
        else:
            items.append((path, raw.strip()))

    print(f"board: {BOARDS[a.board]['label']}")
    worst, bad = 0, 0
    for name, text in items:
        lines, pages = layout(text, a.board, fonts)
        worst = max(worst, pages)
        over = (a.max_pages and pages > a.max_pages) or (a.max_lines and len(lines) > a.max_lines)
        bad += bool(over)
        if len(items) <= 40 or over:
            print(f"  {'FAIL' if over else 'ok  '} {len(lines):>3} lines {pages} page(s)  {name}")
        if a.show:
            for p, style, ln in lines:
                print(f"      p{p} {style[:4]:<4} |{ln}")
    if len(items) > 40:
        print(f"  {len(items)} inputs, worst {worst} page(s), {bad} over limit")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
