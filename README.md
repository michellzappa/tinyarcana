# tinyarcana

A tarot reader for the [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75)
round display (466x466). It ships with the twenty-two Major Arcana, a past /
present / future spread, and an inner reading that composes what the three
cards say to each other. Offline. No account, no API, no cloud.

## The reading

1. **Shuffle.** Hold the deck. The chip's hardware random number generator
   (thermal and RF noise, fed through the SAR ADC entropy source) is stirred
   with your touch: where your finger sits, how long it stays, how it moves.
2. **Cut.** Release. The deck splits and re-stacks.
3. **Deal.** Three cards slide out, face down: Past, Present, Future.
4. **Turn.** Tap a card to flip it.
5. **Read.** Tap a turned card: it fills the screen. Tap it away for its
   meaning in that position, under the card's Golden Dawn glyph: a zodiac
   sign, a planet, or an elemental triangle for the Fool, the Hanged Man and
   Judgement. The glyphs are drawn from stroke tables (`src/glyphs.cpp`), so
   no symbol font is embedded.
6. **Go inside.** PWR opens the inner reading.

From the deck screen, PWR opens the menu. Settings persist across boots and
currently cover the active deck, brightness, and the underlying
card line. Rider-Waite-Smith is the first and default deck; the firmware is
structured for additional 22-card Major-only decks.

In the menu, BOOT moves between items and PWR opens one. In Settings, BOOT
moves between rows, PWR changes the selected value, and tapping the bottom
edge returns to the menu. The selected menu or settings row has a filled
circular marker; unselected rows have hollow markers.

### Inner reading

Not a lookup. `src/tarot_engine.cpp` builds the reading from the spread:

| Layer | What it reads |
| --- | --- |
| The arc | Whether the numbers climb, fall, peak or dip across the three positions, and which rows of the Fool's Journey (outer, inner, greater world) they cross |
| Elements | The dominant element, and whether adjacent positions share, feed or oppose each other (Fire/Water, Air/Earth) |
| Threads | Twenty-two curated pairs with their own sentence, ordered where order matters (the Tower before the Star is not the Star before the Tower) |
| The hidden card | The quintessence: the three numbers summed and digit-reduced to a major, doubled in weight if it is already on the table |
| The question | The Future card's closing question |

9240 ordered spreads, no two read the same.

## Controls

| Input | Deck | Spread | Card | Inner |
| --- | --- | --- | --- | --- |
| **Touch** | hold to shuffle | tap to turn / open a card | tap: card to meaning, meaning to spread; swipe between cards | tap for next page |
| **BOOT** | quick draw (no shuffle) | turn next card, then open | next card | next page |
| **BOOT + PWR** or **BOOT hold** | | close the reading | close the reading | close the reading |
| **PWR** | menu | inner reading (turns remaining cards first) | inner reading | back to spread |
| **PWR hold ~6 s** | power off (hardware) | | | |

## Build and flash

Needs [PlatformIO](https://platformio.org/) and, for the asset step, Python
with Pillow.

```sh
python3 -m venv .venv && .venv/bin/pip install pillow && .venv/bin/python scripts/build_assets.py
```

```sh
pio run -t uploadfs
```

```sh
pio run -t upload
```

The round board stores one 168x295 RGB565 bitmap per card and samples it for
both the spread and full-card views. The shared source size avoids duplicating
every image and leaves room for about five Major-only decks in LittleFS.

If upload fails with the port busy, Headroom's LaunchAgent owns it. See
[AGENTS.md](AGENTS.md).

## Layout

```
src/
  main.cpp          screen state machine
  deck.*            deck registry and stable deck/card access
  settings.*        persistent NVS settings
  ui.*              every screen, all painted into one PSRAM frame
  tarot_data.h      22 cards: keywords, essence, per-position meanings, questions
  tarot_engine.*    inner reading composer
  cards.*           LittleFS card bitmaps, rounded blits, procedural back
  text.*            anti-aliased Lora text, wrapping, paging
  entropy.*         hardware RNG + touch stirring
  board_display.*   CO5300 round panel and AXP2101
  board_input.*     BOOT, PWR and CST9217 touch
  fonts/            generated glyph tables
assets/cards/             Rider-Waite-Smith majors (public domain)
assets/decks/             additional Major-only deck asset instructions
assets/fonts/             Lora (SIL OFL)
scripts/build_assets.py   cards + fonts
data/<env>/               generated LittleFS payload (gitignored)
```

### Tarot artwork and fair use

The included Rider-Waite-Smith artwork is identified as public domain in
[LICENSE](LICENSE). Any additional Thoth, Marseille, or other deck artwork
must be reviewed separately: a particular scan, edition, restoration, or
digital redraw may still be protected. Where an additional deck is included
for private prototyping, criticism, scholarship, or comparison, its use is
intended to rely only on a fair-use/fair-dealing rationale where that doctrine
applies. Fair use is jurisdiction-specific and is not blanket permission to
redistribute commercial deck artwork. Confirm the applicable rights and add
the appropriate credit or permission before distributing a build containing
additional decks.

## Licence

MIT. Third-party material and its licences are listed at the end of
[LICENSE](LICENSE).

Board bring-up is copied from the author's sibling projects
[esp32-lofiair](https://github.com/michellzappa/esp32-lofiair),
[headroom](https://github.com/michellzappa/headroom) and esp32-rhythm, which
in turn follow Waveshare's examples for these boards.
