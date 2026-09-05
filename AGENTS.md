# tinyarcana: agent notes

## Which board

The round 1.75 is the only supported product target. The 1.8 was a pilot and
is no longer built or maintained.

| Env | Board | Panel | Touch | PWR key |
| --- | --- | --- | --- | --- |
| `amoled-175-round` (default) | Waveshare ESP32-S3-Touch-AMOLED-1.75 | 466x466 CO5300, col offset 6 | CST9217 @ 0x5A, both axes mirrored | AXP2101 PEKEY IRQ |

Read the MAC before every flash. Every Waveshare AMOLED on this desk looks
identical over USB and takes a different pin map:

| Device | MAC | Env | Factory backup |
| --- | --- | --- | --- |
| AMOLED 1.75 round | `28:84:85:57:38:14` | `amoled-175-round` | `waveshare-unknown-288485573814-factory-20260904.bin` |

Other identical boards on this desk belong to other projects and must not be
flashed from here. `AGENTS.local.md` holds that inventory; it is not in the
repo. If the MAC on the cable is not the one above, stop.

```sh
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py -p /dev/cu.usbmodem101 read-mac
```

## Pins and bring-up are copied, not derived

Round: `pin_config.h`, panel init, PMU IRQ setup and the CST9217 protocol
are copied from two sibling projects on this desk (see `AGENTS.local.md`) and
from Waveshare's own examples. Two things there are easy to lose:

- Wire runs at **100 kHz** after touch attach. At 400 kHz the 15-byte point
  packets came back with a corrupt ACK on a sibling board's unit.
- Touch is **interrupt-gated**: a packet is
  read only after the falling edge on TP_INT, and a finger counts as lifted
  after 120 ms without a new point. Polling every frame was tried first and made single taps need two: the poll consumed
  and ACKed the packet before the next frame looked. Do not go back to it.
