# Bundled UI fonts

These unmodified static TrueType fonts are bundled for consistent offline rendering of the
Hermes-inspired BLCAD interface. They are open-source alternatives chosen for the design; they
are not font files copied from the Hermes website.

| Files | UI role | License |
| --- | --- | --- |
| `Barlow-Regular.ttf`, `Barlow-Medium.ttf` | Readable controls, labels and model tree | `Barlow-OFL.txt` |
| `BarlowCondensed-Medium.ttf` | Tall condensed workspace and panel headings | `BarlowCondensed-OFL.txt` |
| `IBMPlexMono-Regular.ttf` | Numeric fields, dimensions and technical metadata | `IBMPlexMono-OFL.txt` |

All fonts use the SIL Open Font License 1.1. The corresponding copyright notices and complete
licenses are retained verbatim in this directory and must accompany redistribution of the
fonts. IBM reserves the font name "Plex"; the supplied files have not been modified or renamed
internally.

Source: the official [Google Fonts repository](https://github.com/google/fonts), pinned to
commit [`e7db6ca603fd4862010383ff17ffd6cc4ffac1c0`](https://github.com/google/fonts/tree/e7db6ca603fd4862010383ff17ffd6cc4ffac1c0),
retrieved on 2026-10-26. [`source-manifest.json`](source-manifest.json) lists every exact upstream
file URL and SHA-256 checksum for reproducibility. The font families originate from the
[Barlow project](https://github.com/jpt/barlow) and [IBM Plex](https://github.com/IBM/plex).

Application registration should use `QFontDatabase::addApplicationFont` and the returned family
names. Typical family names are `Barlow`, `Barlow Condensed`, and `IBM Plex Mono`; the static
medium files may also expose a `Medium` family alias. No system font installation or network
request is needed at runtime.
