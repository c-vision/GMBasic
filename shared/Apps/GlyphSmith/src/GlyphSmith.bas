' FILE: shared/Apps/GlyphSmith/src/GlyphSmith.bas
' ============================================================================
' GlyphSmith — bitmap font workshop (headless).
'
' Legge un font BDF 2.1, espone ogni glifo come griglia di celle editabili,
' applica operazioni di editing (linea, rettangolo, flip, shift, clone),
' riscrive il font in BDF ed esporta in formati che altri tool aprono:
'   BMP 24-bit (foglio di contatto) | PSF2 (font console Linux) |
'   PBM P1 ASCII | header C con i bitmap.
'
' Gli export binari sono possibili grazie a IO.WRITEBYTES (scrittura
' byte-esatta: arriva anche 0x00, che IO.FWRITE invece tronca).
'
' La conversione TrueType/OpenType NON sta qui e non va scritta in BASIC:
' appartiene al runtime C (namespace FONTS, stb_truetype gia' vendorizzato).
' Vedi TODO.md (M5).
'
' MODI (BASIC non ha argv: simboli compile-time o .glyphsmith.toml)
'   (default)                   status: banner + percorsi + riepilogo font
'   -DGLYPHSMITH_MODE_CHECK     autocheck deterministico (parse, metriche,
'                               pixel, ogni operazione, round-trip, export)
'   -DGLYPHSMITH_MODE_SHEET     scrive gli export in <app>/bin
'   -DGLYPHSMITH_MODE_EDIT      applica lo script batch di editing, poi salva
'   -DGLYPHSMITH_MODE_BMPIN     importa un foglio BMP 24-bit come glifi (ritaglio)
'   -DGLYPHSMITH_MODE_GUI       finestra grafica (griglia zoomata, lista, metriche)
'
' RUN (dalla root del monorepo):
'   bash tools/sh/gmb_c99native_run.sh ../shared/Apps/GlyphSmith/src/GlyphSmith.bas
'   tools/sh/harness.sh il shared/Apps/GlyphSmith/src/GlyphSmith.bas
' ============================================================================
IMPORT IO
IMPORT STRINGS
IMPORT BITS
IMPORT FONTS

' --- Limiti dichiarati (array fissi: parita' fra i backend) -----------------
CONST GS_GMAX = 64
CONST GS_ROWS = 32
CONST GS_ROWBUF = 2048
CONST GS_PROPS = 48

' --- Percorsi e configurazione ---------------------------------------------
DIM g_appDir AS STRING
DIM g_projectFile AS STRING
DIM g_cfgMode AS STRING
DIM g_bdfFile AS STRING
DIM g_outDir AS STRING
DIM g_outAbs AS STRING
DIM g_bmpScale AS INTEGER
DIM g_cellsPerRow AS INTEGER
DIM g_editScript AS STRING

' --- Import da font outline (TrueType/OpenType) -----------------------------
' La rasterizzazione NON sta qui: e' il runtime FONTS (C condiviso, stb_truetype).
' BASIC orchestra soltanto: ciclo sui codepoint, soglia, scrittura del BDF.
DIM g_importFont AS STRING
DIM g_importSize AS INTEGER
DIM g_importFirst AS LONG
DIM g_importLast AS LONG
DIM g_importThreshold AS INTEGER
DIM g_importOut AS STRING

' --- Import da bitmap testuale (formato ASCII art prodotto dall'app) ---------
DIM g_textSrc AS STRING
DIM g_textOut AS STRING

' --- Import da foglio BMP (griglia di celle, ritaglio dei margini) -----------
DIM g_bmpinSrc AS STRING
DIM g_bmpinScale AS INTEGER
DIM g_bmpinCols AS INTEGER
DIM g_bmpinCellW AS INTEGER
DIM g_bmpinCellH AS INTEGER
DIM g_bmpinFirst AS LONG
DIM g_bmpinTrim AS STRING
DIM g_bmpinOut AS STRING

' --- M4: GUI (finestra PIXEL sullo stesso modello) --------------------------
DIM g_guiZoom AS INTEGER
DIM g_guiFrames AS INTEGER
DIM g_guiGlyph AS INTEGER

' --- Intestazione del font --------------------------------------------------
DIM g_startFont AS STRING
DIM g_fontName AS STRING
DIM g_pointSize AS INTEGER
DIM g_xres AS INTEGER
DIM g_yres AS INTEGER
DIM g_bbW AS INTEGER
DIM g_bbH AS INTEGER
DIM g_bbX AS INTEGER
DIM g_bbY AS INTEGER
DIM g_declChars AS INTEGER

' --- Proprieta' del font (coppie nome/valore, verbatim) ---------------------
DIM g_propName(GS_PROPS) AS STRING
DIM g_propValue(GS_PROPS) AS STRING
DIM g_nProps AS INTEGER

' --- Glifi: metriche + griglia di righe '0'/'1' -----------------------------
DIM g_enc(GS_GMAX) AS LONG
DIM g_gw(GS_GMAX) AS INTEGER
DIM g_gh(GS_GMAX) AS INTEGER
DIM g_gx(GS_GMAX) AS INTEGER
DIM g_gy(GS_GMAX) AS INTEGER
DIM g_dwx(GS_GMAX) AS INTEGER
DIM g_swx(GS_GMAX) AS INTEGER
DIM g_nRows(GS_GMAX) AS INTEGER
DIM g_gname(GS_GMAX) AS STRING
DIM g_row(GS_ROWBUF) AS STRING
DIM g_nGlyphs AS INTEGER

' --- Scratch per shift/flip (nessun array passato come parametro) -----------
DIM g_tmp(GS_ROWS) AS STRING

' --- M2: clipboard di regione e cronologia undo/redo (stack limitato) -------
CONST GS_UNDO = 4
' Proprieta' del PCF in scrittura (globali: gli array di stringhe locali a un SUB
' non sono affidabili su tutte le gambe, e il writer PCF deve essere portabile).
DIM g_pcfN(64) AS STRING
DIM g_pcfV(64) AS STRING

DIM g_clipW AS INTEGER
DIM g_clipH AS INTEGER
DIM g_clipRows(GS_ROWS) AS STRING
DIM g_undoIdx(GS_UNDO) AS INTEGER
DIM g_undoW(GS_UNDO) AS INTEGER
DIM g_undoH(GS_UNDO) AS INTEGER
DIM g_undoGx(GS_UNDO) AS INTEGER
DIM g_undoGy(GS_UNDO) AS INTEGER
DIM g_undoDw(GS_UNDO) AS INTEGER
DIM g_undoSw(GS_UNDO) AS INTEGER
DIM g_undoNR(GS_UNDO) AS INTEGER
DIM g_undoRow(GS_UNDO * GS_ROWS) AS STRING
DIM g_undoTop AS INTEGER
DIM g_redoIdx(GS_UNDO) AS INTEGER
DIM g_redoW(GS_UNDO) AS INTEGER
DIM g_redoH(GS_UNDO) AS INTEGER
DIM g_redoGx(GS_UNDO) AS INTEGER
DIM g_redoGy(GS_UNDO) AS INTEGER
DIM g_redoDw(GS_UNDO) AS INTEGER
DIM g_redoSw(GS_UNDO) AS INTEGER
DIM g_redoNR(GS_UNDO) AS INTEGER
DIM g_redoRow(GS_UNDO * GS_ROWS) AS STRING
DIM g_redoTop AS INTEGER

' --- M4: TUI (vista sullo stesso modello, pilotata da uno script di tasti) ---
DIM g_tuiKeys AS STRING
DIM g_tuiRender AS STRING
DIM g_tuiOut AS STRING
DIM g_tuiGlyph AS INTEGER
DIM g_tuiX AS INTEGER
DIM g_tuiY AS INTEGER

' --- Contatori dell'autocheck ----------------------------------------------
DIM g_ok AS INTEGER
DIM g_ko AS INTEGER

' ============================================================================
SUB Main()
    DIM mode AS STRING
    BootPaths()
    ConfigureDefaults()
    PrintBanner()
    LoadSettings()
    mode = ComputeMode()
    IF LEN(mode) = 0 THEN mode = STRINGS.UPPER(g_cfgMode)
    IF LEN(mode) = 0 THEN mode = "STATUS"

    IF mode = "CHECK" THEN
        RunCheck()
    ELSEIF mode = "SHEET" THEN
        RunSheet()
    ELSEIF mode = "EDIT" THEN
        RunEdit()
    ELSEIF mode = "IMPORT" THEN
        RunImport()
    ELSEIF mode = "TEXTIN" THEN
        RunTextIn()
    ELSEIF mode = "BMPIN" THEN
        RunBmpIn()
    ELSEIF mode = "GUI" THEN
        RunGui()
    ELSEIF mode = "TUI" THEN
        RunTui()
    ELSE
        PrintStatus()
    END IF
END SUB

FUNCTION ComputeMode() AS STRING
    DIM m AS STRING
    m = ""
#IFDEF GLYPHSMITH_MODE_CHECK
    m = "CHECK"
#ENDIF
#IFDEF GLYPHSMITH_MODE_SHEET
    m = "SHEET"
#ENDIF
#IFDEF GLYPHSMITH_MODE_EDIT
    m = "EDIT"
#ENDIF
#IFDEF GLYPHSMITH_MODE_IMPORT
    m = "IMPORT"
#ENDIF
#IFDEF GLYPHSMITH_MODE_TEXTIN
    m = "TEXTIN"
#ENDIF
#IFDEF GLYPHSMITH_MODE_BMPIN
    m = "BMPIN"
#ENDIF
#IFDEF GLYPHSMITH_MODE_GUI
    m = "GUI"
#ENDIF
#IFDEF GLYPHSMITH_MODE_TUI
    m = "TUI"
#ENDIF
    ComputeMode = m
END FUNCTION

SUB BootPaths()
    DIM srcDir AS STRING, appDir AS STRING
    srcDir = PROJECTFOLDER()
    IF LEN(srcDir) = 0 THEN srcDir = IO.CWD()
    appDir = IO.PARENTDIR(RTrimSlash(srcDir))
    IF LEN(appDir) = 0 THEN appDir = srcDir
    g_appDir = RTrimSlash(appDir)
END SUB

SUB ConfigureDefaults()
    g_cfgMode = ""
    g_bdfFile = "fixtures/mini8.bdf"
    g_outDir = "bin"
    g_bmpScale = 4
    g_cellsPerRow = 8
    g_editScript = "edits/demo.txt"
    g_importFont = ""
    g_importSize = 16
    g_importFirst = 32
    g_importLast = 126
    g_importThreshold = 128
    g_importOut = "imported.bdf"
    g_textSrc = "bin/mini8.ascii"
    g_textOut = "from_text.bdf"
    g_bmpinSrc = "bin/sheet.bmp"
    g_bmpinScale = 4
    g_bmpinCols = 0
    g_bmpinCellW = 0
    g_bmpinCellH = 0
    g_bmpinFirst = 32
    g_bmpinTrim = "1"
    g_bmpinOut = "from_bmp.bdf"
    g_guiZoom = 8
    g_guiFrames = 2
    g_guiGlyph = 0
    g_tuiKeys = "edits/keys.txt"
    g_tuiRender = "0"
    g_tuiOut = "tui.bdf"
END SUB

SUB PrintBanner()
    PRINT "+------------------------------------------------------------------+"
    PRINT "|  GlyphSmith - bitmap font workshop (BDF -> BMP / PSF2 / PBM / C)  |"
    PRINT "+------------------------------------------------------------------+"
    PRINT ""
END SUB

' ============================================================================
' Config: .glyphsmith.toml (sottoinsieme TOML, come le altre app)
' ============================================================================
SUB LoadSettings()
    DIM dir AS STRING, candidate AS STRING
    DIM i AS INTEGER, found AS INTEGER
    dir = IO.CWD()
    found = 0
    FOR i = 1 TO 8
        IF found = 0 THEN
            candidate = RTrimSlash(dir) + "/.glyphsmith.toml"
            IF IO.FILEEXISTS(candidate) THEN
                g_projectFile = candidate
                ParseSettingsFile(candidate)
                found = 1
            ELSE
                dir = IO.PARENTDIR(dir)
                IF LEN(dir) = 0 THEN found = 1
            END IF
        END IF
    NEXT i
    ' Fallback: gli harness possono lanciare con CWD = root del monorepo (la app non
    ' e' un antenato), quindi si cerca anche nella cartella della app.
    IF LEN(g_projectFile) = 0 THEN
        candidate = g_appDir + "/.glyphsmith.toml"
        IF IO.FILEEXISTS(candidate) THEN
            g_projectFile = candidate
            ParseSettingsFile(candidate)
        END IF
    END IF
END SUB

SUB ParseSettingsFile(path AS STRING)
    DIM lines
    DIM raw AS STRING, section AS STRING, key AS STRING, val AS STRING
    DIM eq AS INTEGER, i AS LONG
    lines = IO.READLINES(path)
    section = ""
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            IF STRINGS.STARTSWITH(raw, "[") THEN
                section = STRINGS.UPPER(BracketInner(raw))
            ELSE
                eq = INSTR(raw, "=")
                IF eq > 1 THEN
                    key = STRINGS.UPPER(STRINGS.TRIM(LEFT$(raw, eq - 1)))
                    val = Unquote(STRINGS.TRIM(MID$(raw, eq + 1, LEN(raw) - eq)))
                    IF section = "SETTINGS" AND key = "MODE" THEN g_cfgMode = STRINGS.UPPER(val)
                    IF section = "SOURCE" AND key = "BDF" THEN g_bdfFile = val
                    IF section = "OUTPUT" AND key = "DIR" THEN g_outDir = val
                    IF section = "OUTPUT" AND key = "BMP_SCALE" THEN g_bmpScale = CINT(VAL(val))
                    IF section = "OUTPUT" AND key = "CELLS_PER_ROW" THEN g_cellsPerRow = CINT(VAL(val))
                    IF section = "EDIT" AND key = "SCRIPT" THEN g_editScript = val
                    IF section = "IMPORT" AND key = "FONT" THEN g_importFont = val
                    IF section = "IMPORT" AND key = "SIZE" THEN g_importSize = CINT(VAL(val))
                    IF section = "IMPORT" AND key = "FIRST" THEN g_importFirst = CLNG(VAL(val))
                    IF section = "IMPORT" AND key = "LAST" THEN g_importLast = CLNG(VAL(val))
                    IF section = "IMPORT" AND key = "THRESHOLD" THEN g_importThreshold = CINT(VAL(val))
                    IF section = "IMPORT" AND key = "OUT" THEN g_importOut = val
                    IF section = "TEXT" AND key = "SOURCE" THEN g_textSrc = val
                    IF section = "TEXT" AND key = "OUT" THEN g_textOut = val
                    IF section = "BMPIN" AND key = "SOURCE" THEN g_bmpinSrc = val
                    IF section = "BMPIN" AND key = "SCALE" THEN g_bmpinScale = CINT(VAL(val))
                    IF section = "BMPIN" AND key = "COLS" THEN g_bmpinCols = CINT(VAL(val))
                    IF section = "BMPIN" AND key = "CELLW" THEN g_bmpinCellW = CINT(VAL(val))
                    IF section = "BMPIN" AND key = "CELLH" THEN g_bmpinCellH = CINT(VAL(val))
                    IF section = "BMPIN" AND key = "FIRST" THEN g_bmpinFirst = CLNG(VAL(val))
                    IF section = "BMPIN" AND key = "TRIM" THEN g_bmpinTrim = val
                    IF section = "BMPIN" AND key = "OUT" THEN g_bmpinOut = val
                    IF section = "GUI" AND key = "ZOOM" THEN g_guiZoom = CINT(VAL(val))
                    IF section = "GUI" AND key = "FRAMES" THEN g_guiFrames = CINT(VAL(val))
                    IF section = "GUI" AND key = "GLYPH" THEN g_guiGlyph = CINT(VAL(val))
                    IF section = "TUI" AND key = "KEYS" THEN g_tuiKeys = val
                    IF section = "TUI" AND key = "RENDER" THEN g_tuiRender = val
                    IF section = "TUI" AND key = "OUT" THEN g_tuiOut = val
                END IF
            END IF
        END IF
    NEXT i
END SUB

FUNCTION BracketInner(s AS STRING) AS STRING
    BracketInner = STRINGS.TRIM(MID$(s, 2, LEN(s) - 2))
END FUNCTION

FUNCTION Unquote(s AS STRING) AS STRING
    DIM t AS STRING, q AS STRING
    q = CHR$(34)
    t = s
    IF LEN(t) >= 2 THEN
        IF STRINGS.STARTSWITH(t, q) AND STRINGS.ENDSWITH(t, q) THEN
            t = MID$(t, 2, LEN(t) - 2)
        END IF
    END IF
    Unquote = STRINGS.TRIM(t)
END FUNCTION

FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF STRINGS.ENDSWITH(s, "/") THEN s = STRINGS.LEFT(s, LEN(s) - 1)
    RTrimSlash = s
END FUNCTION

SUB Fail(msg AS STRING)
    PRINT "errore: "; msg
    ERROR 5
END SUB

' ============================================================================
' Percorsi
' ============================================================================
FUNCTION BdfPath() AS STRING
    IF STRINGS.STARTSWITH(g_bdfFile, "/") THEN
        BdfPath = g_bdfFile
    ELSE
        BdfPath = g_appDir + "/" + g_bdfFile
    END IF
END FUNCTION

FUNCTION ScriptPath() AS STRING
    IF STRINGS.STARTSWITH(g_editScript, "/") THEN
        ScriptPath = g_editScript
    ELSE
        ScriptPath = g_appDir + "/" + g_editScript
    END IF
END FUNCTION

SUB EnsureOutDir()
    DIM d AS STRING
    d = g_outDir
    IF LEN(d) = 0 THEN d = "bin"
    IF NOT STRINGS.STARTSWITH(d, "/") THEN d = g_appDir + "/" + d
    IF NOT IO.DIREXISTS(d) THEN MKDIR (d)
    g_outAbs = d
END SUB

FUNCTION OutPath(name AS STRING) AS STRING
    OutPath = g_outAbs + "/" + name
END FUNCTION

FUNCTION FileSizeOf(path AS STRING) AS LONG
    DIM h AS LONG, n AS LONG
    FileSizeOf = -1
    IF IO.FILEEXISTS(path) THEN
        h = IO.FOPEN(path)
        IF h >= 0 THEN
            n = IO.LOF(h)
            IO.FCLOSE(h)
            FileSizeOf = n
        END IF
    END IF
