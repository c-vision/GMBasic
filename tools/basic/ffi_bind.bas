' ============================================================================
' ffi_bind.bas — genera le dichiarazioni FFI (DECLARE NATIVE) da un header C.
'
' Perche': la superficie FFI (106 DECLARE NATIVE in packages/ e shared/libs/) e' scritta
' a mano ed e' una classe di bug ricorrente (arità, out-param, alias). Questo tool legge il
' VERO header e produce le righe canoniche, oppure (`--check`) verifica che quelle scritte a
' mano combacino con l'header (drift = exit 1, per la CI).
'
' Fonte delle firme: `clang -Xclang -ast-dump -fsyntax-only <header>` (una riga per funzione,
' con la posizione nel file). I NOMI dei parametri si leggono dalla riga dell'header.
'
' Uso:
'   ffi_bind.bas --header <h> --lib <nome> [--only <pref>] [--strip <pref>] [--prefix <PRE>]
'                [--include <dir>] [--out <file>] [--check <file.inc>]
'   ffi_bind.bas --selftest
'
' Mappatura C -> BASIC (conservativa: cio' che non e' mappabile si SALTA e si dice):
'   void f(...)                  -> DECLARE NATIVE SUB      F(...)
'   double|float                 -> DOUBLE
'   int|long|short|size_t|intN_t -> LONG      (bool -> LONG)
'   char*|const char*            -> STRING
'   void*                        -> QUAD
'   T* p, <len>                  -> p() AS <T> (array: BYTE se T e' byte, altrimenti DOUBLE/STRING)
'   T** p                        -> BYREF p AS <T>
'   struct/union per valore, varargs, function pointer -> SKIP (mai silenzioso)
' ============================================================================
IMPORT SYSTEM
IMPORT IO
IMPORT STRINGS

DIM g_hdr AS STRING
DIM g_lib AS STRING
DIM g_strip AS STRING
DIM g_prefix AS STRING
DIM g_out AS STRING
DIM g_check AS STRING
DIM g_inc AS STRING
DIM g_only AS STRING
DIM g_skip AS LONG
DIM g_n AS LONG
DIM g_gen(1024) AS STRING     ' righe generate
DIM g_sym(1024) AS STRING     ' simbolo C (alias) per riga
DIM g_sig(1024) AS STRING     ' firma normalizzata per riga

' ── utilità ─────────────────────────────────────────────────────────────────
FUNCTION Qt() AS STRING
  RETURN CHR$(34)
END FUNCTION

FUNCTION Q(s AS STRING) AS STRING
  DIM t AS STRING
  t = Qt()
  RETURN t + s + t
END FUNCTION

FUNCTION IsIdentCh(c AS STRING) AS LONG
  IF LEN(c) = 0 THEN RETURN 0
  DIM k AS LONG
  k = ASC(c)
  IF k >= 48 AND k <= 57 THEN RETURN 1
  IF k >= 65 AND k <= 90 THEN RETURN 1
  IF k >= 97 AND k <= 122 THEN RETURN 1
  IF c = "_" THEN RETURN 1
  RETURN 0
END FUNCTION

' Ultimo identificatore della stringa (nome di un declarator C): "const char *name" -> name
FUNCTION LastIdent(s AS STRING) AS STRING
  DIM i AS LONG, cur AS STRING, best AS STRING
  cur = ""
  FOR i = 1 TO LEN(s)
    DIM c AS STRING
    c = MID$(s, i, 1)
    IF IsIdentCh(c) THEN
      cur = cur + c
    ELSE
      IF LEN(cur) > 0 THEN
        IF LEN(best) = 0 OR cur <> "const" THEN
          IF cur <> "const" AND cur <> "unsigned" AND cur <> "signed" AND cur <> "struct" AND cur <> "union" AND cur <> "enum" THEN best = cur
        END IF
      END IF
      cur = ""
    END IF
  NEXT i
  IF LEN(cur) > 0 THEN
    IF cur <> "const" AND cur <> "unsigned" AND cur <> "signed" AND cur <> "struct" AND cur <> "union" AND cur <> "enum" THEN best = cur
  END IF
  RETURN best
