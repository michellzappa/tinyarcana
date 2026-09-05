#!/usr/bin/env python3
"""Writes the training corpus: N one-page readings per spread, via the Batch API.

Three steps, because a batch runs asynchronously and may take up to 24 hours:

  python3 tools/synthesize.py submit  sample.jsonl --index card_index.json -o batch.id
  python3 tools/synthesize.py status  batch.id
  python3 tools/synthesize.py collect batch.id --books corpus_majors.txt -o corpus.jsonl

One request per spread asks for every variant at once. The reference passages
are then sent once instead of once per variant, and the model can make the
variants deliberately unlike each other, which single-variant requests cannot.

`collect` is where the gates run. A reading is kept only if it is 45-55 words,
lays out on one page on the round board, and shares no 8-gram with the source
books. Rejects are written alongside with the reason, so a bad STYLE block
shows up as a pattern rather than a silent shortfall.

Cost: Batch API is half price. `submit --dry-run` prints one full prompt and a
measured estimate before anything is spent.
"""
import argparse, collections, importlib.util, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODEL = "claude-opus-5"

# Loaded rather than imported so fit_check.py stays a standalone script.
_spec = importlib.util.spec_from_file_location("fit_check", os.path.join(ROOT, "tools", "fit_check.py"))
fit_check = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fit_check)

STYLE = """You write the inner reading for a tarot device. Twenty-two majors, a
Past / Present / Future spread. The device holds no other text: what you write
is the whole reading.

VOICE
Plain modern English. Short declarative sentences. Second person. Concrete
nouns. No occult vocabulary, no Qabalah, no astrology, no capitalised
abstractions, no "energies" or "vibrations". Say one thing per sentence. You
may be blunt. Do not console. Do not hedge.

FORM
45 to 55 words. ONE paragraph. No headings, no line breaks, no closing question
- the device adds its own question underneath.

CONTENT
The reading is about the three cards TOGETHER. Never a card-by-card lookup.
Build it from the structural facts you are given, which come from the device's
own engine. At this length you cannot use every fact. Choose the two or three
that carry the spread and drop the rest. Which ones you choose is the reading.

The reference passages tell you what each card means. Take the substance and
leave the phrasing. Do not quote them. Never name a book or an author.

CONSTRAINTS
Refer to cards by the names given. Never invent a card, a number, a suit or a
position. Do not mention the hidden card unless the facts name it.

VARIANTS
You will be asked for several readings of the same spread. Make them genuinely
different: a different pair of facts, a different way in, a different last
sentence. They are not paraphrases of one another. A person who draws this
spread twice should not recognise the second reading."""

SCHEMA = {
    "type": "object",
    "properties": {
        "readings": {"type": "array", "items": {"type": "string"}},
    },
    "required": ["readings"],
    "additionalProperties": False,
}


def build_user(d, index, n):
    """The per-spread half of the prompt: facts from the engine, then sources."""
    facts = "\n".join("  " + l for l in d["text"].split("\n")
                      if l.strip() and l.strip()[0] not in "#>")
    parts = [
        "SPREAD", "",
        f"  Past:    {d['past_name']}  ({d['elements'][0]})",
        f"  Present: {d['present_name']}  ({d['elements'][1]})",
        f"  Future:  {d['future_name']}  ({d['elements'][2]})",
        f"  Hidden:  {d['hidden_name']}", "",
        "STRUCTURAL FACTS", "", facts, "",
        "REFERENCE PASSAGES", "",
    ]
    for name in (d["past_name"], d["present_name"], d["future_name"]):
        parts.append(f"  --- {name} ---")
        for row in index.get(name, [])[:2]:
            parts.append("  " + re.sub(r"\s+", " ", row["text"]).strip()[:520] + " [...]")
        parts.append("")
    parts.append(f"Write {n} different readings of this spread.")
    return "\n".join(parts)


def requests_for(rows, index, n):
    from anthropic.types.message_create_params import MessageCreateParamsNonStreaming
    from anthropic.types.messages.batch_create_params import Request
    out = []
    for d in rows:
        cid = f"s{d['past']}-{d['present']}-{d['future']}"
        out.append(Request(
            custom_id=cid,
            params=MessageCreateParamsNonStreaming(
                model=MODEL,
                max_tokens=16000,
                # The style block is identical on every request, so it caches.
                system=[{"type": "text", "text": STYLE,
                         "cache_control": {"type": "ephemeral"}}],
                thinking={"type": "adaptive"},
                output_config={"format": {"type": "json_schema", "schema": SCHEMA}},
                messages=[{"role": "user", "content": build_user(d, index, n)}],
            ),
        ))
    return out


# ---- gates -------------------------------------------------------------

def ngrams(text, n=8):
    toks = re.findall(r"[a-z]+", text.lower())
    return {tuple(toks[i:i + n]) for i in range(len(toks) - n + 1)}


def load_book_ngrams(path, n=8):
    text = re.sub(r"<<<[^>]*>>>", " ", open(path, encoding="utf8").read())
    return ngrams(text, n)


def check(reading, book_ngrams, fonts, lo, hi):
    """Returns None if the reading passes, else why it failed."""
    words = len(reading.split())
    if not lo <= words <= hi:
        return f"words={words}"
    if "\n" in reading.strip():
        return "multi-paragraph"
    # Whatever boards fit_check knows about. The 1.8 was dropped when the
    # project went round-only; naming boards here again would just rot.
    for board in fit_check.BOARDS:
        _, pages = fit_check.layout(reading + "\n\n>x", board, fonts)
        if pages > 1:
            return f"{board}={pages}pages"
    if book_ngrams:
        hit = ngrams(reading) & book_ngrams
        if hit:
            return "8gram:" + " ".join(next(iter(hit)))
    return None


