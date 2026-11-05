' ==============================================================================
' cellfont_from_bdf.bas — genera il bitmap ASCII 8x16 usato dal font a CELLE del TUI.
'
' `gmb_gfx_cellfont.c` contiene una tabella [95][16] (ASCII 32..126, 8x16, 1 bit/px,
' MSB-left) usata dal render TUI->pixel (SDLConsole). Prima era generata da
' Resources/C64_Pro_Mono-STYLE.otf, font che la licenza style64 vieta di ridistribuire.
' Qui la tabella si rigenera da Terminus 8x16 (SIL OFL 1.1), che ha la matrice 8x16.
'
' USO (dalla root del repo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/cellfont_from_bdf.bas cfb -- \
'        ter-u16n.bdf GMBasic/native/gmb_runtime_c/src/gmb_gfx_cellfont_bitmap.h
'
' Il file generato e' un HEADER con la sola tabella e l'avviso di licenza OFL.
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_cp(256) AS LONG
DIM g_w(256) AS LONG
DIM g_h(256) AS LONG
DIM g_xoff(256) AS LONG
DIM g_yoff(256) AS LONG
DIM g_nrows(256) AS LONG
DIM g_rows(256, 32) AS LONG
DIM g_ng AS LONG

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION FieldAt(s AS STRING, k AS LONG) AS STRING
  DIM parts$() AS STRING, all$() AS STRING, i AS LONG, n AS LONG
  parts$ = SPLIT(TrimS(s), " ")
  all$ = parts$
  n = 0
  FOR i = 0 TO UBOUND(all$)
    IF LEN(all$) > 0 THEN
      IF n = k THEN RETURN all$(i)
      n = n + 1
    END IF
  NEXT i
  RETURN ""
END FUNCTION

' Split su spazi multipli: restituisce il k-esimo token (0-based).
FUNCTION TokAt(s AS STRING, k AS LONG) AS STRING
  DIM i AS LONG, n AS LONG, st AS LONG, idx AS LONG
  i = 1
  idx = 0
  DO WHILE i <= LEN(s)
    DO WHILE i <= LEN(s) AND (MID$(s, i, 1) = " " OR MID$(s, i, 1) = CHR$(9))
      i = i + 1
    LOOP
    IF i > LEN(s) THEN EXIT DO
    st = i
    DO WHILE i <= LEN(s) AND MID$(s, i, 1) <> " " AND MID$(s, i, 1) <> CHR$(9)
      i = i + 1
    LOOP
    IF idx = k THEN RETURN MID$(s, st, i - st)
    idx = idx + 1
  LOOP
  RETURN ""
END FUNCTION

FUNCTION HexVal(h AS STRING) AS LONG
  DIM i AS LONG, v AS LONG, c AS LONG, ch AS STRING
  v = 0
  FOR i = 1 TO LEN(h)
    ch = UCASE$(MID$(h, i, 1))
    c = 0
    IF ch >= "0" AND ch <= "9" THEN c = ASC(ch) - 48
    IF ch >= "A" AND ch <= "F" THEN c = ASC(ch) - 55
    v = v * 16 + c
  NEXT i
  RETURN v
END FUNCTION

FUNCTION Hex2(v AS LONG) AS STRING
  DIM d AS STRING, hi AS LONG, lo AS LONG
  d = "0123456789ABCDEF"
  hi = (v \ 16) MOD 16
  lo = v MOD 16
  RETURN MID$(d, hi + 1, 1) + MID$(d, lo + 1, 1)
END FUNCTION

SUB AddGlyph(cp AS LONG, w AS LONG, h AS LONG, xoff AS LONG, yoff AS LONG, n AS LONG)
  DIM i AS LONG
  IF g_ng >= 256 THEN EXIT SUB
  g_cp(g_ng) = cp
  g_w(g_ng) = w
  g_h(g_ng) = h
  g_xoff(g_ng) = xoff
  g_yoff(g_ng) = yoff
  g_nrows(g_ng) = n
  FOR i = 0 TO n - 1
    g_rows(g_ng, i) = g_rows(255, i)
  NEXT i
  g_ng = g_ng + 1
END SUB

FUNCTION GlyphIndex(cp AS LONG) AS LONG
  DIM i AS LONG
  FOR i = 0 TO g_ng - 1
    IF g_cp(i) = cp THEN RETURN i
  NEXT i
  RETURN -1
END FUNCTION

' ---- parse BDF ---------------------------------------------------------------
SUB ParseBdf(path AS STRING)
  DIM t AS STRING, ln$() AS STRING, i AS LONG, line AS STRING
  DIM enc AS LONG, bbxw AS LONG, bbxh AS LONG, xo AS LONG, yo AS LONG
  DIM inBmp AS LONG, n AS LONG, k AS LONG
  enc = -1
  bbxw = 0
  bbxh = 0
  xo = 0
  yo = 0
  inBmp = 0
  n = 0
  g_ng = 0
  t = IO.READFILE(path)
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN EXIT SUB
  FOR i = 0 TO UBOUND(ln$)
    line = TrimS(ln$(i))
    IF LEFT$(line, 9) = "ENCODING " THEN
      enc = VAL(TokAt(line, 1))
      n = 0
    ELSEIF LEFT$(line, 4) = "BBX " THEN
      bbxw = VAL(TokAt(line, 1))
      bbxh = VAL(TokAt(line, 2))
      xo = VAL(TokAt(line, 3))
      yo = VAL(TokAt(line, 4))
    ELSEIF line = "BITMAP" THEN
      inBmp = -1
      n = 0
    ELSEIF inBmp <> 0 THEN
      IF line = "ENDCHAR" THEN
        inBmp = 0
        IF enc >= 0 THEN
          AddGlyph(enc, bbxw, bbxh, xo, yo, n)
        END IF
        enc = -1
      ELSEIF LEN(line) > 0 THEN
        IF n < 32 THEN
          g_rows(255, n) = HexVal(line)
          n = n + 1
        END IF
      END IF
    END IF
  NEXT i