END FUNCTION

' ============================================================================
' MODE: STATUS
' ============================================================================
SUB PrintStatus()
    PRINT "project file : "; g_projectFile
    PRINT "app dir      : "; g_appDir
    PRINT "sorgente BDF : "; BdfPath(); "  ("; PresenceText(BdfPath()); ")"
    PRINT "output dir   : "; g_outDir
    PRINT "bmp scale    : "; g_bmpScale
    PRINT "celle/riga   : "; g_cellsPerRow
    PRINT "script edit  : "; ScriptPath(); "  ("; PresenceText(ScriptPath()); ")"
    PRINT "limiti       : glifi "; GS_GMAX; " | righe "; GS_ROWS; " | proprieta "; GS_PROPS
    PRINT ""
    IF IO.FILEEXISTS(BdfPath()) THEN
        LoadBdf(BdfPath())
        PRINT "font         : "; g_fontName
        PRINT "glifi        : "; g_nGlyphs; " (dichiarati "; g_declChars; ")"
        PRINT "bounding box : "; g_bbW; "x"; g_bbH; " a "; g_bbX; ","; g_bbY
    END IF
    PRINT ""
    PRINT "modi         : -DGLYPHSMITH_MODE_CHECK | _SHEET | _EDIT | _IMPORT | _TEXTIN | _TUI | _BMPIN | _GUI"
END SUB

FUNCTION PresenceText(p AS STRING) AS STRING
    IF IO.FILEEXISTS(p) THEN
        PresenceText = "presente"
    ELSE
        PresenceText = "assente"
    END IF
END FUNCTION

' ============================================================================
' Parsing BDF 2.1
' ============================================================================
SUB LoadBdf(path AS STRING)
    DIM lines
    DIM raw AS STRING, key AS STRING, rest AS STRING
    DIM i AS LONG, state AS INTEGER, cur AS INTEGER
    IF NOT IO.FILEEXISTS(path) THEN Fail("sorgente BDF assente: " + path)
    g_nGlyphs = 0
    g_nProps = 0
    g_declChars = 0
    state = 0
    cur = -1
    lines = IO.READLINES(path)
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 THEN
            key = STRINGS.UPPER(FirstWord(raw))
            rest = RestAfter(raw)
            IF state = 2 THEN
                IF key = "ENDCHAR" THEN
                    state = 0
                    cur = -1
                ELSE
                    IF cur >= 0 THEN AppendRow(cur, key)
                END IF
            ELSE
                IF key = "STARTFONT" THEN g_startFont = rest
                IF key = "FONT" THEN g_fontName = rest
                IF key = "SIZE" THEN ParseSize(rest)
                IF key = "FONTBOUNDINGBOX" THEN ParseBoundingBox(rest)
                IF key = "STARTPROPERTIES" THEN state = 1
                IF key = "ENDPROPERTIES" THEN state = 0
                IF state = 1 THEN
                    IF key <> "STARTPROPERTIES" AND key <> "ENDPROPERTIES" THEN AddProp(raw)
                END IF
                IF key = "CHARS" THEN g_declChars = CINT(VAL(rest))
                IF key = "STARTCHAR" THEN cur = NewGlyph(rest)
                IF cur >= 0 THEN
                    IF key = "ENCODING" THEN g_enc(cur) = CLNG(VAL(rest))
                    IF key = "SWIDTH" THEN g_swx(cur) = CINT(VAL(TokAt(rest, 0)))
                    IF key = "DWIDTH" THEN g_dwx(cur) = CINT(VAL(TokAt(rest, 0)))
                    IF key = "BBX" THEN ParseGlyphBox(cur, rest)
                    IF key = "BITMAP" THEN state = 2
                END IF
            END IF
        END IF
    NEXT i
    AddStdProps()
END SUB

SUB ParseSize(rest AS STRING)
    g_pointSize = CINT(VAL(TokAt(rest, 0)))
    g_xres = CINT(VAL(TokAt(rest, 1)))
    g_yres = CINT(VAL(TokAt(rest, 2)))
END SUB

SUB ParseBoundingBox(rest AS STRING)
    g_bbW = CINT(VAL(TokAt(rest, 0)))
    g_bbH = CINT(VAL(TokAt(rest, 1)))
    g_bbX = CINT(VAL(TokAt(rest, 2)))
    g_bbY = CINT(VAL(TokAt(rest, 3)))
END SUB

SUB ParseGlyphBox(g AS INTEGER, rest AS STRING)
    g_gw(g) = CINT(VAL(TokAt(rest, 0)))
    g_gh(g) = CINT(VAL(TokAt(rest, 1)))
    g_gx(g) = CINT(VAL(TokAt(rest, 2)))
    g_gy(g) = CINT(VAL(TokAt(rest, 3)))
END SUB

FUNCTION NewGlyph(nm AS STRING) AS INTEGER
    DIM g AS INTEGER
    IF g_nGlyphs >= GS_GMAX THEN
        Fail("troppi glifi: limite " + NumS(GS_GMAX))
    END IF
    g = g_nGlyphs
    g_nGlyphs = g_nGlyphs + 1
    g_enc(g) = 0
    g_gw(g) = g_bbW
    g_gh(g) = g_bbH
    g_gx(g) = 0
    g_gy(g) = 0
    g_dwx(g) = g_bbW
    g_swx(g) = g_bbW * 62
    g_nRows(g) = 0
    g_gname(g) = nm
    NewGlyph = g
END FUNCTION

SUB AddProp(raw AS STRING)
    DIM key AS STRING, val AS STRING
    IF g_nProps < GS_PROPS THEN
        key = FirstWord(raw)
        val = RestAfter(raw)
        g_propName(g_nProps) = key
        g_propValue(g_nProps) = val
        g_nProps = g_nProps + 1
    END IF
END SUB

' --- righe BITMAP: da esadecimale a griglia di '0'/'1' ----------------------
SUB AppendRow(g AS INTEGER, hexs AS STRING)
    DIM w AS INTEGER, r AS INTEGER, out AS STRING, c AS INTEGER
    IF g >= 0 THEN
    IF g < GS_GMAX THEN
        w = g_gw(g)
        r = g_nRows(g)
        IF r < g_gh(g) THEN
        IF r < GS_ROWS THEN
        IF w > 0 THEN
            out = ""
            FOR c = 0 TO w - 1
                IF RowBit(hexs, c) = 1 THEN
                    out = out + "1"
                ELSE
                    out = out + "0"
                END IF
            NEXT c
            g_row(g * GS_ROWS + r) = out
            g_nRows(g) = r + 1
        END IF
        END IF
        END IF
    END IF
    END IF
END SUB

FUNCTION RowBit(hexs AS STRING, col AS INTEGER) AS INTEGER
    DIM nb AS INTEGER, bi AS INTEGER, p AS INTEGER, v AS INTEGER
    RowBit = 0
    nb = INT(LEN(hexs) / 2)
    bi = INT(col / 8)
    IF bi < nb THEN
        p = bi * 2 + 1
        v = HexVal(MID$(hexs, p, 1)) * 16 + HexVal(MID$(hexs, p + 1, 1))
        RowBit = BITS.BITGET(v, 7 - (col - bi * 8))
    END IF
END FUNCTION

FUNCTION HexVal(s AS STRING) AS INTEGER
    DIM d AS STRING, p AS INTEGER
    d = "0123456789ABCDEF"
    HexVal = 0
    IF LEN(s) > 0 THEN
        p = INSTR(d, STRINGS.UPPER(LEFT$(s, 1)))
        IF p > 0 THEN HexVal = p - 1
    END IF
END FUNCTION

' --- tokenizzazione minima --------------------------------------------------
FUNCTION FirstWord(s AS STRING) AS STRING
    DIM i AS INTEGER, out AS STRING, done AS INTEGER, ch AS STRING
    out = ""
    done = 0
    FOR i = 1 TO LEN(s)
        IF done = 0 THEN
            ch = MID$(s, i, 1)
            IF ch = " " OR ch = CHR$(9) THEN
                done = 1
            ELSE
                out = out + ch
            END IF
        END IF
    NEXT i
    FirstWord = out
END FUNCTION

FUNCTION RestAfter(s AS STRING) AS STRING
    DIM i AS INTEGER, out AS STRING, done AS INTEGER, ch AS STRING
    out = ""
    done = 0
    FOR i = 1 TO LEN(s)
        ch = MID$(s, i, 1)
        IF done = 0 THEN
            IF ch = " " OR ch = CHR$(9) THEN
                out = STRINGS.TRIM(MID$(s, i + 1, LEN(s) - i))
                done = 1
            END IF
        END IF
    NEXT i
    RestAfter = out
END FUNCTION

FUNCTION TokAt(s AS STRING, k AS INTEGER) AS STRING
    DIM i AS INTEGER, idx AS INTEGER, cur AS STRING, ch AS STRING, res AS STRING
    idx = 0
    cur = ""
    res = ""
    FOR i = 1 TO LEN(s)
        ch = MID$(s, i, 1)
        IF ch = " " OR ch = CHR$(9) THEN
            IF LEN(cur) > 0 THEN
                IF idx = k THEN res = cur
                idx = idx + 1
                cur = ""
            END IF
        ELSE
            cur = cur + ch
        END IF
    NEXT i
    IF LEN(cur) > 0 THEN
        IF idx = k THEN res = cur
    END IF
    TokAt = res
END FUNCTION

' ============================================================================
' Modello: griglia e operazioni di editing
' ============================================================================
FUNCTION RowAt(g AS INTEGER, r AS INTEGER) AS STRING
    DIM s AS STRING
    s = ""
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            IF r >= 0 THEN
                IF r < g_nRows(g) THEN s = g_row(g * GS_ROWS + r)
            END IF
        END IF
    END IF
    RowAt = s
END FUNCTION

FUNCTION GetPx(g AS INTEGER, x AS INTEGER, y AS INTEGER) AS INTEGER
    DIM row AS STRING, w AS INTEGER, h AS INTEGER
    GetPx = 0
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            w = g_gw(g)
            h = g_gh(g)
            IF x >= 0 THEN
                IF x < w THEN
                    IF y >= 0 THEN
                        IF y < h THEN
                            row = RowAt(g, y)
                            IF MID$(row, x + 1, 1) = "1" THEN GetPx = 1
                        END IF
                    END IF
                END IF
            END IF
        END IF
    END IF
END FUNCTION

SUB SetPx(g AS INTEGER, x AS INTEGER, y AS INTEGER, ch AS STRING)
    DIM w AS INTEGER, h AS INTEGER, r AS INTEGER, row AS STRING, out AS STRING, i AS INTEGER
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            w = g_gw(g)
            h = g_gh(g)
            IF x >= 0 THEN
                IF x < w THEN
                    IF y >= 0 THEN
                        IF y < h THEN
                            r = g * GS_ROWS + y
                            row = g_row(r)
                            out = ""
                            FOR i = 1 TO w
                                IF i = x + 1 THEN
                                    out = out + ch
                                ELSE
                                    out = out + MID$(row, i, 1)
                                END IF
                            NEXT i
                            g_row(r) = out
                        END IF
                    END IF
                END IF
            END IF
        END IF
    END IF
END SUB

SUB ClearGlyph(g AS INTEGER)
    DIM i AS INTEGER, out AS STRING, w AS INTEGER, r AS INTEGER
    IF g >= 0 THEN
    IF g < GS_GMAX THEN
        w = g_gw(g)
        out = ""
        FOR i = 1 TO w
            out = out + "0"
        NEXT i
        FOR r = 0 TO g_gh(g) - 1
            IF r < GS_ROWS THEN g_row(g * GS_ROWS + r) = out
        NEXT r
        IF g_nRows(g) < g_gh(g) THEN g_nRows(g) = g_gh(g)
    END IF
    END IF
END SUB

SUB CloneGlyph(src AS INTEGER, dst AS INTEGER)
    DIM i AS INTEGER, r AS INTEGER
    IF src >= 0 THEN
    IF src < GS_GMAX THEN
    IF dst >= 0 THEN
    IF dst < GS_GMAX THEN
        g_enc(dst) = g_enc(src)
        g_gw(dst) = g_gw(src)
        g_gh(dst) = g_gh(src)
        g_gx(dst) = g_gx(src)
        g_gy(dst) = g_gy(src)
        g_dwx(dst) = g_dwx(src)
        g_swx(dst) = g_swx(src)
        g_nRows(dst) = g_nRows(src)
        g_gname(dst) = g_gname(src)
        FOR r = 0 TO g_nRows(src) - 1
            IF r < GS_ROWS THEN g_row(dst * GS_ROWS + r) = g_row(src * GS_ROWS + r)
        NEXT r
    END IF
    END IF
    END IF
    END IF
END SUB

SUB FlipH(g AS INTEGER)
    DIM r AS INTEGER, row AS STRING, out AS STRING, i AS INTEGER, w AS INTEGER
    IF g >= 0 THEN
    IF g < GS_GMAX THEN
        w = g_gw(g)
        FOR r = 0 TO g_nRows(g) - 1
            IF r < GS_ROWS THEN
                row = g_row(g * GS_ROWS + r)
                out = ""
                FOR i = w TO 1 STEP -1
                    out = out + MID$(row, i, 1)
                NEXT i
                g_row(g * GS_ROWS + r) = out
            END IF
        NEXT r
    END IF
    END IF
END SUB

SUB FlipV(g AS INTEGER)
    DIM r AS INTEGER, n AS INTEGER
    IF g >= 0 THEN
    IF g < GS_GMAX THEN
        n = g_nRows(g)
        FOR r = 0 TO n - 1
            IF r < GS_ROWS THEN g_tmp(r) = g_row(g * GS_ROWS + r)
        NEXT r
        FOR r = 0 TO n - 1
            IF r < GS_ROWS THEN g_row(g * GS_ROWS + r) = g_tmp(n - 1 - r)
        NEXT r
    END IF
    END IF
END SUB

SUB ShiftGlyph(g AS INTEGER, dx AS INTEGER, dy AS INTEGER)
    DIM r AS INTEGER, n AS INTEGER, x AS INTEGER, y AS INTEGER, v AS INTEGER
    IF g >= 0 THEN
    IF g < GS_GMAX THEN
        n = g_nRows(g)
        FOR r = 0 TO n - 1
            IF r < GS_ROWS THEN g_tmp(r) = g_row(g * GS_ROWS + r)
        NEXT r
        ClearGlyph(g)
        FOR y = 0 TO n - 1
            FOR x = 0 TO g_gw(g) - 1
                IF y < GS_ROWS THEN
                    IF MID$(g_tmp(y), x + 1, 1) = "1" THEN
                        SetPx(g, x + dx, y + dy, "1")
                    END IF
                END IF
            NEXT x
        NEXT y
    END IF
    END IF
END SUB

SUB RectFill(g AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, ch AS STRING)
    DIM x AS INTEGER, y AS INTEGER, xa AS INTEGER, xb AS INTEGER, ya AS INTEGER, yb AS INTEGER
    xa = x1
    xb = x2
    ya = y1
    yb = y2
    IF xa > xb THEN
        xa = x2
        xb = x1
    END IF
    IF ya > yb THEN
        ya = y2
        yb = y1
    END IF
    FOR y = ya TO yb
        FOR x = xa TO xb
            SetPx(g, x, y, ch)
        NEXT x
    NEXT y
END SUB

SUB DrawLine(g AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER, ch AS STRING)
    DIM dx AS INTEGER, dy AS INTEGER, steps AS INTEGER, i AS INTEGER
    DIM xs AS SINGLE, ys AS SINGLE, ax AS SINGLE, ay AS SINGLE
    dx = x2 - x1
    dy = y2 - y1
    steps = AbsInt(dx)
    IF AbsInt(dy) > steps THEN steps = AbsInt(dy)
    IF steps = 0 THEN
        SetPx(g, x1, y1, ch)
    ELSE
        xs = dx
        ys = dy
        xs = xs / steps
        ys = ys / steps
        ax = x1
        ay = y1
        FOR i = 0 TO steps
            SetPx(g, INT(ax + 0.5), INT(ay + 0.5), ch)
            ax = ax + xs
            ay = ay + ys
        NEXT i
    END IF
END SUB

FUNCTION AbsInt(v AS INTEGER) AS INTEGER
    IF v < 0 THEN
        AbsInt = -v
    ELSE
        AbsInt = v
    END IF
END FUNCTION

FUNCTION InkCount(g AS INTEGER) AS INTEGER
    DIM r AS INTEGER, c AS INTEGER, n AS INTEGER, row AS STRING
    n = 0
    FOR r = 0 TO g_nRows(g) - 1
        row = RowAt(g, r)
        FOR c = 1 TO LEN(row)
            IF MID$(row, c, 1) = "1" THEN n = n + 1
        NEXT c
    NEXT r
    InkCount = n
END FUNCTION

FUNCTION GlyphSig(g AS INTEGER) AS STRING
    DIM r AS INTEGER, s AS STRING
    s = NumS(g_enc(g)) + ":" + NumS(g_gw(g)) + "x" + NumS(g_gh(g)) + "|"
    FOR r = 0 TO g_nRows(g) - 1
        s = s + RowAt(g, r) + ";"
    NEXT r
    GlyphSig = s
END FUNCTION

FUNCTION ModelSignature() AS STRING
    DIM g AS INTEGER, s AS STRING, i AS INTEGER
    s = g_fontName + "#" + NumS(g_nProps) + "#" + NumS(g_declChars) + "#"
    FOR i = 0 TO g_nProps - 1
        s = s + g_propName(i) + "=" + g_propValue(i) + ";"
    NEXT i
    FOR g = 0 TO g_nGlyphs - 1
        s = s + GlyphSig(g)
    NEXT g
    ModelSignature = s
END FUNCTION

FUNCTION SameGlyph(a AS INTEGER, b AS INTEGER) AS INTEGER
    SameGlyph = 0
    IF GlyphSig(a) = GlyphSig(b) THEN SameGlyph = 1
END FUNCTION

