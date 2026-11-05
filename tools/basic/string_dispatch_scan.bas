' ==============================================================================
' string_dispatch_scan.bas — Phase 4 TokenKind gate (residui di dispatch per lexeme)
'
' Scansiona GMBasicSharp/common/Compiler e GMBasicSharp/common/IlBackendCodegen
' cercando confronti per GRAFIA (.Lexeme.Equals("X"), .Lexeme == "X", .Lexeme != "X",
' .Lexeme.StartsWith("X")) su nomi che hanno gia' un TokenKind Coll* in
' Ast/CollMembers.cs. Regola: il dispatch dei membri di collection deve usare
' CollMembers.Is(token, kind); un confronto diretto su un nome mappato fa fallire
' il check (exit 1).
'
' Uso (dalla root del monorepo, gamba nativa):
'   gmbnative tools/basic/string_dispatch_scan.bas [--all]
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/string_dispatch_scan.bas sds [--all]
'
' --all stampa anche i confronti per nomi senza TokenKind (frontiera/HTTP/UDT).
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_files(8192) AS STRING
DIM g_nfiles AS LONG
DIM g_names(4096) AS STRING
DIM g_nnames AS LONG
DIM g_res(4096) AS STRING
DIM g_nres AS LONG
DIM g_other(8192) AS STRING
DIM g_nother AS LONG
DIM g_exempt(256) AS STRING
DIM g_nexempt AS LONG
DIM g_showAll AS LONG

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

' Nome (eventualmente qualificato con punti) che termina prima di pos.
FUNCTION IdentBefore(t AS STRING, pos AS LONG) AS STRING
  DIM i AS LONG, e AS LONG
  i = pos - 1
  DO WHILE i >= 1 AND MID$(t, i, 1) = " "
    i = i - 1
  LOOP
  IF i < 1 THEN RETURN ""
  IF IsIdentCh(MID$(t, i, 1)) = 0 THEN RETURN ""
  e = i
  DO WHILE i >= 1
    IF IsIdentCh(MID$(t, i, 1)) OR MID$(t, i, 1) = "." THEN
      i = i - 1
    ELSE
      EXIT DO
    END IF
  LOOP
  RETURN MID$(t, i + 1, e - i)
END FUNCTION

' Stringa tra virgolette che inizia subito dopo la prima virgoletta dopo from.
FUNCTION QuotedAfter(t AS STRING, startAt AS LONG) AS STRING
  DIM p AS LONG, i AS LONG, out AS STRING
  p = INSTR(startAt, t, CHR$(34))
  IF p = 0 THEN RETURN ""
  i = p + 1
  DO WHILE i <= LEN(t) AND MID$(t, i, 1) <> CHR$(34)
    out = out + MID$(t, i, 1)
    i = i + 1
  LOOP
  RETURN out
END FUNCTION

FUNCTION LastComp(name AS STRING) AS STRING
  DIM p AS LONG, i AS LONG
  p = 0
  FOR i = 1 TO LEN(name)
    IF MID$(name, i, 1) = "." THEN p = i
  NEXT i
  IF p = 0 THEN RETURN name
  RETURN MID$(name, p + 1)
END FUNCTION

FUNCTION IsMemberReceiver(name AS STRING) AS LONG
  DIM l AS STRING
  l = LastComp(name)
  IF l = "MethodName" OR l = "Field" OR l = "PropertyName" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsFrontier(fn AS STRING) AS LONG
  IF fn = "Compiler.Expressions.Functions.cs" THEN RETURN -1
  IF fn = "Compiler.Expressions.cs" THEN RETURN -1
  IF fn = "Compiler.Dbf.cs" THEN RETURN -1
  IF fn = "Compiler.Isam.cs" THEN RETURN -1
  IF fn = "TokenKind.cs" THEN RETURN -1
  IF fn = "CollMembers.cs" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsAllowed(name AS STRING) AS LONG
  DIM l AS STRING
  l = LCASE$(name)
  IF l = "length" OR l = "columns" OR l = "number" OR l = "linenumber" THEN RETURN -1
  IF l = "tostring" OR l = "savetofile" OR l = "symmetricexcept" THEN RETURN -1
  RETURN 0
END FUNCTION

' Eccezioni MOTIVATE (file, membro): il confronto per lexeme NON è dispatch di un
' membro di collection. Ritorna il motivo (stringa vuota = nessuna eccezione).
' Ogni eccezione è STAMPATA nel report: non è un silenziamento nascosto.
FUNCTION ExemptReason(fn AS STRING, nm AS STRING) AS STRING
  ' `.Count` su HyperLogLogType: è una property della struttura HLL, protetta da una
  ' guardia di tipo esplicita, non un membro di collection (IlProgramCompiler.Expressions.cs:969).
  IF fn = "IlProgramCompiler.Expressions.cs" AND LCASE$(nm) = "count" THEN
    RETURN "HLL .Count su HyperLogLogType (guardia di tipo), non membro di collection"
  END IF
  RETURN ""
