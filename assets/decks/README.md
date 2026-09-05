# Additional deck assets

The firmware currently ships RWS from `assets/cards/`. To add another
artwork-backed Major-only deck, create a directory named for its registry ID here and add
exactly these files:

```text
00.webp ... 21.webp
```

For a deck with custom face-down artwork, also add `back.webp`. Register its
basename in `DeckDefinition::backAsset`; the asset builder writes it as
`back_168x295.565`. If no back is registered, the firmware uses its procedural
back.

`scripts/build_assets.py` discovers these directories after RWS and creates
one `168x295` RGB565 bitmap per card under the round board's LittleFS payload.
The corresponding deck metadata, meanings, and registry entry still belong in
`src/` so the artwork cannot be selected without its matching semantics.

Only add artwork whose redistribution terms are clear and include its credit
in `LICENSE`.

The bundled `gptarot` pack uses the first (`a`) illustration for each Major
from the GPTarot.ai image set and its `Reverse.webp` artwork as `back.webp`.
GPTarot artwork is credited in `LICENSE` and is not covered by this
repository's MIT license.

The bundled `marseille` pack is the Marseille 1736 Major Arcana set supplied
from the project's ODX Notion deck collection. Its redistribution status has
not been independently verified; it is not covered by this repository's MIT
license.

A deck may set `DeckDefinition::assetDir` to `nullptr` while its meanings are
being developed. The firmware then uses its face fallback and procedural back;
this is how the Thoth semantic pack is installed until a licensed image set is
available.
