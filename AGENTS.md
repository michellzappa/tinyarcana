# tinyarcana: agent notes

## Which board

Two SKUs, two PlatformIO envs. The round one is the product; the 1.8 was the
first target and stays buildable.

| Env | Board | Panel | Touch | PWR key |
| --- | --- | --- | --- | --- |
| `amoled-175-round` (default) | Waveshare ESP32-S3-Touch-AMOLED-1.75 | 466x466 CO5300, col offset 6 | CST9217 @ 0x5A, both axes mirrored | AXP2101 PEKEY IRQ |
| `amoled-18` | Waveshare ESP32-S3-Touch-AMOLED-1.8 | 368x448 SH8601 portrait | FT3168 @ 0x38 | TCA9554 EXIO4 |

Read the MAC before every flash. Every Waveshare AMOLED on this desk looks
identical over USB and takes a different pin map:

| Device | MAC | Env | Factory backup |
| --- | --- | --- | --- |
| AMOLED 1.75 round | `80:45:6b:33:d0:60` | `amoled-175-round` | `waveshare-175-round-...-factory.bin` |
| AMOLED 1.8 | `1c:db:d4:7a:08:8c` | `amoled-18` | `waveshare-18-...-factory.bin` |

Other identical boards on this desk belong to other projects and must not be
flashed from here. `AGENTS.local.md` holds that inventory; it is not in the
repo. If the MAC on the cable is not one of the two above, stop.

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

1.8: from the sibling projects too. PWR is read through the
TCA9554 input register (EXIO4); the AXP2101 still powers off on a ~6 s hold.

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

`data/cards/*.565` (gitignored) and `src/fonts/*.h` come from one script:

```sh
python3 -m venv .venv && .venv/bin/pip install pillow && .venv/bin/python scripts/build_assets.py
```

Cards are the 22 RWS majors in `assets/cards`, resampled per env to the two
sizes in `CARD_SIZES` (spread and full-screen) as `data/<env>/cards/NN_WxH.565`.
`scripts/data_dir.py` points PlatformIO's data dir at `data/<env>`. Sizes
must match `src/cards.h`; change both, re-run the script and `uploadfs`. The
fonts are Lora (SIL OFL) from `~/Library/Fonts`, rasterised to 8-bit alpha;
the firmware blends them itself (`src/text.cpp`). Arduino_GFX's own text is
not used anywhere.

The LittleFS image only needs re-uploading when the cards change. Firmware
iterations are `pio run -t upload` alone.

## Partition table

`partitions.csv`: 4 MB app (single ota_0, huge_app style), 8 MB LittleFS at
0x410000 labelled `spiffs` because that is the label Arduino's LittleFS mounts
by default. The board has 16 MB; the rest is unused.

## Randomness

`src/entropy.cpp` calls `bootloader_random_enable()` once at boot and leaves
it on. The header says to disable it before the ADC, the radio, or I2S on
the original ESP32 only; this is an S3 and the I2S audio path is unaffected.
If the ADC or Wi-Fi are ever added, disable it first.

## Audio

`src/audio.cpp` renders an additive synth on core 0 into the ES8311 at
22050 Hz, 16-bit stereo. The codec is clocked from BCLK
(`mclk_from_mclk_pin = false`), so **no MCLK pin is driven**. That is
deliberate: MCLK is GPIO42 on the 1.75 and GPIO16 on the 1.75C, and this
firmware does not know which unit it is on. BCLK 9, WS 45, DOUT 8 and
PA_EN 46 are the same on both and on the 1.8.
The driver's coefficient table needs `rate * 32` to be a listed clock;
22050 (705600 Hz) is. Changing the sample rate means checking that table.

## Text and layout

Position meanings in `src/tarot_data.h` must fit five lines of `lora_body`
at 340 px on the 1.8 (about 160 characters); the round detail column is
narrower but eleven lines tall. Text on the round face is wrapped per line
against the chord (`widthAt()` in `ui.cpp`), never a fixed width. The inner reading is paged by
`txtLayout()`; a paragraph starting with `#` is a heading, `>` italic, an
empty line a gap. `tarotCompose()` writes into a 2600-byte buffer; if the
engine grows, grow `innerText` in `main.cpp` and `INNER_MAX_LINES` in `ui.cpp`.

## Frame rate

Every loop paints a full frame into PSRAM and pushes it over QSPI.
That is roughly 12 to 15 frames per second. Animations are timed in
milliseconds, not frames, so they stay the right length regardless.

## Backing up flash over this link

`read-flash` with `-b 921600` stalled at 1.6% on this board's USB-JTAG-serial
port ("Serial data stream stopped"). The default rate read all 16 MB in about
100 s. Do not pass a baud rate.

## Reading the engine on the host

`src/tarot_engine.cpp` needs no Arduino runtime. `tarot_engine.h` includes
`<Arduino.h>` for the integer types only, so `tools/host_shim/Arduino.h`
(stdint plus stddef) is enough to compile the engine on macOS or Linux.
`tools/dump_readings.cpp` uses that to print every reading the device can
compose, as JSON Lines:

```sh
c++ -std=c++17 -O2 -I src -I tools/host_shim tools/dump_readings.cpp src/tarot_engine.cpp -o /tmp/dump_readings
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

The 22 majors at both render sizes cost 4.77 MB. All 78 cards at both sizes
cost 16.90 MB, and the board has 16 MB of flash, so a full deck cannot ship
as-is. If minors are ever added they get the spread size only (majors both
plus minors spread-only is 7.47 MB). MZ chose majors on 2026-09-02.

## Reading length is a word budget, checked as lines

`tools/fit_check.py` mirrors `txtLayout()` in `src/text.cpp` exactly, including
the 8 px paragraph gap that costs no line, the heading rule, and the per-line
width from the chord. It reads the advances out of `src/fonts/*.h`, so it stays
correct when the fonts are rebuilt.

```sh
python3 tools/fit_check.py --max-pages 2 reading.txt
python3 tools/fit_check.py --board 18 --jsonl --field text --max-pages 2 readings.jsonl
```

Measured 2026-09-02 on the round board: a page of the inner reading holds
**12 lines**, 31 to 39 characters each depending on where the line sits on the
chord. The 1.8 holds 13 lines at a constant 316 px.

Every reading the engine composes today is **3 pages** (9238 of 9240; two are
4). Written prose in the same register reaches 2 pages at the same word count,
because the engine spends most of a page on its four headings.

Write to a **word budget, not a line budget**. A generator cannot count rendered
lines, and the two boards wrap the same text differently. Measured against
prose in the reading register:

| words | fits 2 pages, round | fits 2 pages, 1.8 |
| --- | --- | --- |
| 120 | 100% | 100% |
| 130 | 91% | 100% |
| 140 | 47% | 95% |
| 150 | 7% | 64% |

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

| words | one page, round | one page, 1.8 |
| --- | --- | --- |
| 40 | 100% | 100% |
| 50 | 100% | 100% |
| 60 | 96% | 98% |
| 70 | 50% | 64% |

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
2. One page on **both** boards, through `fit_check.py`.
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
