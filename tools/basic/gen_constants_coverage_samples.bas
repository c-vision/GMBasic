' gen_constants_coverage_samples.bas — genera i sample di copertura delle costanti globali
' (da gen_constants_coverage_samples.py). Uso: --root DIR [--out DIR] [--guide-dir DIR]
IMPORT IO
IMPORT GREP

DIM g_name(400) AS STRING
DIM g_sect(400) AS STRING
DIM g_expr(400) AS STRING
DIM g_cnt AS LONG
DIM g_auto AS STRING

SUB AddConst(n AS STRING, s AS STRING, e AS STRING)
  g_cnt = g_cnt + 1
  g_name(g_cnt) = n
  g_sect(g_cnt) = s
  g_expr(g_cnt) = e
END SUB

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

SUB WriteOut(p AS STRING, t AS STRING)
  DIM h AS LONG, d AS STRING, i AS LONG
  i = LEN(p)
  DO WHILE i > 1
    IF MID$(p, i, 1) = "/" THEN
      d = LEFT$(p, i - 1)
      IF NOT DIREXISTS(d) THEN MKDIR d
      EXIT DO
    END IF
    i = i - 1
  LOOP
  h = IO.FCREATE(p)
  IO.FWRITE(h, t)
  IO.FCLOSE(h)
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, out AS STRING, guide AS STRING
  DIM cf AS STRING, ls$() AS STRING, l AS STRING, s AS STRING
  DIM i AS LONG, p AS LONG, q AS LONG, j AS LONG, h AS LONG
  DIM n AS STRING, e AS STRING, block AS STRING, sec AS STRING
  DIM colors AS STRING, keys AS STRING, modes AS STRING, txt AS STRING
  DIM nc AS LONG, nk AS LONG
  root = IO.CWD() : out = "" : guide = ""
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--root" AND i < COMMANDCOUNT() THEN i = i + 1 : root = COMMAND$(i)
    IF COMMAND$(i) = "--out" AND i < COMMANDCOUNT() THEN i = i + 1 : out = COMMAND$(i)
    IF COMMAND$(i) = "--guide-dir" AND i < COMMANDCOUNT() THEN i = i + 1 : guide = COMMAND$(i)
    i = i + 1
  LOOP
  IF LEN(out) = 0 THEN out = root + "/samples/Community"
  cf = root + "/common/StdLib/StdLib.Constants.cs"
  IF NOT FILEEXISTS(cf) THEN cf = root + "/GMBasicSharp/common/StdLib/StdLib.Constants.cs"
  IF NOT FILEEXISTS(cf) THEN PRINT "non trovato: StdLib.Constants.cs" : RETURN 2
  g_auto = "GENERATO da tools/basic/gen_constants_coverage_samples.bas — non modificare a mano."
  ls$ = IO.READLINES(cf)
  block = ""
  g_cnt = 0
  FOR i = 0 TO UBOUND(ls$)
    l = TRIM$(ls$(i))
    IF LEFT$(l, 2) = "//" THEN
      s = TRIM$(MID$(l, 3, LEN(l) - 2))
      IF LEN(s) > 0 THEN
        block = s
      END IF
    ELSE
      p = 1
      DO WHILE p <= LEN(l)
        p = INSTR(p, l, "AddConst(")
        IF p = 0 THEN EXIT DO
        q = INSTR(p, l, CHR$(34))
        IF q = 0 THEN EXIT DO
        j = INSTR(q + 1, l, CHR$(34))
        IF j = 0 THEN EXIT DO
        n = MID$(l, q + 1, j - q - 1)
        p = INSTR(j, l, ",")
        e = ""
        IF p > 0 THEN
          q = INSTR(p, l, ")")
          IF q > p THEN e = TRIM$(MID$(l, p + 1, q - p - 1))
        END IF
        AddConst(n, block, e)
        p = j + 1
      LOOP
    END IF
  NEXT i
  ' colori: sezione ex-COLOR, esclusi K_* e set pulsanti
  colors = "" : keys = "" : modes = ""
  FOR i = 1 TO g_cnt
    IF LEFT$(g_name(i), 2) = "K_" THEN
      keys = keys + "    knum = knum + 1 : kn(knum) = " + Q(g_name(i)) + " : kc(knum) = %" + g_name(i) + CHR$(10)
    ELSEIF INSTR(LOWER$(g_sect(i)), "ex-color") > 0 THEN
      IF g_name(i) <> "OK" AND g_name(i) <> "CANCEL" AND g_name(i) <> "OKCANCEL" AND g_name(i) <> "YESNO" AND g_name(i) <> "ABORT" AND g_name(i) <> "RETRY" AND g_name(i) <> "IGNORE" AND g_name(i) <> "YES" AND g_name(i) <> "NO" AND g_name(i) <> "YESNOCANCEL" AND g_name(i) <> "ABORTRETRYIGNORE" AND g_name(i) <> "RETRYCANCEL" AND g_name(i) <> "CLOSED" AND g_name(i) <> "NONE" AND g_name(i) <> "SINGLE" AND g_name(i) <> "DOUBLE" AND g_name(i) <> "ROUNDED" AND g_name(i) <> "LEFT" AND g_name(i) <> "CENTER" AND g_name(i) <> "RIGHT" THEN
        colors = colors + "    cnt = cnt + 1 : cn(cnt) = " + Q(g_name(i)) + " : ci(cnt) = %" + g_name(i) + CHR$(10)
        nc = nc + 1
      ELSE
        modes = modes + "    PRINT " + Q("  %" + g_name(i)) + "; TAB(24); %" + g_name(i) + CHR$(10)
      END IF
    ELSEIF INSTR(LOWER$(g_sect(i)), "ex-console") > 0 OR INSTR(LOWER$(g_sect(i)), "ex-joy") > 0 OR INSTR(LOWER$(g_sect(i)), "ex-io") > 0 OR INSTR(LOWER$(g_sect(i)), "capability") > 0 OR INSTR(LOWER$(g_sect(i)), "reranker") > 0 OR INSTR(LOWER$(g_sect(i)), "browser") > 0 OR INSTR(LOWER$(g_sect(i)), "governance") > 0 OR INSTR(LOWER$(g_sect(i)), "allineamento") > 0 OR INSTR(LOWER$(g_sect(i)), "ricezione") > 0 THEN
      modes = modes + "    PRINT " + Q("  %" + g_name(i)) + "; TAB(24); %" + g_name(i) + CHR$(10)
    ELSE
      modes = modes + "    PRINT " + Q("  %" + g_name(i)) + "; TAB(24); %" + g_name(i) + CHR$(10)
    END IF
  NEXT i
  ' colori
  txt = "' FILE: Console/ConstantsColorsCoverage.bas" + CHR$(10) + "' " + g_auto + CHR$(10) + "IMPORT CONSOLE" + CHR$(10) + CHR$(10) + "DIM cn(300) AS STRING" + CHR$(10) + "DIM ci(300) AS INTEGER" + CHR$(10) + "DIM cnt AS INTEGER" + CHR$(10) + CHR$(10) + "FUNCTION ColorIndex(nm AS STRING) AS INTEGER" + CHR$(10) + "  DIM i AS INTEGER" + CHR$(10) + "  FOR i = 1 TO cnt" + CHR$(10) + "    IF cn(i) = UPPER$(nm) THEN ColorIndex = ci(i) : EXIT FUNCTION" + CHR$(10) + "  NEXT i" + CHR$(10) + "  ColorIndex = 0" + CHR$(10) + "END FUNCTION" + CHR$(10) + CHR$(10) + "SUB Main()" + CHR$(10) + "  DIM i AS INTEGER, j AS INTEGER, t AS INTEGER, s AS STRING" + CHR$(10) + "  cnt = 0" + CHR$(10) + colors + "  FOR i = 1 TO cnt - 1" + CHR$(10) + "    FOR j = i + 1 TO cnt" + CHR$(10) + "      IF ci(j) < ci(i) THEN t = ci(i) : ci(i) = ci(j) : ci(j) = t : s = cn(i) : cn(i) = cn(j) : cn(j) = s" + CHR$(10) + "    NEXT j" + CHR$(10) + "  NEXT i" + CHR$(10) + "  PRINT " + Q("Costanti colore nominate (ANSI-256): ") + "; cnt" + CHR$(10) + "END SUB" + CHR$(10)
  WriteOut(out + "/Console/ConstantsColorsCoverage.bas", txt)
  ' tasti
  txt = "' FILE: Console/ConstantsKeysCoverage.bas" + CHR$(10) + "' " + g_auto + CHR$(10) + "IMPORT CONSOLE" + CHR$(10) + CHR$(10) + "DIM kn(200) AS STRING" + CHR$(10) + "DIM kc(200) AS INTEGER" + CHR$(10) + "DIM knum AS INTEGER" + CHR$(10) + CHR$(10) + "FUNCTION KeyName(code AS INTEGER) AS STRING" + CHR$(10) + "  DIM i AS INTEGER" + CHR$(10) + "  FOR i = 1 TO knum" + CHR$(10) + "    IF kc(i) = code THEN KeyName = kn(i) : EXIT FUNCTION" + CHR$(10) + "  NEXT i" + CHR$(10) + "  KeyName = " + CHR$(34) + CHR$(34) + CHR$(10) + "END FUNCTION" + CHR$(10) + CHR$(10) + "SUB Main()" + CHR$(10) + "  DIM i AS INTEGER, j AS INTEGER, t AS INTEGER, s AS STRING" + CHR$(10) + "  knum = 0" + CHR$(10) + keys + "  PRINT " + Q("Costanti tasto (%K_*): ") + "; knum" + CHR$(10) + "END SUB" + CHR$(10)
  WriteOut(out + "/Console/ConstantsKeysCoverage.bas", txt)
  ' modi
  txt = "' FILE: Console/ConstantsModesCoverage.bas" + CHR$(10) + "' " + g_auto + CHR$(10) + "IMPORT CONSOLE" + CHR$(10) + CHR$(10) + "SUB Main()" + CHR$(10) + modes + "END SUB" + CHR$(10)
  WriteOut(out + "/Console/ConstantsModesCoverage.bas", txt)
  IF LEN(guide) > 0 THEN
    txt = "# Guida utente — Costanti globali (`%NOME`)" + CHR$(10) + CHR$(10) + "> " + g_auto + CHR$(10)
    FOR i = 1 TO g_cnt
      txt = txt + "- \`%" + g_name(i) + "\` = " + g_expr(i) + "  (" + g_sect(i) + ")" + CHR$(10)
    NEXT i
    WriteOut(guide + "/Costanti-Globali-Guida-Utente.md", txt)
  END IF
  PRINT "scritti 3 sample in "; out + "/Console"
  RETURN 0
END FUNCTION
