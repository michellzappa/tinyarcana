#!/usr/bin/env python3
"""Scores and repairs OCR damage in the reference corpus.

The books are grounding material for corpus synthesis, never training text, but
the model copies register from what it reads - so damaged passages produce
damaged readings. This does two jobs:

  score   percent of tokens not in the system dictionary, per source. Clean
          prose sits near 1%. Above ~8% means a bad text layer, and the usual
          fix is a different extraction of the same book, not repair.
  clean   the mechanical repairs, which are the only safe ones.

Repairable: soft hyphens, hyphenation across a line break, a missing space
after a full stop, run-together words that split into two dictionary words,
letter-spaced runs.

Not repairable: character misreads (`tbe` for the, `vou` for you, `crrds` for
cards) and case misreads (`sPecialthanks`). Nothing recovers those, which is
why `score` exists - drop the source instead.

  python3 tools/clean_corpus.py score corpus.txt
  python3 tools/clean_corpus.py clean corpus.txt -o corpus.clean.txt
"""
import argparse, collections, os, re, sys

DICT = "/usr/share/dict/words"
SUFFIXES = ("s", "es", "ed", "d", "ing", "ly", "er", "est", "ies")


def load_dict(path=DICT):
    if not os.path.exists(path):
        sys.exit(f"no dictionary at {path}; scoring needs one")
    return {w.strip().lower() for w in open(path, errors="ignore") if w.strip()}


def known(word, D):
    """Dictionary membership with the inflections web2 leaves out."""
    w = word.lower().strip("'")
    if len(w) < 3 or w in D:
        return True
    for s in SUFFIXES:
        if w.endswith(s) and w[: -len(s)] in D:
            return True
    if w.endswith("ies") and w[:-3] + "y" in D:
        return True
    if w.endswith("ing") and (w[:-3] + "e" in D or w[:-3] in D):
        return True
    return False


WORD = re.compile(r"[A-Za-z][A-Za-z']{2,}")


def score(text, D):
    words = WORD.findall(text)
    if not words:
        return 0.0, 0, collections.Counter()
    bad = collections.Counter(w.lower() for w in words if not known(w, D))
    return sum(bad.values()) / len(words) * 100, len(words), bad


def split_run_together(word, D, freq):
    """`ofthe` -> `of the`, but only when the evidence is strong.

    Dictionary membership alone splits proper names - `Lotterhand` becomes
    "Lotter hand", `Jayanti` becomes "Jay anti". Three extra conditions stop
    that: the token must be lowercase (names are capitalised), rare in this
    corpus (a real word recurs), and both halves must be common in it.
    """
    if len(word) < 5 or not word.islower() or known(word, D):
        return None
    if freq.get(word, 0) > 3:
        return None
    for i in range(2, len(word) - 1):
        a, b = word[:i], word[i:]
        if a in D and b in D and freq.get(a, 0) >= 50 and freq.get(b, 0) >= 50:
            return f"{a} {b}"
    return None


def clean(text, D, do_split=False):
    """Repairs bodies only; the <<<label>>> markers are held out and restored."""
    labels = re.findall(r"<<<[^>]*>>>", text)
    text = re.sub(r"<<<[^>]*>>>", "\x00", text)
    counts = collections.Counter()

    n = text.count("­")
    text = text.replace("­", "")
    counts["soft hyphen"] = n

    # Hyphenation across a line break, then the same across a space.
    text, n = re.subn(r"-\n(?=[a-z])", "", text)
    counts["hyphen + newline"] = n
    # `contem- plate` is one broken word; `balancing- of` is two words with a
    # stray hyphen. Join only when the join is a real word, else drop the
    # hyphen and keep the space.
    def rejoin(m):
        a, b = m.group(1), m.group(2)
        return a + b if known(a + b, D) else f"{a} {b}"
    text, n = re.subn(r"\b([a-z]{2,})-\s+([a-z]{2,})\b", rejoin, text)
    counts["hyphen + space"] = n

    # Letter-spaced runs: "s h e" -> "she". Collapse only when the joined
    # result is a real word, which lets the rule reach down to three letters
    # without turning "I a m" into "Iam".
    def collapse(m):
        joined = m.group(0).replace(" ", "")
        return joined if known(joined, D) else m.group(0)
    text, n = re.subn(r"\b(?:[A-Za-z] ){2,}[A-Za-z]\b", collapse, text)
    counts["letter-spaced run"] = n

    text, n = re.subn(r"([a-z]{2,})\.([A-Z][a-z]{2,})", r"\1. \2", text)
    counts["missing space after ."] = n

    # Off by default. The system dictionary is web2 (1934) and lacks ordinary
    # modern compounds - artwork, childcare, channeling - so the rule splits
    # real words. It repairs a few hundred genuine run-togethers and damages a
    # comparable number of good ones, which is not a trade worth taking
    # unsupervised. Enable with --split and read the diff.
    if do_split:
        freq = collections.Counter(w.lower() for w in WORD.findall(text))
        out, splits = [], 0
        for tok in re.split(r"(\W+)", text):
            if tok.isalpha() and len(tok) > 4:
                sp = split_run_together(tok, D, freq)
                if sp:
                    out.append(sp)
                    splits += 1
                    continue
            out.append(tok)
        text = "".join(out)
        counts["run-together split"] = splits

    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    for lab in labels:
        text = text.replace("\x00", lab, 1)
    return text, counts


def sources(text):
    """Yields (label, body) for a <<<label>>> corpus, else one unlabelled body."""
    parts = re.split(r"<<<([^>]*)>>>", text)
    if len(parts) == 1:
        return [("(whole file)", text)]
    return [(parts[i], parts[i + 1]) for i in range(1, len(parts), 2)]


def cmd_score(a):
    D = load_dict()
    text = open(a.corpus, encoding="utf8", errors="ignore").read()
    print(f"{'source':<44} {'words':>9} {'unknown':>8}")
    for label, body in sorted(sources(text), key=lambda s: -score(s[1], D)[0]):
        r, n, bad = score(body, D)
        flag = "  <-- bad text layer" if r > 8 else ""
        print(f"  {label[:42]:<42} {n:>9,} {r:>7.2f}%{flag}")
        if a.show and r > 8:
            print("      " + ", ".join(f"{w}({c})" for w, c in bad.most_common(10)))
    print("\nclean prose scores near 1%. Above 8% means a bad extraction:")
    print("look for another copy of the book before trying to repair it.")
    return 0


def cmd_clean(a):
    D = load_dict()
    text = open(a.corpus, encoding="utf8", errors="ignore").read()
    before = score(text, D)[0]
    out, counts = clean(text, D, do_split=a.split)
    after = score(out, D)[0]
    for k, v in counts.items():
        print(f"  {k:<24} {v:>6,}", file=sys.stderr)
    print(f"  unknown-token rate       {before:.2f}% -> {after:.2f}%", file=sys.stderr)
    open(a.out, "w", encoding="utf8").write(out)
    print(f"wrote {a.out}", file=sys.stderr)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("score")
    s.add_argument("corpus")
    s.add_argument("--show", action="store_true", help="list the worst tokens")
    s.set_defaults(fn=cmd_score)
    s = sub.add_parser("clean")
    s.add_argument("corpus")
    s.add_argument("-o", "--out", required=True)
    s.add_argument("--split", action="store_true",
                   help="also split run-together words (see the note in clean())")
    s.set_defaults(fn=cmd_clean)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