END FUNCTION

' Split di s sui separatori di PRIMO livello (parentesi/quadre bilanciate).
' Riempe g_parts e ritorna quante parti.
DIM g_parts(128) AS STRING
FUNCTION SplitTop(s AS STRING, sep AS STRING) AS LONG
  DIM i AS LONG, depth AS LONG, cur AS STRING, n AS LONG
  n = 0 : cur = "" : depth = 0
  FOR i = 1 TO LEN(s)
    DIM c AS STRING
    c = MID$(s, i, 1)
    IF c = "(" OR c = "[" THEN depth = depth + 1
    IF c = ")" OR c = "]" THEN depth = depth - 1
    IF c = sep AND depth = 0 THEN
      g_parts(n) = TRIM$(cur) : n = n + 1 : cur = ""
    ELSE
      cur = cur + c
    END IF
  NEXT i
  IF LEN(TRIM$(cur)) > 0 THEN g_parts(n) = TRIM$(cur) : n = n + 1
  RETURN n
END FUNCTION

' ── mapping dei tipi ────────────────────────────────────────────────────────
FUNCTION IsBytePtr(t AS STRING) AS LONG
  DIM u AS STRING
  u = LCASE(TRIM$(t))
  u = REPLACE$(u, "const ", "")
  u = REPLACE$(u, "unsigned ", "")
  IF INSTR(u, "char*") > 0 OR INSTR(u, "char *") > 0 THEN RETURN 1
  IF INSTR(u, "uint8_t*") > 0 OR INSTR(u, "uint8_t *") > 0 THEN RETURN 1
  RETURN 0
END FUNCTION

FUNCTION IsNumPtr(t AS STRING) AS LONG
  DIM u AS STRING
  u = LCASE(TRIM$(t))
  IF INSTR(u, "*") = 0 THEN RETURN 0
  IF IsBytePtr(t) = 1 THEN RETURN 0
  IF INSTR(u, "void") > 0 THEN RETURN 0
  IF INSTR(u, "char") > 0 THEN RETURN 0
  RETURN 1
END FUNCTION

' Tipo BASIC per un tipo C scalare; "" = non mappabile.
FUNCTION BasicType(t AS STRING) AS STRING
  DIM u AS STRING
  u = LCASE(TRIM$(t))
  u = REPLACE$(u, "const ", "")
  u = REPLACE$(u, "unsigned ", "")
  u = REPLACE$(u, "signed ", "")
  IF u = "double" OR u = "float" OR u = "long double" THEN RETURN "DOUBLE"
  IF u = "void" THEN RETURN ""
  IF u = "char*" OR u = "char *" THEN RETURN "STRING"
  IF INSTR(u, "struct ") = 1 OR INSTR(u, "union ") = 1 OR INSTR(u, "enum ") = 1 THEN RETURN ""
  IF INSTR(u, "*") > 0 THEN
    IF INSTR(u, "char") > 0 THEN RETURN "STRING"
    IF INSTR(u, "void") > 0 THEN RETURN "QUAD"
    RETURN ""
  END IF
  IF INSTR(u, "bool") > 0 THEN RETURN "LONG"
  RETURN "LONG"
END FUNCTION

' Nome di parametro che rappresenta una lunghezza/conteggio (n, len, count, ...).
FUNCTION IsLenName(nm AS STRING) AS LONG
  DIM u AS STRING
  u = LCASE(TRIM$(nm))
  IF u = "n" OR u = "nn" OR u = "len" OR u = "length" OR u = "count" OR u = "cnt" THEN RETURN 1
  IF u = "cap" OR u = "size" OR u = "num" OR u = "nsize" OR u = "nlen" THEN RETURN 1
  RETURN 0
END FUNCTION

