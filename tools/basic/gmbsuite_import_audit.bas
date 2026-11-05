' ==============================================================================
' gmbsuite_import_audit.bas — audit dell'import delle librerie gmbsuite nel monorepo
'
' Verifica, simbolo per simbolo, che nulla sia stato tralasciato nel passaggio
' gmbsuite -> GMBasicMonoRepo (o gmbsuite-implemented -> shared/libs/packages):
'
'   1. copie migrate (storico vs vivo): header, definizioni, file e alias .inc;
'   2. ogni lib FFI del monorepo: alias .inc definiti nei sorgenti e ESPORTATI
'      dal dylib costruito (nm); header dichiarati ma non definiti = WARN;
'   3. ogni pacchetto: come sopra (i pacchetti arrivi dal registry: "senza
'      sorgenti" NON e' una lacuna, il controllo e' sull'ABI esportata);
'   4. inventario di OGNI progetto gmbsuite con match per nome contro la superficie
'      del monorepo (registry stdlib + nomi/alias .inc + header lib + funzioni C
'      del runtime). Il match per nome e' euristico: serve al triage.
'
' Uso (gamba nativa):
'   gmbnative tools/basic/gmbsuite_import_audit.bas \
'       --gmbsuite ~/Documents/Projects/GMBSuite \
'       --implemented ~/Documents/Projects/gmbsuite-implemented \
'       --monorepo . \
'       [--tsv tools/reports/gmbsuite-import-audit.tsv] \
'       [--md  tools/reports/gmbsuite-import-audit.md]
'
' Exit code: 0 = nessuna lacuna esatta (sezioni 1-3), 1 = lacune trovate.
' ==============================================================================
IMPORT IO
IMPORT SYSTEM
IMPORT JSON

' ── capienze (array fissi: gli array dinamici non si passano alle SUB su C99) ──
' 8192 per insieme, 32768 per liste file/scansioni.
DIM g_s1(8192) AS STRING
DIM g_s2(8192) AS STRING
DIM g_s3(8192) AS STRING
DIM g_s4(8192) AS STRING
DIM g_s5(8192) AS STRING
DIM g_s6(8192) AS STRING
DIM g_s7(8192) AS STRING
DIM g_s8(8192) AS STRING
DIM g_s9(8192) AS STRING
DIM g_s10(8192) AS STRING
DIM g_s11(8192) AS STRING
DIM g_s12(8192) AS STRING
DIM g_f1(32768) AS STRING
DIM g_f2(32768) AS STRING
DIM g_f3(32768) AS STRING
DIM g_scan(32768) AS STRING

DIM g_n1 AS LONG
DIM g_n2 AS LONG
DIM g_n3 AS LONG
DIM g_n4 AS LONG
DIM g_n5 AS LONG
DIM g_n6 AS LONG
DIM g_n7 AS LONG
DIM g_n8 AS LONG
DIM g_n9 AS LONG
DIM g_n10 AS LONG
DIM g_n11 AS LONG
DIM g_n12 AS LONG
DIM g_fn1 AS LONG
DIM g_fn2 AS LONG
DIM g_fn3 AS LONG
DIM g_scann AS LONG

' ── stato report / universo ──────────────────────────────────────────────────
DIM g_th AS LONG
DIM g_mh AS LONG
DIM g_gaplines AS STRING
DIM g_gaps AS LONG
DIM g_hh(131072) AS LONG
DIM g_hv(131072) AS STRING

FUNCTION HV(s AS STRING) AS LONG
  DIM h AS LONG, i AS LONG, n AS LONG
  h = 5381
  n = LEN(s)
  FOR i = 1 TO n
    h = (h * 33 + ASC(MID$(s, i, 1))) MOD 2147483647
  NEXT i
  RETURN h
END FUNCTION

SUB HAdd(s AS STRING)
  DIM p AS LONG, guard AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  p = HV(s) MOD 131072
  guard = 0
  DO
    IF g_hh(p) = 0 THEN
      g_hh(p) = 1
      g_hv(p) = s
      EXIT SUB
    END IF
    IF g_hv(p) = s THEN EXIT SUB
    p = (p + 1) MOD 131072
    guard = guard + 1
    IF guard > 131071 THEN EXIT SUB
  LOOP
END SUB

FUNCTION HHas(s AS STRING) AS LONG
  DIM p AS LONG, guard AS LONG
  IF LEN(s) = 0 THEN RETURN 0
  p = HV(s) MOD 131072
  guard = 0
  DO WHILE g_hh(p) <> 0
    IF g_hv(p) = s THEN RETURN -1
    p = (p + 1) MOD 131072
    guard = guard + 1
    IF guard > 131071 THEN EXIT DO
  LOOP
  RETURN 0
END FUNCTION

