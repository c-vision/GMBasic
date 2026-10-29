' gmb-stage-hunks.bas — mette in stage SOLO gli hunk di un file che contengono almeno
' uno dei marker indicati. Serve nel working tree condiviso da piu' AI: si committano
' le proprie modifiche senza toccare quelle non committate di altre sessioni.
'
' Uso: gmb-stage-hunks.bas <file> <marker> [<marker>...]
'   Esempio: gmb-stage-hunks.bas native/gmb_c99_emit.c gmb_main_rc
'
' Exit: 0 = almeno un hunk in stage; 1 = nessun diff/hunk o git apply fallito;
'       2 = uso errato o SHELL non disponibile.
SET SHELL ON

FUNCTION Main() AS INTEGER
  DIM narg AS INTEGER
  DIM path AS STRING
  DIM tmp AS STRING
  DIM diff AS STRING
  DIM lines(20000) AS STRING
  DIM keep(20000) AS INTEGER
  DIM nlines AS INTEGER
  DIM i AS INTEGER
  DIM j AS INTEGER
  DIM k AS INTEGER
  DIM firstH AS INTEGER
  DIM hs AS INTEGER
  DIM he AS INTEGER
  DIM sel AS INTEGER
  DIM total AS INTEGER
  DIM chosen AS INTEGER
  DIM ok AS INTEGER
  DIM ln AS STRING
  DIM m AS STRING
  DIM cmd AS STRING
  DIM q AS STRING
  DIM rc AS LONG

  narg = COMMANDCOUNT()
  IF narg < 2 THEN
    PRINT "uso: gmb-stage-hunks.bas <file> <marker> [<marker>...]"
    RETURN 2
  END IF
  path = COMMAND(1)
  q = CHR$(34)

  cmd = "git diff -- " + q + path + q
  diff = SYSTEM.SHELLOUT(cmd)
  IF LEN(TRIM$(diff)) = 0 THEN
    PRINT "[stage-hunks] nessun diff per "; path
    RETURN 1
  END IF

  diff = REPLACE(diff, CHR$(13), "")
  nlines = 0
  DO WHILE nlines < 20000
    k = INSTR(diff, CHR$(10))
    IF k = 0 THEN
      IF LEN(diff) > 0 THEN lines(nlines) = diff : nlines = nlines + 1
      EXIT DO
    END IF
    lines(nlines) = LEFT$(diff, k - 1)
    nlines = nlines + 1
    diff = MID$(diff, k + 1)
    IF LEN(diff) = 0 THEN EXIT DO
  LOOP
  IF LEN(diff) > 0 THEN
    PRINT "[stage-hunks] diff troppo grande (>20000 righe): "; path
    RETURN 1
  END IF

  firstH = -1
  total = 0
  FOR i = 0 TO nlines - 1
    IF LEFT$(lines(i), 2) = "@@" THEN
      IF firstH < 0 THEN firstH = i
      total = total + 1
    END IF
  NEXT i
  IF firstH < 0 THEN
    PRINT "[stage-hunks] nessun hunk in "; path
    RETURN 1
  END IF

  ' seleziona gli hunk che contengono almeno un marker
  sel = 0
  FOR i = firstH TO nlines - 1
    IF LEFT$(lines(i), 2) = "@@" THEN
      hs = i
      he = nlines - 1
      FOR j = i + 1 TO nlines - 1
        IF LEFT$(lines(j), 2) = "@@" THEN he = j - 1 : EXIT FOR
      NEXT j
      ok = 0
      FOR k = 2 TO narg
        m = COMMAND(k)
        IF LEN(m) > 0 THEN
          FOR j = hs TO he
            IF INSTR(lines(j), m) > 0 THEN ok = 1 : EXIT FOR
          NEXT j
        END IF
        IF ok THEN EXIT FOR
      NEXT k
      IF ok THEN
        keep(hs) = 1
        sel = sel + 1
      END IF
    END IF
  NEXT i

  IF sel = 0 THEN
    PRINT "[stage-hunks] nessun hunk con i marker indicati in "; path
    RETURN 1
  END IF

  tmp = TEMPFILE()
  OPEN tmp FOR OUTPUT AS #1
  FOR i = 0 TO firstH - 1
    IF LEN(lines(i)) > 0 THEN PRINT #1, lines(i)
  NEXT i
  FOR i = firstH TO nlines - 1
    IF LEFT$(lines(i), 2) = "@@" THEN chosen = keep(i)
    IF chosen = 1 AND LEN(lines(i)) > 0 THEN PRINT #1, lines(i)
  NEXT i
  CLOSE #1

  cmd = "git apply --cached " + q + tmp + q
  rc = SYSTEM.SHELLCODE(cmd)
  IF rc <> 0 THEN
    PRINT "[stage-hunks] git apply --cached FALLITO: "; path
    KILL tmp
    RETURN 1
  END IF
  KILL tmp
  PRINT "[stage-hunks] "; path; ": "; sel; "/"; total; " hunk in stage"
  RETURN 0
END FUNCTION
