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
- [x] **PCF** (`mini8.pcf`): tabelle PROPERTIES/ACCELERATORS/METRICS/BITMAPS/BDF_ENCODINGS,
      header e campi `format` little-endian, campi delle tabelle big-endian, righe paddate a 4
      byte con bit **MSB-first** (pixel piu' a sinistra = bit alto, come le righe hex del BDF).
      **Validato con FreeType 2.13**: faccia aperta, charmap `[32, 65, 66]`, bitmap identici al
      BDF riga per riga con il pitch. **md5 identico sulle quattro leg** (IL, PCodeVM, VM-C,
      C99Native) e riletto dal `check` con 135 asserzioni, fra cui l'ordine dei bit su una riga
      non simmetrica (`0x7c` di `B`) e gli estremi + lunghezza della tabella `BDF_ENCODINGS`.
- [x] **BDF 2.2 properties**: `AddStdProps` completa in LETTURA le proprieta' standard che il
      sorgente non dichiara — `RESOLUTION_X/Y` (dal `SIZE`), `FONT_ASCENT`/`FONT_DESCENT` e
      `DEFAULT_CHAR` (dal modello) — cosi' BDF e PCF le emettono e il round trip resta
      idempotente (9 proprieta' prima e dopo). `FONT_TYPE` non e' una proprieta' della BDF 2.2:
      non viene emessa.
- [x] **Import da BMP** (`bmpin`): un BMP 24-bit BI_RGB viene letto come griglia di celle —
      campionamento a blocchi `scale`×`scale` (inchiostro = almeno un pixel non bianco),
      ritaglio dei margini, un glifo per cella non vuota con encoding da `first`. Il `check`
      verifica il giro completo modello → `SheetBmp` → `BmpLoad` → modello su due glifi con
      inchiostro su tutti i bordi (148 asserzioni). Limite dichiarato: il foglio non porta le
      metriche per glifo, quindi il box del glifo importato e' il box dell'inchiostro.
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
      **Non più bloccata** (misurato 2026-11-03 dopo il fix dell'imbuto input in GMBasic): il
      terminale è ora un **produttore** della coda eventi globale (`gmb_input_pump` → `gmb_gfx_input`)
      e `INKEY()` ne è un consumatore, come i controlli TUI e `ON KEY`. Su **VM/C** e **C99Native**
      una pressione reale su pty arriva: `PROBE key=[x] code= 120` (tasto inviato 400 ms dopo
      l'avvio, letto dal ciclo). Prima il C99 testuale linkava gli stub di `gmb_ui_null.c`
      (`[ui] build testuale: API UI no-op`); su **IL**/**PCode** i runner restano muti **per
      progetto** (headless: `GMBASIC_HEADLESS=1`, feeder di console spento), non per difetto.
      Resta da fare solo il cablaggio lato app: nel `mode = "tui"` reale, quando `[tui] keys` è
      vuoto, leggere da `INKEY()` (stessa `TuiApply`).
- [x] **GUI** (`mode = "gui"`): **canvas sulla pagina grafica** (`SCREEN 12`), non una WINDOW:
      barra del titolo, glifo zoomato (`zoom`), pannello metriche (`enc`, `bbox`, `offset`, `dwidth`)
      e legenda; cambio glifo con `n`/`p`, ESC/`q` per uscire. Il layout si calcola da `TEXTHEIGHT()`
      (il font di sistema è scalato dalla render-scale: con costanti fisse titolo e pannello si
      sovrapponevano, visto nel primo frame). Disegno con `LINE ... "B"/"BF"` + `DRAW TEXT`.
      `frames = 0` = finestra interattiva; `frames > 0` = si chiude da sola dopo n frame (16 ms l'uno)
      e lascia i marker deterministici `GUI-READY` / `GUI-FRAME n` / `GUI-EXIT` in `bin/gui.log` —
      **non** su stdout: con una finestra grafica attiva `PRINT` finisce nel buffer di celle, che il
      present non compone (stdout muto oltre a `ESC[2J`). **Gate a pixel**: `POINT(x,y)` rilegge barra
      del titolo, sfondo e inchiostro del glifo e scrive `GUI-PIXELS title=4 bg=1 ink=15 ok=1`;
      l'ultimo frame si salva in `bin/gui.png` (`SAVE SCREEN`), così la resa si ispeziona senza
      catturare il desktop. **Verificato**: `present_start: video=1 video_driver=cocoa`,
      `renderer=metal`, 640x480 logici / 1280x960 drawable; dal `gui.png` si vede il frame completo
      (titolo, glifo `A`, metriche, legenda); 200/200 `GUI-FRAME` + `GUI-EXIT`, exit 0. La WINDOW
      `... PIXEL` è stata provata per prima e **non è utilizzabile su questa leg**: la finestra si
      apre ma mostra la pagina grezza, senza chrome — lo stesso sample canonico
      `windows/WindowPixel.bas` resta blu e vuoto. Sul 4-backend: PCodeVM / VM-C / C99Native verdi;
      **IL bloccato** dal bug del backend descritto qui sotto (non dall'app).
- [x] Both front-ends must keep the `check` mode byte-identical (golden artefacts in
      `bin/`), so the headless path stays the reference. Verificato dopo TUI e GUI: `148 ok,
      0 falliti` + `GLYPHSMITH-CHECK-OK` su PCodeVM/VM-C/C99Native e `bin/mini8.pcf` con md5
      `a6deb85ba334e545d80f94c626fca12d` identico sulle quattro leg.

## Bloccante noto — IL: `DRAW TEXT` con stringa non letterale

- [ ] Riprodotto il 2026-11-04. Sul file dell'app la leg **IL** non arriva a eseguire il check:
      `[ILGEN] NotSupportedException: Backend IL: accesso a un array non DIMensionato`. Il difetto si
      riduce a `DRAW TEXT` con stringa **non letterale** quando un SUB ne contiene più di una:

      DIM n AS INTEGER
      n = 7
      DRAW TEXT "a " + STR(n), 10, 10
      DRAW TEXT "b " + STR(n), 10, 30     ' IL: InvalidProgramException

      Stringhe letterali → verde. Con `STR(n)` fallisce anche **una sola** istruzione
      (`InvalidProgramException`); con una funzione utente (`NumS`) una passa e **due** falliscono in
      ILGEN (`NotSupportedException`): il discriminante è la stringa non letterale, non il numero di
      righe. Il sospetto
      è localizzato nell'emissione IL della concatenazione usata come argomento di `DRAW TEXT`
      (`GMBasicSharp/common/IlBackendCodegen/Partial/IlProgramCompiler.Statements.Graphics.cs`,
      `EmitDrawTextStatement` → `EmitExpression(stmt.Text)`), da confermare sul dump `.il`.
      Correzione **fuori dall'app**: richiede il permesso esplicito per toccare GMBasic (AGENTS §0).
      Finché è aperto, GlyphSmith non passa su IL (nessun esito del check, non un check fallito).

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
      variable), with the same default on all backends. **Fuori dall'app**: è un'opzione del
      registry di GMBasic letta dal runtime `FONTS` (C) → serve il permesso esplicito per toccare
      `GMBasic`.
- [ ] Collections `.ttc` (indice > 0) e glifi oltre il BMP: oggi `TTGLYPHS` esplora 0..0xFFFF e
      usa l'indice 0. **Fuori dall'app**: `FONTS` è runtime C di GMBasic (`gmb_fonts.c`) →
      permesso esplicito richiesto.