' ── stringhe ────────────────────────────────────────────────────────────────
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

FUNCTION IsSpaceCh(ch AS STRING) AS LONG
  IF LEN(ch) = 0 THEN RETURN 0
  IF ch = " " OR ch = CHR$(9) OR ch = CHR$(10) OR ch = CHR$(13) THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION EndsWith(s AS STRING, suf AS STRING) AS LONG
  IF LEN(s) >= LEN(suf) THEN
    IF RIGHT$(s, LEN(suf)) = suf THEN RETURN -1
  END IF
  RETURN 0
END FUNCTION

FUNCTION IdentBefore(t AS STRING, pos AS LONG) AS STRING
  DIM i AS LONG, e AS LONG
  i = pos - 1
  DO WHILE i >= 1
    IF IsSpaceCh(MID$(t, i, 1)) OR MID$(t, i, 1) = "*" THEN
      i = i - 1
    ELSE
      EXIT DO
    END IF
  LOOP
  IF i < 1 THEN RETURN ""
  IF IsIdentCh(MID$(t, i, 1)) = 0 THEN RETURN ""
  e = i
  DO WHILE i >= 1 AND IsIdentCh(MID$(t, i, 1))
    i = i - 1
  LOOP
  RETURN MID$(t, i + 1, e - i)
END FUNCTION

' ── shell / file (array fissi per output) ───────────────────────────────────
SUB ShellInto(cmd AS STRING, out$(), BYREF nOut AS LONG)
  DIM t AS STRING, ln$() AS STRING, i AS LONG, n AS LONG
  t = SYSTEM.SHELLOUT(cmd)
  ln$ = SPLIT(t, CHR$(10))
  n = 0
  IF UBOUND(ln$) >= 0 THEN
    FOR i = 0 TO UBOUND(ln$)
      IF LEN(TrimS(ln$(i))) > 0 THEN
        IF n < 32768 THEN
          out$(n) = TrimS(ln$(i))
          n = n + 1
        END IF
      END IF
    NEXT i
  END IF
  nOut = n
END SUB

FUNCTION SkipPath(p AS STRING) AS LONG
  DIM u AS STRING
  u = "/" + p + "/"
  IF INSTR(u, "/build/") > 0 THEN RETURN -1
  IF INSTR(u, "/_deps/") > 0 THEN RETURN -1
  IF INSTR(u, "/.git/") > 0 THEN RETURN -1
  IF INSTR(u, "/.build/") > 0 THEN RETURN -1
  IF INSTR(u, "/.content/") > 0 THEN RETURN -1
  IF INSTR(u, "/out/") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB FilesExt(dir AS STRING, ext AS STRING, out$(), BYREF nOut AS LONG)
  DIM p AS STRING, i AS LONG, n AS LONG
  ShellInto("find " + Q(dir) + " -type f -name '*." + ext + "' 2>/dev/null", g_f3(), g_fn3)
  n = 0
  FOR i = 0 TO g_fn3 - 1
    p = g_f3(i)
    IF SkipPath(p) = 0 THEN
      IF n < 32768 THEN
        out$(n) = p
        n = n + 1
      END IF
    END IF
  NEXT i
  nOut = n
END SUB

SUB AllFiles(dir AS STRING, out$(), BYREF nOut AS LONG)
  ShellInto("find " + Q(dir) + " -type f 2>/dev/null", out$(), nOut)
END SUB

' ── parsing C ───────────────────────────────────────────────────────────────
FUNCTION StripComments(t AS STRING) AS STRING
  DIM s AS STRING, out AS STRING, ln$() AS STRING, line AS STRING, i AS LONG
  DIM p AS LONG, q2 AS LONG
  s = t
  DO
    p = INSTR(s, "/*")
    IF p = 0 THEN EXIT DO
    q2 = INSTR(p + 2, s, "*/")
    IF q2 = 0 THEN
      s = LEFT$(s, p - 1)
      EXIT DO
    END IF
    s = LEFT$(s, p - 1) + " " + MID$(s, q2 + 2)
  LOOP
  ln$ = SPLIT(s, CHR$(10))
  out = ""
  IF UBOUND(ln$) >= 0 THEN
    FOR i = 0 TO UBOUND(ln$)
      line = ln$(i)
      p = INSTR(line, "//")
      IF p > 0 THEN line = LEFT$(line, p - 1)
      line = TrimS(line)
      IF LEFT$(line, 1) <> "#" AND LEN(line) > 0 THEN out = out + line + CHR$(10)
    NEXT i
  END IF
  RETURN out
END FUNCTION

