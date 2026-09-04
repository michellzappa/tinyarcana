#!/usr/bin/env python3
"""Turns a shelf of tarot books into grounding material for corpus synthesis.

Four steps: extract text from the library, score each file, keep the
majors-only sources, and index passages per card. Nothing here becomes
training text. The books ground what a reading says; the reading is written
from the engine's own structure in the engine's own register. Training a small
model on book prose makes it reproduce the books, which at this scale it will.

  python3 tools/build_corpus.py extract ~/Dev/Tarot -o work/
  python3 tools/build_corpus.py score   work/
  python3 tools/build_corpus.py select  work/ -o work/corpus.txt
  python3 tools/build_corpus.py index   work/corpus.txt -o work/card_index.json

`extract` needs pdftotext (poppler). textutil is macOS. .mobi, .djvu and .lit
are skipped: no tool here reads them, and nothing in the keep list needs them.

Run `tools/clean_corpus.py clean` on the output of `select` before `index`.
"""
import argparse, collections, json, os, re, subprocess, sys, zipfile, html

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The majors-only sources. A book earns a place by covering all 22 and barely
# mentioning the suits, which is what makes it usable without slicing chapters.
# Substrings, matched against the extracted filename.
KEEP = [
    ("Living_the_Tarot_OCR", "Jayanti, Living the Tarot"),
    ("Thursday_Night_Tarot", "Lotterhand, Thursday Night Tarot"),
    ("Qabalistic_Tarot", "Wang, Qabalistic Tarot"),
    ("Haindl_Tarot_-_Major_Arcana", "Pollack, Haindl Major Arcana"),
    ("Journey_of_the_Hero", "Banzhaf, Journey of the Hero"),
    ("Key_to_the_Wisdom_of_the_Ages", "Case, Key to the Wisdom of the Ages"),
    ("Inspirational_Thoughts_on_the_Tarot", "Davies, Inspirational Thoughts"),
    ("Tarot_Meditations", "Tarot Meditations"),
    ("Seventy-Eight_Degrees", "Pollack, 78 Degrees (majors slice)"),
]
# Two traps in this library, both real, both cost a session to find:
#  - "Living the Tarot.PDF" and "Living the Tarot_OCR.pdf" are the same book.
#    The plain one has a rotten text layer: 21% of its tokens are not words.
#    Always check for a second copy before trying to repair an extraction.
#  - Lotterhand has lost its leading capitals throughout: `arot` for Tarot,
#    `abala` for Qabala. No rule fixes that. Drop it or accept it.

MAJORS = ["The Fool", "The Magician", "The High Priestess", "The Empress",
          "The Emperor", "The Hierophant", "The Lovers", "The Chariot",
          "Strength", "The Hermit", "Wheel of Fortune", "Justice",
          "The Hanged Man", "Death", "Temperance", "The Devil", "The Tower",
          "The Star", "The Moon", "The Sun", "Judgement", "The World"]
ALT = {"The High Priestess": ["high priestess", "the papess"],
       "Wheel of Fortune": ["wheel of fortune"],
       "The Hanged Man": ["hanged man"],
       "Judgement": ["judgement", "judgment"],
       "Strength": ["strength", "fortitude"],
       "The Hierophant": ["hierophant", "the pope"]}
SUIT = re.compile(r"(pentacles|wands|cups|swords|batons)", re.I)


def safe(rel):
    return re.sub(r"[^A-Za-z0-9._-]", "", rel.replace("/", "_").replace(" ", "_"))


def cmd_extract(a):
    out = os.path.join(a.out, "text")
    os.makedirs(out, exist_ok=True)
    n = 0
    for dirpath, _, files in os.walk(a.library):
        for f in sorted(files):
            if f.startswith("."):
                continue
            src = os.path.join(dirpath, f)
            ext = f.rsplit(".", 1)[-1].lower() if "." in f else ""
            dst = os.path.join(out, safe(os.path.relpath(src, a.library)) + ".txt")
            if os.path.exists(dst) and os.path.getsize(dst):
                continue
            try:
                if ext == "pdf":
                    subprocess.run(["pdftotext", "-q", "-enc", "UTF-8", src, dst], check=False)
                elif ext == "txt":
                    open(dst, "w").write(open(src, errors="ignore").read())
                elif ext in ("doc", "docx", "rtf"):
                    t = subprocess.run(["textutil", "-convert", "txt", "-stdout", src],
                                       capture_output=True).stdout.decode("utf8", "ignore")
                    open(dst, "w").write(t)
                elif ext == "epub":
                    z = zipfile.ZipFile(src)
                    parts = []
                    for nm in z.namelist():
                        if nm.lower().endswith((".xhtml", ".html", ".htm")):
                            t = z.read(nm).decode("utf8", "ignore")
                            parts.append(html.unescape(re.sub(r"<[^>]+>", " ", t)))
                    open(dst, "w").write("\n".join(parts))
                else:
                    continue
                n += 1
            except Exception as e:
                print(f"  skip {f}: {e}", file=sys.stderr)
    print(f"extracted {n} files to {out}", file=sys.stderr)
    return 0


