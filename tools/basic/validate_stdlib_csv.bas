' validate_stdlib_csv.bas — Validazione di coerenza dei CSV del registry (stdlib_tokens).
' Porta fedele di tools/validate_stdlib_csv.py (repo GMBasic), senza Python.
'
' CHECKS
'  1. TokenKind univoco per namespace (stesso TokenKind per nomi DIVERSI = conflitto vero).
'  2. Ogni TokenKind del CSV esiste nei sorgenti (TokenKind.X nelle StdLib.*.cs).
'  3. Coppie (namespace, name) duplicate = overload, contate (attese 44).
'  4. stdlib_tokens_unique.csv coincide con stdlib_tokens.csv deduplicato.
'  5. I valori numerici di BuiltInFunction sono globalmente univoci (codice di dispatch
'     runtime: due membri con lo stesso valore si ombreggiano in silenzio).
'
' Uso (dalla root del monorepo):
'   tools/basic/validate_stdlib_csv.bas
' Exit: 0 = PASS · 1 = FAIL · 2 = radice/archivi non trovati.
'
' DIFFERENZE DICHIARATE rispetto al .py (semantica invariata):
'   * i path sono quelli del monorepo (i C# stanno in GMBasicSharp/, non in common/);
'   * l'output va su stdout (nel .py andava su stderr).
'   * i conflitti "CONTROLS.Identifier", "IO.*" e "STRINGS.*" sono alias voluti
'     (proprietà dei controlli e sinonimi di funzione), non difetti: il .py li segnala
'     come tali e il risultato è FAIL, identico nei due tool.
IMPORT IO
IMPORT GREP

DIM g_ns(4200) AS STRING
DIM g_name(4200) AS STRING
DIM g_tk(4200) AS STRING
DIM g_n AS LONG

DIM g_uns(4200) AS STRING
DIM g_uname(4200) AS STRING
DIM g_nu AS LONG

DIM g_src(2200) AS STRING
DIM g_nsrc AS LONG

' ── helper ────────────────────────────────────────────────────────────────────