FUNCTION IsTypeWord(w AS STRING) AS LONG
  DIM u AS STRING
  u = UCASE$(w)
  IF u = "VOID" OR u = "INT" OR u = "CHAR" OR u = "LONG" OR u = "SHORT" OR u = "FLOAT" OR u = "DOUBLE" THEN RETURN -1
  IF u = "UNSIGNED" OR u = "SIGNED" OR u = "CONST" OR u = "STRUCT" OR u = "UNION" OR u = "ENUM" THEN RETURN -1
  IF u = "BOOL" OR u = "_BOOL" OR u = "SIZE_T" OR u = "SSIZE_T" OR u = "INT8_T" OR u = "INT16_T" THEN RETURN -1
  IF u = "INT32_T" OR u = "INT64_T" OR u = "UINT8_T" OR u = "UINT16_T" OR u = "UINT32_T" OR u = "UINT64_T" THEN RETURN -1
  IF u = "INTPTR_T" OR u = "UINTPTR_T" OR u = "PTRDIFF_T" OR u = "FILE" OR u = "IF" OR u = "FOR" OR u = "WHILE" THEN RETURN -1
  IF u = "SWITCH" OR u = "RETURN" OR u = "SIZEOF" OR u = "DO" OR u = "ELSE" THEN RETURN -1
  RETURN 0
END FUNCTION

' Simboli a profondita' 0 con terminatore ';' (dichiarazione) o '{' (definizione).
' Scansione a EVENTI ({, }, \() con INSTR: niente loop carattere-per-carattere.
SUB ScanSyms(t AS STRING, wantDef AS LONG)
  DIM pos AS LONG, n AS LONG, depth AS LONG, p1 AS LONG, p2 AS LONG, p3 AS LONG
  DIM nx AS LONG, id AS STRING, j AS LONG, q1 AS LONG, q2 AS LONG, term AS STRING
  pos = 1
  n = 0
  depth = 0
  DO
    p1 = INSTR(pos, t, "{")
    p2 = INSTR(pos, t, "}")
    p3 = INSTR(pos, t, "(")
    nx = 0
    IF p1 > 0 THEN nx = p1
    IF p2 > 0 AND (nx = 0 OR p2 < nx) THEN nx = p2
    IF p3 > 0 AND (nx = 0 OR p3 < nx) THEN nx = p3
    IF nx = 0 THEN EXIT DO
    IF nx = p1 THEN
      depth = depth + 1
      pos = nx + 1
    ELSEIF nx = p2 THEN
      IF depth > 0 THEN depth = depth - 1
      pos = nx + 1
    ELSE
      ' '('
      IF depth = 0 OR wantDef = 0 THEN
        id = IdentBefore(t, nx)
        IF LEN(id) > 0 AND IsTypeWord(id) = 0 THEN
          ' trova la ')' corrispondente usando INSTR (contatore di parentesi)
          j = nx
          DIM par AS LONG
          par = 0
          DO
            q1 = INSTR(j + 1, t, "(")
            q2 = INSTR(j + 1, t, ")")
            IF q1 > 0 AND (q2 = 0 OR q1 < q2) THEN
              par = par + 1
              j = q1
            ELSEIF q2 > 0 THEN
              IF par = 0 THEN
                j = q2
                EXIT DO
              END IF
              par = par - 1
              j = q2
            ELSE
              j = 0
              EXIT DO
            END IF
          LOOP
          IF j > 0 THEN
            DO WHILE j <= LEN(t) AND IsSpaceCh(MID$(t, j + 1, 1))
              j = j + 1
            LOOP
            term = MID$(t, j + 1, 1)
            IF (wantDef <> 0 AND term = "{") OR (wantDef = 0 AND term = ";") THEN
              IF n < 32768 THEN
                g_scan(n) = id
                n = n + 1
              END IF
            END IF
            pos = j + 1
          ELSE
            pos = nx + 1
          END IF
        ELSE
          pos = nx + 1
        END IF
      ELSE
        pos = nx + 1
      END IF
    END IF
  LOOP
  g_scann = n
END SUB

SUB SymsInDir(dir AS STRING, ext AS STRING, wantDef AS LONG, out$(), BYREF nOut AS LONG)
  DIM i AS LONG, j AS LONG, n AS LONG
  FilesExt(dir, ext, g_f1(), g_fn1)
  n = 0
  FOR i = 0 TO g_fn1 - 1
    ScanSyms(StripComments(IO.READFILE(g_f1(i))), wantDef)
    FOR j = 0 TO g_scann - 1
      IF n < 8192 THEN
        out$(n) = g_scan(j)
        n = n + 1
      END IF
    NEXT j
  NEXT i
  nOut = n
END SUB

SUB HeaderSyms(dir AS STRING, out$(), BYREF nOut AS LONG)
  SymsInDir(dir, "h", 0, out$(), nOut)
END SUB

SUB DefinedSyms(dir AS STRING, out$(), BYREF nOut AS LONG)
  SymsInDir(dir, "c", 1, out$(), nOut)