' Esiste un parametro di lunghezza DOPO l'indice startAt? (names = g_parts dei nomi)
FUNCTION IsLenNameAfter(names() AS STRING, np AS LONG, startAt AS LONG) AS LONG
  DIM j AS LONG
  FOR j = startAt + 1 TO np - 1
    IF IsLenName(names(j)) = 1 THEN RETURN 1
  NEXT j
  RETURN 0
END FUNCTION

' Una dichiarazione generata dalla firma C: ritorna "" se non mappabile (SKIP).
FUNCTION GenDecl(sym AS STRING, ret AS STRING, params AS STRING, pnames AS STRING) AS STRING
  DIM rt AS STRING, kw AS STRING, pt AS STRING
  DIM np AS LONG, i AS LONG
  DIM parts(128) AS STRING
  DIM names(128) AS STRING
  DIM decl AS STRING
  DIM basicName AS STRING

  ' nome BASIC: strip + MAIUSCOLO
  basicName = sym
  IF LEN(g_strip) > 0 THEN
    IF LEFT(LCASE(basicName), LEN(g_strip)) = LCASE(g_strip) THEN
      basicName = MID$(basicName, LEN(g_strip) + 1)
    END IF
  END IF
  basicName = g_prefix + UCASE(basicName)

  ' parametri
  np = SplitTop(params, ",")
  FOR i = 0 TO np - 1
    parts(i) = g_parts(i)
  NEXT i
  IF LEN(pnames) > 0 THEN
    DIM n2 AS LONG
    n2 = SplitTop(pnames, ",")
    FOR i = 0 TO n2 - 1
      names(i) = LastIdent(g_parts(i))
      IF LEN(names(i)) = 0 THEN names(i) = "p" + STR$(i + 1)
    NEXT i
  ELSE
    FOR i = 0 TO np - 1
      names(i) = "p" + STR$(i + 1)
    NEXT i
  END IF

  ' return: void -> SUB
  rt = BasicType(ret)
  IF LCASE(TRIM$(ret)) = "void" THEN
    kw = "SUB"
    decl = "DECLARE NATIVE SUB " + basicName + " LIB " + Qt() + g_lib + Qt() + " ALIAS " + Qt() + sym + Qt() + " ("
  ELSE
    IF LEN(rt) = 0 THEN RETURN ""
    kw = "FUNCTION"
    decl = "DECLARE NATIVE FUNCTION " + basicName + " LIB " + Qt() + g_lib + Qt() + " ALIAS " + Qt() + sym + Qt() + " ("
  END IF

  DIM argbuf AS STRING
  argbuf = ""
  FOR i = 0 TO np - 1
    DIM t AS STRING, bt AS STRING, nm AS STRING, arr AS LONG
    t = parts(i)
    nm = names(i)
    IF INSTR(t, "...") > 0 THEN RETURN ""          ' varargs: non supportate
    IF INSTR(t, "(*") > 0 OR INSTR(t, "(* )") > 0 THEN RETURN ""   ' function pointer: non mappabile
    IF INSTR(t, "(*") > 0 OR INSTR(REPLACE$(t, " ", ""), "(*") > 0 THEN RETURN ""
    arr = 0
    IF INSTR(t, "**") > 0 THEN
      ' BYREF scalare (char** -> BYREF x AS STRING)
      DIM inner AS STRING
      inner = TRIM$(REPLACE$(t, "**", ""))
      bt = BasicType(inner)
      IF LEN(bt) = 0 THEN RETURN ""
      IF argbuf <> "" THEN argbuf = argbuf + ", "
      argbuf = argbuf + "BYREF " + nm + " AS " + bt
    ELSEIF INSTR(t, "*") > 0 THEN
      ' puntatore: array se un parametro (immediato per i buffer di byte, uno
      ' qualunque successivo per i numerici: `cf, dates, n`) e' una lunghezza.
      IF IsBytePtr(t) = 1 THEN
        IF i + 1 <= np - 1 AND IsLenName(names(i + 1)) = 1 THEN arr = 1
      ELSE
        IF IsLenNameAfter(names, np, i) = 1 THEN arr = 1
      END IF
      IF arr = 1 THEN
        IF IsBytePtr(t) = 1 THEN bt = "BYTE" ELSE bt = "DOUBLE"
        IF LEN(bt) = 0 THEN RETURN ""
        IF argbuf <> "" THEN argbuf = argbuf + ", "
        argbuf = argbuf + nm + "() AS " + bt
      ELSE
        IF IsBytePtr(t) = 1 THEN bt = "STRING" ELSE bt = BasicType(t)
        IF LEN(bt) = 0 THEN RETURN ""
        IF argbuf <> "" THEN argbuf = argbuf + ", "
        argbuf = argbuf + nm + " AS " + bt
      END IF
    ELSE
      bt = BasicType(t)
      IF LEN(bt) = 0 THEN RETURN ""
      IF argbuf <> "" THEN argbuf = argbuf + ", "
      argbuf = argbuf + nm + " AS " + bt
    END IF
  NEXT i
  decl = decl + argbuf + ")"
  IF kw = "FUNCTION" THEN decl = decl + " AS " + rt
  RETURN decl
