# Fonts and font-derived data — origin and license

All font material here is third-party. This file records **what each source declares** (name
table of the font: copyright, license, license URL), so nothing is guessed.

## Files in this folder

| file | family | copyright | license | redistributable? |
|---|---|---|---|---|
| `font.ttf` | **Cascadia Code** Regular 2407.024 | © 2021 Microsoft Corporation | **SIL OFL 1.1** — [`licenses/OFL-1.1-CascadiaCode.txt`](licenses/OFL-1.1-CascadiaCode.txt) | **yes** |
| `Sixtyfour.ttf` | **Sixtyfour** (variable: `SCAN` / `BLED` axes) | © 2021 The Sixtyfour Project Authors ([`jenskutilek/homecomputer-fonts`](https://github.com/jenskutilek/homecomputer-fonts), also on Google Fonts) | **SIL OFL 1.1** — [`licenses/OFL-1.1-Sixtyfour.txt`](licenses/OFL-1.1-Sixtyfour.txt) | **yes** |

Font-derived data (a bitmap is a derivative of the font, so it keeps the font's license):

| where | what | source | license |
|---|---|---|---|
| `GMBasic/native/gmb_runtime_c/src/gmb_gfx_cellfont_bitmap.h` | ASCII 32–126, 8×16, 1 bit/px | **Terminus** 8×16 (`ter-u16n.bdf`), © 2020 Dimitar Toshkov Zhekov | **SIL OFL 1.1** — [`licenses/OFL-1.1-Terminus.txt`](licenses/OFL-1.1-Terminus.txt) |

Regenerate the bitmap with (from the repository root):

```sh
bash tools/sh/gmb_c99native_run.sh ../tools/basic/cellfont_from_bdf.bas cfb -- \
    ter-u16n.bdf GMBasic/native/gmb_runtime_c/src/gmb_gfx_cellfont_bitmap.h
```

## What was removed and why

| file | family | why it could not stay |
|---|---|---|
| `SFMono-Semibold.otf` | **SF Mono** (Apple) | the license embedded in the file allows use *"solely in conjunction with Apple-branded applications"* and **forbids embedding** it in other software; Apple's developer license (EA1370) is narrower still — **mock-ups only** — and forbids derivative works |
| `C64_Pro_Mono-STYLE.otf` | **C64 Pro Mono** (style64.org) | the style64 license forbids selling the font, including it in a collection, or offering it for download — so a public repository cannot ship it |

Replacements:

- `sfmono` / `sf mono` (font alias in the runtime and in the graphics fallback chain) →
  **Sixtyfour**;
- C64 console mode → **Sixtyfour** (its identity: *fonts based on the Commodore 64*). The PETSCII
  remap that existed to reach C64 Pro Mono's glyph layout is now an identity function, because
  the console uses a Unicode font;
- the **8×16 ASCII bitmap** inside `gmb_gfx_cellfont.c` (previously generated from C64 Pro Mono)
  → regenerated from **Terminus 8×16**, which has exactly the 8×16 matrix.

**Box-drawing does not depend on any font**: `gmb_gfx_cellfont.c` draws the frames
procedurally from the cell geometry (`cf_box_mask` / `cf_box`, `0x2500`–`0x257F`), so lines
connect with any font at any size.

## How to check a font yourself

The license lives in the `name` table of the font (IDs 0 copyright, 13 license, 14 license URL):

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

Bitmap fonts (BDF) carry the same information as header lines: `COPYRIGHT` and `NOTICE`.

Nothing else in the repository bundles fonts: the copies under
`shared/samples/**/work/Resources/` are **build artifacts** (not tracked).
