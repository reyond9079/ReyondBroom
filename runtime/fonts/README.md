# UI font subset

`ReyondBroom-CJK.otf` is a renamed subset of Noto Sans CJK JP Regular, under the
SIL Open Font License 1.1 (see LICENSE-Noto.txt). The subset contains the
characters used by the shipped translation catalog.

Source revision: `f8d157532fbfaeda587e826d4cd5b21a49186f7c`

Source file: https://github.com/notofonts/noto-cjk/blob/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf

SHA256: `68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5`

Regenerate after changing `localization/ui.tsv` with FontTools 4.60.1 and
`python scripts/generate-localization.py --font /path/to/NotoSansCJKjp-Regular.otf`.
The Windows UI continues using Segoe UI for Latin and Cyrillic text.

Linux uses the bundled subset for its base UI font; Latin and Cyrillic ranges
are included independently of the current translation catalog.

## Viewport labels

Source Sans Pro Regular uses the SIL Open Font License 1.1 (see
LICENSE-SourceSansPro.txt). This is the same font family used for viewport
labels in TrenchBroom. The original font is included unchanged.

`ReyondLabels.kfont` and `ReyondLabels.tga` are a proportional atlas generated
from that font for FTE's native bitmap-font loader. Missing glyphs, including
Cyrillic, use the already bundled OFL-licensed Noto subset. They include Latin, Cyrillic and common punctuation. The editor needs no system font installation
or additional FreeType DLL. Rebuild with `python scripts/bake-label-font.py`
(Pillow and FontTools required for generation only).
