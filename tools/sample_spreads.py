#!/usr/bin/env python3
"""Chooses a stratified subset of spreads to synthesise readings for.

The engine composes each reading from a fixed set of sentence templates, so the
space it can say is finite and enumerable. This picks the smallest set of
spreads that exercises every one of them at least MIN times, then tops up with
random spreads so the corpus is not all edge cases.

Strata covered:
  card x position   every major in Past, Present and Future     (66)
  fact template     every distinct sentence the engine emits    (~518)
  hidden card       every quintessence result                   (22)
  doubled hidden    the hidden card already on the table, x position

  python3 tools/dump_readings.py > readings.jsonl   # see dump_readings.cpp
  python3 tools/sample_spreads.py readings.jsonl --target 600 -o sample.jsonl
"""
import argparse, collections, json, random, re, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def card_names():
    src = open(os.path.join(ROOT, "src", "tarot_data.h"), encoding="utf8").read()
    return re.findall(r'\{"([^"]+)",\s*"[IVXL0-9]*",\s*EL_', src)


def templater(names):
    rx = re.compile("|".join(sorted((re.escape(n) for n in names), key=len, reverse=True)))
    def t(s):
        s = rx.sub("<C>", s)
        s = re.sub(r"\b\d+\b", "<N>", s)
        s = re.sub(r"\b(Fire|Water|Air|Earth)\b", "<E>", s)
        s = re.sub(r"\b(Past|Present|Future)\b", "<P>", s)
        return " ".join(s.split())
    return t


def features(d, tmpl):
    """Every stratum this spread belongs to."""
    f = {("pos", 0, d["past"]), ("pos", 1, d["present"]), ("pos", 2, d["future"]),
         ("hidden", d["hidden"])}
    section = None
    for para in d["text"].split("\n"):
        p = para.strip()
        if not p:
            continue
        if p[0] == "#":
            section = p[1:]
            continue
        if p[0] == ">":
            continue
        f.add(("fact", section, tmpl(p)))
        if "doubles its weight" in p:
            for i, k in enumerate(("past", "present", "future")):
                if d[k] == d["hidden"]:
                    f.add(("doubled", i))
    return f


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("readings", help="readings.jsonl from dump_readings")
    ap.add_argument("--target", type=int, default=600, help="total spreads to emit")
    ap.add_argument("--min", type=int, default=2, help="times to cover each stratum")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("-o", "--out", help="write chosen readings as JSONL")
    a = ap.parse_args()

    tmpl = templater(card_names())
    rows = [json.loads(l) for l in open(a.readings, encoding="utf8") if l.strip()]
    feats = [features(d, tmpl) for d in rows]
    universe = set().union(*feats)
    print(f"spreads {len(rows)}   strata {len(universe)}", file=sys.stderr)

    need = collections.Counter({k: a.min for k in universe})
    chosen, taken = [], set()
    # Greedy: repeatedly take the spread covering the most still-needed strata.
    while any(v > 0 for v in need.values()):
        best, best_gain = None, 0
        for i, f in enumerate(feats):
            if i in taken:
                continue
            gain = sum(1 for k in f if need[k] > 0)
            if gain > best_gain:
                best, best_gain = i, gain
        if best is None:
            break
        taken.add(best)
        chosen.append(best)
        for k in feats[best]:
            if need[k] > 0:
                need[k] -= 1

    cover = len(chosen)
    rnd = random.Random(a.seed)
    pool = [i for i in range(len(rows)) if i not in taken]
    rnd.shuffle(pool)
    chosen += pool[:max(0, a.target - len(chosen))]

    counts = collections.Counter()
    for i in chosen:
        counts.update(feats[i])
    missing = [k for k in universe if counts[k] == 0]
    print(f"cover set {cover}   + random {len(chosen)-cover}   total {len(chosen)}", file=sys.stderr)
    print(f"strata hit {len(universe)-len(missing)}/{len(universe)}"
          f"   uncovered {len(missing)}", file=sys.stderr)
    kinds = collections.Counter(k[0] for k in universe)
    for kind in sorted(kinds):
        hit = sum(1 for k in universe if k[0] == kind and counts[k] > 0)
        print(f"  {kind:<8} {hit}/{kinds[kind]}", file=sys.stderr)

    out = open(a.out, "w", encoding="utf8") if a.out else sys.stdout
    for i in sorted(chosen):
        out.write(json.dumps(rows[i]) + "\n")
    if a.out:
        out.close()


if __name__ == "__main__":
    main()