' ============================================================================
' M2 — trasformazioni, clipboard, bearing, insert/remove, undo/redo
' Tutte le operazioni lavorano sul modello (g_row + metriche), mai su un secondo
' modello: cosi' i quattro backend restano identici.
' ============================================================================
SUB Rot90(g AS INTEGER)
    DIM ow AS INTEGER, oh AS INTEGER, x AS INTEGER, y AS INTEGER, r AS INTEGER, s AS STRING
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            ow = g_gw(g)
            oh = g_gh(g)
            IF ow > 0 THEN
                IF oh > 0 THEN
                    IF ow <= GS_ROWS THEN
                        IF oh <= GS_ROWS THEN
                            FOR r = 0 TO oh - 1
                                g_tmp(r) = g_row(g * GS_ROWS + r)
                            NEXT r
                            g_gw(g) = oh
                            g_gh(g) = ow
                            g_nRows(g) = ow
                            FOR y = 0 TO ow - 1
                                s = ""
                                FOR x = 0 TO oh - 1
                                    IF MID$(g_tmp(oh - 1 - x), y + 1, 1) = "1" THEN
                                        s = s + "1"
                                    ELSE
                                        s = s + "0"
                                    END IF
                                NEXT x
                                g_row(g * GS_ROWS + y) = s
                            NEXT y
                            FOR r = ow TO GS_ROWS - 1
                                g_row(g * GS_ROWS + r) = ""
                            NEXT r
                        END IF
                    END IF
                END IF
            END IF
        END IF
    END IF
END SUB

' Shear orizzontale: la riga y si sposta di k*y pixel (k puo' essere negativo).
SUB ShearH(g AS INTEGER, k AS INTEGER)
    DIM w AS INTEGER, h AS INTEGER, x AS INTEGER, y AS INTEGER, dx AS INTEGER, r AS INTEGER
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            w = g_gw(g)
            h = g_gh(g)
            FOR r = 0 TO h - 1
                IF r < GS_ROWS THEN g_tmp(r) = g_row(g * GS_ROWS + r)
            NEXT r
            ClearGlyph(g)
            FOR y = 0 TO h - 1
                dx = k * y
                FOR x = 0 TO w - 1
                    IF MID$(g_tmp(y), x + 1, 1) = "1" THEN SetPx(g, x + dx, y, "1")
                NEXT x
            NEXT y
        END IF
    END IF
END SUB

SUB RegionCopy(g AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, x2 AS INTEGER, y2 AS INTEGER)
    DIM xa AS INTEGER, xb AS INTEGER, ya AS INTEGER, yb AS INTEGER
    DIM x AS INTEGER, y AS INTEGER, s AS STRING
    xa = x1
    xb = x2
    ya = y1
    yb = y2
    IF xa > xb THEN
        xa = x2
        xb = x1
    END IF
    IF ya > yb THEN
        ya = y2
        yb = y1
    END IF
    g_clipW = xb - xa + 1
    g_clipH = yb - ya + 1
    IF g_clipW < 1 THEN g_clipW = 1
    IF g_clipH < 1 THEN g_clipH = 1
    FOR y = 0 TO g_clipH - 1
        s = ""
        FOR x = 0 TO g_clipW - 1
            IF GetPx(g, xa + x, ya + y) = 1 THEN
                s = s + "1"
            ELSE
                s = s + "0"
            END IF
        NEXT x
        IF y < GS_ROWS THEN g_clipRows(y) = s
    NEXT y
END SUB

' Incolla l'inchiostro della regione (non cancella i pixel di destinazione).
SUB RegionPaste(g AS INTEGER, x AS INTEGER, y AS INTEGER)
    DIM cx AS INTEGER, cy AS INTEGER
    FOR cy = 0 TO g_clipH - 1
        FOR cx = 0 TO g_clipW - 1
            IF cy < GS_ROWS THEN
                IF MID$(g_clipRows(cy), cx + 1, 1) = "1" THEN SetPx(g, x + cx, y + cy, "1")
            END IF
        NEXT cx
    NEXT cy
END SUB

' Ricalcola gli attributi dall'inchiostro: BBX x, DWIDTH, SWIDTH e offset verticale.
SUB BearingRecompute(g AS INTEGER)
    DIM w AS INTEGER, h AS INTEGER, x AS INTEGER, y AS INTEGER
    DIM x0 AS INTEGER, y0 AS INTEGER, x1 AS INTEGER, y1 AS INTEGER, found AS INTEGER
    found = 0
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            w = g_gw(g)
            h = g_gh(g)
            x0 = w
            y0 = h
            x1 = -1
            y1 = -1
            FOR y = 0 TO h - 1
                FOR x = 0 TO w - 1
                    IF GetPx(g, x, y) = 1 THEN
                        found = 1
                        IF x < x0 THEN x0 = x
                        IF y < y0 THEN y0 = y
                        IF x > x1 THEN x1 = x
                        IF y > y1 THEN y1 = y
                    END IF
                NEXT x
            NEXT y
            IF found = 1 THEN
                g_gx(g) = x0
                g_dwx(g) = x1 - x0 + 1
                IF g_dwx(g) < 1 THEN g_dwx(g) = 1
                g_gy(g) = y1 + 1
                IF g_gy(g) <> 0 THEN g_gy(g) = -g_gy(g)
                IF g_pointSize > 0 THEN g_swx(g) = INT(g_dwx(g) * 1000.0 / g_pointSize + 0.5)
            END IF
        END IF
    END IF
END SUB

SUB InsertGlyph(enc AS LONG, nm AS STRING, w AS INTEGER, h AS INTEGER)
    DIM g AS INTEGER, r AS INTEGER, i AS INTEGER, s AS STRING
    IF g_nGlyphs >= GS_GMAX THEN Fail("troppi glifi: limite " + NumS(GS_GMAX))
    IF w < 1 THEN w = 1
    IF h < 1 THEN h = 1
    IF w > 32 THEN w = 32
    IF h > GS_ROWS THEN h = GS_ROWS
    g = NewGlyph(nm)
    g_enc(g) = enc
    g_gw(g) = w
    g_gh(g) = h
    g_dwx(g) = w
    g_swx(g) = w * 62
    g_nRows(g) = h
    s = ""
    FOR i = 1 TO w
        s = s + "0"
    NEXT i
    FOR r = 0 TO h - 1
        g_row(g * GS_ROWS + r) = s
    NEXT r
    IF w > g_bbW THEN g_bbW = w
    IF h > g_bbH THEN g_bbH = h
    g_declChars = g_nGlyphs
END SUB

SUB RemoveGlyph(g AS INTEGER)
    DIM i AS INTEGER, r AS INTEGER, dst AS INTEGER, src AS INTEGER
    IF g >= 0 THEN
        IF g < g_nGlyphs THEN
            FOR i = g TO g_nGlyphs - 2
                dst = i
                src = i + 1
                g_enc(dst) = g_enc(src)
                g_gw(dst) = g_gw(src)
                g_gh(dst) = g_gh(src)
                g_gx(dst) = g_gx(src)
                g_gy(dst) = g_gy(src)
                g_dwx(dst) = g_dwx(src)
                g_swx(dst) = g_swx(src)
                g_nRows(dst) = g_nRows(src)
                g_gname(dst) = g_gname(src)
                FOR r = 0 TO GS_ROWS - 1
                    g_row(dst * GS_ROWS + r) = g_row(src * GS_ROWS + r)
                NEXT r
            NEXT i
            g_nGlyphs = g_nGlyphs - 1
            FOR r = 0 TO GS_ROWS - 1
                g_row(g_nGlyphs * GS_ROWS + r) = ""
            NEXT r
            g_declChars = g_nGlyphs
        END IF
    END IF
END SUB

' --- cronologia: stack limitato di snapshot di UN glifo (le edit di script) ---
SUB USlotCopy(src AS INTEGER, dst AS INTEGER)
    DIM r AS INTEGER
    g_undoIdx(dst) = g_undoIdx(src)
    g_undoW(dst) = g_undoW(src)
    g_undoH(dst) = g_undoH(src)
    g_undoGx(dst) = g_undoGx(src)
    g_undoGy(dst) = g_undoGy(src)
    g_undoDw(dst) = g_undoDw(src)
    g_undoSw(dst) = g_undoSw(src)
    g_undoNR(dst) = g_undoNR(src)
    FOR r = 0 TO GS_ROWS - 1
        g_undoRow(dst * GS_ROWS + r) = g_undoRow(src * GS_ROWS + r)
    NEXT r
END SUB

SUB RSlotCopy(src AS INTEGER, dst AS INTEGER)
    DIM r AS INTEGER
    g_redoIdx(dst) = g_redoIdx(src)
    g_redoW(dst) = g_redoW(src)
    g_redoH(dst) = g_redoH(src)
    g_redoGx(dst) = g_redoGx(src)
    g_redoGy(dst) = g_redoGy(src)
    g_redoDw(dst) = g_redoDw(src)
    g_redoSw(dst) = g_redoSw(src)
    g_redoNR(dst) = g_redoNR(src)
    FOR r = 0 TO GS_ROWS - 1
        g_redoRow(dst * GS_ROWS + r) = g_redoRow(src * GS_ROWS + r)
    NEXT r
END SUB

SUB USlotFromGlyph(g AS INTEGER, slot AS INTEGER)
    DIM r AS INTEGER
    g_undoIdx(slot) = g
    g_undoW(slot) = g_gw(g)
    g_undoH(slot) = g_gh(g)
    g_undoGx(slot) = g_gx(g)
    g_undoGy(slot) = g_gy(g)
    g_undoDw(slot) = g_dwx(g)
    g_undoSw(slot) = g_swx(g)
    g_undoNR(slot) = g_nRows(g)
    FOR r = 0 TO GS_ROWS - 1
        g_undoRow(slot * GS_ROWS + r) = g_row(g * GS_ROWS + r)
    NEXT r
END SUB

SUB RSlotFromGlyph(g AS INTEGER, slot AS INTEGER)
    DIM r AS INTEGER
    g_redoIdx(slot) = g
    g_redoW(slot) = g_gw(g)
    g_redoH(slot) = g_gh(g)
    g_redoGx(slot) = g_gx(g)
    g_redoGy(slot) = g_gy(g)
    g_redoDw(slot) = g_dwx(g)
    g_redoSw(slot) = g_swx(g)
    g_redoNR(slot) = g_nRows(g)
    FOR r = 0 TO GS_ROWS - 1
        g_redoRow(slot * GS_ROWS + r) = g_row(g * GS_ROWS + r)
    NEXT r
END SUB

SUB USlotToGlyph(slot AS INTEGER)
    DIM g AS INTEGER, r AS INTEGER
    g = g_undoIdx(slot)
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            g_gw(g) = g_undoW(slot)
            g_gh(g) = g_undoH(slot)
            g_gx(g) = g_undoGx(slot)
            g_gy(g) = g_undoGy(slot)
            g_dwx(g) = g_undoDw(slot)
            g_swx(g) = g_undoSw(slot)
            g_nRows(g) = g_undoNR(slot)
            FOR r = 0 TO GS_ROWS - 1
                g_row(g * GS_ROWS + r) = g_undoRow(slot * GS_ROWS + r)
            NEXT r
        END IF
    END IF
END SUB

SUB RSlotToGlyph(slot AS INTEGER)
    DIM g AS INTEGER, r AS INTEGER
    g = g_redoIdx(slot)
    IF g >= 0 THEN
        IF g < GS_GMAX THEN
            g_gw(g) = g_redoW(slot)
            g_gh(g) = g_redoH(slot)
            g_gx(g) = g_redoGx(slot)
            g_gy(g) = g_redoGy(slot)
            g_dwx(g) = g_redoDw(slot)
            g_swx(g) = g_redoSw(slot)
            g_nRows(g) = g_redoNR(slot)
            FOR r = 0 TO GS_ROWS - 1
                g_row(g * GS_ROWS + r) = g_redoRow(slot * GS_ROWS + r)
            NEXT r
        END IF
    END IF
END SUB

' Salva lo stato del glifo PRIMA di modificarlo; azzera il ramo redo.
SUB HistPush(g AS INTEGER)
    DIM i AS INTEGER
    IF g < 0 THEN g = 0
    IF g >= GS_GMAX THEN g = GS_GMAX - 1
    IF g_undoTop >= GS_UNDO THEN
        FOR i = 1 TO GS_UNDO - 1
            USlotCopy(i, i - 1)
        NEXT i
        g_undoTop = GS_UNDO - 1
    END IF
    USlotFromGlyph(g, g_undoTop)
    g_undoTop = g_undoTop + 1
    g_redoTop = 0
END SUB

SUB DoUndo()
    DIM slot AS INTEGER
    IF g_undoTop > 0 THEN
        slot = g_undoTop - 1
        IF g_redoTop < GS_UNDO THEN
            RSlotFromGlyph(g_undoIdx(slot), g_redoTop)
            g_redoTop = g_redoTop + 1
        END IF
        USlotToGlyph(slot)
        g_undoTop = slot
    END IF
END SUB

SUB DoRedo()
    DIM slot AS INTEGER
    IF g_redoTop > 0 THEN
        slot = g_redoTop - 1
        IF g_undoTop < GS_UNDO THEN
            USlotFromGlyph(g_redoIdx(slot), g_undoTop)
            g_undoTop = g_undoTop + 1
        END IF
        RSlotToGlyph(slot)
        g_redoTop = slot
    END IF
END SUB

' ============================================================================
' M4 — TUI: una vista sullo STESSO modello (nessun secondo stato)
' Il frame e' deterministico e i tasti arrivano da uno script, cosi' il percorso
' interattivo resta verificabile nel check; il terminale e' solo una sorgente.
' ============================================================================
FUNCTION Has(s AS STRING, sub AS STRING) AS INTEGER
    DIM i AS INTEGER, j AS INTEGER, ls AS INTEGER, lb AS INTEGER, ok AS INTEGER
    Has = 0
    ls = LEN(s)
    lb = LEN(sub)
    IF lb > 0 THEN
        IF ls >= lb THEN
            FOR i = 1 TO ls - lb + 1
                ok = 1
                FOR j = 1 TO lb
                    IF MID$(s, i + j - 1, 1) <> MID$(sub, j, 1) THEN ok = 0
                NEXT j
                IF ok = 1 THEN Has = 1
            NEXT i
        END IF
    END IF
END FUNCTION

FUNCTION TuiFrame() AS STRING
    DIM s AS STRING, top AS STRING, ch AS STRING, g AS INTEGER
    DIM r AS INTEGER, x AS INTEGER
    g = g_tuiGlyph
    IF g < 0 THEN g = 0
    IF g >= GS_GMAX THEN g = GS_GMAX - 1
    top = "+"
    FOR x = 1 TO g_gw(g) + 2
        top = top + "-"
    NEXT x
    top = top + "+"
    s = "glifo " + NumS(g) + " '" + g_gname(g) + "' enc " + NumS(g_enc(g)) + " box " + NumS(g_gw(g)) + "x" + NumS(g_gh(g)) + " inchiostro " + NumS(InkCount(g))
    s = s + CHR$(10) + top
    FOR r = 0 TO g_gh(g) - 1
        s = s + CHR$(10) + "| "
        FOR x = 0 TO g_gw(g) - 1
            IF GetPx(g, x, r) = 1 THEN
                ch = "#"
            ELSE
                ch = "."
            END IF
            IF x = g_tuiX THEN
                IF r = g_tuiY THEN
                    IF GetPx(g, x, r) = 1 THEN
                        ch = "X"
                    ELSE
                        ch = "+"
                    END IF
                END IF
            END IF
            s = s + ch
        NEXT x
        s = s + " |"
    NEXT r
    s = s + CHR$(10) + top
    s = s + CHR$(10) + "cursore " + NumS(g_tuiX) + "," + NumS(g_tuiY) + "  clip " + NumS(g_clipW) + "x" + NumS(g_clipH) + "  undo " + NumS(g_undoTop) + " redo " + NumS(g_redoTop)
    TuiFrame = s
END FUNCTION

SUB TuiApply(k AS STRING)
    DIM g AS INTEGER
    g = g_tuiGlyph
    IF k = "l" OR k = "L" THEN
        IF g_tuiX < g_gw(g) - 1 THEN g_tuiX = g_tuiX + 1
    ELSEIF k = "h" OR k = "H" THEN
        IF g_tuiX > 0 THEN g_tuiX = g_tuiX - 1
    ELSEIF k = "j" OR k = "J" THEN
        IF g_tuiY < g_gh(g) - 1 THEN g_tuiY = g_tuiY + 1
    ELSEIF k = "k" OR k = "K" THEN
        IF g_tuiY > 0 THEN g_tuiY = g_tuiY - 1
    ELSEIF k = " " THEN
        HistPush(g)
        IF GetPx(g, g_tuiX, g_tuiY) = 1 THEN
            SetPx(g, g_tuiX, g_tuiY, "0")
        ELSE
            SetPx(g, g_tuiX, g_tuiY, "1")
        END IF
    ELSEIF k = "n" OR k = "N" THEN
        IF g_tuiGlyph < g_nGlyphs - 1 THEN g_tuiGlyph = g_tuiGlyph + 1
        g_tuiX = 0
        g_tuiY = 0
    ELSEIF k = "p" OR k = "P" THEN
        IF g_tuiGlyph > 0 THEN g_tuiGlyph = g_tuiGlyph - 1
        g_tuiX = 0
        g_tuiY = 0
    ELSEIF k = "c" OR k = "C" THEN
        HistPush(g)
        ClearGlyph(g)
    ELSEIF k = "x" OR k = "X" THEN
        HistPush(g)
        FlipH(g)
    ELSEIF k = "y" OR k = "Y" THEN
        HistPush(g)
        FlipV(g)
    ELSEIF k = "b" OR k = "B" THEN
        HistPush(g)
        BearingRecompute(g)
    ELSEIF k = "u" OR k = "U" THEN
        DoUndo()
    ELSEIF k = "r" OR k = "R" THEN
        DoRedo()
    ELSE
        Fail("tasto TUI non riconosciuto: " + k)
    END IF
END SUB

FUNCTION TuiKeysPath() AS STRING
    IF STRINGS.STARTSWITH(g_tuiKeys, "/") THEN
        TuiKeysPath = g_tuiKeys
    ELSE
        TuiKeysPath = g_appDir + "/" + g_tuiKeys
    END IF
END FUNCTION