def cmd_score(a):
    """Majors coverage and suit density per extracted file."""
    pats = {m: re.compile("|".join(re.escape(x) for x in ALT.get(m, [m.lower()])), re.I)
            for m in MAJORS}
    d = os.path.join(a.work, "text")
    rows = []
    for f in sorted(os.listdir(d)):
        t = open(os.path.join(d, f), errors="ignore").read()
        w = len(t.split())
        if w < 3000:
            continue
        hits = [len(r.findall(t)) for r in pats.values()]
        cov = sum(1 for h in hits if h >= 3)
        rows.append((cov, sum(hits) / w * 1000, len(SUIT.findall(t)) / w * 1000, w, f))
    rows.sort(key=lambda r: (-r[0], r[2]))
    print(f"{'cov/22':>6} {'maj/1k':>7} {'suit/1k':>8} {'words':>8}  file")
    for cov, maj, suit, w, f in rows[:40]:
        flag = "  <- majors-only" if cov == 22 and suit < 2.0 else ""
        print(f"{cov:>6} {maj:>7.2f} {suit:>8.2f} {w:>8,}  {f[:56]}{flag}")
    return 0


def cmd_select(a):
    """Concatenate the keep list, slicing 78 Degrees at its minors transition."""
    d = os.path.join(a.work, "text")
    files = os.listdir(d)
    parts, report = [], []
    for needle, label in KEEP:
        match = next((f for f in files if needle in f), None)
        if not match:
            report.append((label, 0, "MISSING"))
            continue
        t = open(os.path.join(d, match), errors="ignore").read()
        if "Seventy-Eight" in needle:
            words = t.split()
            for i in range(0, len(words) - 2000, 2000):
                if len(SUIT.findall(" ".join(words[i:i + 2000]))) / 2000 * 1000 > 6.0:
                    t = " ".join(words[:i])
                    break
        parts.append(f"\n\n<<<{label}>>>\n\n" + t)
        report.append((label, len(t.split()), "ok"))
    open(a.out, "w").write("\n".join(parts))
    for label, w, note in report:
        print(f"  {label:<40} {w:>9,}  {note}", file=sys.stderr)
    print(f"\nwrote {a.out}", file=sys.stderr)
    return 0


def cmd_index(a):
    """Per-card passages, by sliding window so bad paragraph breaks do not matter."""
    pats = {m: re.compile("|".join(re.escape(x) for x in ALT.get(m, [m.lower()])), re.I)
            for m in MAJORS}
    text = open(a.corpus, errors="ignore").read()
    books = re.split(r"<<<([^>]*)>>>", text)
    WIN, STEP = 900, 450
    wins = []
    for i in range(1, len(books), 2):
        body = " ".join(books[i + 1].split())
        for j in range(0, max(len(body) - WIN, 1), STEP):
            wins.append((books[i], body[j:j + WIN]))
    idx = collections.defaultdict(list)
    for label, w in wins:
        hits = sorted(((len(r.findall(w)), m) for m, r in pats.items()), reverse=True)
        n, m = hits[0]
        if n < 2 or n < hits[1][0] * 2:      # one card must clearly dominate
            continue
        idx[m].append((round(n / (len(w) / 1000), 2), label, w))
    out = {}
    for m in MAJORS:
        rows, seen, keep = sorted(idx[m], reverse=True), set(), []
        for s, l, w in rows:                  # spread across books before repeating
            if l in seen and len(keep) < 6:
                continue
            seen.add(l)
            keep.append((s, l, w))
            if len(keep) == 10:
                break
        out[m] = [{"score": s, "source": l, "text": w} for s, l, w in keep]
        print(f"  {m:<22} {len(out[m]):>3} passages", file=sys.stderr)
    json.dump(out, open(a.out, "w"), indent=1)
    print(f"wrote {a.out}", file=sys.stderr)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("extract"); s.add_argument("library"); s.add_argument("-o", "--out", required=True); s.set_defaults(fn=cmd_extract)
    s = sub.add_parser("score");   s.add_argument("work"); s.set_defaults(fn=cmd_score)
    s = sub.add_parser("select");  s.add_argument("work"); s.add_argument("-o", "--out", required=True); s.set_defaults(fn=cmd_select)
    s = sub.add_parser("index");   s.add_argument("corpus"); s.add_argument("-o", "--out", required=True); s.set_defaults(fn=cmd_index)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
