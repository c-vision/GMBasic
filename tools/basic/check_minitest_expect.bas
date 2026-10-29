' check_minitest_expect.bas — GATE STATICO degli oracoli `' EXPECT:` dei MiniTest.
' Porta fedele di tools/check_minitest_expect.py (repo GMBasic), senza Python.
'
' PERCHE' ESISTE
' L'oracolo di un MiniTest è la riga
'
'     ' EXPECT: <riga1>|<riga2>|...
'
' e viene letto da DUE consumer, con la STESSA regola stretta:
'   * tests/GMBASIC.IlBackend.Tests/MiniTestsParityTests.cs -> ReadExpect():
'     prima riga che, dopo TrimStart(), inizia ESATTAMENTE con "' EXPECT: ";
'   * tools/sh/test_minitests_c99native -> grep -m1 "^' EXPECT: " (a colonna 0).
' Ogni deviazione NON è un errore visibile: il file smette semplicemente di avere un
' oracolo e il test torna a verificare solo l'exit code. Questo gate rende visibili i
' difetti senza eseguire nulla (nessuna build, nessun MiniTest).
'
' CHECKS (errore = exit 1)
'   E1 oracolo dichiarato in prosa nell'header ma nessuna riga canonica
'   E2 marker malformato (tentativo di oracolo non canonico)
'   E3 marker fuori dall'header (dopo la prima riga eseguibile)
'   E4 marker multiplo (l'harness legge solo il primo)
'   E5 oracolo vuoto
'   E6 test negativo non a colonna 0 (il gate C99Native usa grep ancorato a ^)
'   E7 oracolo con separatore '|' finale (l'harness non lo produce mai)
' WARNING (non fanno fallire, salvo --strict)
'   W1 nessun oracolo; esentati se l'header dichiara `' TAG: library|interactive|heavy`
'      oppure "ENV-GATED".
'
' Uso (dalla root del monorepo):
'   tools/basic/check_minitest_expect.bas [--strict] [--no-oracle] [--dir DIR]
' Exit: 0 pulito · 1 difetti · 2 radice non trovata.
'
' DIFFERENZE DICHIARATE rispetto al .py (reference invariata nella semantica):
'   * `--dir` ha default `shared/minitests` (nel monorepo il corpus sta lì, non in
'     `minitests/` alla radice come nel repo originale);
'   * `--json` non è portato: il gate serve come exit code + righe leggibili.
IMPORT IO
IMPORT GREP

DIM g_marker AS STRING
DIM g_negative AS STRING

' findings
DIM g_n AS LONG
DIM g_code(8000) AS STRING
DIM g_path(8000) AS STRING
DIM g_line(8000) AS LONG
DIM g_det(8000) AS STRING
DIM g_iserr(8000) AS INTEGER
DIM g_fi(8000) AS LONG

' file del corpus, ordinati per path (come `sorted(rglob)`)
DIM g_nf AS LONG
DIM g_file(2600) AS STRING
DIM g_rel(2600) AS STRING

' ── helper di stringa ─────────────────────────────────────────────────────────

FUNCTION IsSpace(c AS STRING) AS INTEGER
  IF c = " " OR c = CHR$(9) OR c = CHR$(13) THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION Lst(s AS STRING) AS STRING
  DIM i AS LONG
  i = 1
  DO WHILE i <= LEN(s)
    IF NOT IsSpace(MID$(s, i, 1)) THEN EXIT DO
    i = i + 1
  LOOP
  RETURN MID$(s, i)
END FUNCTION

FUNCTION Rst(s AS STRING) AS STRING
  DIM i AS LONG
  i = LEN(s)
  DO WHILE i >= 1
    IF NOT IsSpace(MID$(s, i, 1)) THEN EXIT DO
    i = i - 1
  LOOP
  RETURN LEFT$(s, i)
END FUNCTION

