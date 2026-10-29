' minitest_census.bas — copertura della superficie canonica da parte dei MiniTest.
' Equivalente del vecchio tools/minitest_census.py, senza Python.
'
' Sorgente della superficie: shared/spec/registry/surface.csv (generato da
' tools/registry-export --all): kind,ns,name,kindName.
' Copertura: PROCEDURA = nome seguito da "(" nel codice (commenti ' rimossi);
' COMANDO/COSTANTE = nome presente come parola nel codice.
'
' Uso (dalla root del monorepo):
'   tools/basic/minitest_census.bas
'
' Nota: i commenti REM non vengono rimossi (rari); i commenti ' sí.
IMPORT IO
IMPORT GREP

DIM g_kind(4000) AS STRING
DIM g_ns(4000) AS STRING
DIM g_name(4000) AS STRING
DIM g_n AS INTEGER

DIM g_calls AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_words AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER

DIM g_nsn(400) AS STRING
DIM g_nstot(400) AS LONG
DIM g_nscov(400) AS LONG
DIM g_nsno AS INTEGER

FUNCTION IsIdStart(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsIdChar(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

SUB NsIndex(ns AS STRING)
  DIM j AS INTEGER
  FOR j = 1 TO g_nsno
    IF g_nsn(j) = ns THEN EXIT SUB
  NEXT j
  IF g_nsno >= 400 THEN EXIT SUB
  g_nsno = g_nsno + 1
  g_nsn(g_nsno) = ns
  g_nstot(g_nsno) = 0
  g_nscov(g_nsno) = 0
END SUB

FUNCTION NsIndexOf(ns AS STRING) AS INTEGER
  DIM j AS INTEGER
  FOR j = 1 TO g_nsno
    IF g_nsn(j) = ns THEN RETURN j
  NEXT j
  RETURN 0
END FUNCTION

' Estrae dal codice gli identificatori e le chiamate (nome seguito da "(").
SUB ScanFile(path AS STRING)
  DIM s AS STRING, c AS STRING, w AS STRING
  DIM i AS LONG, st AS LONG
  DIM inStr AS INTEGER, inComment AS INTEGER
  s = IO.READFILE(path)
  IF LEN(s) = 0 THEN EXIT SUB
  i = 1 : inStr = 0 : inComment = 0
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF inComment THEN
      IF c = CHR$(10) THEN inComment = 0
      i = i + 1
    ELSEIF inStr THEN
      IF c = CHR$(34) THEN inStr = 0
      i = i + 1
    ELSEIF c = CHR$(39) THEN
      inComment = 1 : i = i + 1
    ELSEIF c = CHR$(34) THEN
      inStr = 1 : i = i + 1
    ELSEIF IsIdStart(c) THEN
      st = i
      DO WHILE i <= LEN(s)
        c = MID$(s, i, 1)
        IF IsIdChar(c) THEN i = i + 1 ELSE EXIT DO
      LOOP
      w = UPPER(MID$(s, st, i - st))
      IF NOT (w IN g_words) THEN g_words(w) = 1
      IF i <= LEN(s) THEN
        IF MID$(s, i, 1) = "(" THEN
          IF NOT (w IN g_calls) THEN g_calls(w) = 1
        END IF
      END IF
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, corpus AS STRING, surf AS STRING
  DIM lines
  DIM flds$() AS STRING
  DIM files$() AS STRING
  DIM i AS LONG, j AS LONG, p AS LONG
  DIM ln AS STRING, k AS STRING, ns AS STRING, nm AS STRING
  DIM procs AS LONG, proccov AS LONG
  DIM cmds AS LONG, cmdcov AS LONG
  DIM consts AS LONG, constcov AS LONG
  DIM idx AS LONG, miss AS LONG
  DIM topn AS LONG
  DIM topidx(400) AS LONG
  DIM topmiss(400) AS LONG
  DIM shown AS LONG

  root = IO.CWD()
  DIM up AS INTEGER
  FOR up = 1 TO 6
    IF FILEEXISTS(root + "/shared/spec/registry/surface.csv") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up
  surf = root + "/shared/spec/registry/surface.csv"
  corpus = root + "/shared/minitests"
  IF NOT FILEEXISTS(surf) THEN PRINT "ERRORE: surface.csv non trovato ("; surf; ")" : RETURN 1
  IF NOT DIREXISTS(corpus) THEN PRINT "ERRORE: corpus non trovato ("; corpus; ")" : RETURN 1

  ' --- superficie
  lines$ = IO.READLINES(surf)
  g_n = 0
  FOR i = 0 TO UBOUND(lines)
    ln = lines(i)
    IF LEN(ln) > 0 AND LEFT$(ln, 1) <> "#" THEN
      flds$ = SPLIT(ln, ",")
      IF UBOUND(flds$) >= 3 THEN
        k = flds$(0)
        IF k = "procedure" OR k = "command" OR k = "constant" THEN
          g_n = g_n + 1
          g_kind(g_n) = k
          g_ns(g_n) = flds$(1)
          g_name(g_n) = UPPER(flds$(2))
          NsIndex(g_ns(g_n))
        END IF
      END IF
    END IF
  NEXT i

  ' --- scansione corpus
  files$ = GREP.FILES(" ", corpus)
  FOR i = 0 TO UBOUND(files$)
    IF RIGHT$(UPPER(files$(i)), 4) = ".BAS" THEN
      IF INSTR(files$(i), "/work/") = 0 AND INSTR(files$(i), "/bin/") = 0 AND INSTR(files$(i), "/obj/") = 0 THEN
        ScanFile(files$(i))
      END IF
    END IF
  NEXT i

  ' --- copertura
  procs = 0 : proccov = 0 : cmds = 0 : cmdcov = 0 : consts = 0 : constcov = 0
  FOR i = 1 TO g_n
    idx = NsIndexOf(g_ns(i))
    IF g_kind(i) = "procedure" THEN
      procs = procs + 1
      g_nstot(idx) = g_nstot(idx) + 1
      IF g_name(i) IN g_calls THEN
        proccov = proccov + 1
        g_nscov(idx) = g_nscov(idx) + 1
      END IF
    ELSEIF g_kind(i) = "command" THEN
      cmds = cmds + 1
      g_nstot(idx) = g_nstot(idx) + 1
      IF g_name(i) IN g_words THEN
        cmdcov = cmdcov + 1
        g_nscov(idx) = g_nscov(idx) + 1
      END IF
    ELSE
      consts = consts + 1
      g_nstot(idx) = g_nstot(idx) + 1
      IF g_name(i) IN g_words THEN
        constcov = constcov + 1
        g_nscov(idx) = g_nscov(idx) + 1
      END IF
    END IF
  NEXT i

  PRINT "superficie: "; g_n; " (procedure "; procs; ", comandi "; cmds; ", costanti "; consts; ")"
  PRINT "MiniTest:"
  PRINT "  procedure coperte: "; proccov; "/"; procs; " (mancano "; procs - proccov; ")"
  PRINT "  comandi coperti:   "; cmdcov; "/"; cmds; " (mancano "; cmds - cmdcov; ")"
  PRINT "  costanti coperte:  "; constcov; "/"; consts; " (mancano "; consts - constcov; ")"

  ' --- namespace con più mancanti (top 15)
  FOR i = 1 TO g_nsno
    topidx(i) = i
    topmiss(i) = g_nstot(i) - g_nscov(i)
  NEXT i
  FOR i = 1 TO g_nsno - 1
    FOR j = i + 1 TO g_nsno
      IF topmiss(j) > topmiss(i) THEN
        idx = topidx(i) : topidx(i) = topidx(j) : topidx(j) = idx
        p = topmiss(i) : topmiss(i) = topmiss(j) : topmiss(j) = p
      END IF
    NEXT j
  NEXT i
  PRINT ""
  PRINT "namespace con più mancanti:"
  topn = g_nsno
  IF topn > 15 THEN topn = 15
  FOR i = 1 TO topn
    idx = topidx(i)
    PRINT "  "; g_nsn(idx); ": "; g_nscov(idx); "/"; g_nstot(idx); " (mancano "; topmiss(i); ")"
  NEXT i

  ' --- primi 40 nomi non coperti (procedure)
  PRINT ""
  PRINT "prime procedure non coperte:"
  shown = 0
  FOR i = 1 TO g_n
    IF g_kind(i) = "procedure" AND NOT (g_name(i) IN g_calls) THEN
      PRINT "  "; g_ns(i); "."; g_name(i)
      shown = shown + 1
      IF shown >= 40 THEN EXIT FOR
    END IF
  NEXT i
  RETURN 0
END FUNCTION