END FUNCTION

FUNCTION BaseName(p AS STRING) AS STRING
  DIM i AS LONG, last AS LONG
  last = 0
  FOR i = 1 TO LEN(p)
    IF MID$(p, i, 1) = "/" THEN last = i
  NEXT i
  IF last = 0 THEN RETURN p
  RETURN MID$(p, last + 1)
END FUNCTION

SUB AddName(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 0 TO g_nnames - 1
    IF g_names(i) = s THEN EXIT SUB
  NEXT i
  IF g_nnames < 4096 THEN
    g_names(g_nnames) = s
    g_nnames = g_nnames + 1
  END IF
END SUB

SUB AddRes(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 0 TO g_nres - 1
    IF g_res(i) = s THEN EXIT SUB
  NEXT i
  IF g_nres < 4096 THEN
    g_res(g_nres) = s
    g_nres = g_nres + 1
  END IF
END SUB

SUB AddOther(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 0 TO g_nother - 1
    IF g_other(i) = s THEN EXIT SUB
  NEXT i
  IF g_nother < 8192 THEN
    g_other(g_nother) = s
    g_nother = g_nother + 1
  END IF
END SUB

SUB AddExempt(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 0 TO g_nexempt - 1
    IF g_exempt(i) = s THEN EXIT SUB
  NEXT i
  IF g_nexempt < 256 THEN
    g_exempt(g_nexempt) = s
    g_nexempt = g_nexempt + 1
  END IF
END SUB

' ---- nomi Coll* da CollMembers.cs -------------------------------------------
SUB LoadCollNames(path AS STRING)
  DIM t AS STRING, u AS STRING, p AS LONG, q1 AS LONG, q2 AS LONG, nm AS STRING
  DIM endp AS LONG
  g_nnames = 0
  IF NOT FILEEXISTS(path) THEN EXIT SUB
  t = IO.READFILE(path)
  u = t
  p = 1
  DO
    p = INSTR(p, t, CHR$(34) + "]" + " = TokenKind.Coll")
    IF p = 0 THEN EXIT DO
    ' la chiave e' ["nome"] = TokenKind.Coll... ; p e' la virgoletta di chiusura
    q2 = p
    q1 = q2 - 1
    DO WHILE q1 >= 1 AND MID$(t, q1, 1) <> CHR$(34)
      q1 = q1 - 1
    LOOP
    IF q1 > 0 AND q2 > q1 THEN
      nm = MID$(t, q1 + 1, q2 - q1 - 1)
      AddName(LCASE$(nm))
    END IF
    p = p + 1
  LOOP
  ' variante senza spazio: ["nome"]=TokenKind.Coll
  p = 1
  DO
    p = INSTR(p, t, CHR$(34) + "]=TokenKind.Coll")
    IF p = 0 THEN EXIT DO
    q2 = p
    q1 = q2 - 1
    DO WHILE q1 >= 1 AND MID$(t, q1, 1) <> CHR$(34)
      q1 = q1 - 1
    LOOP
    IF q1 > 0 AND q2 > q1 THEN
      nm = MID$(t, q1 + 1, q2 - q1 - 1)
      AddName(LCASE$(nm))
    END IF
    p = p + 1
  LOOP
END SUB

FUNCTION CollHas(name AS STRING) AS LONG
  DIM i AS LONG, l AS STRING
  l = LCASE$(name)
  FOR i = 0 TO g_nnames - 1
    IF g_names(i) = l THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

' ---- scansione di una riga ---------------------------------------------------
SUB ScanLine(rel AS STRING, lineno AS LONG, line AS STRING)
  DIM pos AS LONG, p AS LONG, rest AS STRING, recv AS STRING, nm AS STRING, why AS STRING
  DIM k AS LONG, c1 AS STRING
  pos = 1
  DO
    p = INSTR(pos, line, ".Lexeme")
    IF p = 0 THEN EXIT DO
    recv = IdentBefore(line, p)
    IF IsMemberReceiver(recv) <> 0 THEN
      rest = MID$(line, p + 7)
      nm = ""
      IF LEFT$(rest, 7) = ".Equals" THEN
        nm = QuotedAfter(rest, 8)
      ELSEIF LEFT$(rest, 11) = ".StartsWith" THEN
        nm = QuotedAfter(rest, 12)
      ELSE
        k = 1
        DO WHILE k <= LEN(rest) AND MID$(rest, k, 1) = " "
          k = k + 1
        LOOP
        c1 = MID$(rest, k, 2)
        IF c1 = "==" OR c1 = "!=" THEN
          k = k + 2
          DO WHILE k <= LEN(rest) AND MID$(rest, k, 1) = " "
            k = k + 1
          LOOP
          IF MID$(rest, k, 1) = CHR$(34) THEN nm = QuotedAfter(rest, k)
        END IF
      END IF
      IF LEN(nm) > 0 THEN
        IF CollHas(nm) <> 0 AND IsAllowed(nm) = 0 THEN
          why = ExemptReason(BaseName(rel), nm)
          IF LEN(why) > 0 THEN
            AddExempt(rel + ":" + LTRIM$(STR$(lineno)) + ": " + nm + " — " + why)
          ELSE
            AddRes(rel + ":" + LTRIM$(STR$(lineno)) + ": " + nm)
          END IF
        ELSE
          AddOther(rel + ":" + LTRIM$(STR$(lineno)) + ": " + nm)
        END IF
      END IF
    END IF
    pos = p + 7
  LOOP
  ' case "X":  -> altri confronti per lexeme
  pos = 1
  DO
    p = INSTR(pos, line, "case " + CHR$(34))
    IF p = 0 THEN EXIT DO
    nm = QuotedAfter(line, p + 6)
    IF LEN(nm) > 0 THEN AddOther(rel + ":" + LTRIM$(STR$(lineno)) + ": " + nm)
    pos = p + 6
  LOOP
END SUB

SUB ScanDir(dir AS STRING, rootBase AS STRING)
  DIM t AS STRING, ln$() AS STRING, i AS LONG, rel AS STRING, fn AS STRING
  DIM lines(20000) AS STRING
  g_nfiles = 0
  t = SYSTEM.SHELLOUT("find " + Q(dir) + " -name '*.cs' 2>/dev/null")
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN EXIT SUB
  FOR i = 0 TO UBOUND(ln$)
    IF LEN(TrimS(ln$(i))) > 0 THEN
      IF g_nfiles < 8192 THEN
        g_files(g_nfiles) = TrimS(ln$(i))
        g_nfiles = g_nfiles + 1
      END IF
    END IF
  NEXT i
  FOR i = 0 TO g_nfiles - 1
    fn = BaseName(g_files(i))
    IF IsFrontier(fn) = 0 THEN
      rel = g_files(i)
      IF LEFT$(rel, LEN(rootBase)) = rootBase THEN rel = MID$(rel, LEN(rootBase) + 1)
      t = IO.READFILE(g_files(i))
      ln$ = SPLIT(t, CHR$(10))
      DIM j AS LONG
      IF UBOUND(ln$) >= 0 THEN
        FOR j = 0 TO UBOUND(ln$)
          ScanLine(rel, j + 1, ln$(j))
        NEXT j
      END IF
    END IF
  NEXT i
END SUB

FUNCTION Main() AS INTEGER
  DIM i AS LONG, a AS STRING, root AS STRING
  DIM exitCode AS LONG
  SET SHELL ON
  root = IO.CWD()
  g_showAll = 0
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--all" THEN g_showAll = -1
    IF a = "--root" AND i < COMMANDCOUNT() THEN
      i = i + 1
      root = COMMAND$(i)
    END IF
    i = i + 1
  LOOP
  ' il runner esegue il binario dentro tools/basic: la root del monorepo e' due livelli sopra
  IF NOT DIREXISTS(root + "/GMBasicSharp") THEN
    IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(root)) + "/GMBasicSharp") THEN
      root = IO.PARENTDIR(IO.PARENTDIR(root))
    END IF
  END IF

  LoadCollNames(root + "/GMBasicSharp/common/Ast/CollMembers.cs")
  ScanDir(root + "/GMBasicSharp/common/Compiler", root + "/")
  ScanDir(root + "/GMBasicSharp/common/IlBackendCodegen", root + "/")

  PRINT "CollMembers mappati: "; g_nnames
  PRINT "Residui di dispatch per lexeme su membri Coll*: "; g_nres
  FOR i = 0 TO g_nres - 1
    PRINT "  "; g_res(i)
  NEXT i
  PRINT "Eccezioni motivate (non sono residui): "; g_nexempt
  FOR i = 0 TO g_nexempt - 1
    PRINT "  "; g_exempt(i)
  NEXT i

  IF g_showAll <> 0 THEN
    PRINT ""
    PRINT "Confronti per lexeme non-Coll (frontiera/UDT/HTTP): "; g_nother
    FOR i = 0 TO g_nother - 1
      IF i >= 200 THEN
        PRINT "  ... e altri "; g_nother - 200
        EXIT FOR
      END IF
      PRINT "  "; g_other(i)
    NEXT i
  END IF

  exitCode = 0
  IF g_nres > 0 THEN
    PRINT ""
    PRINT "FALLITO: usare CollMembers.Is(token, TokenKind.Coll*)."
    exitCode = 1
  ELSE
    PRINT ""
    PRINT "OK: nessun dispatch per lexeme su membri Coll*."
  END IF
  RETURN exitCode
END FUNCTION
