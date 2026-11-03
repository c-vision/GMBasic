# GlyphSmith — bitmap font workshop (BASIC)

**GlyphSmith** is a headless editor for **bitmap fonts**: it reads a `.bdf` font, exposes every
glyph as an edit cell grid, applies editing operations (line, rectangle, flip, shift, clone),
writes the font **back to BDF** and exports it to formats that other tools can open: **BMP**,
**PSF2**, **PBM** and a **C header** with the raw bitmaps. It also **imports TrueType/OpenType
outlines into BDF**, delegating the rasterisation to the C runtime (namespace `FONTS`).

Written in BASIC and verified on the four backends of the monorepo (IL, PCodeVM, VM-C,
C99Native). No third-party code, no upstream project: it is an original app.

The app exists because the language now has **byte-exact writing** (`IO.WRITEBYTES`): until
today the only text write (`IO.FWRITE`) stopped at the first `0x00`, so binary formats such as
BMP and PSF2 were impossible to emit from BASIC.

## Formats

| Format | Direction | Notes |
|---|---|---|
| **TTF / OTF** | read (import) | outline → BDF via `FONTS.TTMETRICS` / `FONTS.TTRENDER`: the app only orchestrates the codepoint loop, the threshold and the metric conversion; the parser and the rasteriser are C (`native/gmb_runtime_c/src/gmb_fonts.c`, stb_truetype) |
| **BDF 2.1** | read + write | subset: `FONT`, `SIZE`, `FONTBOUNDINGBOX`, `STARTPROPERTIES`/`ENDPROPERTIES`, `CHARS`, and per glyph `STARTCHAR`, `ENCODING`, `SWIDTH`, `DWIDTH`, `BBX`, `BITMAP`, `ENDCHAR`. The standard 2.2 properties (`RESOLUTION_X/Y`, `FONT_ASCENT`/`FONT_DESCENT`, `DEFAULT_CHAR`) are added at load time when the source does not declare them |
| **BMP** (24-bit) | read + write | contact sheet of every glyph, scaled by `bmp_scale`, `cells_per_row` per row; opens in Preview/Paint. The `check` mode reads the sheet back with `IO.READBYTES` and verifies header, row size and the black pixels against the model. The same reader is the `bmpin` importer: a 24-bit BI_RGB bitmap becomes glyphs cell by cell (see below) |
| **PSF2** | write | Linux console font: magic `0x72b54a86`, header 32 bytes, glyphs padded to `maxW`×`maxH` |
| **PSF1** | write | legacy Linux console font: magic `0x36 0x04`, 256 slots of `charsize` bytes, 8 px per row; a glyph wider than 8 px is an explicit error |
| **ASCII art** | read + write | `mini8.ascii`: one block per glyph (`@ enc name w h` then `#`/`.` rows). The `check` re-reads it with `TextLoad` and compares every glyph with the model |
| **Sixel** | read + write | `mini8.six`: DEC stream (`ESC P q "1;1;w;h`, then per band `#0` + one character per column, `-` between bands, closed by `ESC \\`). The `check` decodes it with `SixelLoad` and compares the glyph with the model |
| **PCF** | write | X11 Portable Compiled Format: `mini8.pcf`, tabelle PROPERTIES/ACCELERATORS/METRICS/BITMAPS/BDF_ENCODINGS. File validato con **FreeType 2.13** (si apre, charmap risolto, bitmap identici al modello, riga per riga con il pitch), **byte-identico** sulle quattro leg e riletto dal `check` |
| **PBM** (P1 ASCII) | write | `P1` bitmap, one glyph per block, same layout as the BMP sheet |
| **C header** | write | `static const unsigned char glyph_<code>[]` + metrics, ready to be `#include`d |

## Model

A glyph is a **row grid of `0`/`1` characters** (`g_row`, flat array
`glyph * GS_ROWS + row`), with its BDF metrics (`ENCODING`, `SWIDTH`, `DWIDTH`,
`BBX`). The font keeps its properties verbatim, so a round-trip BDF → model → BDF preserves
name, size, bounding box and properties.

Every edit is expressed on the grid: nothing is pre-rendered, so the operations are exact and
the same on all backends.

## Modes

The app takes **no arguments** (BASIC has no `argv`): the mode comes from a compile-time symbol
or from the configuration file.