END FUNCTION

' Firma normalizzata (senza nomi dei parametri) per il confronto in --check.
' Normalizza UN parametro alla sola forma tipologica:
'   "rate AS DOUBLE"   -> "AS DOUBLE"
'   "cf() AS DOUBLE"   -> "() AS DOUBLE"     (array: la forma conta, il nome no)
'   "BYREF x AS STRING"-> "BYREF AS STRING"
FUNCTION NormParam(p AS STRING) AS STRING
  DIM s AS STRING, u AS STRING
  s = TRIM$(p)
  u = UCASE(s)
  DIM k AS LONG
  k = INSTR(u, " AS ")
  IF k = 0 THEN RETURN s
  DIM nm AS STRING, ty AS STRING
  nm = TRIM$(LEFT$(s, k - 1))
  ty = TRIM$(MID$(s, k + 4))
  IF RIGHT$(nm, 2) = "()" THEN RETURN "() AS " + ty
  IF LEFT$(u, 5) = "BYREF" THEN RETURN "BYREF AS " + ty
  RETURN "AS " + ty
END FUNCTION

FUNCTION NormSig(decl AS STRING) AS STRING
  DIM o AS LONG, c AS LONG, inside AS STRING, out AS STRING, n AS LONG, i AS LONG, depth AS LONG
  o = INSTR(decl, "(")
  IF o = 0 THEN RETURN decl
  ' parentesi BILANCIATE: un array "cf()" contiene una ")" che non chiude la firma
  depth = 0 : c = 0
  FOR i = o TO LEN(decl)
    DIM ch AS STRING
    ch = MID$(decl, i, 1)
    IF ch = "(" THEN depth = depth + 1
    IF ch = ")" THEN
      depth = depth - 1
      IF depth = 0 THEN c = i : EXIT FOR
    END IF
  NEXT i
  IF c = 0 THEN RETURN decl
  inside = MID$(decl, o + 1, c - o - 1)
  n = SplitTop(inside, ",")
  out = ""
  FOR i = 0 TO n - 1
    IF out <> "" THEN out = out + ", "
    out = out + NormParam(g_parts(i))
  NEXT i
  RETURN "(" + out + ")" + MID$(decl, c)
END FUNCTION

FUNCTION AddGen(decl AS STRING, sym AS STRING) AS LONG
  IF LEN(decl) = 0 THEN
    g_skip = g_skip + 1
    RETURN 0
  END IF
  IF g_n >= 1024 THEN RETURN 0
  g_gen(g_n) = decl
  g_sym(g_n) = sym
  g_sig(g_n) = NormSig(decl)
  g_n = g_n + 1
  RETURN 1
END FUNCTION

' ── lettura header + AST ────────────────────────────────────────────────────
DIM g_hlines(8192) AS STRING
DIM g_nh AS LONG
FUNCTION LoadHeader(path AS STRING) AS LONG
  DIM ln AS STRING
  g_nh = 0
  ON ERROR GOTO NoHeader
  OPEN path FOR INPUT AS #7
  DO WHILE NOT EOF(7)
    LINE INPUT #7, ln
    IF g_nh < 8192 THEN
      g_hlines(g_nh) = ln
      g_nh = g_nh + 1
    END IF
  LOOP
  CLOSE #7