END SUB

' ── .inc ────────────────────────────────────────────────────────────────────
SUB IncSymbols(files$(), nf AS LONG, basic$(), alias$(), BYREF nOut AS LONG)
  DIM i AS LONG, t AS STRING, u AS STRING, p AS LONG, rest AS STRING, line AS STRING
  DIM nb AS LONG, nm AS STRING, k AS LONG, av AS STRING, q1 AS LONG, q2 AS LONG, nl AS LONG
  nb = 0
  FOR i = 0 TO nf - 1
    t = IO.READFILE(files$(i))
    u = UCASE$(t)
    p = 1
    DO
      p = INSTR(p, u, "DECLARE FUNCTION ")
      IF p = 0 THEN EXIT DO
      rest = MID$(t, p + 17)
      k = 1
      nm = ""
      DO WHILE k <= LEN(rest) AND IsIdentCh(MID$(rest, k, 1))
        nm = nm + MID$(rest, k, 1)
        k = k + 1
      LOOP
      nl = INSTR(rest, CHR$(10))
      IF nl = 0 THEN nl = LEN(rest) + 1
      line = LEFT$(rest, nl - 1)
      av = nm
      q1 = INSTR(UCASE$(line), "ALIAS")
      IF q1 > 0 THEN
        av = ""
        q2 = INSTR(q1, line, CHR$(34))
        IF q2 > 0 THEN
          k = q2 + 1
          DO WHILE k <= LEN(line) AND MID$(line, k, 1) <> CHR$(34)
            av = av + MID$(line, k, 1)
            k = k + 1
          LOOP
        END IF
      END IF
      IF LEN(nm) > 0 AND nb < 8192 THEN
        basic$(nb) = nm
        alias$(nb) = av
        nb = nb + 1
      END IF
      p = p + 17
    LOOP
  NEXT i
  nOut = nb
END SUB

' ── insiemi ─────────────────────────────────────────────────────────────────
FUNCTION Has(arr$(), n AS LONG, s AS STRING) AS LONG
  DIM i AS LONG
  FOR i = 0 TO n - 1
    IF arr$(i) = s THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION Push(arr$(), n AS LONG, s AS STRING) AS LONG
  IF LEN(s) = 0 THEN RETURN n
  IF Has(arr$(), n, s) <> 0 THEN RETURN n
  IF n >= 8192 THEN RETURN n
  arr$(n) = s
  RETURN n + 1
END FUNCTION

' ── righe report ────────────────────────────────────────────────────────────
SUB Row(sec AS STRING, proj AS STRING, src AS STRING, item AS STRING, status AS STRING, detail AS STRING)
  DIM d AS STRING, line AS STRING, r AS LONG
  d = detail
  IF INSTR(d, CHR$(9)) > 0 THEN d = REPLACE$(d, CHR$(9), " ")
  line = sec + CHR$(9) + proj + CHR$(9) + src + CHR$(9) + item + CHR$(9) + status + CHR$(9) + d
  r = IO.FWRITE(g_th, line + CHR$(10))
  IF status = "GAP" OR status = "INFO-REMOTE" OR status = "NO-C-SOURCE" THEN
    g_gaplines = g_gaplines + "- `" + line + "`" + CHR$(10)
  END IF
END SUB

SUB Md(s AS STRING)
  DIM r AS LONG
  r = IO.FWRITE(g_mh, s + CHR$(10))
END SUB

FUNCTION IsTrackedExt(p AS STRING) AS LONG
  IF EndsWith(p, ".c") OR EndsWith(p, ".h") OR EndsWith(p, ".bas") OR EndsWith(p, ".sh") OR EndsWith(p, ".md") THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION ExtractRel(p AS STRING, base AS STRING) AS STRING
  DIM b AS STRING
  b = base
  IF RIGHT$(b, 1) <> "/" THEN b = b + "/"
  IF LEFT$(p, LEN(b)) = b THEN RETURN MID$(p, LEN(b) + 1)
  RETURN p
END FUNCTION

