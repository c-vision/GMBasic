' ==============================================================================
' bsp_engine_parity.bas — parita' fra i due motori BSP (managed e C).
'
' I due motori eseguono la stessa pagina precompilata: il managed in una VM P-Code/IL
' dentro il processo .NET, il C in una VM/C. Lo stato e' UNO SOLO. Questo strumento,
' per ogni motore, imposta `bspEngine` in `.webapp.toml`, riavvia l'app, scarica le
' pagine, normalizza i campi dinamici (timestamp, uptime, pid) e confronta.
' Ripristina la configurazione originale alla fine (anche in caso di errore).
'
' Uso (dalla root del monorepo, gamba nativa):
'   tools/sh/webapp_up.sh start
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/bsp_engine_parity.bas bep -- \
'        index.bsp canvas.bsp wasm.bsp [--url http://127.0.0.1:8090] [--diff-lines 12]
'
' Uscita: 0 = tutte identiche, 1 = almeno una differenza.
' ==============================================================================
IMPORT IO
IMPORT SYSTEM
IMPORT HTTP

DIM g_root AS STRING
DIM g_toml AS STRING
DIM g_up AS STRING
DIM g_url AS STRING
DIM g_pages(64) AS STRING
DIM g_npages AS LONG
DIM g_diffLines AS LONG

DIM g_a(64) AS STRING
DIM g_b(64) AS STRING

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION IsDigitCh(ch AS STRING) AS LONG
  DIM c AS LONG
  IF LEN(ch) = 0 THEN RETURN 0
  c = ASC(ch)
  IF c >= 48 AND c <= 57 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION Digits(s AS STRING, i AS LONG, n AS LONG) AS LONG
  DIM k AS LONG
  IF i + n - 1 > LEN(s) THEN RETURN 0
  FOR k = i TO i + n - 1
    IF IsDigitCh(MID$(s, k, 1)) = 0 THEN RETURN 0
  NEXT k
  RETURN -1
END FUNCTION

FUNCTION DatetimeAt(s AS STRING, i AS LONG, BYREF nlen AS LONG) AS LONG
  DIM k AS LONG
  nlen = 0
  IF Digits(s, i, 4) = 0 THEN RETURN 0
  IF MID$(s, i + 4, 1) <> "-" THEN RETURN 0
  IF Digits(s, i + 5, 2) = 0 THEN RETURN 0
  IF MID$(s, i + 7, 1) <> "-" THEN RETURN 0
  IF Digits(s, i + 8, 2) = 0 THEN RETURN 0
  k = i + 10
  IF MID$(s, k, 1) <> "T" AND MID$(s, k, 1) <> " " THEN RETURN 0
  IF Digits(s, k + 1, 2) = 0 THEN RETURN 0
  IF MID$(s, k + 3, 1) <> ":" THEN RETURN 0
  IF Digits(s, k + 4, 2) = 0 THEN RETURN 0
  IF MID$(s, k + 6, 1) <> ":" THEN RETURN 0
  IF Digits(s, k + 7, 2) = 0 THEN RETURN 0
  k = k + 9
  IF MID$(s, k, 1) = "." THEN
    k = k + 1
    DO WHILE k <= LEN(s) AND IsDigitCh(MID$(s, k, 1)) <> 0
      k = k + 1
    LOOP
  END IF
  IF MID$(s, k, 1) = "Z" THEN k = k + 1
  nlen = k - i
  RETURN -1
END FUNCTION

FUNCTION TimeAt(s AS STRING, i AS LONG, BYREF nlen AS LONG) AS LONG
  nlen = 0
  IF Digits(s, i, 2) = 0 THEN RETURN 0
  IF MID$(s, i + 2, 1) <> ":" THEN RETURN 0
  IF Digits(s, i + 3, 2) = 0 THEN RETURN 0
  IF MID$(s, i + 5, 1) <> ":" THEN RETURN 0
  IF Digits(s, i + 6, 2) = 0 THEN RETURN 0
  nlen = 8
  RETURN -1