SUB RunTui()
    DIM lines
    DIM i AS LONG, k AS STRING, path AS STRING
    path = TuiKeysPath()
    IF NOT IO.FILEEXISTS(path) THEN Fail("script tasti assente: " + path)
    LoadBdf(BdfPath())
    g_tuiGlyph = 0
    g_tuiX = 0
    g_tuiY = 0
    PRINT "tui          : "; path
    PRINT TuiFrame()
    lines = IO.READLINES(path)
    FOR i = 0 TO UBOUND(lines)
        k = lines(i)
        IF LEN(k) > 0 THEN
            IF MID$(k, 1, 1) <> "#" THEN
                TuiApply(k)
                IF g_tuiRender = "1" THEN PRINT TuiFrame()
            END IF
        END IF
    NEXT i
    PRINT TuiFrame()
    EnsureOutDir()
    BdfSave(OutPath(g_tuiOut))
    PRINT "bdf          : "; OutPath(g_tuiOut); " ("; FileSizeOf(OutPath(g_tuiOut)); " byte)"
END SUB

' ============================================================================
' Serializzazione
' ============================================================================
FUNCTION NumS(n AS LONG) AS STRING
    NumS = STRINGS.TRIM(STR(n))
END FUNCTION

FUNCTION Pow2(k AS INTEGER) AS LONG
    DIM i AS INTEGER, v AS LONG
    v = 1
    FOR i = 1 TO k
        v = v * 2
    NEXT i
    Pow2 = v
END FUNCTION

FUNCTION Hex2(v AS INTEGER) AS STRING
    DIM d AS STRING, hi AS INTEGER, lo AS INTEGER
    d = "0123456789ABCDEF"
    hi = INT(v / 16)
    lo = v - hi * 16
    Hex2 = MID$(d, hi + 1, 1) + MID$(d, lo + 1, 1)
END FUNCTION

' Byte k (0-based, little endian) di un intero: pura aritmetica, nessun array
' come parametro (la mutazione di array BYTE passati a SUB non e' portabile).
FUNCTION ByteOf(v AS LONG, k AS INTEGER) AS INTEGER
    DIM t AS LONG, i AS INTEGER
    t = v
    FOR i = 1 TO k
        t = INT(t / 256)
    NEXT i
    ByteOf = t - INT(t / 256) * 256
END FUNCTION

SUB WText(h AS LONG, s AS STRING)
    IO.FWRITE(h, s + CHR$(10))
END SUB

' Proprieta' standard BDF 2.2 / X11 che i font reali dichiarano sempre: se il
' sorgente non le ha si aggiungono UNA volta in lettura, cosi' BDF e PCF le
' emettono e il round trip resta idempotente (la seconda lettura non aggiunge nulla).
SUB AddStdProps()
    DIM i AS INTEGER, hasRes AS INTEGER, hasAsc AS INTEGER, hasDsc AS INTEGER, hasDef AS INTEGER
    DIM up AS STRING, asc AS INTEGER, dsc AS INTEGER, def AS INTEGER, v AS INTEGER
    hasRes = 0
    hasAsc = 0
    hasDsc = 0
    hasDef = 0
    i = 0
    WHILE i < g_nProps
        up = STRINGS.UPPER(g_propName(i))
        IF up = "RESOLUTION_X" OR up = "RESOLUTION_Y" THEN hasRes = 1
        IF up = "FONT_ASCENT" THEN hasAsc = 1
        IF up = "FONT_DESCENT" THEN hasDsc = 1
        IF up = "DEFAULT_CHAR" THEN hasDef = 1
        i = i + 1
    WEND
    asc = 0
    dsc = 0
    i = 0
    WHILE i < g_nGlyphs
        v = g_gy(i) + g_gh(i)
        IF v > asc THEN asc = v
        v = 0
        IF g_gy(i) < 0 THEN v = -g_gy(i)
        IF v > dsc THEN dsc = v
        i = i + 1
    WEND
    def = g_enc(0)
    i = 0
    WHILE i < g_nGlyphs
        IF g_enc(i) = 32 THEN def = 32
        i = i + 1
    WEND
    IF hasRes = 0 THEN
        AddPropKV("RESOLUTION_X", NumS(g_xres))
        AddPropKV("RESOLUTION_Y", NumS(g_yres))
    END IF
    IF hasAsc = 0 THEN AddPropKV("FONT_ASCENT", NumS(asc))
    IF hasDsc = 0 THEN AddPropKV("FONT_DESCENT", NumS(dsc))
    IF hasDef = 0 THEN AddPropKV("DEFAULT_CHAR", NumS(def))
END SUB

SUB AddPropKV(name AS STRING, value AS STRING)
    IF g_nProps < GS_PROPS THEN
        g_propName(g_nProps) = name
        g_propValue(g_nProps) = value
        g_nProps = g_nProps + 1
    END IF
END SUB

FUNCTION PropInt(name AS STRING) AS INTEGER
    DIM i AS INTEGER
    PropInt = -1
    i = 0
    WHILE i < g_nProps
        IF STRINGS.UPPER(g_propName(i)) = STRINGS.UPPER(name) THEN PropInt = CINT(VAL(g_propValue(i)))
        i = i + 1
    WEND
END FUNCTION

SUB BdfSave(path AS STRING)
    DIM h AS LONG, g AS INTEGER, r AS INTEGER, c AS INTEGER, i AS INTEGER
    DIM nb AS INTEGER, v AS LONG, s AS STRING, row AS STRING
    h = IO.FCREATE(path)
    IF h < 0 THEN Fail("creazione impossibile: " + path)
    WText(h, "STARTFONT 2.1")
    WText(h, "FONT " + g_fontName)
    WText(h, "SIZE " + NumS(g_pointSize) + " " + NumS(g_xres) + " " + NumS(g_yres))
    WText(h, "FONTBOUNDINGBOX " + NumS(g_bbW) + " " + NumS(g_bbH) + " " + NumS(g_bbX) + " " + NumS(g_bbY))
    WText(h, "STARTPROPERTIES " + NumS(g_nProps))
    FOR i = 0 TO g_nProps - 1
        WText(h, g_propName(i) + " " + g_propValue(i))
    NEXT i
    WText(h, "ENDPROPERTIES")
    WText(h, "CHARS " + NumS(g_nGlyphs))
    FOR g = 0 TO g_nGlyphs - 1
        WText(h, "STARTCHAR " + g_gname(g))
        WText(h, "ENCODING " + NumS(g_enc(g)))
        WText(h, "SWIDTH " + NumS(g_swx(g)) + " 0")
        WText(h, "DWIDTH " + NumS(g_dwx(g)) + " 0")
        WText(h, "BBX " + NumS(g_gw(g)) + " " + NumS(g_gh(g)) + " " + NumS(g_gx(g)) + " " + NumS(g_gy(g)))
        WText(h, "BITMAP")
        nb = INT((g_gw(g) + 7) / 8)
        FOR r = 0 TO g_gh(g) - 1
            row = RowAt(g, r)
            s = ""
            FOR c = 0 TO nb - 1
                v = 0
                FOR i = 0 TO 7
                    IF c * 8 + i < LEN(row) THEN
                        IF MID$(row, c * 8 + i + 1, 1) = "1" THEN v = v + Pow2(7 - i)
                    END IF
                NEXT i
                s = s + Hex2(CINT(v))
            NEXT c
            WText(h, s)
        NEXT r
        WText(h, "ENDCHAR")
    NEXT g
    WText(h, "ENDFONT")
    IO.FCLOSE(h)
END SUB

' --- export: foglio BMP 24-bit (24 bit, righe dal basso, BGR) ----------------
SUB SheetBmp(path AS STRING)
    DIM maxW AS INTEGER, maxH AS INTEGER, g AS INTEGER
    DIM scale AS INTEGER, cols AS INTEGER, rowsN AS INTEGER
    DIM cellW AS INTEGER, cellH AS INTEGER, width AS INTEGER, height AS INTEGER
    DIM rb AS LONG, imgSize AS LONG, fileSize AS LONG, i AS LONG
    DIM x AS INTEGER, y AS INTEGER, sx AS INTEGER, sy AS INTEGER
    DIM col AS INTEGER, row AS INTEGER, x0 AS INTEGER, y0 AS INTEGER
    DIM px AS INTEGER, py AS INTEGER, idx AS LONG, n AS LONG
    maxW = 1
    maxH = 1
    FOR g = 0 TO g_nGlyphs - 1
        IF g_gw(g) > maxW THEN maxW = g_gw(g)
        IF g_gh(g) > maxH THEN maxH = g_gh(g)
    NEXT g
    scale = g_bmpScale
    IF scale < 1 THEN scale = 1
    cols = g_cellsPerRow
    IF cols < 1 THEN cols = 1
    IF cols > g_nGlyphs AND g_nGlyphs > 0 THEN cols = g_nGlyphs
    rowsN = INT((g_nGlyphs + cols - 1) / cols)
    IF rowsN < 1 THEN rowsN = 1
    cellW = (maxW + 2) * scale
    cellH = (maxH + 2) * scale
    width = cols * cellW
    height = rowsN * cellH
    rb = INT((width * 3 + 3) / 4) * 4
    imgSize = rb * height
    fileSize = 54 + imgSize

    DIM buf(fileSize - 1) AS BYTE
    FOR i = 0 TO fileSize - 1
        buf(i) = 255
    NEXT i
    buf(0) = 66
    buf(1) = 77
    buf(2) = ByteOf(fileSize, 0)
    buf(3) = ByteOf(fileSize, 1)
    buf(4) = ByteOf(fileSize, 2)
    buf(5) = ByteOf(fileSize, 3)
    ' campi che il riempimento bianco lascerebbe a 0xFF: azzerati per un BMP valido
    buf(6) = 0
    buf(7) = 0
    buf(8) = 0
    buf(9) = 0
    buf(10) = 54
    buf(11) = 0
    buf(12) = 0
    buf(13) = 0
    buf(14) = 40
    buf(15) = 0
    buf(16) = 0
    buf(17) = 0
    buf(18) = ByteOf(width, 0)
    buf(19) = ByteOf(width, 1)
    buf(20) = ByteOf(width, 2)
    buf(21) = ByteOf(width, 3)
    buf(22) = ByteOf(height, 0)
    buf(23) = ByteOf(height, 1)
    buf(24) = ByteOf(height, 2)
    buf(25) = ByteOf(height, 3)
    buf(26) = 1
    buf(27) = 0
    buf(28) = 24
    buf(29) = 0
    buf(30) = 0
    buf(31) = 0
    buf(32) = 0
    buf(33) = 0
    buf(34) = ByteOf(imgSize, 0)
    buf(35) = ByteOf(imgSize, 1)
    buf(36) = ByteOf(imgSize, 2)
    buf(37) = ByteOf(imgSize, 3)
    buf(38) = ByteOf(2835, 0)
    buf(39) = ByteOf(2835, 1)
    buf(40) = ByteOf(2835, 2)
    buf(41) = ByteOf(2835, 3)
    buf(42) = ByteOf(2835, 0)
    buf(43) = ByteOf(2835, 1)
    buf(44) = ByteOf(2835, 2)
    buf(45) = ByteOf(2835, 3)
    buf(46) = 0
    buf(47) = 0
    buf(48) = 0
    buf(49) = 0
    buf(50) = 0
    buf(51) = 0
    buf(52) = 0
    buf(53) = 0

    FOR g = 0 TO g_nGlyphs - 1
        col = g - INT(g / cols) * cols
        row = INT(g / cols)
        x0 = col * cellW + scale
        y0 = row * cellH + scale
        FOR py = 0 TO g_gh(g) - 1
            FOR px = 0 TO g_gw(g) - 1
                IF GetPx(g, px, py) = 1 THEN
                    FOR sy = 0 TO scale - 1
                        FOR sx = 0 TO scale - 1
                            idx = 54 + (height - 1 - (y0 + py * scale + sy)) * rb + (x0 + px * scale + sx) * 3
                            buf(idx) = 0
                            buf(idx + 1) = 0
                            buf(idx + 2) = 0
                        NEXT sx
                    NEXT sy
                END IF
            NEXT px
        NEXT py
    NEXT g
    n = IO.WRITEBYTES(path, buf)
    IF n <> fileSize THEN Fail("scrittura BMP incompleta: " + path)
END SUB

' --- export: PBM P1 ASCII (stesso layout del foglio, 1 pixel = 1 carattere) --
SUB PbmSave(path AS STRING)
    DIM maxW AS INTEGER, maxH AS INTEGER, g AS INTEGER
    DIM cols AS INTEGER, rowsN AS INTEGER, width AS INTEGER, height AS INTEGER
    DIM x AS INTEGER, y AS INTEGER, col AS INTEGER, row AS INTEGER
    DIM x0 AS INTEGER, y0 AS INTEGER, h AS LONG, s AS STRING
    maxW = 1
    maxH = 1
    FOR g = 0 TO g_nGlyphs - 1
        IF g_gw(g) > maxW THEN maxW = g_gw(g)
        IF g_gh(g) > maxH THEN maxH = g_gh(g)
    NEXT g
    cols = g_cellsPerRow
    IF cols < 1 THEN cols = 1
    IF cols > g_nGlyphs AND g_nGlyphs > 0 THEN cols = g_nGlyphs
    rowsN = INT((g_nGlyphs + cols - 1) / cols)
    IF rowsN < 1 THEN rowsN = 1
    width = cols * (maxW + 2)
    height = rowsN * (maxH + 2)
    h = IO.FCREATE(path)
    WText(h, "P1")
    WText(h, NumS(width) + " " + NumS(height))
    FOR y = 0 TO height - 1
        s = ""
        FOR x = 0 TO width - 1
            s = s + PixelOfSheet(x, y, maxW, maxH, cols) + " "
        NEXT x
        WText(h, s)
    NEXT y
    IO.FCLOSE(h)
END SUB

FUNCTION PixelOfSheet(x AS INTEGER, y AS INTEGER, maxW AS INTEGER, maxH AS INTEGER, cols AS INTEGER) AS STRING
    DIM col AS INTEGER, row AS INTEGER, gx AS INTEGER, gy AS INTEGER, g AS INTEGER
    g = -1
    col = INT(x / (maxW + 2))
    row = INT(y / (maxH + 2))
    gx = x - col * (maxW + 2) - 1
    gy = y - row * (maxH + 2) - 1
    g = row * cols + col
    PixelOfSheet = "0"
    IF g >= 0 AND g < g_nGlyphs THEN
        IF GetPx(g, gx, gy) = 1 THEN PixelOfSheet = "1"
    END IF
END FUNCTION

' --- export: PSF2 (font console Linux) --------------------------------------
SUB Psf2Save(path AS STRING)
    DIM maxW AS INTEGER, maxH AS INTEGER, g AS INTEGER
    DIM bpr AS INTEGER, charSize AS INTEGER, total AS LONG, i AS LONG
    DIM r AS INTEGER, c AS INTEGER, v AS LONG, bit AS INTEGER, idx AS LONG, n AS LONG
    maxW = 1
    maxH = 1
    FOR g = 0 TO g_nGlyphs - 1
        IF g_gw(g) > maxW THEN maxW = g_gw(g)
        IF g_gh(g) > maxH THEN maxH = g_gh(g)
    NEXT g
    bpr = INT((maxW + 7) / 8)
    charSize = bpr * maxH
    total = 32 + g_nGlyphs * charSize
    DIM buf(total - 1) AS BYTE
    FOR i = 0 TO total - 1
        buf(i) = 0
    NEXT i
    ' magic 0x72b54a86 in little endian
    buf(0) = 134
    buf(1) = 74
    buf(2) = 181
    buf(3) = 114
    buf(7) = ByteOf(g_nGlyphs, 0)
    buf(8) = ByteOf(g_nGlyphs, 1)
    buf(9) = ByteOf(g_nGlyphs, 2)
    buf(10) = ByteOf(g_nGlyphs, 3)
    buf(11) = ByteOf(charSize, 0)
    buf(12) = ByteOf(charSize, 1)
    buf(13) = ByteOf(charSize, 2)
    buf(14) = ByteOf(charSize, 3)
    buf(15) = ByteOf(maxH, 0)
    buf(16) = ByteOf(maxH, 1)
    buf(17) = ByteOf(maxH, 2)
    buf(18) = ByteOf(maxH, 3)
    buf(19) = ByteOf(maxW, 0)
    buf(20) = ByteOf(maxW, 1)
    buf(21) = ByteOf(maxW, 2)
    buf(22) = ByteOf(maxW, 3)
    FOR g = 0 TO g_nGlyphs - 1
        FOR r = 0 TO maxH - 1
            FOR c = 0 TO bpr - 1
                v = 0
                FOR bit = 0 TO 7
                    IF GetPx(g, c * 8 + bit, r) = 1 THEN v = v + Pow2(7 - bit)
                NEXT bit
                idx = 32 + g * charSize + r * bpr + c
                buf(idx) = CINT(v)
            NEXT c
        NEXT r
    NEXT g
    n = IO.WRITEBYTES(path, buf)
    IF n <> total THEN Fail("scrittura PSF2 incompleta: " + path)
END SUB

' --- export: header C --------------------------------------------------------
SUB HeaderC(path AS STRING)
    DIM h AS LONG, g AS INTEGER, r AS INTEGER, c AS INTEGER, i AS INTEGER
    DIM nb AS INTEGER, v AS LONG, s AS STRING, row AS STRING
    h = IO.FCREATE(path)
    IF h < 0 THEN Fail("creazione impossibile: " + path)
    WText(h, "/* generato da GlyphSmith: " + g_fontName + " */")
    WText(h, "#ifndef GLYPHSMITH_MINI8_H")
    WText(h, "#define GLYPHSMITH_MINI8_H")
    WText(h, "#define GS_GLYPHS " + NumS(g_nGlyphs))
    WText(h, "#define GS_HEIGHT " + NumS(g_bbH))
    FOR g = 0 TO g_nGlyphs - 1
        WText(h, "")
        WText(h, "/* U+" + Hex2(0) + NumS(g_enc(g)) + " */")
        WText(h, "static const unsigned char glyph_" + NumS(g_enc(g)) + "[] = {")
        nb = INT((g_gw(g) + 7) / 8)
        FOR r = 0 TO g_gh(g) - 1
            row = RowAt(g, r)
            s = "    "
            FOR c = 0 TO nb - 1
                v = 0
                FOR i = 0 TO 7
                    IF c * 8 + i < LEN(row) THEN
                        IF MID$(row, c * 8 + i + 1, 1) = "1" THEN v = v + Pow2(7 - i)
                    END IF
                NEXT i
                s = s + "0x" + Hex2(CINT(v)) + ", "
            NEXT c
            WText(h, s)
        NEXT r
        WText(h, "};")
    NEXT g
    WText(h, "")
    WText(h, "#endif")
    IO.FCLOSE(h)
