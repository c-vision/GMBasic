# TODO — GlyphSmith

Status: **M1** — headless editor of BDF bitmap fonts: BDF parser (subset 2.1),
in-memory glyph model, editing operations, **BDF round-trip**, export **BMP**,
**PSF2**, **PBM** and **C header**; `check` mode with deterministic assertions.
Verified on the four backends of the monorepo (IL, PCodeVM, VM-C, C99Native).

## M2 — editing comfort (still headless)

- [x] **Undo/redo** con stack limitato (`GS_UNDO` = 4 snapshot di un singolo glifo, righe +
      metriche); ogni verbo di editing fa `HistPush` **prima** di agire. `insert`/`remove` non
      sono undoable (cambiano la lista dei glifi: si rilancia lo script).
- [x] **Copy/paste di regione** fra glifi: `region` (clipboard, `g_clipW/H`) + `paste`
      (solo inchiostro, non svuota la destinazione).
- [x] **Rotazione 90°** (`rot90`, scambio larghezza/altezza) e **shear** (`shear <g> <k>`,
      la riga y si sposta di `k*y`).
- [x] **Bearing automatico** (`bearing <g>`): `BBX` x, `DWIDTH`, `SWIDTH` e offset verticale
      ricalcolati dal bounding box dell'inchiostro.
- [x] **Inserimento/rimozione glifo** (`insert <enc> <name> <w> <h>`, `remove <g>`): la lista
      si sposta e `CHARS` segue il modello.

## M3 — formats

- [x] **BMP read-back**: il `check` rilegge `sheet.bmp` con `IO.READBYTES` e verifica
      header (offset pixel, larghezza, altezza, 24 bpp, dimensione), riga multipla di 4
      senza padding, pixel neri = `inchiostro x scale^2`, più un pixel acceso e uno spento
      del glifo `A` letti dal file. Check deterministico: 52 -> 64 controlli.
- [x] **PSF1** (256 slots, 8-pixel wide) in addition to PSF2: `mini8.psf1`,
      `4 + 256 * charsize` byte, magic `0x36 0x04`; il check verifica header, dimensione e un
      byte di glifo calcolato dal modello. Un glifo piu' largo di 8 px e' un errore dichiarato.
- [ ] **PCF** writer (binary tables with accelerators) and **BDF v2.2** properties.
- [x] **ASCII art** per terminale: `mini8.ascii` (`@ enc name w h` + righe `#`/`.`), riletto
      dal modo `textin` e confrontato glifo per glifo con il modello nel check.
- [x] **Sixel** per terminali che lo supportano: `mini8.six` (stream DEC `ESC P q "1;1;w;h`,
      bande da 6 righe, un carattere per colonna, `-` fra le bande, chiuso da `ESC \\`),
      scritto con `ASC` + `IO.WRITEBYTES` e **riletto** da `SixelLoad` nel check con confronto
      di firma. Non validato con un decoder esterno.
- [x] **Import of a plain text bitmap** (`#`/`.` grid): modo `textin`, config `[text]`
      (`source`, `out`); `TextLoad` costruisce il modello e `BdfSave` scrive il BDF.

## M4 — interactive editor

- [x] **TUI** (`mode = "tui"`): cursore `l/h/j/k`, toggle del pixel con spazio, `n/p` fra i
      glifi, `c` svuota, `x/y` flip, `b` bearing, `u/r` undo/redo; frame deterministico
      (`TuiFrame`) e tasti da `[tui] keys`, cosi' il percorso e' verificato nel check.
- [ ] Lettura dei tasti dal **terminale** (oggi arrivano dallo script `[tui] keys`): stesso
      `TuiApply`, cambia solo la sorgente.
- [ ] **GUI** window with zoom, glyph list, metrics panel (`WINDOW DEFINE`, `IMAGE*`).
- [ ] Both front-ends must keep the `check` mode byte-identical (golden artefacts in
      `bin/`), so the headless path stays the reference.

## M5 — TrueType / OpenType (RUNTIME, NOT BASIC)

The conversion of an outline font is **not** a BASIC algorithm: it belongs to the C runtime
(`native/gmb_runtime_c`, namespace `FONTS`), shared by every backend (§0bis). `stb_truetype` was
vendored but **dead code** (no `stbtt_InitFont` call: SDL_ttf did the rasterisation); it is now the
engine behind the `FONTS` functions.

- [x] `FONTS.TTRENDER(path, size, codepoint)` → byte array (alpha bitmap), via `stbtt_InitFont` /
      `stbtt_MakeCodepointBitmap` (`native/gmb_runtime_c/src/gmb_fonts.c`).
- [x] `FONTS.TTMETRICS(path, size, codepoint)` → 8 numeri: w, h, xoff, yoff, advance, ascent,
      descent, line gap (`stbtt_GetCodepointHMetrics`, `stbtt_GetCodepointBitmapBox`,
      `stbtt_GetFontVMetrics`).
- [x] `FONTS.TTGLYPHS(path)` → codepoint coperti dal cmap, crescenti (surrogati esclusi).
- [x] Parità 4 backend (IL/PCode via `libgmb_fonts`, VM/C e C99Native via le varianti `_list`) +
      MiniTest `shared/minitests/fonts/tt_functions.bas` + selftest C `tools/c/gmb_fonts_selftest.c`
      (22 invarianti su un font reale).
- [x] A BASIC **importer** app that calls `FONTS.*` and writes a BDF: BASIC only orchestrates
      (loop over codepoints, threshold, write), never parses the outline. Implemented as
      `mode = "import"` + `[import]` config (`font`, `size`, `first`, `last`, `threshold`, `out`).
      On Andale Mono 16 px, U+32..U+7E → 64 glyphs, `FONTBOUNDINGBOX 9 13`, 7447-byte BDF,
      **byte-identical** on IL / PCodeVM / VM-C / C99Native; `status` re-reads the produced BDF
      (round-trip through the app's own parser).
- [ ] Rendering hinting/antialias policy as an **option in the registry** (never an environment
      variable), with the same default on all backends.
- [ ] Collections `.ttc` (indice > 0) e glifi oltre il BMP: oggi `TTGLYPHS` esplora 0..0xFFFF e
      usa l'indice 0.