END FUNCTION

FUNCTION NormText(s AS STRING) AS STRING
  DIM out AS STRING, i AS LONG, n AS LONG, ln AS LONG, dlen AS LONG
  i = 1
  out = ""
  DO WHILE i <= LEN(s)
    ' ISO datetime
    dlen = 0
    IF DatetimeAt(s, i, dlen) <> 0 THEN
      out = out + "<TS>"
      i = i + dlen
    ELSE
      ' ora sola (non preceduta da cifra o '-')
      ln = 0
      IF TimeAt(s, i, ln) <> 0 THEN
        IF i = 1 OR (IsDigitCh(MID$(s, i - 1, 1)) = 0 AND MID$(s, i - 1, 1) <> "-" AND MID$(s, i - 1, 1) <> ":") THEN
          out = out + "<TIME>"
          i = i + ln
        ELSE
          out = out + MID$(s, i, 1)
          i = i + 1
        END IF
      ELSEIF MID$(s, i, 3) = "up " AND Digits(s, i + 3, 1) <> 0 THEN
        n = i + 3
        DO WHILE n <= LEN(s) AND IsDigitCh(MID$(s, n, 1)) <> 0
          n = n + 1
        LOOP
        IF MID$(s, n, 1) = "s" THEN
          out = out + "up <N>s"
          i = n + 1
        ELSE
          out = out + MID$(s, i, 1)
          i = i + 1
        END IF
      ELSEIF MID$(s, i, 4) = "pid " AND Digits(s, i + 4, 1) <> 0 THEN
        n = i + 4
        DO WHILE n <= LEN(s) AND IsDigitCh(MID$(s, n, 1)) <> 0
          n = n + 1
        LOOP
        out = out + "pid <N>"
        i = n
      ELSE
        out = out + MID$(s, i, 1)
        i = i + 1
      END IF
    END IF
  LOOP
  RETURN out
END FUNCTION

' ---- toml -------------------------------------------------------------------
SUB SetEngine(engine AS STRING)
  DIM t AS STRING, ln$() AS STRING, i AS LONG, out AS STRING, p AS LONG, h AS LONG, wr AS LONG
  t = IO.READFILE(g_toml)
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN EXIT SUB
  out = ""
  FOR i = 0 TO UBOUND(ln$)
    p = INSTR(ln$(i), "bspEngine")
    IF p > 0 AND LEFT$(LTRIM$(ln$(i)), 9) = "bspEngine" THEN
      out = out + "bspEngine = " + Q(engine) + CHR$(10)
    ELSE
      IF i < UBOUND(ln$) THEN out = out + ln$(i) + CHR$(10)
    END IF
  NEXT i
  wr = SYSTEM.SHELLOUT("rm -f " + Q(g_toml))
  h = IO.FCREATE(g_toml)
  wr = IO.FWRITE(h, out)
  IO.FCLOSE(h)
END SUB

FUNCTION Healthy() AS LONG
  DIM r AS STRING
  r = HTTP.GET(g_url + "/api/health")
  IF LEN(r) > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB RestartAndWait()
  DIM i AS LONG, wr AS LONG
  wr = SYSTEM.SHELLOUT(Q(g_up) + " restart >/dev/null 2>&1")
  FOR i = 1 TO 60
    IF Healthy() <> 0 THEN EXIT SUB
    wr = SYSTEM.SHELLOUT("sleep 0.5")
  NEXT i
END SUB

FUNCTION Fetch(url AS STRING) AS STRING
  RETURN NormText(HTTP.GET(url))
END FUNCTION