END SUB

' --- export: PSF1 (magic 0x36 0x04, 256 slot da `charsize` byte, 8 px/riga) ----
' PSF1 non ha header con altezza: la dimensione la decide il chiamante. Qui il charsize
' e' l'altezza del FONTBOUNDINGBOX (o del glifo piu' alto) e ogni riga sta in un byte,
' MSB = pixel x=0. I glifi piu' larghi di 8 px sono un errore dichiarato, non un troncamento.
SUB Psf1Save(path AS STRING)
    DIM hmax AS INTEGER, g AS INTEGER, r AS INTEGER, x AS INTEGER
    DIM total AS LONG, i AS LONG, cs AS INTEGER, slot AS INTEGER
    DIM b AS INTEGER, row AS STRING, n AS LONG
    hmax = g_bbH
    IF hmax < 1 THEN hmax = 1
    FOR g = 0 TO g_nGlyphs - 1
        IF g_gh(g) > hmax THEN hmax = g_gh(g)
        IF g_gw(g) > 8 THEN Fail("PSF1: glifo piu' largo di 8 px (encoding " + NumS(g_enc(g)) + ")")
    NEXT g
    cs = hmax
    total = 4 + 256 * cs
    DIM buf(total - 1) AS BYTE
    FOR i = 0 TO total - 1
        buf(i) = 0
    NEXT i
    buf(0) = 54
    buf(1) = 4
    buf(2) = 0
    buf(3) = ByteOf(cs, 0)
    FOR g = 0 TO g_nGlyphs - 1
        slot = g_enc(g)
        IF slot >= 0 THEN
            IF slot < 256 THEN
                FOR r = 0 TO g_gh(g) - 1
                    IF r < cs THEN
                        row = RowAt(g, r)
                        b = 0
                        FOR x = 0 TO 7
                            IF x < LEN(row) THEN
                                IF MID$(row, x + 1, 1) = "1" THEN b = b + Pow2(7 - x)
                            END IF
                        NEXT x
                        buf(4 + slot * cs + r) = b
                    END IF
                NEXT r
            END IF
        END IF
    NEXT g
    n = IO.WRITEBYTES(path, buf)
    IF n <> total THEN Fail("scrittura PSF1 incompleta: " + path)
END SUB

' --- export: ASCII art (un blocco per glifo: '#' acceso, '.' spento) --------
SUB AsciiSave(path AS STRING)
    DIM h AS LONG, g AS INTEGER, r AS INTEGER, x AS INTEGER, row AS STRING, s AS STRING
    h = IO.FCREATE(path)
    IF h < 0 THEN Fail("creazione impossibile: " + path)
    WText(h, "GS-ASCII 1")
    WText(h, "FONT " + g_fontName)
    WText(h, "SIZE " + NumS(g_bbW) + " " + NumS(g_bbH))
    FOR g = 0 TO g_nGlyphs - 1
        WText(h, "@ " + NumS(g_enc(g)) + " " + g_gname(g) + " " + NumS(g_gw(g)) + " " + NumS(g_gh(g)))
        FOR r = 0 TO g_gh(g) - 1
            row = RowAt(g, r)
            s = ""
            FOR x = 0 TO g_gw(g) - 1
                IF x < LEN(row) THEN
                    IF MID$(row, x + 1, 1) = "1" THEN
                        s = s + "#"
                    ELSE
                        s = s + "."
                    END IF
                ELSE
                    s = s + "."
                END IF
            NEXT x
            WText(h, s)
        NEXT r
    NEXT g
    IO.FCLOSE(h)
END SUB

' --- export: anteprima sixel (un carattere per colonna, bande da 6 righe) ---
' Stream DEC: ESC P q " 1;1;<w>;<h>  poi per ogni banda "#0" + w caratteri (63+bit)
' e un '-' di fine banda, chiuso da ESC \\. Nessuna palette: il colore 0 e' quello
' di default del terminale. Il file e' byte-esatto, quindi il check lo rilegge.
SUB SixelSave(g AS INTEGER, path AS STRING)
    DIM w AS INTEGER, h AS INTEGER, bands AS INTEGER, band AS INTEGER
    DIM x AS INTEGER, k AS INTEGER, r AS INTEGER, v AS INTEGER, s AS STRING
    DIM i AS LONG, n AS LONG
    IF g < 0 THEN Fail("sixel: glifo fuori limite")
    IF g >= GS_GMAX THEN Fail("sixel: glifo fuori limite")
    w = g_gw(g)
    h = g_gh(g)
    IF w < 1 THEN Fail("sixel: glifo vuoto")
    IF h < 1 THEN Fail("sixel: glifo vuoto")
    bands = INT((h + 5) / 6)
    s = CHR$(27) + "Pq" + CHR$(34) + "1;1;" + NumS(w) + ";" + NumS(h)
    FOR band = 0 TO bands - 1
        s = s + "#0"
        FOR x = 0 TO w - 1
            v = 0
            FOR k = 0 TO 5
                r = band * 6 + k
                IF r < h THEN
                    IF GetPx(g, x, r) = 1 THEN v = v + Pow2(k)
                END IF
            NEXT k
            s = s + CHR$(63 + v)
        NEXT x
        s = s + "-"
    NEXT band
    s = s + CHR$(27) + CHR$(92)
    DIM buf(LEN(s) - 1) AS BYTE
    FOR i = 1 TO LEN(s)
        buf(i - 1) = ASC(MID$(s, i, 1))
    NEXT i
    n = IO.WRITEBYTES(path, buf)
    IF n <> LEN(s) THEN Fail("scrittura sixel incompleta: " + path)
END SUB

' Rilettura della sixel prodotta sopra: decodifica le bande e ricostruisce la griglia.
SUB SixelLoad(g AS INTEGER, path AS STRING)
    DIM raw()
    DIM w AS INTEGER, h AS INTEGER, bands AS INTEGER, band AS INTEGER
    DIM x AS INTEGER, k AS INTEGER, j AS LONG, i AS LONG, last AS LONG, v AS INTEGER
    IF NOT IO.FILEEXISTS(path) THEN Fail("sixel assente: " + path)
    raw = IO.READBYTES(path)
    last = UBOUND(raw)
    w = g_gw(g)
    h = g_gh(g)
    IF w < 1 THEN Fail("sixel: glifo vuoto")
    IF h < 1 THEN Fail("sixel: glifo vuoto")
    bands = INT((h + 5) / 6)
    i = -1
    FOR j = 0 TO last
        IF i < 0 THEN
            IF raw(j) = 35 THEN i = j
        END IF
    NEXT j
    IF i < 0 THEN Fail("sixel: nessun dato nel file")
    ClearGlyph(g)
    FOR band = 0 TO bands - 1
        IF i + 1 > last THEN Fail("sixel: stream troncato")
        IF raw(i) <> 35 THEN Fail("sixel: atteso selettore di colore")
        IF raw(i + 1) <> 48 THEN Fail("sixel: selettore diverso da 0")
        i = i + 2
        FOR x = 0 TO w - 1
            IF i > last THEN Fail("sixel: banda troncata")
            v = raw(i) - 63
            IF v < 0 THEN Fail("sixel: carattere fuori intervallo")
            IF v > 63 THEN Fail("sixel: carattere fuori intervallo")
            FOR k = 0 TO 5
                IF INT(v / Pow2(k)) - INT(v / Pow2(k + 1)) * 2 = 1 THEN
                    SetPx(g, x, band * 6 + k, "1")
                END IF
            NEXT k
            i = i + 1
        NEXT x
        IF i <= last THEN
            IF raw(i) = 45 THEN i = i + 1
        END IF
    NEXT band
END SUB

' --- export: PCF (X11 Portable Compiled Format) ------------------------------
' File validato con FreeType (la faccia si apre, il charmap e' Unicode e i bitmap
' coincidono con il BDF di partenza).
' Header e campi "format" sono little-endian; i campi DENTRO le tabelle sono
' big-endian (convenzione del PCF reale: il formato 0xE dichiara byte order MSB,
' bit LSB-first, righe paddate a 4 byte). Tabelle: PROPERTIES(1), ACCELERATORS(2),
' METRICS(4), BITMAPS(8), BDF_ENCODINGS(32); gli acceleratori sono obbligatori.
' Il file si assembla come stringa ESADECIMALE e si converte in byte alla fine:
' CHR$(0) in questo dialetto e' la stringa vuota, e un array BYTE passato BYREF
' non riceve le scritture sulle gambe gestite (lo dimostra il check su PCode).
' HexVal(ch) esiste gia' nell'app (una cifra esadecimale -> 0..15).
FUNCTION H1(v AS INTEGER) AS STRING
    DIM d AS STRING, x AS INTEGER
    d = "0123456789abcdef"
    x = v - INT(v / 16) * 16
    H1 = MID$(d, x + 1, 1)
END FUNCTION

FUNCTION HB(v AS INTEGER) AS STRING
    DIM x AS INTEGER, r AS STRING
    x = v - INT(v / 256) * 256
    r = H1(INT(x / 16)) + H1(x)
    HB = r
END FUNCTION

FUNCTION H8(v AS LONG) AS STRING
    DIM x AS LONG, r AS STRING
    x = v
    IF x < 0 THEN x = x + 256
    r = HB(x)
    H8 = r
END FUNCTION

FUNCTION HLE16(v AS LONG) AS STRING
    DIM x AS LONG, r AS STRING
    x = v
    IF x < 0 THEN x = x + 65536
    r = HB(x - INT(x / 256) * 256) + HB(INT(x / 256))
    HLE16 = r
END FUNCTION

FUNCTION HLE32(v AS LONG) AS STRING
    DIM x AS LONG, b0 AS INTEGER, b1 AS INTEGER, b2 AS INTEGER, b3 AS INTEGER, r AS STRING
    x = v
    IF x < 0 THEN x = x + 4294967296
    b0 = x - INT(x / 256) * 256
    x = INT(x / 256)
    b1 = x - INT(x / 256) * 256
    x = INT(x / 256)
    b2 = x - INT(x / 256) * 256
    x = INT(x / 256)
    b3 = x
    r = HB(b0) + HB(b1) + HB(b2) + HB(b3)
    HLE32 = r
END FUNCTION

FUNCTION HBE16(v AS LONG) AS STRING
    DIM x AS LONG, r AS STRING
    x = v
    IF x < 0 THEN x = x + 65536
    r = HB(INT(x / 256)) + HB(x - INT(x / 256) * 256)
    HBE16 = r
END FUNCTION

FUNCTION HBE32(v AS LONG) AS STRING
    DIM x AS LONG, b0 AS INTEGER, b1 AS INTEGER, b2 AS INTEGER, b3 AS INTEGER, r AS STRING
    x = v
    IF x < 0 THEN x = x + 4294967296
    b3 = x - INT(x / 256) * 256
    x = INT(x / 256)
    b2 = x - INT(x / 256) * 256
    x = INT(x / 256)
    b1 = x - INT(x / 256) * 256
    x = INT(x / 256)
    b0 = x
    r = HB(b0) + HB(b1) + HB(b2) + HB(b3)
    HBE32 = r
END FUNCTION

' testo + NUL finale, come stringa esadecimale
FUNCTION HText(t AS STRING) AS STRING
    DIM x AS INTEGER, s AS STRING
    s = ""
    FOR x = 1 TO LEN(t)
        s = s + HB(ASC(MID$(t, x, 1)))
    NEXT x
    HText = s + "00"
END FUNCTION

FUNCTION Align4(v AS LONG) AS LONG
    Align4 = INT((v + 3) / 4) * 4
END FUNCTION

SUB PcfSave(path AS STRING)
    DIM g AS INTEGER, r AS INTEGER, c AS INTEGER, x AS INTEGER, i AS INTEGER
    DIM nb AS INTEGER, pb AS INTEGER, v AS INTEGER, row AS STRING, ch AS STRING
    DIM nNameOff(64) AS LONG, nValOff(64) AS LONG
    DIM propHdr AS LONG, strStart AS LONG, strTotal AS LONG, pos AS LONG, pad AS LONG
    DIM propSize AS LONG, accSize AS LONG, metSize AS LONG, bitHdr AS LONG, dataStart AS LONG
    DIM dataSize AS LONG, bitSize AS LONG, encSize AS LONG
    DIM offProp AS LONG, offAcc AS LONG, offMet AS LONG, offBit AS LONG, offEnc AS LONG
    DIM total AS LONG, offNow AS LONG, sz AS LONG
    DIM minEnc AS INTEGER, maxEnc AS INTEGER
    DIM n AS LONG, s1 AS LONG, s2 AS LONG, s4 AS LONG, s8 AS LONG
    DIM ascMax AS INTEGER, dscMax AS INTEGER, asc AS INTEGER, dsc AS INTEGER
    DIM minLs AS INTEGER, maxRs AS INTEGER, maxAdv AS INTEGER
    DIM minAs AS INTEGER, maxAs AS INTEGER, minDs AS INTEGER, maxDs AS INTEGER
    DIM up AS STRING, vv AS STRING
    DIM nP AS INTEGER, hasReg AS INTEGER, hasEnc AS INTEGER
    DIM s AS STRING
    IF g_nGlyphs < 1 THEN Fail("PCF: nessun glifo")
    IF g_nProps > 62 THEN Fail("PCF: troppe proprieta' (" + NumS(g_nProps) + ")")
    n = g_nGlyphs

    ' Proprieta' del PCF: quelle del modello senza virgolette, piu' il charset se manca
    ' (senza CHARSET_REGISTRY FreeType apre la faccia ma senza charmap).
    hasReg = 0
    hasEnc = 0
    nP = 0
    FOR i = 0 TO g_nProps - 1
        g_pcfN(nP) = g_propName(i)
        vv = g_propValue(i)
        IF LEN(vv) >= 2 THEN
            IF MID$(vv, 1, 1) = CHR$(34) THEN
                IF MID$(vv, LEN(vv), 1) = CHR$(34) THEN vv = MID$(vv, 2, LEN(vv) - 2)
            END IF
        END IF
        g_pcfV(nP) = vv
        up = STRINGS.UPPER(g_pcfN(nP))
        IF up = "CHARSET_REGISTRY" THEN hasReg = 1
        IF up = "CHARSET_ENCODING" THEN hasEnc = 1
        nP = nP + 1
    NEXT i
    IF hasReg = 0 THEN
        g_pcfN(nP) = "CHARSET_REGISTRY"
        g_pcfV(nP) = "ISO8859"
        nP = nP + 1
    END IF
    IF hasEnc = 0 THEN
        g_pcfN(nP) = "CHARSET_ENCODING"
        g_pcfV(nP) = "1"
        nP = nP + 1
    END IF

    pos = 0
    FOR i = 0 TO nP - 1
        nNameOff(i) = pos
        pos = pos + LEN(g_pcfN(i)) + 1
        nValOff(i) = pos
        pos = pos + LEN(g_pcfV(i)) + 1
    NEXT i
    strTotal = pos
    propHdr = 8 + nP * 9
    pad = 4 - (nP - INT(nP / 4) * 4)
    IF pad = 4 THEN pad = 0
    strStart = propHdr + pad + 4
    propSize = strStart + strTotal
    accSize = 48

    metSize = 8 + 12 * n
    bitHdr = 8 + 4 * n + 16
    dataStart = Align4(bitHdr)
    dataSize = 0
    s1 = 0
    s2 = 0
    s4 = 0
    s8 = 0
    FOR g = 0 TO n - 1
        nb = INT((g_gw(g) + 7) / 8)
        pb = INT((nb + 3) / 4) * 4
        dataSize = dataSize + g_gh(g) * pb
        s1 = s1 + g_gh(g) * nb
        s2 = s2 + g_gh(g) * (INT((nb + 1) / 2) * 2)
        s4 = s4 + g_gh(g) * pb
        s8 = s8 + g_gh(g) * (INT((nb + 7) / 8) * 8)
    NEXT g
    bitSize = dataStart + dataSize

    minEnc = g_enc(0)
    maxEnc = g_enc(0)
    FOR g = 1 TO n - 1
        IF g_enc(g) < minEnc THEN minEnc = g_enc(g)
        IF g_enc(g) > maxEnc THEN maxEnc = g_enc(g)
    NEXT g
    IF minEnc < 0 OR maxEnc > 255 THEN Fail("PCF: encoding fuori da 0..255")
    encSize = 14 + 2 * (maxEnc - minEnc + 1)

    minLs = g_gx(0)
    maxRs = g_gx(0) + g_gw(0)
    maxAdv = g_dwx(0)
    asc = g_gh(0) + g_gy(0)
    dsc = 0
    IF g_gy(0) < 0 THEN dsc = -g_gy(0)
    minAs = asc
    maxAs = asc
    minDs = dsc
    maxDs = dsc
    FOR g = 1 TO n - 1
        IF g_gx(g) < minLs THEN minLs = g_gx(g)
        IF g_gx(g) + g_gw(g) > maxRs THEN maxRs = g_gx(g) + g_gw(g)
        IF g_dwx(g) > maxAdv THEN maxAdv = g_dwx(g)
        asc = g_gh(g) + g_gy(g)
        dsc = 0
        IF g_gy(g) < 0 THEN dsc = -g_gy(g)
        IF asc < minAs THEN minAs = asc
        IF asc > maxAs THEN maxAs = asc
        IF dsc < minDs THEN minDs = dsc
        IF dsc > maxDs THEN maxDs = dsc
    NEXT g
    ascMax = maxAs
    dscMax = maxDs

    total = 88 + propSize + accSize + metSize + bitSize + encSize
    IF total > 262144 THEN Fail("PCF: file troppo grande (" + NumS(total) + ")")
    offProp = 88
    offAcc = offProp + propSize
    offMet = offAcc + accSize
    offBit = offMet + metSize
    offEnc = offBit + bitSize

    ' --- header (little-endian) -----------------------------------------------
    s = "01" + "66" + "63" + "70" + HLE32(5)
    s = s + HLE32(1) + HLE32(14) + HLE32(propSize) + HLE32(offProp)
    s = s + HLE32(2) + HLE32(14) + HLE32(accSize) + HLE32(offAcc)
    s = s + HLE32(4) + HLE32(14) + HLE32(metSize) + HLE32(offMet)
    s = s + HLE32(8) + HLE32(14) + HLE32(bitSize) + HLE32(offBit)
    s = s + HLE32(32) + HLE32(14) + HLE32(encSize) + HLE32(offEnc)

    ' --- PROPERTIES ------------------------------------------------------------
    s = s + HLE32(14) + HBE32(nP)
    FOR i = 0 TO nP - 1
        s = s + HBE32(nNameOff(i)) + "01" + HBE32(nValOff(i))
    NEXT i
    FOR i = 1 TO pad
        s = s + "00"
    NEXT i
    s = s + HBE32(strTotal)
    FOR i = 0 TO nP - 1
        s = s + HText(g_pcfN(i)) + HText(g_pcfV(i))
    NEXT i

    ' --- ACCELERATORS ----------------------------------------------------------
    s = s + HLE32(14) + "0000000000000000"
    s = s + HBE32(ascMax) + HBE32(dscMax) + HBE32(0)
    s = s + HBE16(minLs) + HBE16(maxRs) + HBE16(maxAdv) + HBE16(minAs) + HBE16(minDs) + HBE16(0)
    s = s + HBE16(minLs) + HBE16(maxRs) + HBE16(maxAdv) + HBE16(maxAs) + HBE16(maxDs) + HBE16(0)

    ' --- METRICS ---------------------------------------------------------------
    s = s + HLE32(14) + HBE32(n)
    FOR g = 0 TO n - 1
        asc = g_gh(g) + g_gy(g)
        dsc = 0
        IF g_gy(g) < 0 THEN dsc = -g_gy(g)
        s = s + HBE16(g_gx(g)) + HBE16(g_gx(g) + g_gw(g)) + HBE16(g_dwx(g))
        s = s + HBE16(asc) + HBE16(dsc) + HBE16(0)
    NEXT g

    ' --- BITMAPS ---------------------------------------------------------------
    s = s + HLE32(14) + HBE32(n)
    offNow = 0
    FOR g = 0 TO n - 1
        nb = INT((g_gw(g) + 7) / 8)
        pb = INT((nb + 3) / 4) * 4
        s = s + HBE32(offNow)
        offNow = offNow + g_gh(g) * pb
    NEXT g
    s = s + HBE32(s1) + HBE32(s2) + HBE32(s4) + HBE32(s8)
    FOR i = 8 + 4 * n + 16 TO dataStart - 1
        s = s + "00"
    NEXT i
    FOR g = 0 TO n - 1
        nb = INT((g_gw(g) + 7) / 8)
        pb = INT((nb + 3) / 4) * 4
        FOR r = 0 TO g_gh(g) - 1
            row = RowAt(g, r)
            FOR c = 0 TO nb - 1
                v = 0
                FOR i = 0 TO 7
                    IF c * 8 + i < LEN(row) THEN
                        IF MID$(row, c * 8 + i + 1, 1) = "1" THEN v = v + Pow2(7 - i)
                    END IF
                NEXT i
                s = s + HB(v)
            NEXT c
            FOR c = nb TO pb - 1
                s = s + "00"
            NEXT c
        NEXT r
    NEXT g

    ' --- BDF_ENCODINGS ---------------------------------------------------------
    s = s + HLE32(14)
    s = s + HBE16(minEnc) + HBE16(maxEnc) + HBE16(0) + HBE16(0) + HBE16(0)
    FOR x = minEnc TO maxEnc
        v = 65535
        FOR g = 0 TO n - 1
            IF g_enc(g) = x THEN v = g
        NEXT g
        s = s + HBE16(v)
    NEXT x

    IF LEN(s) <> total * 2 THEN Fail("PCF: lunghezza interna incoerente (" + NumS(LEN(s) / 2) + " vs " + NumS(total) + "; prop " + NumS(propSize) + " acc " + NumS(accSize) + " met " + NumS(metSize) + " bit " + NumS(bitSize) + " enc " + NumS(encSize) + " nP " + NumS(nP) + ")")
    DIM buf(total - 1) AS BYTE
    FOR i = 0 TO total - 1
        buf(i) = HexVal(MID$(s, i * 2 + 1, 1)) * 16 + HexVal(MID$(s, i * 2 + 2, 1))
    NEXT i
    sz = IO.WRITEBYTES(path, buf)
    IF sz <> total THEN Fail("scrittura PCF incompleta: " + path)
