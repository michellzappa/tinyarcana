# Additional deck assets

The firmware currently ships RWS from `assets/cards/`. To add another
Major-only deck, create a directory named for its registry ID here and add
exactly these files:

```text
00.webp ... 21.webp
```

`scripts/build_assets.py` discovers these directories after RWS and creates
one `168x295` RGB565 bitmap per card under the round board's LittleFS payload.
The corresponding deck metadata, meanings, and registry entry still belong in
`src/` so the artwork cannot be selected without its matching semantics.

Only add artwork whose redistribution terms are clear and include its credit
in `LICENSE`.

The bundled `gptarot` pack uses the first (`a`) illustration for each Major
from the GPTarot.ai image set. GPTarot artwork is credited in `LICENSE` and is
not covered by this repository's MIT license.