- Both touch axes are mirrored (Waveshare's example: `setMirrorXY(true, true)`).
- No AXP2101 rail is written on the round board; the vendor demos never do.

## The serial port has one owner

Another KeepAlive LaunchAgent on this desk opens every `/dev/cu.usbmodem*`.
Flashing while it holds the port can leave the board half-written. Stop that
agent, flash, then put it back - `AGENTS.local.md` names it.

```sh
pio run -e amoled-175-round -t uploadfs && pio run -e amoled-175-round -t upload
```

`lsof /dev/cu.usbmodem*` tells you who has it. If the holder is not the agent
`AGENTS.local.md` names, do not kill it; ask.

## Assets are generated

`data/amoled-175-round/decks/<id>/*.565` (gitignored) and `src/fonts/*.h` come
from one script:

```sh
python3 -m venv .venv && .venv/bin/pip install pillow && .venv/bin/python scripts/build_assets.py
```

Cards are Major-only deck packs. RWS is sourced from `assets/cards`; future
decks use `assets/decks/<id>`. Each card is resampled once to 168x295 as
`data/<env>/decks/<id>/NN_168x295.565` and sampled for both display sizes.
The size must match `src/cards.h`; change it, re-run the script and
`uploadfs`. The
fonts are Lora (SIL OFL) from `~/Library/Fonts`, rasterised to 8-bit alpha;
the firmware blends them itself (`src/text.cpp`). Arduino_GFX's own text is
not used anywhere.

The LittleFS image only needs re-uploading when the cards change. Firmware
iterations are `pio run -t upload` alone.

## Partition table

`partitions.csv`: 4 MB app (single ota_0, huge_app style), about 11.9 MiB
LittleFS at 0x410000 labelled `spiffs` because that is the label Arduino's
LittleFS mounts by default, and a 64 KiB coredump at the end of flash. Five
22-card packs at the shared 168x295 size fit within that filesystem.

## Randomness

`src/entropy.cpp` calls `bootloader_random_enable()` once at boot and leaves
it on. The header says to disable it before the ADC or radio on the original
ESP32 only.
If the ADC or Wi-Fi are ever added, disable it first.

## Text and layout

Position meanings in `src/tarot_data.h` must fit the round detail column.
Text on the round face is wrapped per line against the chord (`widthAt()` in
`ui.cpp`), never a fixed width. The inner reading is paged by
`txtLayout()`; a paragraph starting with `#` is a heading, `>` italic, an
empty line a gap. `tarotCompose()` writes into a 2600-byte buffer; if the
engine grows, grow `innerText` in `main.cpp` and `INNER_MAX_LINES` in `ui.cpp`.

**Measure text against the chord before placing it.** The round face narrows
fast towards the bottom: 282 px of usable width at baseline 404, 172 px at 444.
Hints sat at 444 in 12 px type for a long time while the longest of them
rendered 219 px, so the bezel ate the ends of every two-line hint. `hint()` now
sits at 404, in 16 px, and puts the wider of two strings on the upper line
where the chord is wider. The reading pages carry no hint at all, because the
text fills the page to `INNER_BOTTOM` and any hint line overprinted it.

`scripts/preview_read.py` mirrors the meaning-page constants. Change one and
change the other, or the preview quietly stops matching the device.

## Frame rate

Every loop paints a full frame into PSRAM and pushes it over QSPI. Measured on
the round board, 2026-09-03: the boot screen runs at **140 ms a frame**, the
deck screen at **250 ms**, of which about 190 ms is the draw itself. Roughly
7 and 4 frames per second, not the 12 to 15 a panel this size might suggest.
Measure before designing an animation around a frame rate.

Animations are timed in milliseconds, not frames, so their length is right
regardless. Their *smoothness* is not. Anything that wants more than about
seven distinct steps a second cannot have them, and per-step fades are
invisible: on the boot screen a single frame covers three ticks. Draw
animations as a function of elapsed time and let a frame show whatever has
happened since the last one. Do not advance a counter once per frame: that
guarantees the animation falls behind and then catches up in one jump.

**`fillArc` scans its bounding box.** For an arc at rim radius that box is the
whole 466x466 screen, so twenty-one of them cost most of a frame. `ringSeg()`
in `ui.cpp` draws a wedge as a fan of short radial lines instead, which touches
only its own pixels and wraps past 360 degrees on its own.

`cardDrawBack()` runs 39,600 times per card and three cards per frame on the
deck screen. Nothing expensive belongs in that loop. It no longer does an
integer divide (only needed when a card is squashed, which the deck screen
never does) or a rounded-corner test (which can only reject a pixel within the
corner radius of the top or bottom edge).

## Backing up flash over this link

`read-flash` with `-b 921600` stalled at 1.6% on this board's USB-JTAG-serial
port ("Serial data stream stopped"). The default rate read all 16 MB in about
100 s. Do not pass a baud rate.

Back up a board's factory flash before the first write to it, and read the MAC
first. `AGENTS.local.md` lists which board is which.

## The reading model

`docs/reading-model.md` is the handover for replacing the engine's prose with a
small language model on the chip. It carries the decisions, the traps and the
remaining work. The sections below cover the tools it uses.

## Reading the engine on the host

`src/tarot_engine.cpp` needs no Arduino runtime. `tarot_engine.h` includes
`<Arduino.h>` for the integer types only, so `tools/host_shim/Arduino.h`
(stdint plus stddef) is enough to compile the engine on macOS or Linux.
`tools/dump_readings.cpp` uses that to print every reading the device can
compose, as JSON Lines:

```sh
c++ -std=c++17 -O2 -I src -I tools/host_shim tools/dump_readings.cpp src/tarot_engine.cpp src/deck.cpp -o /tmp/dump_readings
```

Neither file is in `src/`, so PlatformIO never sees them and the shim cannot
shadow the real `Arduino.h`.

Numbers the dump reports, measured 2026-09-02:

- **9240 ordered spreads**, not 1540. 22 x 21 x 20. The README's 1540 counts
  unordered sets of three; Past/Present/Future order changes the reading, so
  9240 is the real output space.
- All 9240 readings are distinct. No collisions.
- Readings run 733 to 1160 bytes, mean 879. The 2600-byte `innerText` buffer
  is less than half used. `dump_readings` exits non-zero if any reading fills
  the buffer, so it doubles as a regression check when the engine grows.
- The whole output space uses **479 distinct word types** and no word appears
  only once. The engine assembles from fixed strings, so its vocabulary is
  closed by construction.

That last number is the trap for anyone adding a language model here. Training
on the engine's own output teaches a model to be the engine, which is a slower
and larger copy of deterministic C. The dump is a scaffold for a corpus, not
the corpus.

## Deck size is a flash decision

The round board stores one 168x295 bitmap per Major. One 22-card pack is about
2.08 MiB; five packs are about 10.4 MiB, leaving filesystem headroom for
metadata. Minor Arcana are out of scope.

## Reading length is a word budget, checked as lines

`tools/fit_check.py` mirrors `txtLayout()` in `src/text.cpp` exactly, including
the 8 px paragraph gap that costs no line, the heading rule, and the per-line
width from the chord. It reads the advances out of `src/fonts/*.h`, so it stays
correct when the fonts are rebuilt.

```sh
python3 tools/fit_check.py --max-pages 2 reading.txt
python3 tools/fit_check.py --board round --jsonl --field text --max-pages 2 readings.jsonl
```

Measured 2026-09-02 on the round board: a page of the inner reading holds
**12 lines**, 31 to 39 characters each depending on where the line sits on the
chord.

Every reading the engine composes today is **3 pages** (9238 of 9240; two are
4). Written prose in the same register reaches 2 pages at the same word count,
because the engine spends most of a page on its four headings.

Write to a **word budget, not a line budget**. A generator cannot count rendered
lines reliably. Measured against prose in the reading register:

| words | fits 2 pages, round |
| --- | --- |
| 120 | 100% |
| 130 | 91% |
| 140 | 47% |
| 150 | 7% |

**Budget 110 to 125 words, cap 130.** The round board binds. Use `fit_check.py`
as the gate that catches the outliers, never as the thing the writer aims at.

`txtLayout()` protects headings only: it keeps a heading with the first line of
its body. Body paragraphs straddle page breaks freely, so page one usually ends
mid-clause. That is the reason a reading feels unfinished on the first screen,
not its length.

## One page is the target

MZ chose one-page readings on 2026-09-02. A generated reading is **45 to 55
words in a single paragraph**, plus the engine's italic closing question.
Measured with `fit_check.py` against prose in the reading register:

| words | one page, round |
| --- | --- |
| 40 | 100% |
| 50 | 100% |
| 60 | 96% |
| 70 | 50% |

Those figures are for **one** paragraph. Two paragraphs cost a whole page: at
50 words two paragraphs fit only 94% of the time, at 60 words only 53%. The
8 px gap plus the extra wrapped line is that expensive. Write one paragraph.

At one page there is no page break, so nothing straddles it and the
widow/orphan problem in `txtLayout()` does not arise. It still affects the
engine's own 3-page readings.

At 50 words a reading cannot carry all of the engine's facts. It carries two or
three. Choosing which ones is a content decision, and it is where the variety
between readings comes from.

## Picking which spreads to write

`tools/sample_spreads.py` takes `readings.jsonl` and returns a stratified
subset. The engine assembles from fixed strings, so its output space is finite:
**531 distinct sentence templates**, plus 66 card-by-position cells, 19 hidden
cards and 3 doubled-hidden cases. 619 strata in total.

```sh
python3 tools/sample_spreads.py readings.jsonl --min 2 --target 839 -o sample.jsonl
```

Cover set sizes, measured 2026-09-02: `--min 1` needs 440 spreads, `--min 2`
needs 839, `--min 3` needs 1200. All of them reach 619/619.

**The quintessence can never be The Fool, The Magician or The High Priestess.**
Three distinct cards sum to at least 0+1+2=3, so the reduction can never name
0, 1 or 2. Only 19 of the 22 majors ever appear as the hidden card. The rarest
are Temperance (102 of 9240 spreads), The Devil (114) and The Tower (126).

## Writing the corpus

`tools/build_corpus.py` extracts, scores, selects and indexes the reference
books; `tools/clean_corpus.py` repairs what OCR damage can be repaired.

`tools/synthesize.py` turns the sampled spreads into training text through the
Batch API, in three steps because a batch runs asynchronously.

```sh
python3 tools/synthesize.py submit sample.jsonl --index card_index.json --dry-run
python3 tools/synthesize.py submit sample.jsonl --index card_index.json -o batch.id
python3 tools/synthesize.py status batch.id
python3 tools/synthesize.py collect batch.id --books corpus_majors.txt -o corpus.jsonl
```

It needs the `anthropic` SDK and a credential; neither is installed in this
repo. Model is `claude-opus-5`. Batch pricing is half of list.

**One request per spread returns every variant.** Sending the reference
passages once instead of once per variant is most of the saving, and asking for
all variants together is what makes them differ from each other. Eight
single-variant requests produce eight paraphrases.

`collect` runs three gates and writes the failures to
`<out>.rejects.jsonl` with a reason, so a bad prompt shows as a pattern:

1. 45 to 55 words, one paragraph.
2. One page on the round board, through `fit_check.py`.
3. No 8-gram shared with the extracted book text.

Gate 3 protects against the model reproducing its source, which a model this
small will otherwise do. It compares against the **extracted** text, which is
what the model was shown, so that is the right comparison. It cannot catch a
passage the model silently repaired while copying, because the OCR errors are
what make the 8-gram unique. Never train on book text directly.

The reference passages still carry OCR noise ("s h e gives us", "situatrons").
Clean them before a real run; the model copies register from what it reads.

## The books are OCR, and one file was the wrong one

`tools/clean_corpus.py score` reports the percent of tokens missing from
`/usr/share/dict/words`, per source. Clean prose sits near 1%. Measured
2026-09-02, after picking the right extractions, every majors source lands
between 1.2% and 5.2%.

Two things that matters for:

- **Wang's 5.2% is not damage.** It is `binah`, `tiphareth`, `kether`,
  `sephiroth` - correctly spelled Hebrew the dictionary lacks. The score finds
  unusual vocabulary as readily as corruption, so read the token list before
  dropping a source. Wang is a poor *voice* source for the opposite reason:
  that vocabulary is exactly what the reading style forbids.
- **`Living the Tarot.PDF` scores 21.2%; `Living the Tarot_OCR.pdf` scores
  4.8%.** Same book, two files in `~/Dev/Tarot`, and the plain one has a
  rotten embedded text layer. Check for a second copy before repairing
  anything - picking the right file recovered more than every repair rule
  combined.

`clean` fixes soft hyphens (504), hyphenation across a space (72),
letter-spaced runs (553) and missing spaces after a full stop (15). Two rules
need care and both are handled:

- A rejoin is applied only when the joined result is a real word.
  `contem- plate` becomes `contemplate`; `balancing- of` keeps its space.
- Run-together splitting (`--split`) is **off by default**. The system
  dictionary is web2 from 1934 and has no `artwork`, `childcare` or
  `channeling`, so the rule splits real words about as often as it fixes
  broken ones.

What no rule can fix: character misreads. Lotterhand has lost leading capitals
throughout - `arot` for Tarot (260), `abala` for Qabala (223), `hat's` for
that's (253). Drop the source or accept it; there is no repair.