FUNCTION ReportDiff(page AS STRING, a AS STRING, b AS STRING) AS LONG
  DIM la$() AS STRING, lb$() AS STRING, i AS LONG, n AS LONG, shown AS LONG
  DIM am AS STRING, bm AS STRING
  la$ = SPLIT(a, CHR$(10))
  lb$ = SPLIT(b, CHR$(10))
  PRINT page; "  DIFF  (managed "; LEN(a); " byte, C "; LEN(b); " byte)"
  IF UBOUND(la$) < UBOUND(lb$) THEN n = UBOUND(lb$) ELSE n = UBOUND(la$)
  shown = 0
  FOR i = 0 TO n
    IF i > UBOUND(la$) OR i > UBOUND(lb$) OR la$(i) <> lb$(i) THEN
      IF shown < g_diffLines THEN
        IF i <= UBOUND(la$) THEN am = la$(i) ELSE am = "<assente>"
        IF i <= UBOUND(lb$) THEN bm = lb$(i) ELSE bm = "<assente>"
        PRINT "   riga "; i + 1; "  managed: "; am
        PRINT "   riga "; i + 1; "  c:       "; bm
        shown = shown + 1
      END IF
    END IF
  NEXT i
  IF shown >= g_diffLines THEN PRINT "   ..."
  RETURN 1
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM a AS STRING, i AS LONG, j AS LONG, fails AS LONG
  DIM backup AS STRING, wr AS LONG, eng AS STRING

  SET SHELL ON
  IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/webapp") THEN
    g_root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD()))
  ELSE
    g_root = IO.CWD()
  END IF
  g_toml = g_root + "/shared/webapp/.webapp.toml"
  g_up = g_root + "/tools/sh/webapp_up.sh"
  g_url = "http://127.0.0.1:8090"
  g_diffLines = 12
  g_npages = 0

  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--url" AND i < COMMANDCOUNT() THEN
      i = i + 1
      g_url = COMMAND$(i)
    ELSEIF a = "--diff-lines" AND i < COMMANDCOUNT() THEN
      i = i + 1
      g_diffLines = VAL(COMMAND$(i))
    ELSEIF LEFT$(a, 2) <> "--" THEN
      IF g_npages < 64 THEN
        g_pages(g_npages) = a
        g_npages = g_npages + 1
      END IF
    END IF
    i = i + 1
  LOOP
  IF g_npages = 0 THEN
    PRINT "uso: bsp_engine_parity.bas <pagina...> [--url URL] [--diff-lines N]"
    RETURN 2
  END IF
  IF NOT FILEEXISTS(g_toml) THEN
    PRINT "ERRORE: .webapp.toml non trovato: "; g_toml
    RETURN 2
  END IF

  backup = g_toml + ".parity-bak"
  wr = SYSTEM.SHELLOUT("cp " + Q(g_toml) + " " + Q(backup))

  fails = 0
  eng = "managed"
  SetEngine(eng)
  RestartAndWait
  FOR j = 0 TO g_npages - 1
    g_a(j) = Fetch(g_url + "/" + g_pages(j))
  NEXT j
  PRINT "reso con motore managed: "; g_npages; " pagine"

  eng = "c"
  SetEngine(eng)
  RestartAndWait
  FOR j = 0 TO g_npages - 1
    g_b(j) = Fetch(g_url + "/" + g_pages(j))
  NEXT j
  PRINT "reso con motore c: "; g_npages; " pagine"

  FOR j = 0 TO g_npages - 1
    IF g_a(j) = g_b(j) THEN
      PRINT g_pages(j); "  OK    ("; LEN(g_b(j)); " byte, identiche)"
    ELSE
      fails = fails + ReportDiff(g_pages(j), g_a(j), g_b(j))
    END IF
  NEXT j

  wr = SYSTEM.SHELLOUT("cp " + Q(backup) + " " + Q(g_toml))
  wr = SYSTEM.SHELLOUT("rm -f " + Q(backup))
  wr = SYSTEM.SHELLOUT(Q(g_up) + " restart >/dev/null 2>&1")

  IF fails > 0 THEN RETURN 1
  RETURN 0
END FUNCTION