END SUB

' ---- posizionamento nella cella 8x16 -----------------------------------------
FUNCTION PlaceRow(gi AS LONG, rowIdx AS LONG) AS LONG
  DIM w AS LONG, h AS LONG, xo AS LONG, yo AS LONG, top AS LONG, y AS LONG
  DIM bits AS LONG, sh AS LONG
  w = g_w(gi)
  h = g_h(gi)
  xo = g_xoff(gi)
  yo = g_yoff(gi)
  IF yo < 0 THEN
    top = 16 - (h + (-yo))
  ELSE
    top = 16 - h
  END IF
  IF top < 0 THEN top = 0
  y = top + rowIdx
  IF y < 0 OR y > 15 THEN RETURN 0
  bits = g_rows(gi, rowIdx)
  IF w = 8 THEN RETURN bits AND 255
  IF w < 8 THEN
    sh = 8 - w - xo
    IF sh >= 0 THEN RETURN (bits * 2 ^ sh) AND 255
    RETURN (bits \ 2 ^ (-sh)) AND 255
  END IF
  RETURN (bits \ 2 ^ (w - 8)) AND 255
END FUNCTION

SUB Emit(s AS STRING)
  g_out = g_out + s + CHR$(10)
END SUB

DIM g_out AS STRING

FUNCTION Main() AS INTEGER
  DIM src AS STRING, dst AS STRING, i AS LONG, cp AS LONG, gi AS LONG
  DIM row AS LONG, body AS STRING, gch AS STRING, lbl AS STRING
  DIM missing AS LONG, missingList AS STRING
  DIM h AS LONG, wr AS LONG

  IF COMMANDCOUNT() < 2 THEN
    PRINT "uso: cellfont_from_bdf.bas <file.bdf> <header.h>"
    RETURN 2
  END IF
  src = COMMAND$(1)
  dst = COMMAND$(2)
  IF NOT FILEEXISTS(src) THEN
    PRINT "ERRORE: BDF non trovato: "; src
    RETURN 1
  END IF

  ParseBdf(src)

  missing = 0
  missingList = ""
  FOR cp = 32 TO 126
    IF GlyphIndex(cp) < 0 THEN
      missing = missing + 1
      IF missing <= 8 THEN missingList = missingList + " " + LTRIM$(STR$(cp))
    END IF
  NEXT cp
  IF missing > 0 THEN
    PRINT "ERRORE: mancano "; missing; " codepoint ASCII nel BDF:"; missingList
    RETURN 1
  END IF

  g_out = ""
  Emit("/* ============================================================================")
  Emit(" * gmb_gfx_cellfont_bitmap.h — tabella ASCII 8x16 del font a CELLE del TUI.")
  Emit(" *")
  Emit(" * GENERATO da tools/basic/cellfont_from_bdf.bas — NON modificare a mano.")
  Emit(" * Sorgente dei glifi: **Terminus** 8x16 (ter-u16n.bdf), Copyright (C) 2020")
  Emit(" * Dimitar Toshkov Zhekov, **SIL Open Font License 1.1** (testo in")
  Emit(" * Resources/licenses/OFL-1.1-Terminus.txt). Il bitmap qui sotto è opera derivata")
  Emit(" * del font e resta sotto la SIL OFL 1.1; il resto del repository è sotto PUL.")
  Emit(" *")
  Emit(" * Formato: [95][16], ASCII 32..126, 1 bit/px, MSB-left, riga 0 = alto.")
  Emit(" * ============================================================================ */")
  Emit("#ifndef GMB_GFX_CELLFONT_BITMAP_H")
  Emit("#define GMB_GFX_CELLFONT_BITMAP_H")
  Emit("")
  Emit("static const unsigned char gmb_cellfont_ascii[95][16] = {")

  FOR cp = 32 TO 126
    gi = GlyphIndex(cp)
    body = ""
    FOR row = 0 TO 15
      IF row > 0 THEN body = body + ","
      body = body + "0x" + Hex2(PlaceRow(gi, row))
    NEXT row
    gch = CHR$(cp)
    IF cp = 32 THEN
      lbl = " "
    ELSE
      lbl = gch
    END IF
    Emit("  {" + body + "}, /* " + LTRIM$(STR$(cp)) + " '" + lbl + "' */")
  NEXT cp

  Emit("};")
  Emit("")
  Emit("#endif /* GMB_GFX_CELLFONT_BITMAP_H */")
  Emit("")

  wr = SYSTEM.SHELLOUT("rm -f " + CHR$(34) + dst + CHR$(34))
  h = IO.FCREATE(dst)
  wr = IO.FWRITE(h, g_out)
  IO.FCLOSE(h)
  PRINT "scritto "; dst; " (95 glifi)"
  RETURN 0
END FUNCTION
