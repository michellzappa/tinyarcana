# esp32-tarot: agent notes

## Which board

Two SKUs, two PlatformIO envs. The round one is the product; the 1.8 was the
first target and stays buildable.

| Env | Board | Panel | Touch | PWR key |
| --- | --- | --- | --- | --- |
| `amoled-175-round` (default) | Waveshare ESP32-S3-Touch-AMOLED-1.75 | 466x466 CO5300, col offset 6 | CST9217 @ 0x5A, both axes mirrored | AXP2101 PEKEY IRQ |
| `amoled-18` | Waveshare ESP32-S3-Touch-AMOLED-1.8 | 368x448 SH8601 portrait | FT3168 @ 0x38 | TCA9554 EXIO4 |

Read the MAC before every flash. Every Waveshare AMOLED on this desk looks
identical over USB and takes a different pin map:

| Device | MAC | Repo / env | Factory backup in `~/Dev/esp32-backups` |
| --- | --- | --- | --- |
| AMOLED 1.75 round (tarot) | `80:45:6b:33:d0:60` | esp32-tarot `amoled-175-round` | `waveshare-175-round-80456b33d060-factory-20260902.bin` |
| AMOLED 1.8 (tarot, first target) | `1c:db:d4:7a:08:8c` | esp32-tarot `amoled-18` | `waveshare-18-1cdbd47a088c-factory-20260902.bin` |
| AMOLED 1.8 (Headroom desk board) | `1c:db:d4:7b:83:00` | headroom, **do not flash** | |
| AMOLED 1.75 round (Headroom) | `a4:cb:8f:d6:55:4c` | headroom | |
| AMOLED 1.75 round (LoFi Air) | `28:84:85:3b:77:e4` | esp32-lofiair | |

```sh
~/.platformio/penv/bin/python ~/.platformio/packages/tool-esptoolpy/esptool.py -p /dev/cu.usbmodem101 read-mac
```

## Pins and bring-up are copied, not derived

Round: `pin_config.h`, panel init, PMU IRQ setup and the CST9217 protocol
are from `esp32-lofiair` (`LOFIAIR_AMOLED_175C` profile) and
`esp32-thinking-orbs`. Two things there are easy to lose:

- Wire runs at **100 kHz** after touch attach. At 400 kHz the 15-byte point
  packets came back with a corrupt ACK on LoFi Air's unit.
- Touch is **interrupt-gated** exactly as LoFi Air ships it: a packet is
  read only after the falling edge on TP_INT, and a finger counts as lifted
  after 120 ms without a new point. Polling every frame (Thinking Orbs
  style) was tried first and made single taps need two: the poll consumed
  and ACKed the packet before the next frame looked. Do not go back to it.
- Both touch axes are mirrored (Waveshare's example: `setMirrorXY(true, true)`).
- No AXP2101 rail is written on the round board; the vendor demos never do.

1.8: from headroom/firmware and esp32-rhythm. PWR is read through the
TCA9554 input register (EXIO4); the AXP2101 still powers off on a ~6 s hold.

## The serial port has one owner

Headroom's host runs from a KeepAlive LaunchAgent with `HEADROOM_ENABLE_USB=1`
and opens every `/dev/cu.usbmodem*`. Flashing while it holds the port can
leave the board half-written. Stop it, flash, put it back:

```sh
launchctl bootout gui/$(id -u)/com.centaur-labs.headroom
```

```sh
pio run -e amoled-175-round -t uploadfs && pio run -e amoled-175-round -t upload
```

```sh
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.centaur-labs.headroom.plist
```

`lsof /dev/cu.usbmodem*` tells you who has it. Do not kill the holder if it is
not Headroom; ask.

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
PA_EN 46 are the same on both and on the 1.8 (esp32-lofiair pin_config.h).
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
