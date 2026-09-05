# tinyarcana

A tarot reader for the [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75)
round display (466x466). It ships with the twenty-two Major Arcana, a past /
present / future spread, and an inner reading that composes what the three
cards say to each other. Offline. No account, no API, no cloud.

## The reading

1. **Shuffle.** Hold the deck. It riffles under your thumb, and a ring of
   twenty-one segments fills at random around the rim, one per numbered
   trump. The chip's hardware random number generator (thermal and RF noise,
   fed through the SAR ADC entropy source) is stirred with your touch: where
   your finger sits, how long it stays, how it moves. The same entropy picks
   the order the ring fills in, so no two shuffles look alike either.
2. **Deal.** Release. The cut happens on release and three cards slide out,
   face down: Past, Present, Future. There is no separate cut animation; the
   deck has been shuffling in your hand the whole time.
3. **Turn.** Tap a card to flip it.
4. **Read.** Tap a turned card: it fills the screen. Tap it away for its
   meaning in that position, under the deck's attribution when it supplies
   one. The glyphs are drawn from stroke tables (`src/glyphs.cpp`), so no
   symbol font is embedded.
5. **Go inside.** PWR opens the inner reading.

The screens say very little. The one instruction on the deck disappears as
soon as you turn a card, and the reading pages carry none at all: a page of
prose on a round display has no room to spare, and the help screen in the menu
documents every control. The boot screen counts the twenty-one numbered
trumps around the rim and names the device; the Fool is unnumbered, so he is
the dot at the centre rather than a mark on the wheel.

From the deck screen, PWR opens the menu. Settings persist across boots and
currently cover the active deck, brightness, whether the quintessence shows
beneath the spread, and whether a draw is three cards or one.

A single card is a different ritual, not a shorter one. One touch on the deck
picks it, there is no hold and no charge ring, the card arrives face down at
full size and turns itself over, and a tap past its meaning sends it back to
the deck. It has no spread screen and no inner reading: that composer is built
from an arc across positions, the elements between neighbours, curated pairs
and a digit-reduced sum, and one card has none of them. Touch to a face-up
card is about 860 ms against roughly 3.5 seconds for the three-card draw.

Rider-Waite-Smith is the first and default deck. GPTarot and Marseille are
also available as 22-card Major-only options; each carries its own card
meanings, numbering, relationship prose, and reading voice. A deck needs
artwork to earn its place: a semantics-only deck shows procedural placeholders
and costs firmware for nothing.

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
| Threads | Twenty-two curated pairs supplied by the selected deck, ordered where order matters (the Tower before the Star is not the Star before the Tower) |
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

That table is the three-card draw. With a single card the deck takes one touch
rather than a hold, there is no spread screen, the card turns itself, and PWR
does nothing except open the menu from the deck. The help screen in the menu
describes whichever draw is switched on.

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

If upload fails with the port busy, another process on the machine has the
serial port open. `lsof /dev/cu.usbmodem*` names it.

## Layout

```
src/
  main.cpp          screen state machine
  deck.*            deck registry and stable deck/card access
  settings.*        persistent NVS settings
  ui.*              every screen, all painted into one PSRAM frame
  tarot_data.h      RWS card meanings and shared data types
  deck_content.*    GPTarot and Marseille meanings and pair prose
  tarot_engine.*    inner reading composer
  cards.*           LittleFS card/back bitmaps, rounded blits, procedural fallback
  text.*            anti-aliased Lora text, wrapping, paging
  entropy.*         hardware RNG + touch stirring
  glyphs.*          Golden Dawn symbols drawn from stroke tables
  board_display.*   CO5300 round panel and AXP2101
  board_input.*     BOOT, PWR and CST9217 touch
  fonts/            generated glyph tables
assets/cards/             Rider-Waite-Smith majors (public domain)
assets/decks/             additional Major-only deck assets and instructions
assets/fonts/             Lora (SIL OFL)
scripts/build_assets.py   cards + fonts
scripts/preview_read.py   meaning-page layout preview (mirrors ui.cpp)
tools/                    host tools for the on-device reading model
docs/reading-model.md     handover for the model work: decisions, traps, next steps
data/<env>/               generated LittleFS payload (gitignored)
```

### Tarot artwork and fair use

The included Rider-Waite-Smith artwork is identified as public domain in
[LICENSE](LICENSE). The bundled Marseille pack is the CC0 Jean Dodal-attributed
Major Arcana pack credited there. Any additional deck artwork
must be reviewed separately: a particular scan, edition, restoration, or
digital redraw may still be protected. Where an additional deck is included
for private prototyping, criticism, scholarship, or comparison, its use is
intended to rely only on a fair-use/fair-dealing rationale where that doctrine
applies. Fair use is jurisdiction-specific and is not blanket permission to
redistribute commercial deck artwork. Confirm the applicable rights and add the appropriate credit or permission
before distributing a build containing additional artwork.

## Licence

MIT. Third-party material and its licences are listed at the end of
[LICENSE](LICENSE).

Board bring-up is copied from the author's sibling projects
[esp32-lofiair](https://github.com/michellzappa/esp32-lofiair),
[headroom](https://github.com/michellzappa/headroom) and esp32-rhythm, which
in turn follow Waveshare's examples for these boards.
