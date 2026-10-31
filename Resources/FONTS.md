# Fonts in `Resources/` — origin and license

Every font here is third-party material. This file records **what the font metadata itself
declares** (name table: copyright, license, license URL), so nothing is guessed.

| file | family | copyright | license | redistributable? |
|---|---|---|---|---|
| `font.ttf` | **Cascadia Code** Regular 2407.024 | © 2021 Microsoft Corporation | **SIL OFL 1.1** (embedded in the name table; full text in [`licenses/OFL-1.1-CascadiaCode.txt`](licenses/OFL-1.1-CascadiaCode.txt)) | **yes** |
| `Sixtyfour.ttf` | **Sixtyfour** (variable: `SCAN`/`BLED` axes) | © 2021 The Sixtyfour Project Authors ([`jenskutilek/homecomputer-fonts`](https://github.com/jenskutilek/homecomputer-fonts), also on Google Fonts) | **SIL OFL 1.1** ([`licenses/OFL-1.1-Sixtyfour.txt`](licenses/OFL-1.1-Sixtyfour.txt)) | **yes** |
| `C64_Pro_Mono-STYLE.otf` | **C64 Pro Mono** 1.21 | © 2010-2019 Style (style64.org) | style64 license: **may not** sell the font, include it in a collection, or offer it for direct download | **no** — removal still pending |

## Why Sixtyfour replaced SF Mono

`SFMono-Semibold.otf` (**SF Mono**, © Apple) has been **removed**: the license embedded in the
file allows use *"solely in conjunction with Apple-branded applications"* and **forbids
embedding** it in other software or using it to create derivative works. Apple's developer
download license (EA1370) is even narrower — mock-ups only — so a macOS app is not a
justification either.

It is replaced by **Sixtyfour**, a variable font *based on the Commodore 64 fonts*, released
under the SIL OFL 1.1: the `sfmono` / `sf mono` font alias in the runtime and in the graphics
fallback chain now resolve to `Sixtyfour.ttf`, so existing configurations keep working.

`font.ttf` (Cascadia Code) remains the general default: it covers **128/128** box-drawing
glyphs, while Sixtyfour covers the C64-style set (heavy lines + blocks + ASCII); the console
frames do not depend on either of them, because `gmb_gfx_cellfont.c` draws box-drawing
**procedurally** from the cell geometry.

## How to check a font yourself

The license lives in the `name` table of the font file (IDs 0 copyright, 13 license,
14 license URL). Without extra tools:

```sh
python3 - <<'PY'
import struct
d = open("Resources/font.ttf","rb").read()
n = struct.unpack(">H", d[4:6])[0]
off = next(struct.unpack(">I", d[12+i*16+8:12+i*16+12])[0] for i in range(n) if d[12+i*16:12+i*16+4] == b"name")
c, s = struct.unpack(">HH", d[off+2:off+6])
for i in range(c):
    pid, eid, lid, nid, ln, so = struct.unpack(">HHHHHH", d[off+6+i*12:off+6+i*12+12])
    if nid in (0, 13, 14) and pid == 3:
        print(nid, d[off+s+so:off+s+so+ln].decode("utf-16-be").strip()[:400])
PY
```

## Consequences (still open)

- **`C64_Pro_Mono-STYLE.otf`** powers the C64 console mode and its PETSCII remap
  (`gmb_console.c`: `gmb_c64_remap`). The style64 license forbids redistribution, so the file
  cannot stay in a public repository. Note that
  `GMBasic/native/gmb_runtime_c/src/gmb_gfx_cellfont.c` contains an 8x16 ASCII bitmap
  **generated from this font**: a derived work of the same licensed font, so it is affected
  too (it must be regenerated from Sixtyfour or another freely licensed font).
- Nothing else in the repository bundles fonts: the copies under
  `shared/samples/**/work/Resources/` are **build artifacts** (not tracked).