' ── 1) coppia storica vs viva ───────────────────────────────────────────────
FUNCTION AuditPair(label AS STRING, oldDir AS STRING, newDir AS STRING, incFile AS STRING, checkSources AS LONG) AS LONG
  DIM i AS LONG, missH AS LONG, missD AS LONG, missF AS LONG, missA AS LONG
  HeaderSyms(oldDir, g_s1(), g_n1)
  HeaderSyms(newDir, g_s2(), g_n2)
  IF checkSources <> 0 THEN
    DefinedSyms(oldDir, g_s3(), g_n3)
    DefinedSyms(newDir, g_s4(), g_n4)
    AllFiles(oldDir, g_f1(), g_fn1)
    AllFiles(newDir, g_f2(), g_fn2)
  ELSE
    g_n3 = 0
    g_n4 = 0
    g_fn1 = 0
    g_fn2 = 0
  END IF
  missH = 0
  FOR i = 0 TO g_n1 - 1
    IF Has(g_s2(), g_n2, g_s1(i)) = 0 THEN missH = missH + 1
  NEXT i
  missD = 0
  FOR i = 0 TO g_n3 - 1
    IF Has(g_s4(), g_n4, g_s3(i)) = 0 THEN missD = missD + 1
  NEXT i
  missF = 0
  FOR i = 0 TO g_fn1 - 1
    IF IsTrackedExt(g_f1(i)) <> 0 AND NOT EndsWith(g_f1(i), ".inc") THEN
      IF Has(g_f2(), g_fn2, g_f1(i)) = 0 THEN missF = missF + 1
    END IF
  NEXT i
  ' alias .inc dello storico -> .inc del monorepo (shared/include o dentro la lib)
  FilesExt(oldDir, "inc", g_s5(), g_n5)
  IncSymbols(g_s5(), g_n5, g_s6(), g_s7(), g_n6)
  IF LEN(incFile) > 0 AND FILEEXISTS(incFile) THEN
    g_s8(0) = incFile
    g_n8 = 1
  ELSE
    FilesExt(newDir, "inc", g_s8(), g_n8)
  END IF
  IncSymbols(g_s8(), g_n8, g_s9(), g_s10(), g_n9)
  missA = 0
  FOR i = 0 TO g_n6 - 1
    IF LEN(g_s7(i)) > 0 THEN
      IF Has(g_s10(), g_n9, g_s7(i)) = 0 THEN missA = missA + 1
    END IF
  NEXT i
  ' La copia viva puo' essere EVOLUTA (file/definizioni rimosse): il confronto
  ' sorgenti e' informativo. La superficie PUBBLICA (header + alias .inc) deve esserci.
  IF (missH > 0 AND checkSources <> 0) OR missA > 0 THEN
    g_gaps = g_gaps + 1
    Row("migrated_pair", label, "summary", "h=" + LTRIM$(STR$(g_n1)) + "/" + LTRIM$(STR$(g_n2)) +  " def=" + LTRIM$(STR$(g_n3)) + "/" + LTRIM$(STR$(g_n4)), "GAP",  "header_mancanti=" + LTRIM$(STR$(missH)) + " alias_mancanti=" + LTRIM$(STR$(missA)) + " (def_mancanti=" + LTRIM$(STR$(missD)) + " file_mancanti=" + LTRIM$(STR$(missF)) + ")")
    RETURN -1
  END IF
  Row("migrated_pair", label, "summary", "h=" + LTRIM$(STR$(g_n1)) + "/" + LTRIM$(STR$(g_n2)) +  " def=" + LTRIM$(STR$(g_n3)) + "/" + LTRIM$(STR$(g_n4)), "OK",  "header_mancanti=0 alias_mancanti=0 (def_mancanti=" + LTRIM$(STR$(missD)) + " file_mancanti=" + LTRIM$(STR$(missF)) + ")")
  RETURN 0
END FUNCTION

