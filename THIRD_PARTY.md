# Third-party material

The repository's own code is under the MIT license in [LICENSE](LICENSE). The files below are not, or not only.

- src/es8311.c, src/es8311.h, src/es8311_reg.h: Espressif Systems,
  Apache License 2.0 (SPDX headers in the files).
- assets/fonts/Lora-*.ttf and the glyph tables generated from them in
  src/fonts/: Lora, Copyright 2011 The Lora Project Authors, SIL Open Font
  License 1.1 (assets/fonts/OFL.txt).
- assets/cards/*.webp: the Rider-Waite-Smith tarot (Pamela Colman Smith,
  1909), public domain.
- assets/decks/gptarot/*.webp: GPTarot artwork by Aesthetic Hermetics and
  Michell Zappa, sourced from GPTarot.ai. These images are third-party
  artwork and are not covered by this repository's MIT license.
- assets/decks/marseille/*.webp: Marseille 1736 Major Arcana set supplied from
  the project's ODX Notion deck collection. Redistribution status was not
  independently verified; these images are not covered by this MIT license.
- Thoth card names, correspondences, and original device meanings in
  src/deck_content.cpp follow the Crowley system. No Crowley-Harris card
  artwork is included.
- The CST9217 touch protocol in src/board_input.cpp follows SensorLib
  (Lewis He, MIT).