END SUB

' --- import: ASCII art (stesso formato di AsciiSave) ------------------------
FUNCTION TextPath() AS STRING
    IF STRINGS.STARTSWITH(g_textSrc, "/") THEN
        TextPath = g_textSrc
    ELSE
        TextPath = g_appDir + "/" + g_textSrc
    END IF
END FUNCTION

FUNCTION BmpInPath() AS STRING
    IF STRINGS.STARTSWITH(g_bmpinSrc, "/") THEN
        BmpInPath = g_bmpinSrc
    ELSE
        BmpInPath = g_appDir + "/" + g_bmpinSrc
    END IF
END FUNCTION

SUB TextLoad(path AS STRING)
    DIM lines
    DIM raw AS STRING, s AS STRING
    DIM i AS LONG, g AS INTEGER, r AS INTEGER, x AS INTEGER
    DIM enc AS LONG, w AS INTEGER, hh AS INTEGER
    IF NOT IO.FILEEXISTS(path) THEN Fail("bitmap testuale assente: " + path)
    StartModel()
    lines = IO.READLINES(path)
    g = -1
    r = 0
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 THEN
            IF STRINGS.STARTSWITH(raw, "@ ") THEN
                enc = CLNG(VAL(TokAt(raw, 1)))
                w = CINT(VAL(TokAt(raw, 3)))
                hh = CINT(VAL(TokAt(raw, 4)))
                g = NewGlyph(TokAt(raw, 2))
                g_enc(g) = enc
                g_gw(g) = w
                g_gh(g) = hh
                g_dwx(g) = w
                g_swx(g) = w * 62
                g_nRows(g) = hh
                r = 0
                IF w > g_bbW THEN g_bbW = w
                IF hh > g_bbH THEN g_bbH = hh
            ELSEIF STRINGS.STARTSWITH(raw, "SIZE ") THEN
                g_pointSize = CINT(VAL(TokAt(raw, 2)))
            ELSEIF STRINGS.STARTSWITH(raw, "FONT ") THEN
                g_fontName = RestAfter(raw)
            ELSEIF g >= 0 THEN
                IF r < GS_ROWS THEN
                    s = ""
                    FOR x = 0 TO LEN(raw) - 1
                        IF MID$(raw, x + 1, 1) = "#" THEN
                            s = s + "1"
                        ELSE
                            s = s + "0"
                        END IF
                    NEXT x
                    g_row(g * GS_ROWS + r) = s
                    r = r + 1
                END IF
            END IF
        END IF
    NEXT i
    IF g_nGlyphs = 0 THEN Fail("bitmap testuale senza glifi: " + path)
    g_declChars = g_nGlyphs
END SUB

SUB RunTextIn()
    DIM src AS STRING
    src = TextPath()
    TextLoad(src)
    EnsureOutDir()
    BdfSave(OutPath(g_textOut))
    PRINT "bitmap testo : "; src
    PRINT "glifi        : "; g_nGlyphs
    PRINT "bounding box : "; g_bbW; "x"; g_bbH
    PRINT "bdf          : "; OutPath(g_textOut); " ("; FileSizeOf(OutPath(g_textOut)); " byte)"
END SUB

' --- import: foglio BMP 24-bit (foglio di contatto o griglia di celle) ------
' Legge il BMP scritto da SheetBmp (o una griglia compatibile): ogni cella e'
' campionata a blocchi scale x scale (inchiostro = almeno un pixel non bianco nel
' blocco), i margini bianchi vengono ritagliati e ogni cella NON vuota diventa un
' glifo, con encoding progressivo da [bmpin] first. Il foglio non registra le
' metriche per glifo: larghezza/altezza sono quindi quelle dell'inchiostro.
SUB BmpLoad(path AS STRING)
    DIM raw()
    DIM boff AS LONG, w AS LONG, h AS LONG, comp AS LONG, rb AS LONG
    DIM bpp AS INTEGER, scale AS INTEGER, cols AS INTEGER
    DIM cellW AS INTEGER, cellH AS INTEGER, rowsN AS INTEGER
    DIM r AS INTEGER, c AS INTEGER, px AS INTEGER, py AS INTEGER
    DIM sx AS INTEGER, sy AS INTEGER, x0 AS INTEGER, y0 AS INTEGER
    DIM xa AS LONG, ya AS LONG, ir AS LONG, isInk AS INTEGER
    DIM gw AS INTEGER, gh AS INTEGER, nrow AS INTEGER
    DIM row AS STRING, p AS INTEGER, minX AS INTEGER, maxX AS INTEGER
    DIM minY AS INTEGER, maxY AS INTEGER, bx AS INTEGER, by AS INTEGER
    DIM bw2 AS INTEGER, bh2 AS INTEGER, trim AS INTEGER
    DIM idx AS INTEGER, enc AS LONG, anyInk AS INTEGER
    IF NOT IO.FILEEXISTS(path) THEN Fail("BMP da importare assente: " + path)
    raw = IO.READBYTES(path)
    IF LEN(raw) < 54 THEN Fail("BMP troppo corto: " + path)
    IF raw(0) <> 66 THEN Fail("BMP: firma 'B' assente: " + path)
    IF raw(1) <> 77 THEN Fail("BMP: firma 'M' assente: " + path)
    boff = raw(10) + raw(11) * 256 + raw(12) * 65536 + raw(13) * 16777216
    w = raw(18) + raw(19) * 256 + raw(20) * 65536 + raw(21) * 16777216
    h = raw(22) + raw(23) * 256 + raw(24) * 65536 + raw(25) * 16777216
    bpp = raw(28)
    comp = raw(30) + raw(31) * 256 + raw(32) * 65536 + raw(33) * 16777216
    IF bpp <> 24 THEN Fail("BMP non supportato: " + NumS(bpp) + " bit (attesi 24)")
    IF comp <> 0 THEN Fail("BMP compresso non supportato: compression " + NumS(comp))
    IF w < 1 THEN Fail("BMP: larghezza non valida")
    IF h < 1 THEN Fail("BMP: altezza non valida (BMP top-down non supportato)")
    rb = INT((w * 3 + 3) / 4) * 4
    IF boff + rb * h > LEN(raw) THEN Fail("BMP troncato: attesi " + NumS(boff + rb * h) + " byte, letti " + NumS(LEN(raw)))

    scale = g_bmpinScale
    IF scale < 1 THEN scale = 1
    cellW = g_bmpinCellW
    cellH = g_bmpinCellH
    cols = g_bmpinCols
    IF cellW < 1 THEN
        IF cols < 1 THEN Fail("BMP: serve [bmpin] cellw oppure cols")
        cellW = INT(w / cols)
    END IF
    IF cols < 1 THEN cols = INT(w / cellW)
    IF cols < 1 THEN Fail("BMP: nessuna colonna di celle (larghezza " + NumS(w) + ", cellw " + NumS(cellW) + ")")
    IF cellH < 1 THEN cellH = cellW
    rowsN = INT(h / cellH)
    IF rowsN < 1 THEN Fail("BMP: nessuna riga di celle (altezza " + NumS(h) + ", cellh " + NumS(cellH) + ")")
    trim = 0
    IF g_bmpinTrim = "1" THEN trim = 1

    StartModel()
    ' il foglio non porta il corpo del font: si usa l'altezza della cella in pixel
    ' del glifo (cella / scale), cosi' SIZE e BBX restano coerenti con l'import.
    g_pointSize = INT(cellH / scale)
    enc = g_bmpinFirst
    FOR r = 0 TO rowsN - 1
        FOR c = 0 TO cols - 1
            x0 = c * cellW
            y0 = r * cellH
            gw = INT(cellW / scale)
            gh = INT(cellH / scale)
            IF gh > GS_ROWS THEN Fail("BMP: cella alta " + NumS(gh) + " righe (limite " + NumS(GS_ROWS) + ")")
            anyInk = 0
            FOR py = 0 TO gh - 1
                row = ""
                FOR px = 0 TO gw - 1
                    isInk = 0
                    FOR sy = 0 TO scale - 1
                        FOR sx = 0 TO scale - 1
                            xa = x0 + px * scale + sx
                            ya = y0 + py * scale + sy
                            IF xa < w THEN
                                IF ya < h THEN
                                    ir = boff + (h - 1 - ya) * rb + xa * 3
                                    IF raw(ir) < 250 THEN isInk = 1
                                    IF raw(ir + 1) < 250 THEN isInk = 1
                                    IF raw(ir + 2) < 250 THEN isInk = 1
                                END IF
                            END IF
                        NEXT sx
                    NEXT sy
                    IF isInk = 1 THEN
                        row = row + "1"
                        anyInk = 1
                    ELSE
                        row = row + "0"
                    END IF
                NEXT px
                g_tmp(py) = row
            NEXT py
            IF anyInk = 1 THEN
                minX = gw
                maxX = -1
                minY = gh
                maxY = -1
                FOR py = 0 TO gh - 1
                    p = INSTR(g_tmp(py), "1")
                    IF p > 0 THEN
                        IF p - 1 < minX THEN minX = p - 1
                        IF py < minY THEN minY = py
                        IF py > maxY THEN maxY = py
                        px = gw - 1
                        WHILE px >= 0
                            IF MID$(g_tmp(py), px + 1, 1) = "1" THEN
                                IF px > maxX THEN maxX = px
                                px = -1
                            ELSE
                                px = px - 1
                            END IF
                        WEND
                    END IF
                NEXT py
                IF trim = 1 THEN
                    bx = minX
                    by = minY
                    bw2 = maxX - minX + 1
                    bh2 = maxY - minY + 1
                ELSE
                    bx = 0
                    by = 0
                    bw2 = gw
                    bh2 = gh
                END IF
                idx = NewGlyph(GlyphName(enc))
                g_enc(idx) = enc
                g_gw(idx) = bw2
                g_gh(idx) = bh2
                g_gx(idx) = 0
                g_gy(idx) = 0
                g_dwx(idx) = bw2
                g_swx(idx) = bw2 * 62
                g_nRows(idx) = bh2
                FOR nrow = 0 TO bh2 - 1
                    g_row(idx * GS_ROWS + nrow) = MID$(g_tmp(by + nrow), bx + 1, bw2)
                NEXT nrow
                IF bw2 > g_bbW THEN g_bbW = bw2
                IF bh2 > g_bbH THEN g_bbH = bh2
                enc = enc + 1
            END IF
        NEXT c
    NEXT r
    IF g_nGlyphs = 0 THEN Fail("BMP senza celle con inchiostro: " + path)
    g_declChars = g_nGlyphs
    g_fontName = "-glyphsmith-bmp-" + NumS(g_bbH) + "-medium-r-normal--" + NumS(g_pointSize) + "-75-75-c-0-iso10646-1"
END SUB

SUB RunBmpIn()
    DIM src AS STRING
    src = BmpInPath()
    BmpLoad(src)
    EnsureOutDir()
    BdfSave(OutPath(g_bmpinOut))
    PRINT "bmp sorgente : "; src
    PRINT "glifi        : "; g_nGlyphs
    PRINT "bounding box : "; g_bbW; "x"; g_bbH
    PRINT "bdf          : "; OutPath(g_bmpinOut); " ("; FileSizeOf(OutPath(g_bmpinOut)); " byte)"
END SUB

' ============================================================================
' MODE: GUI (finestra PIXEL — seconda vista sullo stesso modello)
' ============================================================================
' Non e' un secondo editor: disegna il modello e applica gli STESSI verbi TUI
' (TuiApply) ai tasti che arrivano da INKEY(). La finestra si chiude da sola dopo
' [gui] frames, cosi' il percorso e' verificabile senza toccare il desktop a mano:
' marker deterministici GUI-READY / GUI-FRAME n / GUI-EXIT.
' Nota: la resa grafica richiede un backend con finestre (verificata su VM/C); su
' un leg senza finestre la finestra non compare e restano i marker di testo.
SUB RunGui()
    DIM f AS INTEGER, nf AS INTEGER, zoom AS INTEGER
    DIM k AS STRING, quit AS INTEGER, prev AS INTEGER
    LoadBdf(BdfPath())
    IF g_nGlyphs < 1 THEN Fail("GUI: nessun glifo")
    zoom = g_guiZoom
    IF zoom < 1 THEN zoom = 1
    nf = g_guiFrames
    IF nf < 1 THEN nf = 1
    IF g_guiGlyph < 0 THEN g_guiGlyph = 0
    IF g_guiGlyph > g_nGlyphs - 1 THEN g_guiGlyph = g_nGlyphs - 1

    SCREEN 12
    WINDOW DEFINE GsMain FROM 20, 20 TO 240, 360 TITLE "GlyphSmith" PIXEL
    WINDOW SELECT GsMain
    PRINT "GUI-READY glyphs="; g_nGlyphs; " zoom="; zoom
    f = 0
    quit = 0
    WHILE f < nf AND quit = 0
        f = f + 1
        GuiDraw(g_guiGlyph, zoom)
        PRINT "GUI-FRAME "; f; " glyph="; g_guiGlyph; " enc="; g_enc(g_guiGlyph); " box="; g_gw(g_guiGlyph); "x"; g_gh(g_guiGlyph)
        k = INKEY()
        IF LEN(k) > 0 THEN
            IF k = CHR$(27) THEN quit = 1
            IF k = "q" THEN quit = 1
            IF k = "n" THEN
                prev = g_guiGlyph + 1
                IF prev > g_nGlyphs - 1 THEN prev = 0
                g_guiGlyph = prev
            END IF
            IF k = "p" THEN
                prev = g_guiGlyph - 1
                IF prev < 0 THEN prev = g_nGlyphs - 1
                g_guiGlyph = prev
            END IF
        END IF
    WEND
    WINDOW SELECT 0
    PRINT "GUI-EXIT frames="; f; " glyph="; g_guiGlyph
END SUB