NoHeader:
  RETURN g_nh
END FUNCTION

' Nomi dei parametri dalla riga dell'header che dichiara `sym` (CSV) — "" se non trovata.
FUNCTION ParamNames(sym AS STRING) AS STRING
  DIM i AS LONG
  FOR i = 0 TO g_nh - 1
    DIM ln AS STRING
    ln = g_hlines(i)
    DIM k AS LONG
    k = INSTR(ln, sym + "(")
    IF k = 0 THEN k = INSTR(ln, sym + " (")
    IF k > 0 THEN
      DIM o AS LONG, c AS LONG, depth AS LONG, j AS LONG
      o = k + LEN(sym)
      depth = 0 : c = 0
      FOR j = o TO LEN(ln)
        DIM ch AS STRING
        ch = MID$(ln, j, 1)
        IF ch = "(" THEN depth = depth + 1
        IF ch = ")" THEN
          depth = depth - 1
          IF depth = 0 THEN c = j : EXIT FOR
        END IF
      NEXT j
      IF c > o THEN RETURN MID$(ln, o + 1, c - o - 1)
    END IF
  NEXT i
  RETURN ""
END FUNCTION

' Analizza UNA riga dell'AST. 1 = dichiarazione generata, 0 = saltata.
' (early RETURN: niente CONTINUE, che l'emitter non risolve dentro le FUNCTION)
FUNCTION ParseDecl(ln AS STRING) AS LONG
  ' posizione: <percorso:riga:col> oppure <line:riga:col>
  DIM lt AS LONG, gt AS LONG
  lt = INSTR(ln, "<")
  gt = INSTR(ln, ">")
  IF gt = 0 THEN RETURN 0
  ' nome e prototipo: dopo "col:N " fino all'ultimo "'"
  DIM firstQ AS LONG, lastQ AS LONG
  firstQ = INSTR(ln, "'")
  IF firstQ = 0 THEN RETURN 0
  lastQ = 0
  DIM q AS LONG
  q = firstQ
  DO WHILE q > 0
    lastQ = q
    q = INSTR(q + 1, ln, "'")
  LOOP
  IF lastQ <= firstQ THEN RETURN 0
  DIM proto AS STRING
  proto = MID$(ln, firstQ + 1, lastQ - firstQ - 1)
  DIM head AS STRING
  head = MID$(ln, gt + 1, firstQ - gt - 1)
  head = TRIM$(head)
  ' head = "col:N name" -> ultimo token = nome
  DIM sym AS STRING
  sym = LastIdent(head)
  IF LEN(sym) = 0 THEN RETURN 0
  IF sym = "used" OR sym = "static" OR sym = "inline" THEN RETURN 0
  ' filtro --only: genera solo i simboli C con questo prefisso
  IF LEN(g_only) > 0 THEN
    IF LEFT$(LCASE(sym), LEN(g_only)) <> LCASE(g_only) THEN RETURN 0
  END IF
  ' proto = "ret (params)"
  DIM o AS LONG, c AS LONG, depth AS LONG, j AS LONG
  o = INSTR(proto, "(")
  IF o = 0 THEN RETURN 0
  depth = 0 : c = 0
  FOR j = o TO LEN(proto)
    DIM ch AS STRING
    ch = MID$(proto, j, 1)
    IF ch = "(" THEN depth = depth + 1
    IF ch = ")" THEN
      depth = depth - 1
      IF depth = 0 THEN c = j : EXIT FOR
    END IF
  NEXT j
  IF c = 0 THEN RETURN 0
  DIM ret AS STRING, params AS STRING
  ret = TRIM$(LEFT$(proto, o - 1))
  params = MID$(proto, o + 1, c - o - 1)
  params = TRIM$(params)
  IF LCASE(params) = "void" THEN params = ""
  DIM pn AS STRING
  pn = ParamNames(sym)
  DIM decl AS STRING
  decl = GenDecl(sym, ret, params, pn)
  DIM ok AS LONG
  ok = AddGen(decl, sym)
  IF ok = 0 THEN PRINT "SKIP "; sym; " :: "; ret; " ("; params; ")"
  RETURN ok