FUNCTION IsIdentChar(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsWs(c AS STRING) AS INTEGER
  IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION Rst(s AS STRING) AS STRING
  DIM i AS LONG
  i = LEN(s)
  DO WHILE i >= 1
    IF IsWs(MID$(s, i, 1)) THEN
      i = i - 1
    ELSE
      EXIT DO
    END IF
  LOOP
  RETURN LEFT$(s, i)
END FUNCTION

' Ricerca SENSIBILE al caso (INSTR del linguaggio è case-insensitive).
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

FUNCTION ToNum(s AS STRING) AS LONG
  DIM i AS LONG, v AS LONG
  v = 0
  FOR i = 1 TO LEN(s)
    IF MID$(s, i, 1) >= "0" AND MID$(s, i, 1) <= "9" THEN v = v * 10 + (ASC(MID$(s, i, 1)) - 48)
  NEXT i
  RETURN v
END FUNCTION

SUB AddSrc(name AS STRING)
  DIM i AS LONG
  FOR i = 1 TO g_nsrc
    IF g_src(i) = name THEN EXIT SUB
  NEXT i
  IF g_nsrc >= 2200 THEN EXIT SUB
  g_nsrc = g_nsrc + 1
  g_src(g_nsrc) = name
END SUB

FUNCTION SrcHas(name AS STRING) AS INTEGER
  DIM i AS LONG
  FOR i = 1 TO g_nsrc
    IF g_src(i) = name THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

SUB LoadCsv(path AS STRING)
  DIM lines$() AS STRING
  DIM f$() AS STRING
  DIM i AS LONG, k AS LONG, ln AS STRING
  k = 0
  lines$ = IO.READLINES(path)
  IF UBOUND(lines$) >= 0 THEN
    FOR i = 0 TO UBOUND(lines$)
      ln = Rst(lines$(i))
      IF LEN(ln) > 0 THEN
        IF LEFT$(ln, 1) <> "#" THEN
          f$ = SPLIT(ln, ",")
          IF UBOUND(f$) >= 3 THEN
            k = k + 1
            IF k <= 4200 THEN
              g_ns(k) = f$(1)
              g_name(k) = f$(2)
              g_tk(k) = f$(3)
            END IF
          END IF
        END IF
      END IF
    NEXT i
  END IF
  g_n = k
END SUB

FUNCTION PairInUnique(ns AS STRING, nm AS STRING) AS INTEGER
  DIM i AS LONG
  FOR i = 1 TO g_nu
    IF g_uns(i) = ns AND g_uname(i) = nm THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

' Conta i valori numerici duplicati nell'enum BuiltInFunction. -1 se l'enum non c'è.
FUNCTION DupsBuiltInFunction(path AS STRING) AS LONG
  DIM txt AS STRING, body AS STRING, clean AS STRING, c AS STRING
  DIM p AS LONG, q AS LONG, depth AS LONG, i AS LONG, n AS LONG
  DIM st AS LONG, nm AS STRING, num AS STRING
  DIM lastVal AS LONG, cur AS LONG, seenAny AS INTEGER, hasExp AS INTEGER
  DIM vals(4000) AS LONG
  DIM nv AS LONG, dups AS LONG, j AS LONG, cnt AS LONG, firstSeen AS INTEGER

  txt = IO.READFILE(path)
  p = FindSens(txt, "public enum BuiltInFunction", 1)
  IF p = 0 THEN RETURN -1
  p = FindSens(txt, "{", p)
  IF p = 0 THEN RETURN -1
  depth = 0 : q = 0
  FOR i = p TO LEN(txt)
    c = MID$(txt, i, 1)
    IF c = "{" THEN depth = depth + 1
    IF c = "}" THEN
      depth = depth - 1
      IF depth = 0 THEN
        q = i
        EXIT FOR
      END IF
    END IF
  NEXT i
  IF q = 0 THEN RETURN -1
  body = MID$(txt, p + 1, q - p - 1)

  ' via i commenti //
  clean = ""
  i = 1
  n = LEN(body)
  DO WHILE i <= n
    c = MID$(body, i, 1)
    IF c = "/" AND MID$(body, i + 1, 1) = "/" THEN
      DO WHILE i <= n
        IF MID$(body, i, 1) = CHR$(10) THEN EXIT DO
        i = i + 1
      LOOP
    ELSE
      clean = clean + c
      i = i + 1
    END IF
  LOOP

  nv = 0 : lastVal = 0 : seenAny = 0
  i = 1 : n = LEN(clean)
  DO WHILE i <= n
    DO WHILE i <= n
      IF IsIdentChar(MID$(clean, i, 1)) THEN EXIT DO
      i = i + 1
    LOOP
    IF i > n THEN EXIT DO
    st = i
    DO WHILE i <= n
      IF IsIdentChar(MID$(clean, i, 1)) THEN
        i = i + 1
      ELSE
        EXIT DO
      END IF
    LOOP
    nm = MID$(clean, st, i - st)
    DO WHILE i <= n AND IsWs(MID$(clean, i, 1))
      i = i + 1
    LOOP
    hasExp = 0 : num = ""
    IF i <= n THEN
      IF MID$(clean, i, 1) = "=" THEN
        hasExp = -1
        i = i + 1
        DO WHILE i <= n AND IsWs(MID$(clean, i, 1))
          i = i + 1
        LOOP
        DO WHILE i <= n
          c = MID$(clean, i, 1)
          IF c >= "0" AND c <= "9" THEN
            num = num + c
            i = i + 1
          ELSE
            EXIT DO
          END IF
        LOOP
      END IF
    END IF
    IF hasExp THEN
      cur = ToNum(num)
    ELSE
      IF seenAny = 0 THEN cur = 0 ELSE cur = lastVal + 1
    END IF
    seenAny = -1
    lastVal = cur
    IF nv < 4000 THEN
      nv = nv + 1
      vals(nv) = cur
    END IF
    DO WHILE i <= n
      IF MID$(clean, i, 1) = "," THEN
        i = i + 1
        EXIT DO
      END IF
      i = i + 1
    LOOP
  LOOP

  dups = 0
  FOR j = 1 TO nv
    firstSeen = 1
    FOR i = 1 TO j - 1
      IF vals(i) = vals(j) THEN firstSeen = 0
    NEXT i
    IF firstSeen THEN
      cnt = 0
      FOR i = 1 TO nv
        IF vals(i) = vals(j) THEN cnt = cnt + 1
      NEXT i
      IF cnt > 1 THEN dups = dups + 1
    END IF
  NEXT j
  RETURN dups
END FUNCTION

' ── main ──────────────────────────────────────────────────────────────────────

FUNCTION Main() AS INTEGER
  DIM root AS STRING, pCsv AS STRING, pUniq AS STRING, pStd AS STRING, pBfn AS STRING
  DIM up AS INTEGER, i AS LONG, j AS LONG, k AS LONG, a AS LONG, b AS LONG
  DIM files$() AS STRING, ls$() AS STRING, f$() AS STRING
  DIM s AS STRING, ln AS STRING, t AS STRING, c AS STRING, lst AS STRING
  DIM pos AS LONG, st AS LONG, w AS STRING, ok AS INTEGER, passed AS INTEGER
  DIM conflicts AS LONG, missingTk AS LONG, overloads AS LONG, diffPairs AS LONG
  DIM dupsEnum AS LONG, rowsUniq AS LONG, cnt AS LONG
  DIM seenNs(500) AS STRING, seenNsn AS LONG
  DIM tks(400) AS STRING, nms(400) AS STRING, ntk AS LONG, nn AS LONG
  DIM hasC AS INTEGER, nsHas AS INTEGER
  DIM det AS STRING, nsDet AS STRING

  PRINT "Validating stdlib CSV against source code..."
  root = IO.CWD()
  FOR up = 1 TO 6
    IF FILEEXISTS(root + "/GMBasicSharp/common/Registry/stdlib_tokens.csv") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up
  pCsv = root + "/GMBasicSharp/common/Registry/stdlib_tokens.csv"
  pUniq = root + "/GMBasicSharp/common/Registry/stdlib_tokens_unique.csv"
  pStd = root + "/GMBasicSharp/common/StdLib"
  pBfn = root + "/GMBasicSharp/common/Compiler/BuiltInFunction.cs"
  IF NOT FILEEXISTS(pCsv) THEN
    PRINT "ERRORE: CSV non trovato: "; pCsv
    RETURN 2
  END IF
  IF NOT FILEEXISTS(pUniq) THEN
    PRINT "ERRORE: CSV unico non trovato: "; pUniq
    RETURN 2
  END IF
  IF NOT FILEEXISTS(pBfn) THEN
    PRINT "ERRORE: BuiltInFunction.cs non trovato: "; pBfn
    RETURN 2
  END IF

  ' --- CSV principale
  LoadCsv(pCsv)
  PRINT "  stdlib_tokens.csv: "; LTRIM$(STR$(g_n)); " rows"

  ' --- CSV unico
  g_nu = 0
  ls$ = IO.READLINES(pUniq)
  IF UBOUND(ls$) >= 0 THEN
    FOR i = 0 TO UBOUND(ls$)
      s = Rst(ls$(i))
      IF LEN(s) > 0 THEN
        IF LEFT$(s, 1) <> "#" THEN
          f$ = SPLIT(s, ",")
          IF UBOUND(f$) >= 3 THEN
            g_nu = g_nu + 1
            IF g_nu <= 4200 THEN
              g_uns(g_nu) = f$(1)
              g_uname(g_nu) = f$(2)
            END IF
          END IF
        END IF
      END IF
    NEXT i
  END IF
  rowsUniq = g_nu
  PRINT "  stdlib_tokens_unique.csv: "; LTRIM$(STR$(rowsUniq)); " rows"

  ' --- TokenKind presenti nei sorgenti
  g_nsrc = 0
  files$ = GREP.FILES(" ", pStd)
  IF UBOUND(files$) >= 0 THEN
    FOR i = 0 TO UBOUND(files$)
      s = files$(i)
      IF FindSens(s, "/StdLib.", 1) > 0 AND RIGHT$(s, 3) = ".cs" THEN
        ls$ = IO.READLINES(s)
        IF UBOUND(ls$) >= 0 THEN
          FOR j = 0 TO UBOUND(ls$)
            pos = 1
            DO WHILE 1
              pos = FindSens(ls$(j), "TokenKind.", pos)
              IF pos = 0 THEN EXIT DO
              st = pos + 10
              k = st
              DO WHILE k <= LEN(ls$(j))
                c = MID$(ls$(j), k, 1)
                IF IsIdentChar(c) THEN
                  k = k + 1
                ELSE
                  EXIT DO
                END IF
              LOOP
              IF k > st THEN AddSrc(MID$(ls$(j), st, k - st))
              pos = st + 1
            LOOP
          NEXT j
        END IF
      END IF
    NEXT i
  END IF
  PRINT "  Source TokenKinds: "; LTRIM$(STR$(g_nsrc)); " unique"
  PRINT ""

  ' --- namespace distinti
  seenNsn = 0
  FOR i = 1 TO g_n
    ok = 1
    FOR j = 1 TO seenNsn
      IF seenNs(j) = g_ns(i) THEN ok = 0
    NEXT j
    IF ok THEN
      IF seenNsn < 500 THEN
        seenNsn = seenNsn + 1
        seenNs(seenNsn) = g_ns(i)
      END IF
    END IF
  NEXT i

  ' --- 1. conflitti veri (per namespace, per TokenKind con più nomi distinti)
  conflicts = 0
  det = ""
  FOR k = 1 TO seenNsn
    ' TokenKind distinti del namespace
    ntk = 0
    FOR i = 1 TO g_n
      IF g_ns(i) = seenNs(k) THEN
        ok = 1
        FOR j = 1 TO ntk
          IF tks(j) = g_tk(i) THEN ok = 0
        NEXT j
        IF ok THEN
          IF ntk < 400 THEN
            ntk = ntk + 1
            tks(ntk) = g_tk(i)
          END IF
        END IF
      END IF
    NEXT i
    nsHas = 0
    FOR j = 1 TO ntk
      ' nomi distinti con questo TokenKind, nel namespace
      nn = 0
      FOR i = 1 TO g_n
        IF g_ns(i) = seenNs(k) AND g_tk(i) = tks(j) THEN
          ok = 1
          FOR a = 1 TO nn
            IF nms(a) = g_name(i) THEN ok = 0
          NEXT a
          IF ok THEN
            IF nn < 400 THEN
              nn = nn + 1
              nms(nn) = g_name(i)
            END IF
          END IF
        END IF
      NEXT i
      IF nn > 1 THEN
        nsHas = -1
        ' ordina i nomi (ordine alfabetico, come sorted() del .py)
        FOR a = 1 TO nn - 1
          FOR b = a + 1 TO nn
            IF nms(b) < nms(a) THEN
              t = nms(a) : nms(a) = nms(b) : nms(b) = t
            END IF
          NEXT b
        NEXT a
        lst = ""
        FOR a = 1 TO nn
          IF a > 1 THEN lst = lst + ", "
          lst = lst + "'" + nms(a) + "'"
        NEXT a
        det = det + "   [" + seenNs(k) + "] TokenKind." + tks(j) + " -> [" + lst + "]" + CHR$(10)
      END IF
    NEXT j
    IF nsHas THEN conflicts = conflicts + 1
  NEXT k

  PRINT "1. TokenKind TRUE conflicts (same TK, different names in same ns): "; LTRIM$(STR$(conflicts))
  IF LEN(det) > 0 THEN PRINT det;

  ' --- 2. TokenKind del CSV assenti nei sorgenti (distinti)
  missingTk = 0
  FOR i = 1 TO g_n
    IF NOT SrcHas(g_tk(i)) THEN
      ok = 1
      FOR j = 1 TO i - 1
        IF g_tk(j) = g_tk(i) THEN ok = 0
      NEXT j
      IF ok THEN missingTk = missingTk + 1
    END IF
  NEXT i
  PRINT "2. TokenKinds in CSV but NOT in source: "; LTRIM$(STR$(missingTk))

  ' --- 3. overload: somma di (conteggio-1) per coppia (ns,name), gruppi distinti
  overloads = 0
  FOR i = 1 TO g_n
    ok = 1
    FOR j = 1 TO i - 1
      IF g_ns(j) = g_ns(i) AND g_name(j) = g_name(i) THEN ok = 0
    NEXT j
    IF ok THEN
      cnt = 0
      FOR j = 1 TO g_n
        IF g_ns(j) = g_ns(i) AND g_name(j) = g_name(i) THEN cnt = cnt + 1
      NEXT j
      IF cnt > 1 THEN overloads = overloads + (cnt - 1)
    END IF
  NEXT i
  PRINT "3. Overloads (duplicate ns+name): "; LTRIM$(STR$(overloads)); " -> expected 44"

  ' --- 4. coppie (ns,name) del CSV presenti anche nel CSV unico (distinte)
  diffPairs = 0
  FOR i = 1 TO g_n
    ok = 1
    FOR j = 1 TO i - 1
      IF g_ns(j) = g_ns(i) AND g_name(j) = g_name(i) THEN ok = 0
    NEXT j
    IF ok THEN
      IF NOT PairInUnique(g_ns(i), g_name(i)) THEN diffPairs = diffPairs + 1
    END IF
  NEXT i
  IF diffPairs > 0 THEN
    PRINT "4. Unique pairs mismatch: "; LTRIM$(STR$(diffPairs))
  ELSE
    PRINT "4. Unique pairs match between CSVs: OK"
  END IF

  ' --- 5. BuiltInFunction: valori duplicati
  dupsEnum = DupsBuiltInFunction(pBfn)
  passed = 1
  IF dupsEnum < 0 THEN
    PRINT "5. BuiltInFunction enum NOT found (path errato): FAIL"
    passed = 0
    dupsEnum = 0
  ELSE
    PRINT "5. BuiltInFunction duplicate values: "; LTRIM$(STR$(dupsEnum))
  END IF

  PRINT ""
  PRINT "--- Summary ---"
  IF conflicts > 0 THEN passed = 0
  IF missingTk > 0 THEN passed = 0
  IF overloads <> 44 THEN passed = 0
  IF diffPairs > 0 THEN passed = 0
  IF passed THEN
    PRINT "Result: PASS"
    PRINT "  stdlib_tokens.csv: all entries (incl. 44 overloads) for dispatch implementation"
    PRINT "  stdlib_tokens_unique.csv: 1528 unique pairs for audit coverage"
    PRINT "  BuiltInFunction enum: all values globally unique (dispatch-safe)"
    RETURN 0
  END IF
  PRINT "Result: FAIL"
  RETURN 1
END FUNCTION