# ---- commands ----------------------------------------------------------

def cmd_submit(a):
    rows = [json.loads(l) for l in open(a.spreads, encoding="utf8") if l.strip()]
    index = json.load(open(a.index, encoding="utf8"))
    reqs = requests_for(rows, index, a.variants)
    print(f"{len(reqs)} requests, {a.variants} readings each "
          f"-> {len(reqs) * a.variants} readings", file=sys.stderr)

    if a.dry_run:
        print("=" * 70 + "\nSYSTEM\n" + "=" * 70)
        print(STYLE)
        print("=" * 70 + f"\nUSER (first of {len(reqs)})\n" + "=" * 70)
        print(build_user(rows[0], index, a.variants))
        import anthropic
        client = anthropic.Anthropic()
        n = client.messages.count_tokens(
            model=MODEL,
            system=[{"type": "text", "text": STYLE}],
            messages=[{"role": "user", "content": build_user(rows[0], index, a.variants)}],
        ).input_tokens
        out_est = a.variants * 90 + 1200          # readings plus thinking
        ins, outs = n * len(reqs), out_est * len(reqs)
        # Opus 5 list price, halved for batch.
        cost = (ins / 1e6 * 5.0 + outs / 1e6 * 25.0) * 0.5
        print("=" * 70, file=sys.stderr)
        print(f"measured input {n} tokens/request", file=sys.stderr)
        print(f"total ~{ins/1e6:.2f}M in, ~{outs/1e6:.2f}M out (estimated)", file=sys.stderr)
        print(f"BATCH COST ESTIMATE: ${cost:,.2f}  (caching will reduce input)", file=sys.stderr)
        return 0

    import anthropic
    batch = anthropic.Anthropic().messages.batches.create(requests=reqs)
    print(f"batch {batch.id}  status {batch.processing_status}", file=sys.stderr)
    (open(a.out, "w") if a.out else sys.stdout).write(batch.id + "\n")
    return 0


def cmd_status(a):
    import anthropic
    b = anthropic.Anthropic().messages.batches.retrieve(read_id(a.batch))
    c = b.request_counts
    print(f"{b.id}  {b.processing_status}")
    print(f"  processing {c.processing}  succeeded {c.succeeded} "
          f"errored {c.errored}  canceled {c.canceled}  expired {c.expired}")
    return 0 if b.processing_status == "ended" else 2


def cmd_collect(a):
    import anthropic
    fonts = {k: fit_check.load_font("lora_" + k) for k in ("read", "read_italic", "small")}
    book_ngrams = load_book_ngrams(a.books) if a.books else set()
    if book_ngrams:
        print(f"book 8-grams: {len(book_ngrams):,}", file=sys.stderr)

    kept, rejects = [], []
    errors = 0
    for res in anthropic.Anthropic().messages.batches.results(read_id(a.batch)):
        if res.result.type != "succeeded":
            errors += 1
            continue
        msg = res.result.message
        text = next((b.text for b in msg.content if b.type == "text"), "")
        try:
            readings = json.loads(text)["readings"]
        except (json.JSONDecodeError, KeyError):
            errors += 1
            continue
        p, pr, f = (int(x) for x in res.custom_id[1:].split("-"))
        for i, r in enumerate(readings):
            r = " ".join(r.split())
            why = check(r, book_ngrams, fonts, a.min_words, a.max_words)
            row = {"past": p, "present": pr, "future": f, "variant": i, "text": r}
            if why:
                rejects.append({**row, "reason": why})
            else:
                kept.append(row)

    with open(a.out, "w", encoding="utf8") as fh:
        for r in kept:
            fh.write(json.dumps(r) + "\n")
    if rejects:
        rp = os.path.splitext(a.out)[0] + ".rejects.jsonl"
        with open(rp, "w", encoding="utf8") as fh:
            for r in rejects:
                fh.write(json.dumps(r) + "\n")
        print(f"rejects -> {rp}", file=sys.stderr)

    total = len(kept) + len(rejects)
    print(f"kept {len(kept)} / {total}"
          f"   rejected {len(rejects)}   request errors {errors}", file=sys.stderr)
    why = collections.Counter(r["reason"].split(":")[0].split("=")[0] for r in rejects)
    for k, v in why.most_common():
        print(f"  {k:<16} {v}", file=sys.stderr)
    words = sum(len(r["text"].split()) for r in kept)
    print(f"corpus: {words:,} words", file=sys.stderr)
    return 0


def read_id(s):
    """Accept a batch id or a file containing one."""
    return open(s, encoding="utf8").read().strip() if os.path.exists(s) else s


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("submit")
    s.add_argument("spreads")
    s.add_argument("--index", required=True)
    s.add_argument("--variants", type=int, default=8)
    s.add_argument("--dry-run", action="store_true")
    s.add_argument("-o", "--out")
    s.set_defaults(fn=cmd_submit)

    s = sub.add_parser("status")
    s.add_argument("batch")
    s.set_defaults(fn=cmd_status)

    s = sub.add_parser("collect")
    s.add_argument("batch")
    s.add_argument("--books", help="corpus_majors.txt, for the 8-gram gate")
    s.add_argument("--min-words", type=int, default=45)
    s.add_argument("--max-words", type=int, default=55)
    s.add_argument("-o", "--out", default="corpus.jsonl")
    s.set_defaults(fn=cmd_collect)

    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