| Mode | How | What it does |
|---|---|---|
| `status` (default) | `[settings] mode = "status"` | banner, resolved paths, font summary, limits |
| `check` | `mode = "check"` or `#DEF GLYPHSMITH_MODE_CHECK` | deterministic self-check: parse, metrics, ink counts, pixel probes, every editing operation, BDF round-trip, all four exports, **BMP read-back** (header, row size, black pixels vs model) |
| `sheet` | `mode = "sheet"` or `#DEF GLYPHSMITH_MODE_SHEET` | writes `sheet.bmp`, `mini8.psf`, `sheet.pbm`, `mini8.h` into the output folder |
| `edit` | `mode = "edit"` or `#DEF GLYPHSMITH_MODE_EDIT` | applies a **batch script** of editing commands, then writes the edited BDF and the exports |
| `import` | `mode = "import"` or `#DEF GLYPHSMITH_MODE_IMPORT` | converts an outline font (`.ttf`/`.otf`) into a BDF over a codepoint range: `FONTS.TTMETRICS` for the box/metrics, `FONTS.TTRENDER` for the alpha bitmap, threshold to `0`/`1` |
| `textin` | `mode = "textin"` or `#DEF GLYPHSMITH_MODE_TEXTIN` | reads an **ASCII art** file (`@ enc name w h` + `#`/`.` rows) into the model and writes a BDF; `[text] source` / `[text] out` |
| `bmpin` | `mode = "bmpin"` or `#DEF GLYPHSMITH_MODE_BMPIN` | reads a **24-bit BMP** as a grid of cells and rebuilds the glyphs, then writes a BDF; `[bmpin] source`, `scale`, `cols`/`cellw`/`cellh`, `first`, `trim`, `out`. Each cell is sampled in `scale`×`scale` blocks (ink = at least one non-white pixel), the white margins are trimmed and every non-empty cell becomes a glyph with encoding `first`, `first+1`, … The sheet carries no per-glyph metrics, so the glyph box is the **ink box** |
| `gui` | `mode = "gui"` or `#DEF GLYPHSMITH_MODE_GUI` | **graphical window** (`SCREEN 12` + `WINDOW DEFINE … PIXEL`): the current glyph zoomed by `[gui] zoom`, a metrics panel (`enc`, `bbx`, `dwx`), `n`/`p` to change glyph, ESC/`q` to quit. It is a second view over the same model (keys go through `TuiApply`) and it closes itself after `[gui] frames`, printing `GUI-READY` / `GUI-FRAME n` / `GUI-EXIT` so the path is checkable. Drawing uses `LINE … "B"/"BF"`. **Control flow verified** on VM/C through its markers (exit on the ESC the host injects); **rendering not verified**: in this environment the graphics backend reports `video_driver=none` / `CreateWindow FAILED: No available video device`, so run it from a desktop session to look at the window |
| `tui` | `mode = "tui"` or `#DEF GLYPHSMITH_MODE_TUI` | **interactive view** over the same model: cursor, pixel toggle, flip, bearing, undo/redo. Keys come from `[tui] keys` (one key per line) so the session is deterministic and checkable; `[tui] render = "1"` prints a frame after every key, and the session writes `[tui] out` (`tui.bdf`) on exit |

Compile-time symbols win over the configuration file.

## Configuration — `.glyphsmith.toml`

Searched upwards from the current directory (up to 8 levels), like the other apps.

```toml
[settings]
mode = "check"

[source]
bdf = "fixtures/mini8.bdf"

[output]
dir = "bin"          # created if missing
bmp_scale = 4
cells_per_row = 8

[edit]
script = "edits/demo.txt"

[import]
font = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"  # relative to the app dir, or absolute
size = 16
first = 32          # codepoint range, inclusive, clamped to 0..65535
last = 126
threshold = 128     # 0..255 coverage above which a pixel becomes '1'
out = "imported.bdf"
```

## Editing script (`mode = "edit"`)

```
clear  <g>
set    <g> <x> <y> <0|1>
rect   <g> <x1> <y1> <x2> <y2> <0|1>   # filled
line   <g> <x1> <y1> <x2> <y2> <0|1>   # Bresenham
flip   <g> h|v
shift  <g> <dx> <dy>
clone  <src> <dst>
rot90  <g>                            # clockwise, width and height swap
shear  <g> <k>                        # row y moves by k*y pixels
region <g> <x1> <y1> <x2> <y2>        # copy a rectangle into the clipboard
paste  <g> <x> <y>                    # ink-only paste of the clipboard
bearing <g>                           # BBX x, DWIDTH, SWIDTH, vertical offset from the ink
insert <enc> <name> <w> <h>           # append an empty glyph
remove <g>                            # delete a glyph, later glyphs shift down
undo                                  # revert the last recorded glyph edit
redo                                  # re-apply the last undone edit
```

