# Fonts in `Resources/` — origin and license

Every font here is third-party material. This file records **what the font metadata itself
declares** (name table: copyright, license, license URL), so nothing is guessed.

| file | family | copyright | license | redistributable? |
|---|---|---|---|---|
| `font.ttf` | **Cascadia Code** Regular 2407.024 | © 2021 Microsoft Corporation | SIL OFL 1.1 (embedded in the name table; full text in [`licenses/OFL-1.1-CascadiaCode.txt`](licenses/OFL-1.1-CascadiaCode.txt)) | **yes** — keep the copyright notice and the license with every copy |
| `SFMono-Semibold.otf` | **SF Mono** Semibold 13.1d0e1 | © 2016-2017 Apple Inc. | Apple's own license: use **only in conjunction with Apple-branded applications** | **no** — removal required before publishing |
| `C64_Pro_Mono-STYLE.otf` | **C64 Pro Mono** 1.21 | © 2010-2019 Style (style64.org) | style64 license: **may not** sell the font, include it in a collection, or offer it for direct download | **no** — removal required before publishing |

## How to check a font yourself

The license lives in the `name` table of the font file (IDs 0 copyright, 13 license,
14 license URL). Without extra tools:

```sh
# macOS: Quick Look / Font Book → ⌘I shows the same fields
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

- **`SFMono-Semibold.otf`** is used as the monospace/UI font of the SDL console and of the
  software graphics (`gmb_sdl_console.c`, `gmb_graphics_text.c`). Apple's license does not
  allow shipping it in a public repository: it must be **replaced** by an OFL monospace font
  (Cascadia Code is already here, and `font.ttf` is the documented fallback) and the file
  removed.
- **`C64_Pro_Mono-STYLE.otf`** powers the C64 console mode. The style64 license forbids
  redistribution, so the file cannot stay. Note that
  `GMBasic/native/gmb_runtime_c/src/gmb_gfx_cellfont.c` is a **bitmap generated from this
  font** (`8x16`, 1 bit/px): a derived work of the same licensed font, so it is affected too.
  Either the C64 mode gets a freely licensed alternative, or the author is asked for
  permission.
- Nothing else in the repository bundles fonts: the copies under
  `shared/samples/**/work/Resources/` are **build artifacts** (not tracked).