FUNCTION IsAlnum(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

' Ricerca SENSIBILE al caso (INSTR del linguaggio è case-insensitive: qui serve
' l'equivalente di `sub in s` del Python, es. "EXPECT: ERROR:" NON deve combaciare
' con "EXPECT: Error:"). Ritorna la posizione 1-based, 0 se assente.
FUNCTION FindSens(hay AS STRING, needle AS STRING, start AS LONG) AS LONG
  DIM i AS LONG, n AS LONG, m AS LONG
  n = LEN(hay) : m = LEN(needle)
  IF m = 0 OR n = 0 THEN RETURN 0
  IF start < 1 THEN start = 1
  FOR i = start TO n - m + 1
    IF MID$(hay, i, m) = needle THEN RETURN i
  NEXT i
  RETURN 0
END FUNCTION

' Sottostringa a confini di parola (come \b...\b), case-insensitive.
FUNCTION HasWord(hay AS STRING, needle AS STRING) AS INTEGER
  DIM H AS STRING, N AS STRING, p AS LONG
  DIM okA AS INTEGER, okB AS INTEGER
  H = UCASE(hay) : N = UCASE(needle)
  IF LEN(N) = 0 THEN RETURN 0
  p = INSTR(1, H, N)
  DO WHILE p > 0
    okA = -1 : okB = -1
    IF p > 1 THEN
      IF IsAlnum(MID$(H, p - 1, 1)) THEN okA = 0
    END IF
    IF p + LEN(N) <= LEN(H) THEN
      IF IsAlnum(MID$(H, p + LEN(N), 1)) THEN okB = 0
    END IF
    IF okA AND okB THEN RETURN -1
    p = INSTR(p + 1, H, N)
  LOOP
  RETURN 0
END FUNCTION

' MARKER canonico: dopo il lstrip deve iniziare ESATTAMENTE con "' EXPECT: ".
FUNCTION IsCanonical(s AS STRING) AS INTEGER
  DIM t AS STRING
  t = Lst(s)
  IF LEN(t) < LEN(g_marker) THEN RETURN 0
  IF LEFT$(t, LEN(g_marker)) = g_marker THEN RETURN -1
  RETURN 0
END FUNCTION

' Tentativo non canonico: apice + spazi + EXPECT + ':' in qualunque casing.
FUNCTION IsTryMarker(s AS STRING) AS INTEGER
  DIM i AS LONG, n AS LONG
  n = LEN(s)
  i = 1
  DO WHILE i <= n AND IsSpace(MID$(s, i, 1))
    i = i + 1
  LOOP
  IF i > n THEN RETURN 0
  IF MID$(s, i, 1) <> "'" THEN RETURN 0
  i = i + 1
  DO WHILE i <= n AND IsSpace(MID$(s, i, 1))
    i = i + 1
  LOOP
  IF UCASE(MID$(s, i, 6)) <> "EXPECT" THEN RETURN 0
  i = i + 6
  DO WHILE i <= n AND IsSpace(MID$(s, i, 1))
    i = i + 1
  LOOP
  IF i <= n THEN
    IF MID$(s, i, 1) = ":" THEN RETURN -1
  END IF
  RETURN 0
END FUNCTION

' `TAG: library|interactive|heavy` (case-insensitive, con \b finale).
FUNCTION HasTagExempt(s AS STRING) AS INTEGER
  DIM H AS STRING, p AS LONG, i AS LONG, w AS STRING, n AS LONG
  H = UCASE(s)
  p = INSTR(1, H, "TAG:")
  DO WHILE p > 0
    i = p + 4
    DO WHILE i <= LEN(H) AND IsSpace(MID$(H, i, 1))
      i = i + 1
    LOOP
    w = MID$(H, i, 11)
    IF LEFT$(w, 7) = "LIBRARY" THEN n = 7
    IF LEFT$(w, 11) = "INTERACTIVE" THEN n = 11
    IF LEFT$(w, 5) = "HEAVY" THEN n = 5
    IF n > 0 THEN
      IF i + n > LEN(H) THEN RETURN -1
      IF NOT IsAlnum(MID$(H, i + n, 1)) THEN RETURN -1
    END IF
    p = INSTR(p + 1, H, "TAG:")
  LOOP
  RETURN 0
END FUNCTION

' `env[\s_-]*gated` (case-insensitive): si cerca GATED e si risale all'ENV.
FUNCTION HasEnvGated(s AS STRING) AS INTEGER
  DIM H AS STRING, p AS LONG, i AS LONG, c AS STRING
  H = UCASE(s)
  p = INSTR(1, H, "GATED")
  DO WHILE p > 0
    i = p - 1
    DO WHILE i >= 1
      c = MID$(H, i, 1)
      IF c = " " OR c = CHR$(9) OR c = "_" OR c = "-" THEN
        i = i - 1
      ELSE
        EXIT DO
      END IF
    LOOP
    IF i >= 3 THEN
      IF MID$(H, i - 2, 3) = "ENV" THEN RETURN -1
    END IF
    p = INSTR(p + 1, H, "GATED")
  LOOP
  RETURN 0
END FUNCTION

' "nessun EXPECT" / "senza EXPECT" / "EXPECT assente" / "EXPECT non esiste":
' il file DICHIARA di non avere l'oracolo (resta W1, non E1).
FUNCTION IsNoOracleClaim(s AS STRING) AS INTEGER
  DIM H AS STRING
  H = UCASE(s)
  IF HasWord(H, "NESSUN EXPECT") THEN RETURN -1
  IF HasWord(H, "SENZA EXPECT") THEN RETURN -1
  IF HasWord(H, "NO EXPECT") THEN RETURN -1
  IF HasWord(H, "NON HA EXPECT") THEN RETURN -1
  IF HasWord(H, "EXPECT ASSENTE") THEN RETURN -1
  IF HasWord(H, "EXPECT NON ESISTE") THEN RETURN -1
  RETURN 0
END FUNCTION

' ── findings ──────────────────────────────────────────────────────────────────

SUB AddFinding(code AS STRING, fi AS LONG, lnum AS LONG, det AS STRING, iserr AS INTEGER)
  IF g_n >= 8000 THEN EXIT SUB
  g_n = g_n + 1
  g_code(g_n) = code
  g_fi(g_n) = fi
  g_path(g_n) = g_rel(fi)
  g_line(g_n) = lnum
  g_det(g_n) = det
  g_iserr(g_n) = iserr
END SUB

FUNCTION Q(s AS STRING) AS STRING
  RETURN "'" + s + "'"
END FUNCTION

FUNCTION Cut90(s AS STRING) AS STRING
  RETURN LEFT$(s, 90)
END FUNCTION

' ── regola per file ───────────────────────────────────────────────────────────

SUB AuditFile(fi AS LONG)
  DIM lines$() AS STRING
  DIM s AS STRING, ls AS STRING, big AS STRING, val AS STRING
  DIM i AS LONG, n AS LONG, hdr AS LONG
  DIM ncan AS LONG, firsti AS LONG, ntry AS LONG, exempt AS INTEGER
  DIM canonIdx(600) AS LONG
  DIM attemptIdx(600) AS LONG
  DIM where AS STRING
  DIM countS AS STRING

  lines$ = IO.READLINES(g_file(fi))
  IF UBOUND(lines$) < 0 THEN
    AddFinding("W1", fi, 0, "nessun oracolo ' EXPECT:", 0)
    EXIT SUB
  END IF
  n = UBOUND(lines$) + 1

  ' header = prima riga eseguibile (commenti a colonna 0 e righe vuote saltati)
  hdr = n
  FOR i = 0 TO n - 1
    s = lines$(i)
    ls = Rst(s)
    IF LEN(Lst(ls)) = 0 OR LEFT$(s, 1) = "'" THEN
      ' commento o vuota: continua
    ELSE
      hdr = i
      EXIT FOR
    END IF
  NEXT i

  ncan = 0 : firsti = -1
  ntry = 0
  FOR i = 0 TO n - 1
    s = lines$(i)
    IF IsCanonical(s) THEN
      ncan = ncan + 1
      IF ncan <= 600 THEN canonIdx(ncan) = i
      IF firsti < 0 THEN firsti = i
    ELSEIF IsTryMarker(s) THEN
      ntry = ntry + 1
      IF ntry <= 600 THEN attemptIdx(ntry) = i
    END IF
  NEXT i

  ' E2 — tentativi non canonici (sempre errore)
  FOR i = 1 TO ntry
    s = Lst(Rst(lines$(attemptIdx(i))))
    AddFinding("E2", fi, attemptIdx(i) + 1, "marker non canonico: " + Q(Cut90(s)) + " (atteso " + CHR$(34) + g_marker + CHR$(34) + ")", 1)
  NEXT i

  IF ncan = 0 THEN
    ' E1 — il file PARLA di EXPECT nell'header ma non ha l'oracolo
    IF ntry = 0 THEN
      FOR i = 0 TO hdr - 1
        s = lines$(i)
        IF HasWord(s, "EXPECT") THEN
          IF NOT IsNoOracleClaim(s) THEN
            AddFinding("E1", fi, i + 1, "oracolo dichiarato in prosa ma nessuna riga " + CHR$(34) + g_marker + CHR$(34) + ": " + Q(Cut90(Lst(Rst(s)))), 1)
            EXIT FOR
          END IF
        END IF
      NEXT i
    END IF
    ' W1 — nessun oracolo (esentato se l'header lo dichiara)
    exempt = 0
    FOR i = 0 TO hdr - 1
      s = lines$(i)
      IF HasTagExempt(s) OR HasEnvGated(s) THEN exempt = -1
    NEXT i
    IF NOT exempt THEN AddFinding("W1", fi, 0, "nessun oracolo ' EXPECT:", 0)
    EXIT SUB
  END IF

  ' E4 — più marker: l'harness legge il PRIMO, gli altri sono morti
  IF ncan > 1 THEN
    where = ""
    FOR i = 1 TO ncan
      IF i > 1 THEN where = where + ", "
      where = where + LTRIM$(STR$(canonIdx(i) + 1))
    NEXT i
    countS = LTRIM$(STR$(ncan))
    AddFinding("E4", fi, canonIdx(2) + 1, countS + " marker canonici (righe " + where + "): l'harness legge solo il primo", 1)
  END IF

  ' E3 — il marker deve stare nel blocco header
  IF firsti >= hdr THEN
    AddFinding("E3", fi, firsti + 1, "marker dopo la prima riga eseguibile (header finisce a riga " + LTRIM$(STR$(hdr + 1)) + ")", 1)
  END IF

  big = lines$(firsti)
  ls = Lst(big)
  val = Rst(MID$(ls, LEN(g_marker) + 1))

  ' E5 — oracolo vuoto
  IF LEN(val) = 0 THEN
    AddFinding("E5", fi, firsti + 1, "oracolo vuoto", 1)
  END IF

  ' E7 — separatore finale: l'harness produce 'a|b', mai 'a|b|'
  IF LEN(val) > 0 THEN
    IF RIGHT$(val, 1) = "|" THEN
      AddFinding("E7", fi, firsti + 1, "oracolo con '|' finale: " + Q(LEFT$(val, 80)), 1)
    END IF
  END IF

  ' E6 — test negativo: il gate C99Native cerca la forma a colonna 0
  IF FindSens(big, "EXPECT: ERROR:", 1) > 0 THEN
    IF LEFT$(big, LEN(g_negative)) <> g_negative THEN
      AddFinding("E6", fi, firsti + 1, "test negativo non a colonna 0: il gate C99Native (grep '^\\' EXPECT: ERROR:') non lo riconoscerebbe", 1)
    END IF
  END IF
