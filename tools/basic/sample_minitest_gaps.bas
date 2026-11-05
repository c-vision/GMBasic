' ==============================================================================
' sample_minitest_gaps.bas — casi d'uso dei SAMPLE non previsti dai MiniTest canonici.
'
' Per ogni Sample taggato `' SURFACE:` estrae le chiamate `NS.FUNC(...)` (nome + arita')
' e le confronta con il file canonico del namespace
' (shared/minitests/<ns>/<ns>_all.bas): un caso d'uso e' "non previsto" se nel
' canonico la stessa funzione compare solo con un'arita' diversa o non compare affatto.
' Nomi confrontati senza distinzione di maiuscole.
'
' Uso (dalla root del monorepo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/sample_minitest_gaps.bas smg
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_hh1(65536) AS LONG
DIM g_hv1(65536) AS STRING
DIM g_hh2(65536) AS LONG
DIM g_hv2(65536) AS STRING
DIM g_loaded(512) AS STRING
DIM g_nloaded AS LONG
DIM g_rows(8192) AS STRING
DIM g_nrows AS LONG

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION IsIdentCh(ch AS STRING) AS LONG
  DIM c AS LONG
  IF LEN(ch) = 0 THEN RETURN 0
  c = ASC(ch)
  IF (c >= 65 AND c <= 90) OR (c >= 97 AND c <= 122) OR (c >= 48 AND c <= 57) OR c = 95 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION HV(s AS STRING) AS LONG
  DIM h AS LONG, i AS LONG
  h = 5381
  FOR i = 1 TO LEN(s)
    h = (h * 33 + ASC(MID$(s, i, 1))) MOD 2147483647
  NEXT i
  RETURN h
END FUNCTION

SUB HSet1(s AS STRING)
  DIM p AS LONG, g AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  p = HV(s) MOD 65536
  g = 0
  DO
    IF g_hh1(p) = 0 THEN
      g_hh1(p) = 1
      g_hv1(p) = s
      EXIT SUB
    END IF
    IF g_hv1(p) = s THEN EXIT SUB
    p = (p + 1) MOD 65536
    g = g + 1
    IF g > 65535 THEN EXIT SUB
  LOOP
END SUB

FUNCTION HHas1(s AS STRING) AS LONG
  DIM p AS LONG, g AS LONG
  IF LEN(s) = 0 THEN RETURN 0
  p = HV(s) MOD 65536
  g = 0
  DO WHILE g_hh1(p) <> 0
    IF g_hv1(p) = s THEN RETURN -1
    p = (p + 1) MOD 65536
    g = g + 1
    IF g > 65535 THEN EXIT DO
  LOOP
  RETURN 0
END FUNCTION

' Entry di g_hh2: chiave in g_hk2, valore (arite) in g_hval2.
DIM g_hk2(65536) AS STRING
DIM g_hval2(65536) AS STRING

FUNCTION HHas2(s AS STRING) AS LONG
  DIM p AS LONG, g AS LONG
  IF LEN(s) = 0 THEN RETURN 0
  p = HV(s) MOD 65536
  g = 0
  DO WHILE g_hh2(p) <> 0
    IF g_hk2(p) = s THEN RETURN -1
    p = (p + 1) MOD 65536
    g = g + 1
    IF g > 65535 THEN EXIT DO
  LOOP
  RETURN 0
END FUNCTION

FUNCTION HGet2(s AS STRING) AS STRING
  DIM p AS LONG, g AS LONG
  IF LEN(s) = 0 THEN RETURN ""
  p = HV(s) MOD 65536
  g = 0
  DO WHILE g_hh2(p) <> 0
    IF g_hk2(p) = s THEN RETURN g_hval2(p)
    p = (p + 1) MOD 65536
    g = g + 1
    IF g > 65535 THEN EXIT DO
  LOOP
  RETURN ""
END FUNCTION

SUB HAdd2(s AS STRING, ar AS STRING)
  DIM p AS LONG, g AS LONG, cur AS STRING
  IF LEN(s) = 0 THEN EXIT SUB
  p = HV(s) MOD 65536
  g = 0
  DO
    IF g_hh2(p) = 0 THEN
      g_hh2(p) = 1
      g_hk2(p) = s
      g_hval2(p) = ar
      EXIT SUB
    END IF
    IF g_hk2(p) = s THEN
      cur = g_hval2(p)
      IF INSTR("," + cur + ",", "," + ar + ",") = 0 THEN g_hval2(p) = cur + "," + ar
      EXIT SUB
    END IF
    p = (p + 1) MOD 65536
    g = g + 1
    IF g > 65535 THEN EXIT SUB
  LOOP
END SUB

SUB PushRow(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 0 TO g_nrows - 1
    IF g_rows(i) = s THEN EXIT SUB
  NEXT i
  IF g_nrows < 8192 THEN
    g_rows(g_nrows) = s
    g_nrows = g_nrows + 1
  END IF
END SUB

' ---- import del file: alias -> namespace ------------------------------------
DIM g_ials(512) AS STRING
DIM g_ins(512) AS STRING
DIM g_nimp AS LONG

SUB LoadImports(t AS STRING)
  DIM ln$() AS STRING, i AS LONG, s AS STRING, rest AS STRING, nm AS STRING
  DIM al AS STRING, k AS LONG
  g_nimp = 0
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN EXIT SUB
  FOR i = 0 TO UBOUND(ln$)
    s = TrimS(ln$(i))
    IF LEFT$(UCASE$(s), 7) = "IMPORT " THEN
      rest = TrimS(MID$(s, 8))
      k = INSTR(UCASE$(rest), " AS ")
      IF k > 0 THEN
        nm = TrimS(LEFT$(rest, k - 1))
        al = TrimS(MID$(rest, k + 4))
        k = INSTR(al, " ")
        IF k > 0 THEN al = LEFT$(al, k - 1)
      ELSE
        k = INSTR(rest, " ")
        IF k > 0 THEN nm = LEFT$(rest, k - 1) ELSE nm = rest
        al = nm
      END IF
      IF LEN(nm) > 0 AND g_nimp < 512 THEN
        g_ials(g_nimp) = UCASE$(al)
        g_ins(g_nimp) = UCASE$(nm)
        g_nimp = g_nimp + 1
      END IF
    END IF
  NEXT i
END SUB

FUNCTION NsOf(alias AS STRING) AS STRING
  DIM i AS LONG
  FOR i = 0 TO g_nimp - 1
    IF g_ials(i) = UCASE$(alias) THEN RETURN g_ins(i)
  NEXT i
  RETURN UCASE$(alias)
END FUNCTION

' ---- estrazione chiamate -----------------------------------------------------
' Identificatore che termina prima di pos (indietro, salta spazi).
FUNCTION IdentBefore(t AS STRING, pos AS LONG) AS STRING
  DIM i AS LONG, e AS LONG
  i = pos - 1
  DO WHILE i >= 1 AND (MID$(t, i, 1) = " " OR MID$(t, i, 1) = CHR$(9))
    i = i - 1
  LOOP
  IF i < 1 THEN RETURN ""
  IF IsIdentCh(MID$(t, i, 1)) = 0 THEN RETURN ""
  e = i
  DO WHILE i >= 1 AND IsIdentCh(MID$(t, i, 1))
    i = i - 1
  LOOP
  RETURN MID$(t, i + 1, e - i)
END FUNCTION

' Arita' della chiamata: virgole di primo livello tra la '(' a openIdx e la ')'.
FUNCTION ArityAt(t AS STRING, openIdx AS LONG) AS LONG
  DIM i AS LONG, depth AS LONG, commas AS LONG, inq AS LONG
  DIM c AS STRING, body AS STRING
  i = openIdx
  depth = 0
  commas = 0
  inq = 0
  DO WHILE i <= LEN(t)
    c = MID$(t, i, 1)
    IF inq <> 0 THEN
      IF c = CHR$(34) THEN
        IF MID$(t, i + 1, 1) = CHR$(34) THEN
          i = i + 1
        ELSE
          inq = 0
        END IF
      END IF
    ELSEIF c = CHR$(34) THEN
      inq = -1
    ELSEIF c = "(" THEN
      depth = depth + 1
    ELSEIF c = ")" THEN
      depth = depth - 1
      IF depth = 0 THEN EXIT DO
    ELSEIF c = "," AND depth = 1 THEN
      commas = commas + 1
    END IF
    i = i + 1
  LOOP
  ' corpo vuoto -> 0 argomenti
  body = TrimS(MID$(t, openIdx + 1, i - openIdx - 1))
  IF LEN(body) = 0 THEN RETURN 0
  RETURN commas + 1
END FUNCTION

' Record chiamate del file corrente
DIM g_cns(8192) AS STRING
DIM g_cfn(8192) AS STRING
DIM g_car(8192) AS LONG
DIM g_ncall AS LONG
' Copia delle chiamate del campione (il canonico riusa gli stessi buffer)
DIM g_sns(8192) AS STRING
DIM g_sfn(8192) AS STRING
DIM g_sar(8192) AS LONG
DIM g_nsample AS LONG

SUB ScanCalls(t AS STRING)
  DIM i AS LONG, p AS LONG, fn AS STRING, ns AS STRING, j AS LONG, dot AS LONG
  g_ncall = 0
  i = 1
  DO
    p = INSTR(i, t, "(")
    IF p = 0 THEN EXIT DO
    fn = IdentBefore(t, p)
    IF LEN(fn) > 0 THEN
      ' torna all'inizio del nome funzione, poi cerca il punto del namespace
      j = p - 1
      DO WHILE j >= 1 AND (MID$(t, j, 1) = " " OR MID$(t, j, 1) = CHR$(9))
        j = j - 1
      LOOP
      j = j - LEN(fn)
      IF j >= 1 AND MID$(t, j, 1) = "." THEN
        j = j - 1
        DO WHILE j >= 1 AND (MID$(t, j, 1) = " " OR MID$(t, j, 1) = CHR$(9))
          j = j - 1
        LOOP
        ns = IdentBefore(t, j + 1)
        IF LEN(ns) > 0 AND g_ncall < 8192 THEN
          g_cns(g_ncall) = NsOf(ns)
          g_cfn(g_ncall) = LCASE$(fn)
          g_car(g_ncall) = ArityAt(t, p)
          g_ncall = g_ncall + 1
        END IF
      END IF
    END IF
    i = p + 1
  LOOP
END SUB

' ---- canonico ---------------------------------------------------------------
FUNCTION NsFile(ns AS STRING, root AS STRING) AS STRING
  DIM l AS STRING
  l = LCASE$(ns)
  RETURN root + "/minitests/" + l + "/" + REPLACE$(l, ".", "_") + "_all.bas"
END FUNCTION

SUB LoadCanonical(ns AS STRING, root AS STRING)
  DIM i AS LONG, path AS STRING, t AS STRING, k AS LONG
  FOR i = 0 TO g_nloaded - 1
    IF g_loaded(i) = ns THEN EXIT SUB
  NEXT i
  IF g_nloaded < 512 THEN
    g_loaded(g_nloaded) = ns
    g_nloaded = g_nloaded + 1
  END IF
  path = NsFile(ns, root)
  IF NOT FILEEXISTS(path) THEN EXIT SUB
  t = IO.READFILE(path)
  ScanCalls(t)
  FOR k = 0 TO g_ncall - 1
    HSet1(g_cns(k) + " " + g_cfn(k) + " " + LTRIM$(STR$(g_car(k))))
    HAdd2(g_cns(k) + " " + g_cfn(k), LTRIM$(STR$(g_car(k))))
  NEXT k
END SUB

FUNCTION Pad(s AS STRING, w AS LONG) AS STRING
  DIM r AS STRING
  r = s
  DO WHILE LEN(r) < w
    r = r + " "
  LOOP
  RETURN r
END FUNCTION

' Campo k (0-based) di una riga TSV di g_rows().
FUNCTION RowField(rowIdx AS LONG, k AS LONG) AS STRING
  DIM f$() AS STRING
  f$ = SPLIT(g_rows(rowIdx), CHR$(9))
  IF k <= UBOUND(f$) THEN RETURN f$(k)
  RETURN ""
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING, a AS STRING, i AS LONG, j AS LONG
  DIM t AS STRING, ln$() AS STRING, outPath AS STRING, h AS LONG, wr AS LONG
  DIM out AS STRING, rel AS STRING, full AS STRING
  DIM nArity AS LONG, nAbsent AS LONG, files AS LONG, key AS STRING

  SET SHELL ON
  IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/minitests") THEN
    root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared"
    outPath = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/tools/reports/sample_minitest_gaps.txt"
  ELSE
    root = "shared"
    outPath = "tools/reports/sample_minitest_gaps.txt"
  END IF
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--out" AND i < COMMANDCOUNT() THEN
      i = i + 1
      outPath = COMMAND$(i)
    END IF
    IF a = "--shared" AND i < COMMANDCOUNT() THEN
      i = i + 1
      root = COMMAND$(i)
    END IF
    i = i + 1
  LOOP

  t = SYSTEM.SHELLOUT("find " + Q(root + "/samples") + " -name '*.bas' 2>/dev/null | sort")
  ln$ = SPLIT(t, CHR$(10))
  files = 0
  nArity = 0
  nAbsent = 0
  g_nrows = 0
  IF UBOUND(ln$) >= 0 THEN
    FOR i = 0 TO UBOUND(ln$)
      full = TrimS(ln$(i))
      IF LEN(full) > 0 THEN
        rel = full
        IF LEFT$(rel, LEN(root) + 1) = root + "/" THEN rel = MID$(rel, LEN(root) + 2)
        t = IO.READFILE(full)
        IF LEFT$(t, 10) = "' SURFACE:" THEN
          files = files + 1
          LoadImports(t)
          ScanCalls(t)
          ' copia: LoadCanonical riusa g_cns/g_cfn/g_car
          g_nsample = g_ncall
          FOR j = 0 TO g_ncall - 1
            g_sns(j) = g_cns(j)
            g_sfn(j) = g_cfn(j)
            g_sar(j) = g_car(j)
          NEXT j
          FOR j = 0 TO g_nsample - 1
            LoadCanonical(g_sns(j), root)
            key = g_sns(j) + " " + g_sfn(j) + " " + LTRIM$(STR$(g_sar(j)))
            IF HHas1(key) = 0 THEN
              IF HHas2(g_sns(j) + " " + g_sfn(j)) <> 0 THEN
                PushRow(rel + CHR$(9) + g_sns(j) + CHR$(9) + g_sfn(j) + CHR$(9) + LTRIM$(STR$(g_sar(j))) + CHR$(9) + "ARITA" + CHR$(9) + HGet2(g_sns(j) + " " + g_sfn(j)))
                nArity = nArity + 1
              ELSE
                PushRow(rel + CHR$(9) + g_sns(j) + CHR$(9) + g_sfn(j) + CHR$(9) + LTRIM$(STR$(g_sar(j))) + CHR$(9) + "ASSENTE" + CHR$(9) + "")
                nAbsent = nAbsent + 1
              END IF
            END IF
          NEXT j
        END IF
      END IF
    NEXT i
  END IF

  out = "SAMPLE → MINITEST: casi d'uso non previsti dai canonici" + CHR$(10) + CHR$(10)
  out = out + "casi: " + LTRIM$(STR$(g_nrows)) + "   sample SURFACE: " + LTRIM$(STR$(files)) + CHR$(10) + CHR$(10)
  out = out + "== ARITA (stessa funzione, arità diversa nel canonico) ==" + CHR$(10)
  FOR i = 0 TO g_nrows - 1
    IF INSTR(g_rows(i), CHR$(9) + "ARITA" + CHR$(9)) > 0 THEN
      out = out + "  " + Pad(RowField(i, 0), 62) + " " + RowField(i, 1) + "." + RowField(i, 2) + "(" + RowField(i, 3) + ")" + "  canonico: " + RowField(i, 5) + CHR$(10)
    END IF
  NEXT i
  out = out + CHR$(10) + "== ASSENTE (funzione non usata nel canonico) ==" + CHR$(10)
  FOR i = 0 TO g_nrows - 1
    IF INSTR(g_rows(i), CHR$(9) + "ASSENTE" + CHR$(9)) > 0 THEN
      out = out + "  " + Pad(RowField(i, 0), 62) + " " + RowField(i, 1) + "." + RowField(i, 2) + "(" + RowField(i, 3) + ")" + CHR$(10)
    END IF
  NEXT i

  wr = SYSTEM.SHELLOUT("rm -f " + Q(outPath))
  h = IO.FCREATE(outPath)
  wr = IO.FWRITE(h, out)
  IO.FCLOSE(h)

  ' conteggi per tipo sulle righe UNICHE
  nArity = 0
  nAbsent = 0
  FOR i = 0 TO g_nrows - 1
    IF INSTR(g_rows(i), CHR$(9) + "ARITA" + CHR$(9)) > 0 THEN nArity = nArity + 1
    IF INSTR(g_rows(i), CHR$(9) + "ASSENTE" + CHR$(9)) > 0 THEN nAbsent = nAbsent + 1
  NEXT i

  PRINT "sample SURFACE: "; files; "   casi: "; g_nrows; "  (arita "; nArity; ", assenti "; nAbsent; ")"
  PRINT "scritto "; outPath
  RETURN 0
END FUNCTION