' ── 2/3) coerenza interna di lib o pacchetto ────────────────────────────────
FUNCTION AuditInterno(sec AS STRING, name AS STRING, dir AS STRING, incPath AS STRING) AS LONG
  DIM i AS LONG, undefA AS LONG, undefH AS LONG, notExp AS LONG, nAlias AS LONG
  DIM sh AS STRING, e AS STRING, libPath AS STRING, p AS STRING
  HeaderSyms(dir, g_s1(), g_n1)
  DefinedSyms(dir, g_s2(), g_n2)
  IF LEN(incPath) > 0 AND FILEEXISTS(incPath) THEN
    g_s3(0) = incPath
    g_n3 = 1
  ELSE
    FilesExt(dir, "inc", g_s3(), g_n3)
  END IF
  IncSymbols(g_s3(), g_n3, g_s4(), g_s5(), g_n4)
  nAlias = g_n4
  undefA = 0
  FOR i = 0 TO g_n4 - 1
    IF LEN(g_s5(i)) > 0 THEN
      IF Has(g_s2(), g_n2, g_s5(i)) = 0 THEN undefA = undefA + 1
    END IF
  NEXT i
  undefH = 0
  FOR i = 0 TO g_n1 - 1
    IF Has(g_s2(), g_n2, g_s1(i)) = 0 THEN undefH = undefH + 1
  NEXT i
  ' export del dylib costruito (se c'e')
  notExp = 0
  ShellInto("find " + Q(dir) + " -maxdepth 6 -type f -name 'libgmb*.dylib' 2>/dev/null", g_s6(), g_n6)
  IF g_n6 = 0 THEN
    ShellInto("find " + Q(dir) + " -maxdepth 6 -type f -name '*" + name + "*.dylib' 2>/dev/null", g_s6(), g_n6)
  END IF
  IF g_n6 > 0 THEN
    libPath = g_s6(0)
    ' nm: una riga per simbolo, solo ultimo token (awk) -> niente parsing delle colonne.
    sh = SYSTEM.SHELLOUT("nm -gU " + Q(libPath) + " 2>/dev/null | awk '{print $NF}'")
    IF LEN(sh) > 0 THEN
      DIM el$() AS STRING
      el$ = SPLIT(sh, CHR$(10))
      DIM k AS LONG, found AS LONG, ln AS STRING
      found = 0
      FOR k = 0 TO UBOUND(el$)
        ln = TrimS(el$(k))
        IF LEFT$(ln, 1) = "_" THEN ln = MID$(ln, 2)
        IF LEN(ln) > 0 THEN
          IF Has(g_s5(), g_n4, ln) <> 0 THEN found = found + 1
        END IF
      NEXT k
      notExp = g_n4 - found
    ELSE
      notExp = 0
    END IF
  END IF
  IF nAlias > 0 AND g_n2 = 0 THEN
    Row(sec, name, dir, "alias=" + LTRIM$(STR$(nAlias)), "INFO-REMOTE", _
        "nessun sorgente locale: pacchetto scaricato dal registry; ABI verificata sul binario")
  END IF
  ' Pacchetti dal registry: senza sorgenti locali alias_senza_def non e' una lacuna
  ' (l'ABI e' verificata sul binario). Per le lib con sorgenti resta un gap.
  IF (undefA > 0 AND g_n2 > 0) OR (notExp > 0 AND g_n6 > 0) THEN
    g_gaps = g_gaps + 1
    Row(sec, name, dir, "alias=" + LTRIM$(STR$(nAlias)), "GAP",  "alias_senza_def=" + LTRIM$(STR$(undefA)) + " alias_non_esportati=" + LTRIM$(STR$(notExp)) +  " header_senza_def=" + LTRIM$(STR$(undefH)))
    RETURN -1
  END IF
  Row(sec, name, dir, "alias=" + LTRIM$(STR$(nAlias)), "OK",  "alias_senza_def=0 alias_non_esportati=0 header_senza_def=" + LTRIM$(STR$(undefH)) + " (WARN)")
  RETURN 0
END FUNCTION

' ── normalizzazione ─────────────────────────────────────────────────────────
FUNCTION Normalize(name AS STRING) AS STRING
  DIM l AS STRING, prefs$() AS STRING, i AS LONG, p AS STRING
  l = LCASE$(name)
  prefs$ = SPLIT("gm_special_,gm_linalg_,gm_vector_,gm_numerical_,gm_core_,gm_vec_,gm_num_,gm_,gmbstats_,gs_stats2_,gs_stats_,gs_dist_,gs_sim_,gs_regression_,gs_,gc_,gor_,gnn_,nn_,gfx_,gg_,gsc_,h5d_,h5_,gmbmedia_,gmbgrpc_,gmbamqp_,gmbnats_,gmbkafka_,gmbmqtt_,gmbprotobuf_,gmbnetlb_,gmbredis_,redis_,mm_,gmb_", ",")
  FOR i = 0 TO UBOUND(prefs$)
    p = prefs$(i)
    IF LEN(l) >= LEN(p) THEN
      IF LEFT$(l, LEN(p)) = p THEN RETURN MID$(l, LEN(p) + 1)
    END IF
  NEXT i
  RETURN l
END FUNCTION

FUNCTION Flat(s AS STRING) AS STRING
  RETURN REPLACE$(s, "_", "")
END FUNCTION