END SUB

' ── main ──────────────────────────────────────────────────────────────────────

FUNCTION Main() AS INTEGER
  DIM root AS STRING, dirArg AS STRING, dirAbs AS STRING
  DIM strictFlag AS INTEGER, noOracle AS INTEGER
  DIM files$() AS STRING
  DIM i AS LONG, j AS LONG, k AS LONG, up AS INTEGER
  DIM fi AS LONG, nerr AS LONG, nwarn AS LONG, shown AS LONG
  DIM t AS STRING, pref AS STRING, rel AS STRING
  DIM codeList(7) AS STRING

  g_marker = "' EXPECT: "
  g_negative = "' EXPECT: ERROR:"
  strictFlag = 0 : noOracle = 0 : dirArg = ""
  FOR i = 1 TO COMMANDCOUNT()
    t = COMMAND$(i)
    IF t = "--strict" THEN strictFlag = -1
    IF t = "--no-oracle" THEN noOracle = -1
    IF t = "--dir" THEN
      IF i < COMMANDCOUNT() THEN dirArg = COMMAND$(i + 1)
    END IF
  NEXT i

  ' radice del monorepo: si risale finché esiste shared/minitests (o .git)
  root = IO.CWD()
  FOR up = 1 TO 6
    IF DIREXISTS(root + "/shared/minitests") OR DIREXISTS(root + "/.git") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up

  IF LEN(dirArg) = 0 THEN dirArg = "shared/minitests"
  IF LEFT$(dirArg, 1) = "/" THEN
    dirAbs = dirArg
  ELSE
    dirAbs = root + "/" + dirArg
  END IF
  IF NOT DIREXISTS(dirAbs) THEN
    PRINT "radice MiniTest non trovata: "; dirAbs
    RETURN 2
  END IF

  ' raccolta file .bas + ordinamento per path (equivalente di sorted(rglob))
  files$ = GREP.FILES(" ", dirAbs)
  g_nf = 0
  IF UBOUND(files$) >= 0 THEN
    FOR i = 0 TO UBOUND(files$)
      IF RIGHT$(UCASE(files$(i)), 4) = ".BAS" THEN
        IF g_nf < 2600 THEN
          g_nf = g_nf + 1
          g_file(g_nf) = files$(i)
        END IF
      END IF
    NEXT i
  END IF
  FOR i = 1 TO g_nf - 1
    FOR j = i + 1 TO g_nf
      IF g_file(j) < g_file(i) THEN
        t = g_file(i) : g_file(i) = g_file(j) : g_file(j) = t
      END IF
    NEXT j
  NEXT i
  pref = root + "/"
  FOR i = 1 TO g_nf
    IF LEFT$(g_file(i), LEN(pref)) = pref THEN
      g_rel(i) = MID$(g_file(i), LEN(pref) + 1)
    ELSE
      g_rel(i) = g_file(i)
    END IF
  NEXT i

  g_n = 0
  FOR fi = 1 TO g_nf
    AuditFile(fi)
  NEXT fi

  nerr = 0 : nwarn = 0
  FOR k = 1 TO g_n
    IF g_iserr(k) THEN nerr = nerr + 1 ELSE nwarn = nwarn + 1
  NEXT k

  IF noOracle THEN
    shown = 0
    FOR fi = 1 TO g_nf
      FOR k = 1 TO g_n
        IF g_fi(k) = fi AND g_iserr(k) = 0 THEN
          PRINT g_path(k)
          shown = shown + 1
        END IF
      NEXT k
    NEXT fi
    PRINT ""
    PRINT LTRIM$(STR$(shown)); " file senza oracolo su "; LTRIM$(STR$(g_nf))
    RETURN 0
  END IF

  ' errori: per codice E1..E7, poi nell'ordine dei path (i file sono già ordinati)
  codeList(1) = "E1" : codeList(2) = "E2" : codeList(3) = "E3" : codeList(4) = "E4"
  codeList(5) = "E5" : codeList(6) = "E6" : codeList(7) = "E7"
  FOR i = 1 TO 7
    FOR fi = 1 TO g_nf
      FOR k = 1 TO g_n
        IF g_iserr(k) THEN
          IF g_fi(k) = fi AND g_code(k) = codeList(i) THEN
            PRINT "ERRORE  "; g_path(k); ":"; LTRIM$(STR$(g_line(k))); "  ["; g_code(k); "] "; g_det(k)
          END IF
        END IF
      NEXT k
    NEXT fi
  NEXT i

  ' warning (W1) in ordine di path
  FOR fi = 1 TO g_nf
    FOR k = 1 TO g_n
      IF g_iserr(k) = 0 AND g_fi(k) = fi THEN
        PRINT "warning "; g_path(k); "  ["; g_code(k); "] "; g_det(k)
      END IF
    NEXT k
  NEXT fi

  PRINT ""
  PRINT "MiniTest esaminati: "; LTRIM$(STR$(g_nf))
  PRINT "oracoli non conformi: "; LTRIM$(STR$(nerr))
  PRINT "senza oracolo (W1):   "; LTRIM$(STR$(nwarn))

  IF nerr > 0 THEN
    PRINT ""
    PRINT "Regola: l'oracolo e' la riga `"; g_marker; "<output normalizzato>`,"
    PRINT "con le righe unite da '|' e NESSUNA riga vuota; una sola riga per file,"
    PRINT "nel blocco di commenti iniziale."
  END IF
  IF nwarn > 0 AND NOT strictFlag THEN
    PRINT ""
    PRINT "Nota: i file senza oracolo verificano solo l'exit code (e la parita' fra"
    PRINT "gambe). Elenco: --no-oracle."
  END IF

  IF nerr > 0 THEN RETURN 1
  IF strictFlag AND nwarn > 0 THEN RETURN 1
  RETURN 0
END FUNCTION