END FUNCTION

FUNCTION ParseAst(ast AS STRING) AS LONG
  DIM a$() AS STRING
  a$ = SPLIT(ast, CHR$(10))
  DIM i AS LONG
  FOR i = 0 TO UBOUND(a$)
    DIM ln AS STRING
    ln = a$(i)
    IF INSTR(ln, "FunctionDecl") > 0 THEN
      DIM dummy AS LONG
      dummy = ParseDecl(ln)
    END IF
  NEXT i
  RETURN g_n
END FUNCTION

' ── --check: confronta le dichiarazioni nel file con quelle generate ────────
' Una riga DECLARE: 1 se confrontata, 0 se non pertinente. Accumula in g_drift/g_found.
DIM g_drift AS LONG
DIM g_found AS LONG
DIM g_seen(1024) AS LONG
FUNCTION CheckDecl(ln AS STRING) AS LONG
  IF LEFT$(UCASE(ln), 15) <> "DECLARE NATIVE " THEN RETURN 0
  DIM al AS STRING
  al = ""
  DIM k AS LONG
  k = INSTR(ln, "ALIAS" + Qt())
  IF k = 0 THEN k = INSTR(ln, "ALIAS " + Qt())
  IF k > 0 THEN
    DIM q1 AS LONG, q2 AS LONG
    q1 = INSTR(k, ln, Qt())
    q2 = 0
    IF q1 > 0 THEN q2 = INSTR(q1 + 1, ln, Qt())
    IF q2 > q1 THEN al = MID$(ln, q1 + 1, q2 - q1 - 1)
  END IF
  IF LEN(al) = 0 THEN RETURN 0
  DIM g AS LONG, hit AS LONG
  hit = -1
  FOR g = 0 TO g_n - 1
    IF g_sym(g) = al THEN hit = g : EXIT FOR
  NEXT g
  IF hit < 0 THEN
    PRINT "EXTRA  "; al; "  (nell'inc, non nell'header)"
    g_drift = g_drift + 1
    RETURN 1
  END IF
  g_seen(hit) = 1
  DIM have AS STRING, want AS STRING
  have = NormSig(ln)
  want = g_sig(hit)
  IF have <> want THEN
    PRINT "DIFF   "; al
    PRINT "  inc   : "; have
    PRINT "  header: "; want
    g_drift = g_drift + 1
  ELSE
    g_found = g_found + 1
  END IF
  RETURN 1
END FUNCTION

FUNCTION DoCheck(path AS STRING) AS LONG
  DIM ln AS STRING
  g_drift = 0 : g_found = 0
  DIM i AS LONG
  FOR i = 0 TO 1023
    g_seen(i) = 0
  NEXT i
  OPEN path FOR INPUT AS #7
  DO WHILE NOT EOF(7)
    LINE INPUT #7, ln
    ln = TRIM$(ln)
    DIM dummy AS LONG
    dummy = CheckDecl(ln)
  LOOP
  CLOSE #7
  DIM g AS LONG
  FOR g = 0 TO g_n - 1
    IF g_seen(g) = 0 THEN
      PRINT "MISSING "; g_sym(g); "  (nell'header, non nell'inc)"
      PRINT "  gen   : "; g_gen(g)
      g_drift = g_drift + 1
    END IF
  NEXT g
  IF g_drift = 0 THEN
    PRINT "ffi_bind: OK ("; g_found; " dichiarazioni allineate all'header)"
    RETURN 0
  END IF
  PRINT "ffi_bind: DRIFT ("; g_drift; " su "; g_n; " generate)"
  RETURN 1
END FUNCTION