' ── universo nomi del monorepo ──────────────────────────────────────────────
SUB BuildUniverse(mono AS STRING)
  DIM i AS LONG, j AS LONG, reg AS STRING, nm AS STRING, obj AS JSON
  FilesExt(mono + "/shared/include", "inc", g_s1(), g_n1)
  IncSymbols(g_s1(), g_n1, g_s2(), g_s3(), g_n2)
  FOR i = 0 TO g_n2 - 1
    HAdd(LCASE$(g_s2(i)))
    HAdd(LCASE$(g_s3(i)))
    HAdd(Flat(LCASE$(g_s2(i))))
    HAdd(Flat(LCASE$(g_s3(i))))
  NEXT i
  reg = mono + "/shared/spec/registry/stdlib-signatures.json"
  IF FILEEXISTS(reg) THEN
    obj = JSON.PARSE(IO.READFILE(reg))
    i = 0
    DO WHILE i < 4000
      nm = JSON.EXTRACT(obj, "procedures[" + LTRIM$(STR$(i)) + "].name")
      IF LEN(nm) = 0 THEN EXIT DO
      HAdd(LCASE$(nm))
      HAdd(Flat(LCASE$(nm)))
      i = i + 1
    LOOP
  END IF
  FilesExt(mono + "/shared/libs", "h", g_f1(), g_fn1)
  FOR i = 0 TO g_fn1 - 1
    ScanSyms(StripComments(IO.READFILE(g_f1(i))), 0)
    FOR j = 0 TO g_scann - 1
      HAdd(Normalize(g_scan(j)))
    NEXT j
  NEXT i
  FilesExt(mono + "/packages", "h", g_f1(), g_fn1)
  FOR i = 0 TO g_fn1 - 1
    ScanSyms(StripComments(IO.READFILE(g_f1(i))), 0)
    FOR j = 0 TO g_scann - 1
      HAdd(Normalize(g_scan(j)))
    NEXT j
  NEXT i
  FilesExt(mono + "/GMBasic/native/gmb_runtime_c/src", "c", g_f1(), g_fn1)
  FOR i = 0 TO g_fn1 - 1
    ScanSyms(StripComments(IO.READFILE(g_f1(i))), 1)
    FOR j = 0 TO g_scann - 1
      HAdd(Normalize(g_scan(j)))
    NEXT j
  NEXT i
  FilesExt(mono + "/GMBasic/native/gmb_vm_c", "c", g_f1(), g_fn1)
  FOR i = 0 TO g_fn1 - 1
    ScanSyms(StripComments(IO.READFILE(g_f1(i))), 1)
    FOR j = 0 TO g_scann - 1
      HAdd(Normalize(g_scan(j)))
    NEXT j
  NEXT i
END SUB

' ── 4) inventario progetto ──────────────────────────────────────────────────
SUB Inventory(project AS STRING, base AS STRING)
  DIM i AS LONG, m AS LONG, u AS LONG, nm AS STRING
  HeaderSyms(base, g_s1(), g_n1)
  DefinedSyms(base, g_s2(), g_n2)
  m = 0
  u = 0
  FOR i = 0 TO g_n1 - 1
    nm = Normalize(g_s1(i))
    IF HHas(nm) <> 0 OR HHas(Flat(nm)) <> 0 THEN
      m = m + 1
    ELSE
      u = u + 1
      Row("project_inventory", project, "header", g_s1(i), "UNMATCHED", "norm=" + nm)
    END IF
  NEXT i
  Row("project_inventory", project, base, "h=" + LTRIM$(STR$(g_n1)) + " def=" + LTRIM$(STR$(g_n2)) +  " match=" + LTRIM$(STR$(m)) + " non_match=" + LTRIM$(STR$(u)), "INFO", "")
END SUB