' Disegno del frame: griglia zoomata del glifo corrente + metriche. Coordinate
' relative al client della finestra selezionata (come i sample WINDOW ... PIXEL).
SUB GuiDraw(gcur AS INTEGER, zoom AS INTEGER)
    DIM px AS INTEGER, py AS INTEGER, ox AS INTEGER, oy AS INTEGER
    CLS
    ox = 12
    oy = 12
    FOR py = 0 TO g_gh(gcur) - 1
        FOR px = 0 TO g_gw(gcur) - 1
            IF GetPx(gcur, px, py) = 1 THEN
                LINE (ox + px * zoom, oy + py * zoom)-(ox + px * zoom + zoom - 1, oy + py * zoom + zoom - 1), 15, "BF"
            END IF
        NEXT px
    NEXT py
    LINE (ox - 2, oy - 2)-(ox + g_gw(gcur) * zoom + 1, oy + g_gh(gcur) * zoom + 1), 7, "B"
    PRINT "glifo "; gcur; " di "; g_nGlyphs - 1
    PRINT "enc   "; g_enc(gcur)
    PRINT "bbx   "; g_gw(gcur); "x"; g_gh(gcur); " a "; g_gx(gcur); ","; g_gy(gcur)
    PRINT "dwx   "; g_dwx(gcur)
    PRINT "n p   glifo precedente / successivo"
    PRINT "esc q esci"
END SUB

SUB SheetAll()
    DIM n AS LONG
    EnsureOutDir()
    SheetBmp(OutPath("sheet.bmp"))
    PbmSave(OutPath("sheet.pbm"))
    Psf2Save(OutPath("mini8.psf"))
    Psf1Save(OutPath("mini8.psf1"))
    AsciiSave(OutPath("mini8.ascii"))
    SixelSave(1, OutPath("mini8.six"))
    PcfSave(OutPath("mini8.pcf"))
    HeaderC(OutPath("mini8.h"))
    PRINT "export       : "; OutPath("sheet.bmp"); " ("; FileSizeOf(OutPath("sheet.bmp")); " byte)"
    PRINT "export       : "; OutPath("sheet.pbm"); " ("; FileSizeOf(OutPath("sheet.pbm")); " byte)"
    PRINT "export       : "; OutPath("mini8.psf"); " ("; FileSizeOf(OutPath("mini8.psf")); " byte)"
    PRINT "export       : "; OutPath("mini8.psf1"); " ("; FileSizeOf(OutPath("mini8.psf1")); " byte)"
    PRINT "export       : "; OutPath("mini8.ascii"); " ("; FileSizeOf(OutPath("mini8.ascii")); " byte)"
    PRINT "export       : "; OutPath("mini8.six"); " ("; FileSizeOf(OutPath("mini8.six")); " byte)"
    PRINT "export       : "; OutPath("mini8.pcf"); " ("; FileSizeOf(OutPath("mini8.pcf")); " byte)"
    PRINT "export       : "; OutPath("mini8.h"); " ("; FileSizeOf(OutPath("mini8.h")); " byte)"
END SUB

' ============================================================================
' MODE: SHEET / EDIT
' ============================================================================
SUB RunSheet()
    LoadBdf(BdfPath())
    PRINT "font         : "; g_fontName
    PRINT "glifi        : "; g_nGlyphs
    SheetAll()
END SUB

SUB RunEdit()
    LoadBdf(BdfPath())
    PRINT "font         : "; g_fontName
    PRINT "script       : "; ScriptPath()
    ApplyEditScript(ScriptPath())
    EnsureOutDir()
    BdfSave(OutPath("edited.bdf"))
    PRINT "bdf edito    : "; OutPath("edited.bdf"); " ("; FileSizeOf(OutPath("edited.bdf")); " byte)"
    SheetAll()
END SUB

SUB ApplyEditScript(path AS STRING)
    DIM lines
    DIM raw AS STRING, cmd AS STRING
    DIM i AS LONG, g AS INTEGER
    IF NOT IO.FILEEXISTS(path) THEN Fail("script di editing assente: " + path)
    lines = IO.READLINES(path)
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            cmd = STRINGS.UPPER(TokAt(raw, 0))
            IF cmd = "CLEAR" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                ClearGlyph(g)
            ELSEIF cmd = "SET" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                SetPx(g, CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), InkChar(TokAt(raw, 4)))
            ELSEIF cmd = "RECT" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                RectFill(g, CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), CINT(VAL(TokAt(raw, 4))), CINT(VAL(TokAt(raw, 5))), InkChar(TokAt(raw, 6)))
            ELSEIF cmd = "LINE" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                DrawLine(g, CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), CINT(VAL(TokAt(raw, 4))), CINT(VAL(TokAt(raw, 5))), InkChar(TokAt(raw, 6)))
            ELSEIF cmd = "FLIP" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                IF STRINGS.UPPER(TokAt(raw, 2)) = "V" THEN
                    FlipV(g)
                ELSE
                    FlipH(g)
                END IF
            ELSEIF cmd = "SHIFT" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                ShiftGlyph(g, CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))))
            ELSEIF cmd = "CLONE" THEN
                g = CINT(VAL(TokAt(raw, 2)))
                HistPush(g)
                CloneGlyph(CINT(VAL(TokAt(raw, 1))), g)
            ELSEIF cmd = "ROT90" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                Rot90(g)
            ELSEIF cmd = "SHEAR" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                ShearH(g, CINT(VAL(TokAt(raw, 2))))
            ELSEIF cmd = "REGION" THEN
                RegionCopy(CINT(VAL(TokAt(raw, 1))), CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), CINT(VAL(TokAt(raw, 4))), CINT(VAL(TokAt(raw, 5))))
            ELSEIF cmd = "PASTE" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                RegionPaste(g, CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))))
            ELSEIF cmd = "BEARING" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                HistPush(g)
                BearingRecompute(g)
            ELSEIF cmd = "INSERT" THEN
                InsertGlyph(CLNG(VAL(TokAt(raw, 1))), TokAt(raw, 2), CINT(VAL(TokAt(raw, 3))), CINT(VAL(TokAt(raw, 4))))
            ELSEIF cmd = "REMOVE" THEN
                RemoveGlyph(CINT(VAL(TokAt(raw, 1))))
            ELSEIF cmd = "UNDO" THEN
                DoUndo()
            ELSEIF cmd = "REDO" THEN
                DoRedo()
            ELSE
                Fail("comando di editing non riconosciuto: " + raw)
            END IF
        END IF
    NEXT i
END SUB

FUNCTION InkChar(s AS STRING) AS STRING
    IF s = "0" THEN
        InkChar = "0"
    ELSE
        InkChar = "1"
    END IF
END FUNCTION

