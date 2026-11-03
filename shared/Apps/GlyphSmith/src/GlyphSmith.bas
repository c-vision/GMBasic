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
    PRINT "modi         : -DGLYPHSMITH_MODE_CHECK | _SHEET | _EDIT"
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

SUB SheetAll()
    DIM n AS LONG
    EnsureOutDir()
    SheetBmp(OutPath("sheet.bmp"))
    PbmSave(OutPath("sheet.pbm"))
    Psf2Save(OutPath("mini8.psf"))
    HeaderC(OutPath("mini8.h"))
    PRINT "export       : "; OutPath("sheet.bmp"); " ("; FileSizeOf(OutPath("sheet.bmp")); " byte)"
    PRINT "export       : "; OutPath("sheet.pbm"); " ("; FileSizeOf(OutPath("sheet.pbm")); " byte)"
    PRINT "export       : "; OutPath("mini8.psf"); " ("; FileSizeOf(OutPath("mini8.psf")); " byte)"
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
                ClearGlyph(CINT(VAL(TokAt(raw, 1))))
            ELSEIF cmd = "SET" THEN
                SetPx(CINT(VAL(TokAt(raw, 1))), CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), InkChar(TokAt(raw, 4)))
            ELSEIF cmd = "RECT" THEN
                RectFill(CINT(VAL(TokAt(raw, 1))), CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), CINT(VAL(TokAt(raw, 4))), CINT(VAL(TokAt(raw, 5))), InkChar(TokAt(raw, 6)))
            ELSEIF cmd = "LINE" THEN
                DrawLine(CINT(VAL(TokAt(raw, 1))), CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))), CINT(VAL(TokAt(raw, 4))), CINT(VAL(TokAt(raw, 5))), InkChar(TokAt(raw, 6)))
            ELSEIF cmd = "FLIP" THEN
                g = CINT(VAL(TokAt(raw, 1)))
                IF STRINGS.UPPER(TokAt(raw, 2)) = "V" THEN
                    FlipV(g)
                ELSE
                    FlipH(g)
                END IF
            ELSEIF cmd = "SHIFT" THEN
                ShiftGlyph(CINT(VAL(TokAt(raw, 1))), CINT(VAL(TokAt(raw, 2))), CINT(VAL(TokAt(raw, 3))))
            ELSEIF cmd = "CLONE" THEN
                CloneGlyph(CINT(VAL(TokAt(raw, 1))), CINT(VAL(TokAt(raw, 2))))
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
    DIM expBmp AS LONG, expPsf AS LONG
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
    KEq(g_nProps, 4, "proprieta del font")
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
    KEq(g_nProps, 4, "proprieta dopo round trip")
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

    PRINT ""
    PRINT "controlli    : "; g_ok; " ok, "; g_ko; " falliti"
    IF g_ko = 0 THEN
        PRINT "GLYPHSMITH-CHECK-OK"
    ELSE
        PRINT "GLYPHSMITH-CHECK-FAIL"
        ERROR 5
    END IF
END SUB
