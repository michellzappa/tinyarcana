# The on-device reading model: handover

The device writes its inner reading with `src/tarot_engine.cpp`, which
assembles sentences from fixed strings. The plan is to replace that prose with
a small language model running on the ESP32-S3 itself, keeping the engine as
the layer that decides what the reading *says*.

**Nothing is trained. No corpus exists. No money has been spent.** The tools
are built and their offline halves are tested. What follows is what was
decided, what changed under it, what will bite you, and what is left.

## What the model is for, and what it is not for

The engine already produces 9240 distinct readings with no latency and perfect
reliability. A model does not make readings more correct; the engine's logic
stays the source of truth either way. It buys two things:

- readings that read as written prose rather than assembled clauses
- the same spread reading differently on different nights

That is a judgement about what the object should feel like. If those two are
not worth the work, the honest answer is to keep the engine and stop here.

## Decided, do not relitigate

| | |
| --- | --- |
| Deck | Majors only. 21 numbered trumps plus the Fool. |
| Reading length | 45 to 55 words, **one paragraph**, one page |
| Division of labour | The model writes prose. The engine picks the facts and keeps the closing question. |
| Architecture | Barista-class: about 8.9M parameters, INT4, Per-Layer Embeddings with the big table memory-mapped from flash |
| Reference | `slvDev/esp32-ai` (MIT). Its `esp32-ai-barista` variant is the closest analogue: 8.9M params, 4.6 MB, asymmetric vocabulary. |
| Corpus shape | 839 stratified spreads x 8 variants |
| Books | Grounding only. Never training text. |

Two paragraphs cost a whole page on the round face: at 50 words, two
paragraphs fit one page only 94% of the time against 100% for one. Write one.

## What changed since the plan was made

The plan was sized against a flash layout that no longer exists. Check all of
this before quoting any of the old numbers.

- **Flash is fully allocated.** `partitions.csv` is now app0 4 MB, spiffs
  11.88 MB, and there is no gap. The 3.88 MB hole the model was originally
  sized into is gone.
- **The filesystem is full.** Read off the board on 2026-09-04:
  `littlefs: 11764/12160 KB`, which is **396 KB free, 96.7% used**. Four deck
  packs at about 2.08 MiB each have taken it. Firmware is 719 KB of the 4 MB
  app partition.
- So there is essentially one place a model can come from: **app0**. Shrinking
  it to 1 MB frees 3 MB against a 719 KB firmware. A 4.6 MB model needs
  another 1.6 MB on top of that, and the only source is a deck. **Fitting the
  model means shipping one fewer deck, or making the model smaller.** That is
  a product decision, not a build one, and it should be made before any
  training starts rather than discovered at flashing time.
- Check the free figure yourself before planning around it; it moves every
  time a deck is added. The boot log prints it on every reset.
- **The PLE table must be memory-mapped, so it needs its own raw partition.**
  A LittleFS file cannot be mmap'd. This is a `partitions.csv` change plus a
  full `uploadfs`, not a file you can drop in.
- **Audio is gone.** The earlier worry about the model competing with the
  synth on core 0 is moot; both cores are free.
- **The screen runs at 4 to 7 frames per second**, not the 12 to 15 the docs
  used to claim. This matters for streaming generated text: see AGENTS.md.

## The traps

**Training on the engine's output teaches a model to be the engine.** All 9240
readings together use 479 word types and no word appears only once, because
the engine assembles from fixed strings. Fit a model to that and you get a
slower, larger, less reliable copy of 658 KB of deterministic C. The engine
dump is a scaffold for a corpus, not the corpus.

**A model this small memorises.** If book text reaches the training set, the
device will eventually print sentences from a copyrighted book verbatim.
Train only on synthesised readings and gate them: `tools/synthesize.py collect`
rejects any reading sharing an 8-gram with the source text. That gate compares
against the *extracted* text, which is what the model was shown, so it catches
lazy copying but not a passage the model silently repaired while copying.

**The library is a keyword sweep, not a tarot shelf.** `~/Dev/Tarot` matched on
"Magician", "Oracle", "Fool", "Major", so it contains Jungian men's work,
Chaldean Oracles scholarship and shamanism ethnography. `tools/build_corpus.py`
holds the keep list and both OCR traps in comments.

**Compress the books into the engine's register during synthesis.** If the
synthesis writes like Pollack, the vocabulary explodes, the output head grows,
and the speed advantage goes with it.

## Remaining work, in order

1. **Regenerate the artefacts.** They were built in a scratchpad under
   `/private/tmp` and are gone. Everything needed is in `tools/`; AGENTS.md
   has the command lines. Order: `dump_readings` -> `build_corpus extract,
   score, select` -> `clean_corpus clean` -> `build_corpus index` ->
   `sample_spreads`.
2. **Run the 20-spread comparison.** Generate the same 20 spreads with a local
   27B on the spare 24 GB M4 and with `claude-opus-5`, run both through the
   same gates, and read them side by side. The API side costs cents. This
   decides whether the whole corpus can be written offline, and gives a real
   tokens-per-second number instead of an estimate.
3. **Write the corpus.** `tools/synthesize.py`, `--dry-run` first. About $25
   through the Batch API, or an overnight run locally.
4. **Gate and measure.** Keep rate, reject reasons, and the vocabulary over
   the whole corpus. Budget was about 1500 output classes; confirm it.
5. **Train.** 8.9M parameters is small enough for the M4 with MLX. This does
   not need a cloud GPU.
6. **Quantise to INT4** and verify the C runtime matches the trained model on
   the host before it goes near the board.
7. **Repartition and integrate.** See the flash note above. Decide what gives
   way to the model partition before this point, not at it.
8. **Stream it.** 50 words is about 70 tokens, roughly seven seconds at
   10 tok/s. Stream word by word into the single page so the wait is the
   ritual rather than a pause. Do not paginate: at one page there is nothing
   to page.

## Open questions

- **What gives way to the model.** A 4.6 MB model does not fit alongside four
  deck packs. Either one deck goes, or the model is built smaller than the
  barista reference. Smaller is worth costing before it is ruled out: the
  output vocabulary here is around 1500 classes against barista's 854 but the
  reading is 50 words, so the sizing is not a straight copy.
- Local or API for the corpus. Step 2 answers it.
- Whether the spare M4 is base or Pro. Roughly 2x throughput between them.
- Whether to keep Lotterhand, 103K words with its leading capitals gone.

## Where things are

| | |
| --- | --- |
| Tools | `tools/`, all runnable offline, all documented in AGENTS.md |
| Books | `~/Dev/Tarot`, 396 files. Copyrighted. `.gitignore` blocks every derived file. |
| Reference implementation | `github.com/slvDev/esp32-ai`, MIT |
| Board inventory and backups | `AGENTS.local.md`, not in this repo |
