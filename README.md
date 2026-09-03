# tinyarcana

A tarot reader for the [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75)
round display (466x466), with the 1.8" portrait board as a second target.
Twenty-two majors, a past / present / future spread, and an inner reading
that composes what the three cards say to each other. Offline. No account,
no API, no cloud.

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

Every step has a sound: a bell on boot, a low drone that rises in pitch and
brightness while your hand is on the deck, a breath when the deck cuts, a
tick per dealt card, a tock and blip per flip, a chord when all three are
up. All of it is synthesised on the chip (`src/audio.cpp`); there are no
sample files.

### Inner reading

Not a lookup. `src/tarot_engine.cpp` builds the reading from the spread:

| Layer | What it reads |
| --- | --- |
| The arc | Whether the numbers climb, fall, peak or dip across the three positions, and which rows of the Fool's Journey (outer, inner, greater world) they cross |
| Elements | The dominant element, and whether adjacent positions share, feed or oppose each other (Fire/Water, Air/Earth) |
| Threads | Twenty-two curated pairs with their own sentence, ordered where order matters (the Tower before the Star is not the Star before the Tower) |
| The hidden card | The quintessence: the three numbers summed and digit-reduced to a major, doubled in weight if it is already on the table |
| The question | The Future card's closing question |

1540 possible spreads, no two read the same.

## Controls

| Input | Deck | Spread | Card | Inner |
| --- | --- | --- | --- | --- |
| **Touch** | hold to shuffle | tap to turn / open a card | tap: card to meaning, meaning to spread; swipe between cards | tap for next page |
| **BOOT** | quick draw (no shuffle) | turn next card, then open | next card | next page |
| **BOOT + PWR** or **BOOT hold** | | close the reading | close the reading | close the reading |
| **PWR** | help | inner reading (turns remaining cards first) | inner reading | back to spread |
| **PWR hold ~6 s** | power off (hardware) | | | |

## Build and flash

Needs [PlatformIO](https://platformio.org/) and, for the asset step, Python
with Pillow.

```sh
python3 -m venv .venv && .venv/bin/pip install pillow && .venv/bin/python scripts/build_assets.py
```

```sh
pio run -e amoled-175-round -t uploadfs
```

```sh
pio run -e amoled-175-round -t upload
```

For the 1.8" board use `-e amoled-18`. Each env has its own `data/<env>`
payload because the card bitmaps are sized per screen.

If upload fails with the port busy, Headroom's LaunchAgent owns it. See
[AGENTS.md](AGENTS.md).

## Layout

```
src/
  main.cpp          screen state machine
  ui.*              every screen, all painted into one PSRAM frame
  tarot_data.h      22 cards: keywords, essence, per-position meanings, questions
  tarot_engine.*    inner reading composer
  cards.*           LittleFS card bitmaps, rounded blits, procedural back
  text.*            anti-aliased Lora text, wrapping, paging
  entropy.*         hardware RNG + touch stirring
  audio.*           additive synth over I2S into the ES8311
  es8311.*          codec driver (Espressif, Apache-2.0)
  board_display.*   CO5300 (round) or AXP2101 -> TCA9554 -> SH8601 (1.8)
  board_input.*     BOOT, PWR, CST9217 (round) or FT3168 (1.8) touch
  fonts/            generated glyph tables
assets/cards/             Rider-Waite-Smith majors (public domain)
assets/fonts/             Lora (SIL OFL)
scripts/build_assets.py   cards + fonts
data/<env>/               generated LittleFS payload (gitignored)
```

## Licence

MIT. Third-party material and its licences are listed at the end of
[LICENSE](LICENSE).

Board bring-up is copied from the author's sibling projects
[esp32-lofiair](https://github.com/michellzappa/esp32-lofiair),
[headroom](https://github.com/michellzappa/headroom) and esp32-rhythm, which
in turn follow Waveshare's examples for these boards.