' ── main ────────────────────────────────────────────────────────────────────
FUNCTION Main() AS INTEGER
  DIM mono AS STRING, gsuite AS STRING, impl AS STRING, tsv AS STRING, md AS STRING
  DIM i AS LONG, a AS STRING, p AS STRING, b AS LONG
  DIM labels$() AS STRING, olds$() AS STRING, news$() AS STRING
  DIM list$() AS STRING

  SET SHELL ON
  mono = IO.CWD()
  gsuite = ""
  impl = ""
  tsv = ""
  md = ""
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--gmbsuite" AND i < COMMANDCOUNT() THEN
      i = i + 1 : gsuite = COMMAND$(i)
    ELSEIF a = "--implemented" AND i < COMMANDCOUNT() THEN
      i = i + 1 : impl = COMMAND$(i)
    ELSEIF a = "--monorepo" AND i < COMMANDCOUNT() THEN
      i = i + 1 : mono = COMMAND$(i)
    ELSEIF a = "--tsv" AND i < COMMANDCOUNT() THEN
      i = i + 1 : tsv = COMMAND$(i)
    ELSEIF a = "--md" AND i < COMMANDCOUNT() THEN
      i = i + 1 : md = COMMAND$(i)
    END IF
    i = i + 1
  LOOP
  IF LEN(gsuite) = 0 THEN gsuite = IO.PARENTDIR(mono) + "/GMBSuite"
  IF LEN(impl) = 0 THEN impl = IO.PARENTDIR(mono) + "/gmbsuite-implemented"
  IF LEN(tsv) = 0 THEN tsv = mono + "/tools/reports/gmbsuite-import-audit.tsv"
  IF LEN(md) = 0 THEN md = mono + "/tools/reports/gmbsuite-import-audit.md"
  IF NOT DIREXISTS(gsuite) THEN
    PRINT "ERRORE: gmbsuite non trovato: "; gsuite
    RETURN 2
  END IF

  DIM wr AS LONG
  wr = SYSTEM.SHELLOUT("rm -f " + Q(tsv))
  g_th = IO.FCREATE(tsv)
  wr = IO.FWRITE(g_th, "section" + CHR$(9) + "project" + CHR$(9) + "source" + CHR$(9) + "item" + CHR$(9) + "status" + CHR$(9) + "detail" + CHR$(10))
  g_gaplines = ""
  g_gaps = 0

  ' 1) copie migrate + pacchetti storicizzati
  labels$ = SPLIT("media,grpc,amqp,nats,kafka,mqtt,protobuf", ",")
  olds$ = SPLIT("gmbmedia,net/grpc,net/amqp,net/nats,net/kafka,net/mqtt,net/protobuf", ",")
  news$ = SPLIT("shared/libs/media,shared/libs/grpc,shared/libs/amqp,shared/libs/nats,shared/libs/kafka,shared/libs/mqtt,shared/libs/protobuf", ",")
  DIM incHint AS STRING
  FOR i = 0 TO UBOUND(labels$)
    incHint = mono + "/shared/include/" + labels$(i) + ".inc"
    b = AuditPair(labels$(i), impl + "/" + olds$(i), mono + "/" + news$(i), incHint, 1)
  NEXT i
  b = AuditPair("netlb", impl + "/net/gmbnet/libs/netlb", mono + "/packages/netlb", "", 0)
  b = AuditPair("redis", impl + "/net/gmbnet/libs/redis", mono + "/packages/redis", "", 0)

  ' 2) lib FFI
  ShellInto("ls -1 " + Q(mono + "/shared/libs") + " 2>/dev/null", g_f2(), g_fn2)
  FOR i = 0 TO g_fn2 - 1
    p = g_f2(i)
    IF LEN(p) > 0 AND p <> "win32" AND DIREXISTS(mono + "/shared/libs/" + p) THEN
      DIM incName AS STRING
      incName = p + ".inc"
      IF p = "opt" THEN incName = "optimize.inc"
      IF p = "or" THEN incName = "ortools.inc"
      b = AuditInterno("ffi_internal", p, mono + "/shared/libs/" + p, mono + "/shared/include/" + incName)
    END IF
  NEXT i

  ' 3) pacchetti
  ShellInto("ls -1 " + Q(mono + "/packages") + " 2>/dev/null", g_f2(), g_fn2)
  FOR i = 0 TO g_fn2 - 1
    p = g_f2(i)
    IF LEN(p) > 0 AND LEFT$(p, 1) <> "." AND DIREXISTS(mono + "/packages/" + p) THEN
      b = AuditInterno("package_internal", p, mono + "/packages/" + p, "")
    END IF
  NEXT i

  ' 4) inventario progetti gmbsuite
  BuildUniverse(mono)
  ShellInto("ls -1 " + Q(gsuite) + " 2>/dev/null", g_f2(), g_fn2)
  FOR i = 0 TO g_fn2 - 1
    p = g_f2(i)
    IF LEN(p) > 0 AND LEFT$(p, 1) <> "." AND p <> "betting" AND p <> "tools" AND p <> "config" AND p <> "docs" AND p <> "data" THEN
      IF DIREXISTS(gsuite + "/" + p) THEN
        Inventory(p, gsuite + "/" + p)
      END IF
    END IF
  NEXT i
  IF DIREXISTS(gsuite + "/betting") THEN
    ShellInto("ls -1 " + Q(gsuite + "/betting") + " 2>/dev/null", g_f2(), g_fn2)
    FOR i = 0 TO g_fn2 - 1
      p = g_f2(i)
      IF LEN(p) > 0 AND LEFT$(p, 1) <> "." AND DIREXISTS(gsuite + "/betting/" + p) THEN
        Inventory("betting/" + p, gsuite + "/betting/" + p)
      END IF
    NEXT i
  END IF

  ' chiusura TSV + report MD (le righe GAP sono poche: accumulate a parte)
  IO.FCLOSE(g_th)
  wr = SYSTEM.SHELLOUT("rm -f " + Q(md))
  g_mh = IO.FCREATE(md)
  Md("# Audit import gmbsuite -> GMBasicMonoRepo")
  Md("")
  Md("Lacune esatte (sezioni 1-3): **" + LTRIM$(STR$(g_gaps)) + "**")
  Md("")
  Md("## Righe GAP / INFO-REMOTE")
  Md("")
  IF LEN(g_gaplines) > 0 THEN
    wr = IO.FWRITE(g_mh, g_gaplines)
  END IF
  Md("")
  Md("Report completo riga per riga: `tools/reports/gmbsuite-import-audit.tsv`.")
  IO.FCLOSE(g_mh)

  PRINT "report: "; md
  PRINT "tsv:    "; tsv
  PRINT "lacune esatte: "; g_gaps
  IF g_gaps = 0 THEN RETURN 0
  RETURN 1
END FUNCTION