Glyph indices are 0-based in BDF order. `edits/demo.txt` draws a frame on glyph 2, flips it
horizontally and shifts it down by one row.

Every glyph mutation pushes a snapshot **before** acting: `undo`/`redo` walk a bounded stack of
`GS_UNDO` (4) snapshots of a single glyph. `insert`/`remove` change the glyph list itself and are
**not** undoable — the script is the source of truth, so the operation is re-run rather than
reverted. `paste` only sets ink, it does not clear the destination rectangle.

## Run

From the monorepo root:

```bash
# native leg (default)
bash tools/sh/gmb_c99native_run.sh ../shared/Apps/GlyphSmith/src/GlyphSmith.bas

# managed legs
tools/sh/harness.sh il    shared/Apps/GlyphSmith/src/GlyphSmith.bas
tools/sh/harness.sh pcode shared/Apps/GlyphSmith/src/GlyphSmith.bas
tools/sh/harness.sh vmc   shared/Apps/GlyphSmith/src/GlyphSmith.bas
```

The app resolves its folder from `PROJECTFOLDER()`, so the configuration and the fixture are
found regardless of the working directory of the runner.

## Output

`bin/` (inside the app folder) — created automatically, not versioned:

| File | Content |
|---|---|
| `roundtrip.bdf` | the model re-serialised (used by the check, byte-stable) |
| `edited.bdf` | the model after the batch script (`edit` mode) |
| `sheet.bmp` | contact sheet, 24-bit BMP |
| `sheet.pbm` | same sheet as ASCII PBM |
| `mini8.psf` | PSF2 console font |
| `mini8.psf1` | PSF1 console font (`4 + 256 * charsize` bytes) |
| `mini8.ascii` | ASCII art of every glyph, re-read by the `check` |
| `mini8.six` | sixel preview of the glyph `A`, re-read by the `check` |
| `mini8.pcf` | X11 PCF font, opened by FreeType and re-read by the `check` |
| `from_text.bdf` | BDF rebuilt from an ASCII art bitmap (`textin` mode) |
| `from_bmp.bdf` | BDF rebuilt from a BMP cell grid (`bmpin` mode) |
| `tui.bdf` | model as left by the TUI session (`tui` mode) |
| `mini8.h` | C header with the bitmaps |
| `imported.bdf` | BDF produced by the `import` mode |

## Boundaries

- **TrueType/OpenType parsing is not in BASIC and never will be**: the outline is read by the C
  runtime (namespace `FONTS`, stb_truetype in `native/gmb_runtime_c/src/gmb_fonts.c`), shared by
  all four backends. The `import` mode only orchestrates: codepoint loop, threshold, BDF metrics.
  The result is byte-identical on IL, PCodeVM, VM-C and C99Native (same C implementation).
- The import covers the **BMP** (0..0xFFFF), a single face (`.ttc` index 0) and one pixel size per
  run; hinting/antialias policy is not configurable yet (a registry option, never an environment
  variable). See `TODO.md` (M5).
- The **TUI** is a view over this same model, not a second editor: it moves a cursor, toggles
  pixels, flips, recomputes bearings and drives the same undo/redo stack. In the check its keys
  come from a script (`[tui] keys`) because the runners are headless; a real terminal is just
  another producer for the same event queue that `INKEY()` consumes (the natives feed it with
  `gmb_input_pump`), and both land in the same `TuiApply`. The **GUI** is a second view over the
  same model.
- **PCF** follows the byte conventions of a real X11 file: the header and each table's `format` field are little-endian, the fields inside the tables are big-endian, rows are padded to 4 bytes and bits are MSB-first (the leftmost pixel is the high bit, exactly as in the BDF hex rows). `CHARSET_REGISTRY`/`CHARSET_ENCODING` are added when the source BDF lacks them, otherwise FreeType builds a face with no charmap.
- Limits are declared, not silent: `GS_GMAX` glyphs, `GS_ROWS` rows per glyph, `GS_BUFMAX`
  bytes per binary export. Exceeding a limit is an explicit error.