' ============================================================================
' MODE: IMPORT — da font outline (TrueType/OpenType) a modello di glifi BDF
' ----------------------------------------------------------------------------
' L'outline non viene letto da BASIC: FONTS.* delega al runtime C (gmb_fonts.c sopra
' stb_truetype). Qui si fa solo orchestrazione: intervallo di codepoint, soglia di
' copertura, costruzione delle righe '0'/'1', metriche BDF.
' ============================================================================
FUNCTION ImportPath() AS STRING
    IF STRINGS.STARTSWITH(g_importFont, "/") THEN
        ImportPath = g_importFont
    ELSE
        ImportPath = g_appDir + "/" + g_importFont
    END IF
END FUNCTION

FUNCTION GlyphName(cp AS LONG) AS STRING
    DIM v AS LONG, hi AS LONG, lo AS LONG
    v = cp
    hi = INT(v / 256)
    lo = v - hi * 256
    GlyphName = "uni" + Hex2(CINT(hi)) + Hex2(CINT(lo))
END FUNCTION

SUB StartModel()
    g_nGlyphs = 0
    g_nProps = 0
    g_declChars = 0
    g_bbW = 0
    g_bbH = 0
    g_bbX = 0
    g_bbY = 0
    g_pointSize = g_importSize
    g_xres = 75
    g_yres = 75
    g_startFont = "2.1"
    g_fontName = "-glyphsmith-imported-" + NumS(g_importSize) + "-medium-r-normal--" + NumS(g_importSize) + "-75-75-c-0-iso10646-1"
    AddProp("FOUNDRY " + CHR$(34) + "GlyphSmith" + CHR$(34))
    AddProp("FAMILY_NAME " + CHR$(34) + "Imported" + CHR$(34))
END SUB

SUB RunImport()
    DIM path AS STRING, w AS INTEGER, h AS INTEGER
    DIM cp AS LONG, idx AS INTEGER, x AS INTEGER, y AS INTEGER, thr AS INTEGER
    DIM row AS STRING, adv AS LONG, swid AS LONG, last AS LONG
    DIM xs AS INTEGER, ys AS INTEGER, gyv AS INTEGER
    path = ImportPath()
    IF NOT IO.FILEEXISTS(path) THEN Fail("font outline assente: " + path)
    IF g_importSize < 1 THEN Fail("size non valido: " + NumS(g_importSize))
    thr = g_importThreshold
    IF thr < 0 THEN thr = 0
    IF thr > 255 THEN thr = 255
    last = g_importLast
    IF last > 65535 THEN last = 65535
    StartModel()

    ' DIM con inizializzatore: e' la forma che rende il risultato una LISTA indicizzabile
    ' anche sul leg C99Native (gkind dedotto dal token della funzione).
    DIM m() = FONTS.TTMETRICS(path, g_importSize, g_importFirst)
    DIM px() = FONTS.TTRENDER(path, g_importSize, g_importFirst)

    FOR cp = g_importFirst TO last
        IF g_nGlyphs < GS_GMAX THEN
            m = FONTS.TTMETRICS(path, g_importSize, cp)
            w = CINT(m(0))
            h = CINT(m(1))
            IF w > 0 THEN
                IF h > 0 THEN
                    IF w <= 32 THEN
                        IF h <= GS_ROWS THEN
                            px = FONTS.TTRENDER(path, g_importSize, cp)
                            idx = NewGlyph(GlyphName(cp))
                            g_enc(idx) = cp
                            g_gw(idx) = w
                            g_gh(idx) = h
                            g_gx(idx) = CINT(m(2))
                            ' BDF: offset dal baseline al BASSO della bitmap = -(yoff + h).
                            ' Lo zero non viene mai negato: evita il -0 nel file (formattazione
                            ' diversa tra leg) restando identico nel contenuto.
                            xs = CINT(m(2))
                            ys = CINT(m(3))
                            gyv = ys + h
                            IF gyv <> 0 THEN gyv = -gyv
                            g_gx(idx) = xs
                            g_gy(idx) = gyv
                            adv = CINT(m(4) + 0.5)
                            IF adv < 1 THEN adv = w
                            g_dwx(idx) = adv
                            swid = CINT(m(4) * 1000.0 / g_importSize + 0.5)
                            IF swid < 1 THEN swid = w * 1000 / g_importSize
                            g_swx(idx) = swid
                            FOR y = 0 TO h - 1
                                row = ""
                                FOR x = 0 TO w - 1
                                    IF px(y * w + x) >= thr THEN
                                        row = row + "1"
                                    ELSE
                                        row = row + "0"
                                    END IF
                                NEXT x
                                g_row(idx * GS_ROWS + y) = row
                            NEXT y
                            g_nRows(idx) = h
                            IF w > g_bbW THEN g_bbW = w
                            IF h > g_bbH THEN g_bbH = h
                        END IF
                    END IF
                END IF
            END IF
        END IF
    NEXT cp

    IF g_nGlyphs = 0 THEN Fail("nessun glifo coperto nell'intervallo " + NumS(g_importFirst) + ".." + NumS(last))
    g_declChars = g_nGlyphs
    EnsureOutDir()
    BdfSave(OutPath(g_importOut))
    PRINT "outline       : "; path
    PRINT "size          : "; g_importSize; " px | soglia "; thr
    PRINT "glifi         : "; g_nGlyphs; " (U+"; NumS(g_importFirst); " .. U+"; NumS(last); ")"
    PRINT "bounding box  : "; g_bbW; "x"; g_bbH
    PRINT "bdf importato : "; OutPath(g_importOut); " ("; FileSizeOf(OutPath(g_importOut)); " byte)"
END SUB

' ============================================================================
' MODE: CHECK — autoverifica deterministica
' ============================================================================
SUB KEq(a AS LONG, b AS LONG, label AS STRING)
    IF a = b THEN
        g_ok = g_ok + 1
    ELSE
        g_ko = g_ko + 1
        PRINT "  FAIL "; label; " (atteso "; b; ", trovato "; a; ")"
    END IF
END SUB

SUB KStr(a AS STRING, b AS STRING, label AS STRING)
    IF a = b THEN
        g_ok = g_ok + 1
    ELSE
        g_ko = g_ko + 1
        PRINT "  FAIL "; label
        PRINT "    atteso : "; b
        PRINT "    trovato: "; a
    END IF
END SUB

SUB KTrue(cond AS INTEGER, label AS STRING)
    IF cond <> 0 THEN
        g_ok = g_ok + 1
    ELSE
        g_ko = g_ko + 1
        PRINT "  FAIL "; label; " (condizione falsa)"
    END IF
END SUB

SUB RunCheck()
    DIM sig AS STRING
    DIM expBmp AS LONG, expPsf AS LONG, expPsf1 AS LONG
    g_ok = 0
    g_ko = 0
    EnsureOutDir()
    LoadBdf(BdfPath())
    PRINT "font         : "; g_fontName
    PRINT "glifi        : "; g_nGlyphs
    PRINT "output       : "; g_outAbs
    PRINT ""

    ' --- 1. parsing e metriche ---
    KEq(g_nGlyphs, 3, "numero glifi")
    KEq(g_declChars, 3, "CHARS dichiarati")
    KEq(g_nProps, 9, "proprieta del font (4 del file + 5 standard)")
    KEq(g_enc(0), 32, "encoding[0] = spazio")
    KEq(g_enc(1), 65, "encoding[1] = A")
    KEq(g_enc(2), 66, "encoding[2] = B")
    KEq(g_gw(1), 8, "larghezza A")
    KEq(g_gh(1), 8, "altezza A")
    KEq(g_gw(2), 8, "larghezza B")
    KEq(g_nRows(1), 8, "righe bitmap A")
    KEq(g_dwx(1), 8, "DWIDTH A")
    KEq(g_bbW, 8, "FONTBOUNDINGBOX larghezza")
    KEq(g_bbH, 8, "FONTBOUNDINGBOX altezza")
    KEq(g_pointSize, 8, "SIZE punto")
    KEq(PropInt("RESOLUTION_X"), g_xres, "RESOLUTION_X dal SIZE")
    KEq(PropInt("RESOLUTION_Y"), g_yres, "RESOLUTION_Y dal SIZE")
    KEq(PropInt("FONT_ASCENT"), 8, "FONT_ASCENT dal modello")
    KEq(PropInt("FONT_DESCENT"), 0, "FONT_DESCENT dal modello")
    KEq(PropInt("DEFAULT_CHAR"), 32, "DEFAULT_CHAR = spazio")

    ' --- 2. inchiostro e pixel ---
    KEq(InkCount(0), 0, "inchiostro spazio")
    KEq(InkCount(1), 16, "inchiostro A")
    KEq(InkCount(2), 21, "inchiostro B")
    KEq(GetPx(1, 3, 0), 1, "A(3,0) acceso")
    KEq(GetPx(1, 2, 0), 0, "A(2,0) spento")
    KEq(GetPx(1, 1, 2), 1, "A(1,2) acceso")
    KEq(GetPx(1, 0, 0), 0, "A(0,0) spento")
    KEq(GetPx(2, 1, 0), 1, "B(1,0) acceso")
    KEq(GetPx(2, 0, 0), 0, "B(0,0) spento")
    KEq(GetPx(2, 5, 5), 1, "B(5,5) acceso")
    KEq(GetPx(2, 6, 5), 0, "B(6,5) spento")
    KEq(GetPx(1, 99, 0), 0, "pixel fuori griglia")

    ' --- 3. operazioni di editing (su un glifo di lavoro, indice 60) ---
    CloneGlyph(1, 60)
    KTrue(SameGlyph(1, 60), "clone A -> 60")
    sig = GlyphSig(60)
    FlipH(60)
    FlipH(60)
    KStr(GlyphSig(60), sig, "flip H due volte = identita")
    KEq(GetPx(60, 3, 0), 1, "flip H conserva il pixel")
    FlipV(60)
    FlipV(60)
    KStr(GlyphSig(60), sig, "flip V due volte = identita")
    ClearGlyph(60)
    KEq(InkCount(60), 0, "clear")
    SetPx(60, 0, 0, "1")
    KEq(GetPx(60, 0, 0), 1, "set pixel")
    ShiftGlyph(60, 2, 0)
    KEq(GetPx(60, 2, 0), 1, "shift +2 in x")
    KEq(GetPx(60, 0, 0), 0, "shift libera l'origine")
    ClearGlyph(60)
    RectFill(60, 1, 1, 3, 3, "1")
    KEq(InkCount(60), 9, "rettangolo pieno 3x3")
    ClearGlyph(60)
    DrawLine(60, 0, 0, 7, 0, "1")
    KEq(InkCount(60), 8, "linea orizzontale")
    ClearGlyph(60)
    DrawLine(60, 0, 0, 7, 7, "1")
    KEq(InkCount(60), 8, "linea diagonale")
    ClearGlyph(60)
    DrawLine(60, 4, 4, 4, 4, "1")
    KEq(InkCount(60), 1, "linea degenere = un pixel")
    ClearGlyph(60)
    KEq(InkCount(60), 0, "clear finale")

    ' --- 4. round trip BDF ---
    sig = ModelSignature()
    BdfSave(OutPath("roundtrip.bdf"))
    KTrue(IO.FILEEXISTS(OutPath("roundtrip.bdf")), "roundtrip.bdf creato")
    LoadBdf(OutPath("roundtrip.bdf"))
    KStr(ModelSignature(), sig, "round trip BDF identico")
    KEq(g_nGlyphs, 3, "glifi dopo round trip")
    KEq(g_nProps, 9, "proprieta dopo round trip (idempotente)")
    KEq(g_nRows(1), 8, "righe A dopo round trip")

    ' --- 5. export ---
    SheetAll()
    expBmp = 54 + (INT((120 * 3 + 3) / 4) * 4) * 40
    expPsf = 32 + 3 * 8
    KTrue(IO.FILEEXISTS(OutPath("sheet.bmp")), "sheet.bmp creato")
    KTrue(IO.FILEEXISTS(OutPath("sheet.pbm")), "sheet.pbm creato")
    KTrue(IO.FILEEXISTS(OutPath("mini8.psf")), "mini8.psf creato")
    KTrue(IO.FILEEXISTS(OutPath("mini8.h")), "mini8.h creato")
    KEq(FileSizeOf(OutPath("sheet.bmp")), expBmp, "dimensione sheet.bmp")
    KEq(FileSizeOf(OutPath("mini8.psf")), expPsf, "dimensione mini8.psf")
    KTrue(FileSizeOf(OutPath("mini8.h")) > 100, "mini8.h non vuoto")
    KTrue(FileSizeOf(OutPath("sheet.pbm")) > 100, "sheet.pbm non vuoto")
    KTrue(IO.FILEEXISTS(OutPath("mini8.psf1")), "mini8.psf1 creato")
    KTrue(IO.FILEEXISTS(OutPath("mini8.ascii")), "mini8.ascii creato")
    expPsf1 = 4 + 256 * g_bbH
    KEq(FileSizeOf(OutPath("mini8.psf1")), expPsf1, "dimensione mini8.psf1")
    KTrue(FileSizeOf(OutPath("mini8.ascii")) > 100, "mini8.ascii non vuoto")

    ' --- 6. BMP: rilettura del foglio e confronto pixel per pixel -------------
    ' Il BMP e' l'unico formato scritto a mano: qui lo si RILEGGE con IO.READBYTES,
    ' si verificano header e riga, e si confronta il numero di pixel neri con il modello
    ' (ogni pixel acceso occupa scale x scale pixel del foglio).
    DIM raw() = IO.READBYTES(OutPath("sheet.bmp"))
    DIM bw AS LONG, bh AS LONG, boff AS LONG, brb AS LONG
    DIM sc2 AS INTEGER, cols2 AS INTEGER, cellW2 AS INTEGER, cellH2 AS INTEGER
    DIM mw2 AS INTEGER, mh2 AS INTEGER, g2 AS INTEGER
    DIM xr AS INTEGER, yr AS INTEGER, ir AS LONG, inkBmp AS LONG, inkModel AS LONG
    DIM x0b AS INTEGER, y0b AS INTEGER

    KEq(UBOUND(raw) + 1, expBmp, "BMP: byte riletti")
    KEq(raw(0), 66, "BMP: firma 'B'")
    KEq(raw(1), 77, "BMP: firma 'M'")
    boff = raw(10) + raw(11) * 256 + raw(12) * 65536 + raw(13) * 16777216
    bw = raw(18) + raw(19) * 256 + raw(20) * 65536 + raw(21) * 16777216
    bh = raw(22) + raw(23) * 256 + raw(24) * 65536 + raw(25) * 16777216
    KEq(boff, 54, "BMP: offset dei pixel")
    KEq(bw, 120, "BMP: larghezza da header")
    KEq(bh, 40, "BMP: altezza da header")
    KEq(raw(28), 24, "BMP: 24 bit per pixel")
    brb = INT((bw * 3 + 3) / 4) * 4
    KEq(54 + brb * bh, expBmp, "BMP: dimensione coerente con l'header")
    KEq(brb - bw * 3, 0, "BMP: riga gia' multipla di 4 (nessun padding)")

    sc2 = g_bmpScale
    IF sc2 < 1 THEN sc2 = 1
    cols2 = g_cellsPerRow
    IF cols2 < 1 THEN cols2 = 1
    IF cols2 > g_nGlyphs AND g_nGlyphs > 0 THEN cols2 = g_nGlyphs
    mw2 = 1
    mh2 = 1
    inkModel = 0
    FOR g2 = 0 TO g_nGlyphs - 1
        IF g_gw(g2) > mw2 THEN mw2 = g_gw(g2)
        IF g_gh(g2) > mh2 THEN mh2 = g_gh(g2)
        inkModel = inkModel + InkCount(g2) * sc2 * sc2
    NEXT g2
    cellW2 = (mw2 + 2) * sc2
    cellH2 = (mh2 + 2) * sc2

    ' conta i pixel neri davvero presenti nel file (BGR = 0,0,0)
    inkBmp = 0
    FOR yr = 0 TO bh - 1
        FOR xr = 0 TO bw - 1
            ir = 54 + (bh - 1 - yr) * brb + xr * 3
            IF raw(ir) = 0 THEN
                IF raw(ir + 1) = 0 THEN
                    IF raw(ir + 2) = 0 THEN inkBmp = inkBmp + 1
                END IF
            END IF
        NEXT xr
    NEXT yr
    KEq(inkBmp, inkModel, "BMP: pixel neri = inchiostro x scale^2")

    ' un pixel acceso e uno spento del glifo 'A' (indice 1), letti dal file
    x0b = cellW2 + sc2
    y0b = sc2
    ir = 54 + (bh - 1 - y0b) * brb + (x0b + 3 * sc2) * 3
    KEq(raw(ir), 0, "BMP: 'A'(3,0) nero nel file")
    ir = 54 + (bh - 1 - y0b) * brb + x0b * 3
    KEq(raw(ir), 255, "BMP: 'A'(0,0) bianco nel file")

    ' --- 8. M2: trasformazioni, clipboard, bearing, insert/remove, undo ------
    ' I glifi di lavoro si creano qui: il fixture ne ha pochi e gli indici alti non
    ' esistono, quindi non si assume nulla sul modello caricato.
    DIM n0 AS INTEGER, wa AS INTEGER, wb AS INTEGER, wc AS INTEGER
    DIM regInk AS INTEGER, yy AS INTEGER, xx2 AS INTEGER, sigLast AS STRING, sigS AS STRING
    n0 = g_nGlyphs
    sigLast = GlyphSig(n0 - 1)
    InsertGlyph(900, "m2a", 8, 8)
    InsertGlyph(901, "m2b", 8, 8)
    InsertGlyph(902, "m2c", 8, 8)
    wa = n0
    wb = n0 + 1
    wc = n0 + 2
    KEq(g_nGlyphs, n0 + 3, "M2: insert aggiunge tre glifi")
    KEq(g_enc(wa), 900, "M2: insert conserva l'encoding")
    KEq(g_nRows(wa), 8, "M2: insert crea le righe")
    KEq(InkCount(wa), 0, "M2: insert nasce vuoto")

    SetPx(wa, 0, 0, "1")
    SetPx(wa, 0, 2, "1")
    SetPx(wa, 1, 0, "1")
    KEq(InkCount(wa), 3, "M2: pattern iniziale")
    HistPush(wa)
    Rot90(wa)
    KEq(g_gw(wa), 8, "M2: rot90 larghezza = altezza precedente")
    KEq(g_gh(wa), 8, "M2: rot90 altezza = larghezza precedente")
    KEq(InkCount(wa), 3, "M2: rot90 conserva l'inchiostro")
    KEq(GetPx(wa, 7, 0), 1, "M2: rot90 (0,0) -> (7,0)")
    KEq(GetPx(wa, 0, 0), 0, "M2: rot90 libera (0,0)")
    DoUndo()
    KEq(GetPx(wa, 0, 0), 1, "M2: undo ripristina (0,0)")
    KEq(GetPx(wa, 7, 0), 0, "M2: undo toglie la rotazione")
    DoRedo()
    KEq(GetPx(wa, 7, 0), 1, "M2: redo riapplica la rotazione")
    DoUndo()

    SetPx(wb, 1, 1, "1")
    SetPx(wb, 2, 1, "1")
    HistPush(wb)
    ShearH(wb, 1)
    KEq(GetPx(wb, 2, 1), 1, "M2: shear sposta la riga 1 di +1")
    KEq(GetPx(wb, 1, 1), 0, "M2: shear libera l'originale")
    KEq(InkCount(wb), 2, "M2: shear conserva l'inchiostro")
    DoUndo()
    KEq(GetPx(wb, 1, 1), 1, "M2: undo dello shear")

    RegionCopy(wa, 0, 0, 2, 2)
    KEq(g_clipW, 3, "M2: regione larghezza")
    KEq(g_clipH, 3, "M2: regione altezza")
    regInk = 0
    FOR yy = 0 TO 2
        FOR xx2 = 0 TO 2
            IF GetPx(wa, xx2, yy) = 1 THEN regInk = regInk + 1
        NEXT xx2
    NEXT yy
    RegionPaste(wc, 0, 0)
    KEq(InkCount(wc), regInk, "M2: paste riproduce l'inchiostro della regione")
    KEq(GetPx(wc, 0, 0), GetPx(wa, 0, 0), "M2: paste (0,0) coerente")

    ClearGlyph(wb)
    SetPx(wb, 2, 1, "1")
    SetPx(wb, 5, 3, "1")
    BearingRecompute(wb)
    KEq(g_gx(wb), 2, "M2: bearing x = primo pixel")
    KEq(g_dwx(wb), 4, "M2: bearing DWIDTH = larghezza inchiostrata")
    KEq(g_gy(wb), -4, "M2: bearing y = -(ultima riga + 1)")

    ' --- 9. M4: TUI sullo stesso modello --------------------------------------
    DIM fr AS STRING
    g_tuiGlyph = wb
    g_tuiX = 2
    g_tuiY = 1
    TuiApply(" ")
    KEq(GetPx(wb, 2, 1), 0, "TUI: spazio spegne il pixel sotto il cursore")
    TuiApply(" ")
    KEq(GetPx(wb, 2, 1), 1, "TUI: spazio riaccende")
    TuiApply("l")
    KEq(g_tuiX, 3, "TUI: 'l' sposta a destra")
    TuiApply("k")
    KEq(g_tuiY, 0, "TUI: 'k' sale")
    KEq(g_tuiX, 3, "TUI: il cursore non esce dal box")
    TuiApply("u")
    KEq(g_tuiX, 3, "TUI: undo non muove il cursore")
    fr = TuiFrame()
    KTrue(Has(fr, "glifo") = 1, "TUI: frame con intestazione")
    KTrue(Has(fr, "cursore 3,0") = 1, "TUI: frame con riga di stato")
    KTrue(Has(fr, "+") = 1, "TUI: frame con bordo")

    ' --- 10. sixel: byte del flusso e round-trip ---------------------------------
    SixelSave(wb, OutPath("mini8.six"))
    DIM sx() = IO.READBYTES(OutPath("mini8.six"))
    KTrue(UBOUND(sx) + 1 > 20, "sixel: file non vuoto")
    KEq(sx(0), 27, "sixel: ESC iniziale")
    KEq(sx(1), 80, "sixel: 'P'")
    KEq(sx(2), 113, "sixel: 'q'")
    KEq(sx(UBOUND(sx)), 92, "sixel: terminatore backslash")
    KEq(sx(UBOUND(sx) - 1), 27, "sixel: ESC del terminatore")
    sigS = GlyphSig(wb)
    ClearGlyph(wb)
    SixelLoad(wb, OutPath("mini8.six"))
    KStr(GlyphSig(wb), sigS, "sixel: round-trip identico al modello")

    RemoveGlyph(wc)
    RemoveGlyph(wb)
    RemoveGlyph(wa)
    KEq(g_nGlyphs, n0, "M2: remove riporta il conteggio")
    KStr(GlyphSig(n0 - 1), sigLast, "M2: remove non altera i glifi precedenti")

    ' --- 7. PSF1 e ASCII art: byte e round-trip ------------------------------
    DIM p1() = IO.READBYTES(OutPath("mini8.psf1"))
    DIM xx AS INTEGER, expByte AS INTEGER, gA AS INTEGER, sig0 AS STRING, sig1 AS STRING
    KEq(UBOUND(p1) + 1, expPsf1, "PSF1: byte totali")
    KEq(p1(0), 54, "PSF1: magic 0x36")
    KEq(p1(1), 4, "PSF1: magic 0x04")
    KEq(p1(2), 0, "PSF1: mode 0")
    KEq(p1(3), g_bbH, "PSF1: charsize = altezza BB")
    expByte = 0
    FOR xx = 0 TO 7
        IF GetPx(1, xx, 0) = 1 THEN expByte = expByte + Pow2(7 - xx)
    NEXT xx
    KEq(p1(4 + 65 * g_bbH), expByte, "PSF1: byte di 'A' riga 0")

    ' --- PCF: si riscrive il file e si verificano header, tabelle e bitmap ----
    PcfSave(OutPath("mini8.pcf"))
    DIM pf() = IO.READBYTES(OutPath("mini8.pcf"))
    DIM offBit AS LONG, offMet AS LONG, bitData AS LONG, i0 AS LONG
    DIM offEnc AS LONG, mn AS INTEGER, mx AS INTEGER
    KEq(pf(0), 1, "PCF: magic 0x01")
    KEq(pf(1), 102, "PCF: 'f'")
    KEq(pf(2), 99, "PCF: 'c'")
    KEq(pf(3), 112, "PCF: 'p'")
    KEq(pf(4), 5, "PCF: cinque tabelle")
    KEq(pf(8), 1, "PCF: PROPERTIES")
    KEq(pf(24), 2, "PCF: ACCELERATORS")
    KEq(pf(40), 4, "PCF: METRICS")
    KEq(pf(56), 8, "PCF: BITMAPS")
    KEq(pf(72), 32, "PCF: BDF_ENCODINGS")
    offMet = pf(52) + pf(53) * 256 + pf(54) * 65536 + pf(55) * 16777216
    KEq(pf(offMet + 7), g_nGlyphs, "PCF: conteggio metriche (big-endian)")
    offBit = pf(68) + pf(69) * 256 + pf(70) * 65536 + pf(71) * 16777216
    bitData = offBit + 24 + 4 * g_nGlyphs
    i0 = bitData + 32
    expByte = 0
    FOR xx = 0 TO 7
        IF GetPx(1, xx, 0) = 1 THEN expByte = expByte + Pow2(7 - xx)
    NEXT xx
    KEq(pf(i0), expByte, "PCF: prima riga di 'A' (bit MSB-first)")

    ' Riga NON simmetrica: e' l'unica che distingue davvero l'ordine dei bit
    ' (0x7c letto LSB-first darebbe 62).
    expByte = 0
    FOR xx = 0 TO 7
        IF GetPx(2, xx, 0) = 1 THEN expByte = expByte + Pow2(7 - xx)
    NEXT xx
    KEq(expByte, 124, "PCF: prima riga di 'B' = 0x7c")
    KEq(pf(i0 + 32), expByte, "PCF: prima riga di 'B' nel file")

    ' Tabella BDF_ENCODINGS: estremi dichiarati e lunghezza fino a fine file.
    offEnc = pf(84) + pf(85) * 256 + pf(86) * 65536 + pf(87) * 16777216
    mn = g_enc(0)
    mx = g_enc(0)
    FOR gA = 1 TO g_nGlyphs - 1
        IF g_enc(gA) < mn THEN mn = g_enc(gA)
        IF g_enc(gA) > mx THEN mx = g_enc(gA)
    NEXT gA
    KEq(pf(offEnc + 4) * 256 + pf(offEnc + 5), mn, "PCF: encoding minimo della tabella")
    KEq(pf(offEnc + 6) * 256 + pf(offEnc + 7), mx, "PCF: encoding massimo della tabella")
    KEq(offEnc + 14 + 2 * (mx - mn + 1), LEN(pf), "PCF: encodings fino a fine file")

    sig0 = ""
    FOR gA = 0 TO g_nGlyphs - 1
        sig0 = sig0 + NumS(g_enc(gA)) + "=" + GlyphSig(gA) + "|"
    NEXT gA
    TextLoad(OutPath("mini8.ascii"))
    sig1 = ""
    FOR gA = 0 TO g_nGlyphs - 1
        sig1 = sig1 + NumS(g_enc(gA)) + "=" + GlyphSig(gA) + "|"
    NEXT gA
    KEq(g_nGlyphs, 3, "ASCII: glifi riletti")
    KStr(sig1, sig0, "ASCII: round trip identico al modello")

    ' --- 12. BMP: import raster (foglio costruito qui e poi riletto) -----------
    ' Verifica il giro completo: modello -> SheetBmp -> BmpLoad -> modello. I due
    ' glifi hanno inchiostro su tutti e quattro i bordi, quindi il ritaglio dei
    ' margini non deve cambiare nulla. Con scale=2 ogni pixel e' un blocco 2x2.
    DIM sc0 AS INTEGER, cl0 AS INTEGER, sigA AS STRING, sigB AS STRING
    sc0 = g_bmpScale
    cl0 = g_cellsPerRow
    StartModel()
    g_bbW = 4
    g_bbH = 3
    InsertGlyph(65, "ga", 4, 3)
    InsertGlyph(66, "gb", 4, 3)
    SetPx(0, 0, 0, "1")
    SetPx(0, 1, 0, "1")
    SetPx(0, 2, 0, "1")
    SetPx(0, 3, 0, "1")
    SetPx(0, 0, 1, "1")
    SetPx(0, 3, 1, "1")
    SetPx(0, 0, 2, "1")
    SetPx(0, 1, 2, "1")
    SetPx(0, 2, 2, "1")
    SetPx(0, 3, 2, "1")
    SetPx(1, 0, 0, "1")
    SetPx(1, 3, 0, "1")
    SetPx(1, 0, 1, "1")
    SetPx(1, 1, 1, "1")
    SetPx(1, 2, 1, "1")
    SetPx(1, 3, 1, "1")
    SetPx(1, 0, 2, "1")
    SetPx(1, 3, 2, "1")
    sigA = GlyphSig(0)
    sigB = GlyphSig(1)
    g_bmpScale = 2
    g_cellsPerRow = 2
    SheetBmp(OutPath("bmpin_src.bmp"))
    KTrue(IO.FILEEXISTS(OutPath("bmpin_src.bmp")), "BMP import: foglio di prova scritto")
    g_bmpinScale = 2
    g_bmpinCols = 2
    g_bmpinCellW = 12
    g_bmpinCellH = 10
    g_bmpinFirst = 65
    g_bmpinTrim = "1"
    BmpLoad(OutPath("bmpin_src.bmp"))
    KEq(g_nGlyphs, 2, "BMP import: due celle con inchiostro")
    KEq(g_enc(0), 65, "BMP import: encoding del primo glifo")
    KEq(g_enc(1), 66, "BMP import: encoding del secondo glifo")
    KEq(g_gw(0), 4, "BMP import: larghezza ritagliata")
    KEq(g_gh(0), 3, "BMP import: altezza ritagliata")
    KStr(GlyphSig(0), sigA, "BMP import: glifo 0 identico al modello")
    KStr(GlyphSig(1), sigB, "BMP import: glifo 1 identico al modello")
    g_bmpScale = sc0
    g_cellsPerRow = cl0

    PRINT ""
    PRINT "controlli    : "; g_ok; " ok, "; g_ko; " falliti"
    IF g_ko = 0 THEN
        PRINT "GLYPHSMITH-CHECK-OK"
    ELSE
        PRINT "GLYPHSMITH-CHECK-FAIL"
        ERROR 5
    END IF
END SUB
