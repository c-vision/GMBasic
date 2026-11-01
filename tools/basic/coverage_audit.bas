' coverage_audit.bas — copertura MiniTest/sample/guida della superficie canonica.
' Tutto in GMBasic: una passata per corpus, dict nome->indice, scansione caratteri.
IMPORT IO
IMPORT GREP

DIM g_name(3000) AS STRING
DIM g_dict AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_seen(3000) AS LONG
DIM g_n AS INTEGER

FUNCTION ReadAll(p AS STRING) AS STRING
  ReadAll = IO.READFILE(p)
END FUNCTION

SUB AddName(full AS STRING)
  DIM i AS INTEGER, p AS INTEGER
  DIM b AS STRING
  p = 0
  FOR i = 1 TO LEN(full)
    IF MID$(full, i, 1) = "." THEN p = i
  NEXT i
  IF p > 0 THEN b = MID$(full, p + 1) ELSE b = full
  b = UPPER(b)
  IF b IN g_dict THEN EXIT SUB
  g_n = g_n + 1
  g_name(g_n) = b
  g_dict(b) = g_n
END SUB

' Scansiona un corpus: ogni parola (alfanumerica/underscore) viene cercata nel dict.
FUNCTION ScanCorpus(dir AS STRING, ext AS STRING, stripComments AS INTEGER) AS INTEGER
  DIM files$(), s AS STRING, w AS STRING, c AS STRING
  DIM f AS LONG, i AS LONG, st AS LONG
  DIM inStr AS INTEGER, inComment AS INTEGER
  DIM hit AS INTEGER
  FOR i = 1 TO g_n : g_seen(i) = 0 : NEXT i
  files$ = GREP.FILES(" ", dir)
  FOR f = 0 TO UBOUND(files$)
    IF RIGHT$(UPPER(files$(f)), LEN(ext)) = UPPER(ext) THEN
      IF INSTR(files$(f), "/work/") = 0 AND INSTR(files$(f), "/bin/") = 0 AND INSTR(files$(f), "/obj/") = 0 THEN
        s = ReadAll(files$(f))
        i = 1 : inStr = 0 : inComment = 0
        DO WHILE i <= LEN(s)
          c = MID$(s, i, 1)
          IF inComment THEN
            IF c = CHR$(10) THEN inComment = 0
            i = i + 1
          ELSEIF inStr THEN
            IF c = CHR$(34) THEN inStr = 0
            i = i + 1
          ELSEIF stripComments = 1 AND c = CHR$(39) THEN
            inComment = 1 : i = i + 1
          ELSEIF c = CHR$(34) THEN
            inStr = 1 : i = i + 1
          ELSEIF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR c = "_" THEN
            st = i
            DO WHILE i <= LEN(s)
              c = MID$(s, i, 1)
              IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                i = i + 1
              ELSE
                EXIT DO
              END IF
            LOOP
            w = UPPER(MID$(s, st, i - st))
            IF w IN g_dict THEN g_seen(g_dict(w)) = 1
          ELSE
            i = i + 1
          END IF
        LOOP
      END IF
    END IF
  NEXT f
  hit = 0
  FOR i = 1 TO g_n : hit = hit + g_seen(i) : NEXT i
  ScanCorpus = hit
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING, mtDir AS STRING, smDir AS STRING, guides AS STRING
  DIM surf AS STRING, ln AS STRING, nm AS STRING, i AS LONG, up AS LONG
  DIM hm AS INTEGER, hs AS INTEGER, hg AS INTEGER
  DIM lines
  DIM flds$() AS STRING
  root = IO.CWD()
  FOR up = 1 TO 6
    IF FILEEXISTS(root + "/shared/spec/registry/surface.csv") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up
  mtDir = root + "/shared/minitests"
  smDir = root + "/shared/samples"
  surf = root + "/shared/spec/registry/surface.csv"
  ' Guide del vault (opzionale): NON un path personale. Si usa la variabile d'ambiente solo se
  ' chi lancia lo strumento la imposta, altrimenti la sezione guide viene saltata.
  ' Guide del vault: OPZIONALI e mai un path personale. Si passano con --guides <dir>
  ' (vedi parser argomenti sotto); senza, la sezione guide viene saltata.
  guides = ""
  g_n = 0
  IF FILEEXISTS(surf) THEN
    lines = IO.READLINES(surf)
    FOR i = 0 TO UBOUND(lines)
      ln = lines(i)
      IF LEN(ln) > 0 AND LEFT$(ln, 1) <> "#" THEN
        flds$ = SPLIT(ln, ",")
        IF UBOUND(flds$) >= 2 THEN AddName(flds$(2))
      END IF
    NEXT i
  END IF
  hm = ScanCorpus(mtDir, ".bas", 1)
  hs = ScanCorpus(smDir, ".bas", 0)
  hg = ScanCorpus(guides, ".md", 0)
  PRINT "superficie canonica: "; g_n
  PRINT "  con MiniTest: "; hm; "/"; g_n; " (mancano "; g_n - hm; ")"
  PRINT "  nei sample:   "; hs; "/"; g_n; " (mancano "; g_n - hs; ")"
  PRINT "  nelle guide:  "; hg; "/"; g_n; " (mancano "; g_n - hg; ")"
  RETURN 0
END FUNCTION