FUNCTION Selftest() AS LONG
  DIM d AS STRING
  d = GenDecl("ofx_fin_pmt", "double", "double, double, double, double, int", "rate, nper, pv, fv, type")
  PRINT d
  IF INSTR(d, "DECLARE NATIVE FUNCTION ") = 0 THEN PRINT "selftest: FAIL (kind)" : RETURN 1
  IF INSTR(d, "ALIAS " + Qt() + "ofx_fin_pmt" + Qt()) = 0 THEN PRINT "selftest: FAIL (alias)" : RETURN 1
  IF INSTR(d, "rate AS DOUBLE") = 0 THEN PRINT "selftest: FAIL (nomi)" : RETURN 1
  d = GenDecl("ofx_x", "void", "const uint8_t *, int64_t", "data, len")
  PRINT d
  IF INSTR(d, "DECLARE NATIVE SUB ") = 0 THEN PRINT "selftest: FAIL (sub)" : RETURN 1
  IF INSTR(d, "data() AS BYTE") = 0 THEN PRINT "selftest: FAIL (array byte)" : RETURN 1
  d = GenDecl("ofx_y", "int", "struct S", "s")
  IF LEN(d) <> 0 THEN PRINT "selftest: FAIL (struct deve essere SKIP)" : RETURN 1
  PRINT "selftest: OK"
  RETURN 0
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM i AS LONG
  SET SHELL ON          ' SYSTEM.SHELLOUT richiede l'opt-in: senza, ritorna ""
  g_hdr = "" : g_lib = "" : g_strip = "" : g_prefix = "" : g_out = "" : g_check = "" : g_inc = "" : g_only = ""
  IF COMMANDCOUNT() = 0 THEN RETURN Selftest()
  IF COMMAND$(1) = "--selftest" THEN RETURN Selftest()
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--header" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_hdr = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--lib" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_lib = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--strip" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_strip = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--prefix" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_prefix = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--out" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_out = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--check" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_check = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--include" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_inc = COMMAND$(i)
    ELSEIF COMMAND$(i) = "--only" AND i < COMMANDCOUNT() THEN
      i = i + 1 : g_only = COMMAND$(i)
    END IF
    i = i + 1
  LOOP
  IF LEN(g_hdr) = 0 OR LEN(g_lib) = 0 THEN
    PRINT "uso: ffi_bind --header <h> --lib <nome> [--only <pref>] [--strip <pref>] [--prefix <PRE>] [--include <dir>] [--out <f>] [--check <inc>]"
    RETURN 2
  END IF
  IF LoadHeader(g_hdr) = 0 THEN
    PRINT "ffi_bind: header non leggibile: "; g_hdr
    RETURN 2
  END IF
  DIM cmd AS STRING
  cmd = "clang -Xclang -ast-dump -fsyntax-only "
  IF LEN(g_inc) > 0 THEN cmd = cmd + "-I" + Q(g_inc) + " "
  cmd = cmd + Q(g_hdr)
  DIM ast AS STRING
  ast = SYSTEM.SHELLOUT(cmd)
  IF INSTR(ast, "FunctionDecl") = 0 THEN
    PRINT "ffi_bind: clang non ha prodotto AST (header con errori?)"
    RETURN 2
  END IF
  DIM n AS LONG
  n = ParseAst(ast)
  PRINT "ffi_bind: "; n; " dichiarazioni generate, "; g_skip; " saltate"
  IF LEN(g_check) > 0 THEN RETURN DoCheck(g_check)
  IF LEN(g_out) > 0 THEN
    DIM fh AS LONG
    fh = IO.FCREATE(g_out)
    DIM h AS LONG
    FOR h = 0 TO n - 1
      IO.FWRITE(fh, g_gen(h) + CHR$(10))
    NEXT h
    IO.FCLOSE(fh)
    PRINT "ffi_bind: scritto "; g_out
  ELSE
    DIM k AS LONG
    FOR k = 0 TO n - 1
      PRINT g_gen(k)
    NEXT k
  END IF
  RETURN 0
END FUNCTION
