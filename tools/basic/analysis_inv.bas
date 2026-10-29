' analysis_inv.bas — bug hunter (GMBasic): copertura delle funzioni ANALYSIS
' (StdLib.Analysis.cs) nei MiniTest minitests/analysis/*.bas.
' Equivalente del vecchio analysis_inv.py, senza Python.
IMPORT IO
IMPORT GREP

DIM g_names(2000) AS STRING
DIM g_n AS INTEGER

FUNCTION ReadAll(path AS STRING) AS STRING
  DIM s AS STRING
  IF NOT FILEEXISTS(path) THEN RETURN ""
  OPEN path FOR INPUT AS #1
  s = IO.FINPUT(IO.LOF(1), 1)
  CLOSE #1
  ReadAll = s
END FUNCTION

SUB AddName(nm AS STRING)
  DIM i AS INTEGER
  nm = UPPER(nm)
  FOR i = 1 TO g_n
    IF g_names(i) = nm THEN EXIT SUB
  NEXT i
  IF g_n < 2000 THEN
    g_n = g_n + 1
    g_names(g_n) = nm
  END IF
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING
  DIM pref AS STRING
  DIM crateDir AS STRING
  DIM s AS STRING
  DIM p AS INTEGER
  DIM q AS INTEGER
  DIM r AS INTEGER
  DIM c AS STRING
  DIM nm AS STRING
  DIM files$()
  DIM f AS INTEGER
  DIM i AS INTEGER
  DIM covered AS INTEGER
  DIM uncovered AS INTEGER
  DIM txt AS STRING
  DIM found AS INTEGER

  root = IO.PARENTDIR(IO.CWD())
  IF FILEEXISTS(root + "/common/Ast/TokenKind.cs") THEN
    pref = root + "/"
  ELSE
    pref = root + "/../GMBasicSharp/"
  END IF
  IF IO.DIREXISTS(root + "/minitests/analysis") THEN
    crateDir = root + "/minitests/analysis"
  ELSEIF IO.DIREXISTS(root + "/shared/minitests/analysis") THEN
    crateDir = root + "/shared/minitests/analysis"
  ELSE
    crateDir = root + "/../shared/minitests/analysis"
  END IF

  g_n = 0
  s = ReadAll(pref + "common/StdLib/StdLib.Analysis.cs")
  p = 1
  DO
    p = INSTR(p, s, "T(TokenKind.")
    IF p = 0 THEN EXIT DO
    q = INSTR(p, s, CHR$(34))
    IF q = 0 THEN EXIT DO
    q = q + 1
    nm = ""
    DO WHILE q <= LEN(s)
      c = MID$(s, q, 1)
      IF c = CHR$(34) THEN EXIT DO
      IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") THEN
        nm = nm + c : q = q + 1
      ELSE
        EXIT DO
      END IF
    LOOP
    IF LEN(nm) > 0 THEN AddName(nm)
    p = p + 11
  LOOP

  files$ = GREP.FILES("EXPECT", crateDir)
  covered = 0 : uncovered = 0
  PRINT "TOTALE funzioni: "; g_n
  FOR i = 1 TO g_n
    found = 0
    FOR f = 0 TO UBOUND(files$)
      txt = UPPER(ReadAll(files$(f)))
      IF INSTR(txt, g_names(i)) > 0 THEN found = 1 : EXIT FOR
    NEXT f
    IF found THEN
      covered = covered + 1
      PRINT "COPERTO  "; g_names(i)
    ELSE
      uncovered = uncovered + 1
      PRINT "----     "; g_names(i)
    END IF
  NEXT i
  PRINT "COPERTI: "; covered; "  | NON COPERTI: "; uncovered
  RETURN uncovered
END FUNCTION
