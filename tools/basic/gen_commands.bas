' gen_commands.bas — genera COMMANDS.TXT (port BASIC di docs/comparison/gen_commands.py).
'
' Fonti (nessun Python, nessuna variabile d'ambiente):
'   1. docs/comparison/recon-stdlib.md  — elenco canonico dei simboli (prodotto da
'      tools/basic/recon_stdlib.bas): sezioni 5-8 = funzioni/comandi/tipi/costanti.
'   2. docs/comparison/command_desc.tsv — catalogo sintassi+descrizione (prodotto da
'      tools/basic/gen_command_desc.bas).
'   3. COMMANDS.TXT corrente            — testo CURATO a mano (vince sul catalogo quando
'      non è un segnaposto) e APPENDICE delle collezioni (blocco informativo, non contato).
'
' Ordine per namespace: COMANDI → FUNZIONI → TIPI → COSTANTI (alfabetico), poi RIEPILOGO.
' Box a 3 colonne 28/66/42 (140 caratteri), come il generatore storico.
'
' Uso (dalla root del monorepo):
'   tools/basic/gen_commands.bas              scrive COMMANDS.TXT
'   tools/basic/gen_commands.bas --check      exit 1 se il file a disco divergerebbe
'   tools/basic/gen_commands.bas --out FILE   scrive su un altro percorso

IMPORT IO
IMPORT GREP

DIM g_root AS STRING
DIM g_out AS STRING
DIM g_chk AS INTEGER

' elenchi canonici (da recon-stdlib.md)
DIM fn_ns(3000) AS STRING
DIM fn_nm(3000) AS STRING
DIM fn_n AS LONG
DIM cm_ns(600) AS STRING
DIM cm_nm(600) AS STRING
DIM cm_n AS LONG
DIM ty_ns(400) AS STRING
DIM ty_nm(400) AS STRING
DIM ty_n AS LONG
DIM co_nm(400) AS STRING
DIM co_n AS LONG

' catalog e testo curato
DIM cat_sy AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM cat_ds AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM old_sy AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM old_ds AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING

' output
DIM out(30000) AS STRING
DIM out_n AS LONG
DIM app(2000) AS STRING
DIM app_n AS LONG

' ── utilità ───────────────────────────────────────────────────────────────────
FUNCTION PadStr(s AS STRING, w AS LONG) AS STRING
  DIM t AS STRING
  t = REPLACE(REPLACE(s, CHR$(10), " "), CHR$(13), " ")
  IF LEN(t) > w THEN t = MID$(t, 1, w)
  DO WHILE LEN(t) < w
    t = t + " "
  LOOP
  RETURN t
END FUNCTION

FUNCTION RTrimPunct(s AS STRING) AS STRING
  DIM t AS STRING, c AS STRING
  t = s
  DO WHILE LEN(t) > 0
    c = RIGHT$(t, 1)
    IF c = " " OR c = "," OR c = ";" OR c = ":" THEN
      t = LEFT$(t, LEN(t) - 1)
    ELSEIF c = "·" THEN
      t = LEFT$(t, LEN(t) - 1)
    ELSE
      EXIT DO
    END IF
  LOOP
  RETURN t
END FUNCTION

FUNCTION ClipWords(s AS STRING, w AS LONG) AS STRING
  ' Taglia a `w` caratteri senza spezzare l'ultima parola (solo descrizioni).
  DIM t AS STRING, cut AS STRING, sp AS LONG, i AS LONG
  t = TRIM(REPLACE(s, CHR$(10), " "))
  IF LEN(t) <= w THEN RETURN t
  cut = MID$(t, 1, w + 1)
  sp = 0
  i = LEN(cut)
  DO WHILE i >= 1
    IF MID$(cut, i, 1) = " " THEN
      sp = i
      EXIT DO
    END IF
    i = i - 1
  LOOP
  IF sp > 0 THEN
    IF sp > w * 0.6 THEN RETURN RTrimPunct(MID$(cut, 1, sp - 1))
  END IF
  RETURN RTrimPunct(MID$(t, 1, w))
END FUNCTION

DIM BOX_TOP AS STRING
DIM BOX_SEP AS STRING
DIM BOX_SEPH AS STRING
DIM BOX_HDR AS STRING
DIM BOX_CLOSE AS STRING
DIM BOX_FULL AS STRING

SUB BuildBoxes()
  DIM d138 AS STRING, d28 AS STRING, d66 AS STRING, d42 AS STRING
  d138 = DUP(138, "─")
  d28 = DUP(28, "─")
  d66 = DUP(66, "─")
  d42 = DUP(42, "─")
  BOX_TOP = "┌" + d138 + "┐"
  BOX_SEP = "├" + d28 + "┬" + d66 + "┬" + d42 + "┤"
  BOX_SEPH = "├" + d28 + "┼" + d66 + "┼" + d42 + "┤"
  BOX_HDR = "│" + PadStr("NOME", 28) + "│" + PadStr("SINTASSI / ESEMPIO", 66) + "│" + PadStr("DESCRIZIONE", 42) + "│"
  BOX_CLOSE = "└" + d28 + "┴" + d66 + "┴" + d42 + "┘"
  BOX_FULL = "└" + d138 + "┘"
END SUB

FUNCTION Row(a AS STRING, b AS STRING, c AS STRING) AS STRING
  RETURN "│" + PadStr(a, 28) + "│" + PadStr(b, 66) + "│" + PadStr(ClipWords(c, 42), 42) + "│"
END FUNCTION

SUB Emit(s AS STRING)
  IF out_n >= 29500 THEN EXIT SUB
  out(out_n) = s
  out_n = out_n + 1
END SUB

FUNCTION Num(n AS LONG) AS STRING
  RETURN TRIM(STR$(n))
END FUNCTION

FUNCTION CatKey(ns AS STRING, nm AS STRING) AS STRING
  RETURN UPPER(ns) + CHR$(9) + UPPER(nm)
END FUNCTION

FUNCTION CatSyntax(ns AS STRING, nm AS STRING) AS STRING
  DIM k AS STRING
  k = CatKey(ns, nm)
  IF k IN cat_sy THEN RETURN cat_sy(k)
  RETURN ""
END FUNCTION

FUNCTION CatDesc(ns AS STRING, nm AS STRING) AS STRING
  DIM k AS STRING
  k = CatKey(ns, nm)
  IF k IN cat_ds THEN RETURN cat_ds(k)
  RETURN ""
END FUNCTION

' un testo del COMMANDS corrente è una descrizione "vera" (non un segnaposto)?
FUNCTION IsPlaceholder(d AS STRING, ns AS STRING, nm AS STRING) AS INTEGER
  DIM u AS STRING, p AS STRING
  u = UPPER(TRIM(d))
  IF LEN(u) = 0 THEN RETURN -1
  IF LEFT$(u, 24) = "COMANDO " + UPPER(ns) + "." THEN RETURN -1
  IF LEFT$(u, 25) = "FUNZIONE " + UPPER(ns) + "." THEN RETURN -1
  IF LEFT$(u, 5) = "TIPO " AND RIGHT$(u, 1) = "." THEN RETURN -1
  IF LEFT$(u, 10) = "COSTANTE %" AND RIGHT$(u, 1) = "." THEN RETURN -1
  p = UPPER("Comando " + ns + "." + nm + ".")
  IF u = p THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION OkText(d AS STRING, ns AS STRING, nm AS STRING) AS INTEGER
  ' frammenti e segnaposto non sono descrizioni utilizzabili
  IF IsPlaceholder(d, ns, nm) <> 0 THEN RETURN 0
  IF INSTR(d, "non implementato") > 0 THEN RETURN 0
  IF INSTR(d, "...") > 0 THEN RETURN 0
  IF INSTR(d, "Parser.") > 0 THEN RETURN 0
  IF CountChar(d, "(") <> CountChar(d, ")") THEN RETURN 0
  RETURN -1
END FUNCTION

FUNCTION CountChar(s AS STRING, c AS STRING) AS LONG
  DIM i AS LONG, n AS LONG
  FOR i = 1 TO LEN(s)
    IF MID$(s, i, 1) = c THEN n = n + 1
  NEXT i
  RETURN n
END FUNCTION

' ── caricamento fonti ────────────────────────────────────────────────────────
SUB AddFn(ns AS STRING, nm AS STRING)
  IF fn_n >= 3000 THEN EXIT SUB
  fn_ns(fn_n) = ns
  fn_nm(fn_n) = nm
  fn_n = fn_n + 1
END SUB

SUB AddCm(ns AS STRING, nm AS STRING)
  IF cm_n >= 600 THEN EXIT SUB
  cm_ns(cm_n) = ns
  cm_nm(cm_n) = nm
  cm_n = cm_n + 1
END SUB

SUB AddTy(ns AS STRING, nm AS STRING)
  IF ty_n >= 400 THEN EXIT SUB
  ty_ns(ty_n) = ns
  ty_nm(ty_n) = nm
  ty_n = ty_n + 1
END SUB

SUB AddCo(nm AS STRING)
  IF co_n >= 400 THEN EXIT SUB
  co_nm(co_n) = nm
  co_n = co_n + 1
END SUB

FUNCTION SplitNsDot(s AS STRING, part AS INTEGER) AS STRING
  ' parte 0 = namespace, parte 1 = nome (split sull'ULTIMO punto)
  DIM i AS LONG, p AS LONG
  p = 0
  FOR i = 1 TO LEN(s)
    IF MID$(s, i, 1) = "." THEN p = i
  NEXT i
  IF p = 0 THEN
    IF part = 0 THEN RETURN ""
    RETURN s
  END IF
  IF part = 0 THEN RETURN LEFT$(s, p - 1)
  RETURN MID$(s, p + 1)
END FUNCTION

SUB LoadRecon(p AS STRING)
  DIM ls$() AS STRING
  DIM i AS LONG, sec AS INTEGER, s AS STRING
  ls$ = IO.READLINES(p)
  sec = 0
  FOR i = 0 TO UBOUND(ls$)
    s = TRIM(ls$(i))
    IF INSTR(s, "## 5. Elenco funzioni") > 0 THEN sec = 5
    IF INSTR(s, "## 6. Elenco comandi") > 0 THEN sec = 6
    IF INSTR(s, "## 7. Elenco tipi") > 0 THEN sec = 7
    IF INSTR(s, "## 8. Elenco costanti") > 0 THEN sec = 8
    IF LEFT$(s, 3) = "```" OR LEN(s) = 0 OR LEFT$(s, 1) = "#" THEN
      ' righe di struttura: niente
    ELSEIF sec = 5 THEN
      AddFn(SplitNsDot(s, 0), SplitNsDot(s, 1))
    ELSEIF sec = 6 THEN
      AddCm(SplitNsDot(s, 0), SplitNsDot(s, 1))
    ELSEIF sec = 7 THEN
      AddTy(SplitNsDot(s, 0), SplitNsDot(s, 1))
    ELSEIF sec = 8 THEN
      IF LEFT$(s, 1) = "%" THEN s = MID$(s, 2)
      AddCo(s)
    END IF
  NEXT i
END SUB

SUB LoadCatalog(p AS STRING)
  DIM ls$() AS STRING
  DIM f$() AS STRING
  DIM i AS LONG, k AS STRING
  IF FILEEXISTS(p) = 0 THEN EXIT SUB
  ls$ = IO.READLINES(p)
  FOR i = 0 TO UBOUND(ls$)
    IF LEFT$(ls$(i), 1) <> "#" AND LEN(TRIM(ls$(i))) > 0 THEN
      f$ = SPLIT(ls$(i), CHR$(9))
      IF UBOUND(f$) >= 4 THEN
        k = UPPER(f$(0)) + CHR$(9) + UPPER(f$(1))
        cat_sy(k) = f$(3)
        cat_ds(k) = f$(4)
      END IF
    END IF
  NEXT i
END SUB

SUB StoreOld(k AS STRING, sx AS STRING, ds AS STRING)
  ' prima occorrenza vince (una tabella può ripetere un nome in più sezioni)
  IF k IN old_sy THEN EXIT SUB
  old_sy(k) = sx
  old_ds(k) = ds
END SUB

SUB LoadOld(p AS STRING)
  DIM ls$() AS STRING
  DIM i AS LONG, ns AS STRING, nm AS STRING, sx AS STRING, ds AS STRING, k AS STRING
  DIM app_i AS LONG, st AS LONG, en AS LONG
  IF FILEEXISTS(p) = 0 THEN EXIT SUB
  ls$ = IO.READLINES(p)
  ' 1) righe curate: namespace in corso + (NOME, SINTASSI, DESCRIZIONE)
  ns = ""
  FOR i = 0 TO UBOUND(ls$)
    IF INSTR(ls$(i), "│Namespace ") > 0 THEN
      ns = NamespaceOf(ls$(i))
    ELSEIF LEFT$(ls$(i), 1) = "│" AND LEN(ns) > 0 THEN
      nm = FieldOf(ls$(i), 1)
      sx = FieldOf(ls$(i), 2)
      ds = FieldOf(ls$(i), 3)
      IF LEN(nm) > 0 THEN
        IF UPPER(nm) <> "NOME" THEN
          k = UPPER(ns) + CHR$(9) + UPPER(nm)
          IF LEFT$(sx, 4) = "x = " THEN
            StoreOld(k + CHR$(9) + "f", sx, ds)
          ELSE
            StoreOld(k + CHR$(9) + "c", sx, ds)
          END IF
        END IF
      END IF
    END IF
  NEXT i
  ' 2) APPENDICE delle collezioni: blocco informativo conservato verbatim
  '    (dal bordo superiore del box APPENDICE fino alla riga prima del box RIEPILOGO).
  app_n = 0
  app_i = -1
  FOR i = 0 TO UBOUND(ls$)
    IF INSTR(ls$(i), "APPENDICE — Metodi di tipo COLLECTIONS") > 0 THEN app_i = i
  NEXT i
  IF app_i < 0 THEN EXIT SUB
  st = app_i
  DO WHILE st > 0
    IF LEFT$(ls$(st), 1) = "┌" THEN EXIT DO
    st = st - 1
  LOOP
  en = app_i
  DO WHILE en < UBOUND(ls$)
    IF LEFT$(ls$(en), 1) = "┌" THEN
      IF INSTR(ls$(en + 1), "RIEPILOGO") > 0 THEN EXIT DO
    END IF
    en = en + 1
  LOOP
  FOR i = st TO en - 1
    IF app_n < 1900 THEN
      app(app_n) = ls$(i)
      app_n = app_n + 1
    END IF
  NEXT i
  DO WHILE app_n > 0
    IF LEN(TRIM(app(app_n - 1))) = 0 THEN
      app_n = app_n - 1
    ELSE
      EXIT DO
    END IF
  LOOP
END SUB

FUNCTION NamespaceOf(l AS STRING) AS STRING
  DIM p AS LONG, q AS LONG
  p = INSTR(l, "│Namespace ")
  IF p = 0 THEN RETURN ""
  q = INSTR(l, "│", p + 2)
  IF q = 0 THEN RETURN ""
  RETURN TRIM(MID$(l, p + 11, q - p - 11))
END FUNCTION

FUNCTION FieldOf(l AS STRING, n AS INTEGER) AS STRING
  ' campo n (1-based) di una riga di tabella con separatore │
  DIM i AS LONG, c AS LONG, a AS LONG, b AS LONG
  c = 0
  a = 0
  FOR i = 1 TO LEN(l)
    IF MID$(l, i, 1) = "│" THEN
      c = c + 1
      IF c = n THEN
        a = i + 1
      ELSEIF c = n + 1 THEN
        b = i
        EXIT FOR
      END IF
    END IF
  NEXT i
  IF a = 0 OR b = 0 THEN RETURN ""
  RETURN TRIM(MID$(l, a, b - a))
END FUNCTION

' ── risoluzione sintassi/descrizione ─────────────────────────────────────────
FUNCTION CmdSyntax(ns AS STRING, nm AS STRING) AS STRING
  DIM generic AS STRING, o AS STRING, c AS STRING
  generic = ns + " " + UPPER(nm)
  o = ""
  IF (CatKey(ns, nm) + CHR$(9) + "c") IN old_sy THEN o = old_sy(CatKey(ns, nm) + CHR$(9) + "c")
  IF LEN(o) > 0 AND o <> generic THEN RETURN o
  c = CatSyntax(ns, nm)
  IF LEN(c) > 0 AND c <> generic THEN RETURN c
  IF LEN(o) > 0 THEN RETURN o
  RETURN generic
END FUNCTION

FUNCTION CmdDesc(ns AS STRING, nm AS STRING) AS STRING
  DIM o AS STRING, c AS STRING
  o = ""
  IF (CatKey(ns, nm) + CHR$(9) + "c") IN old_ds THEN o = old_ds(CatKey(ns, nm) + CHR$(9) + "c")
  IF LEN(o) > 0 THEN
    IF OkText(o, ns, nm) <> 0 THEN RETURN o
  END IF
  c = CatDesc(ns, nm)
  IF LEN(c) > 0 THEN RETURN c
  RETURN "Comando " + ns + "." + UPPER(nm) + "."
END FUNCTION

FUNCTION FuncSyntax(ns AS STRING, nm AS STRING) AS STRING
  DIM c AS STRING, o AS STRING, generic AS STRING
  generic = "x = " + ns + "." + UPPER(nm) + "()"
  ' metadati curati (port della FUNCTION_META del generatore storico): la sintassi del
  ' catalogo usa i nomi dei parametri della registrazione StdLib, che per queste voci
  ' sono fuorvianti (es. "from/to" con un nome riservato).
  IF UPPER(ns) = "AI" AND UPPER(nm) = "KGADDNODE" THEN RETURN "x = AI.KGADDNODE(nome)"
  IF UPPER(ns) = "AI" AND UPPER(nm) = "KGADDEDGE" THEN RETURN "x = AI.KGADDEDGE(a, b)"
  IF UPPER(ns) = "AI" AND UPPER(nm) = "KGQUERY" THEN RETURN "q$ = AI.KGQUERY(nodo o ...)"
  c = CatSyntax(ns, nm)
  IF LEN(c) > 0 THEN
    IF LEFT$(c, 4) = "x = " AND c <> generic THEN RETURN c
  END IF
  o = ""
  IF (CatKey(ns, nm) + CHR$(9) + "f") IN old_sy THEN o = old_sy(CatKey(ns, nm) + CHR$(9) + "f")
  IF LEN(o) > 0 THEN RETURN o
  IF LEN(c) > 0 THEN RETURN c
  RETURN generic
END FUNCTION

FUNCTION FuncDesc(ns AS STRING, nm AS STRING) AS STRING
  DIM o AS STRING, c AS STRING
  o = ""
  IF (CatKey(ns, nm) + CHR$(9) + "f") IN old_ds THEN o = old_ds(CatKey(ns, nm) + CHR$(9) + "f")
  IF LEN(o) > 0 THEN
    IF OkText(o, ns, nm) <> 0 THEN RETURN o
  END IF
  c = CatDesc(ns, nm)
  IF LEN(c) > 0 THEN RETURN c
  RETURN "Funzione " + ns + "." + UPPER(nm) + "."
END FUNCTION

FUNCTION TypeDesc(ns AS STRING, nm AS STRING) AS STRING
  DIM c AS STRING
  c = CatDesc(ns, "TYPE:" + nm)
  IF LEN(c) > 0 THEN RETURN c
  RETURN "Tipo " + nm + "."
END FUNCTION

FUNCTION ConstDesc(ns AS STRING, nm AS STRING) AS STRING
  DIM c AS STRING
  c = CatDesc(ns, "%" + nm)
  IF LEN(c) > 0 THEN RETURN c
  RETURN "Costante %" + nm + "."
END FUNCTION

' ── ordinamento (alfabetico case-insensitive, su una lista di nomi) ──────────
SUB SortNames(a$(), n AS LONG)
  DIM i AS LONG, j AS LONG, t AS STRING
  FOR i = 1 TO n - 1
    j = i
    DO WHILE j > 0
      IF UPPER(a$(j - 1)) > UPPER(a$(j)) THEN
        t = a$(j - 1) : a$(j - 1) = a$(j) : a$(j) = t
      ELSE
        EXIT DO
      END IF
      j = j - 1
    LOOP
  NEXT i
END SUB

' ── generazione ──────────────────────────────────────────────────────────────
DIM ns_list(4000) AS STRING
DIM ns_n AS LONG
DIM wrk(3000) AS STRING
DIM wrk_n AS LONG
DIM tot_cmd AS LONG
DIM tot_func AS LONG
DIM tot_type AS LONG
DIM tot_const AS LONG

SUB BuildNsList()
  DIM i AS LONG, k AS LONG, ns AS STRING
  ns_n = 0
  FOR i = 0 TO fn_n - 1
    ns_list(ns_n) = UPPER(fn_ns(i)) : ns_n = ns_n + 1
  NEXT i
  FOR i = 0 TO cm_n - 1
    ns_list(ns_n) = UPPER(cm_ns(i)) : ns_n = ns_n + 1
  NEXT i
  FOR i = 0 TO ty_n - 1
    ns_list(ns_n) = UPPER(ty_ns(i)) : ns_n = ns_n + 1
  NEXT i
  IF co_n > 0 THEN ns_list(ns_n) = "CONSTANTS" : ns_n = ns_n + 1
  SortNames(ns_list(), ns_n)
  k = 0
  FOR i = 0 TO ns_n - 1
    IF ns_list(i) <> "" THEN
      IF k = 0 OR ns_list(i) <> ns_list(k - 1) THEN
        ns_list(k) = ns_list(i)
        k = k + 1
      END IF
    END IF
  NEXT i
  ns_n = k
END SUB

SUB CollectNames(kind AS INTEGER, ns AS STRING)
  DIM i AS LONG
  wrk_n = 0
  IF kind = 0 THEN
    FOR i = 0 TO cm_n - 1
      IF UPPER(cm_ns(i)) = ns THEN wrk(wrk_n) = cm_nm(i) : wrk_n = wrk_n + 1
    NEXT i
  ELSEIF kind = 1 THEN
    FOR i = 0 TO fn_n - 1
      IF UPPER(fn_ns(i)) = ns THEN wrk(wrk_n) = fn_nm(i) : wrk_n = wrk_n + 1
    NEXT i
  ELSEIF kind = 2 THEN
    FOR i = 0 TO ty_n - 1
      IF UPPER(ty_ns(i)) = ns THEN wrk(wrk_n) = ty_nm(i) : wrk_n = wrk_n + 1
    NEXT i
  ELSEIF kind = 3 THEN
    IF ns = "CONSTANTS" THEN
      FOR i = 0 TO co_n - 1
        wrk(wrk_n) = co_nm(i) : wrk_n = wrk_n + 1
      NEXT i
    END IF
  END IF
  SortNames(wrk(), wrk_n)
END SUB

SUB BuildCommands()
  DIM i AS LONG, j AS LONG, ns AS STRING
  DIM rows AS LONG, k AS LONG
  out_n = 0
  tot_cmd = 0 : tot_func = 0 : tot_type = 0 : tot_const = 0
  Emit(BOX_TOP)
  Emit("│" + PadStr("GMBASIC — Inventario comandi, funzioni, tipi e costanti", 138) + "│")
  Emit("│" + PadStr("Generato da tools/basic/gen_commands.bas, fonte: recon-stdlib.md + command_desc.tsv", 138) + "│")
  Emit("│" + PadStr("Ordine per namespace: COMANDI → FUNZIONI → TIPI → COSTANTI (alfabetico).", 138) + "│")
  Emit(BOX_FULL)
  FOR i = 0 TO ns_n - 1
    ns = ns_list(i)
    ' raccoglie le righe del namespace: prima contarle per sapere se emettere la sezione
    rows = 0
    CollectNames(0, ns) : rows = rows + wrk_n
    CollectNames(1, ns) : rows = rows + wrk_n
    CollectNames(2, ns) : rows = rows + wrk_n
    IF ns = "CONSTANTS" THEN rows = rows + co_n
    IF rows > 0 THEN
      Emit("")
      Emit(BOX_TOP)
      Emit("│" + PadStr("Namespace " + ns, 138) + "│")
      Emit(BOX_SEP)
      Emit(BOX_HDR)
      Emit(BOX_SEPH)
      ' COMANDI
      CollectNames(0, ns)
      FOR j = 0 TO wrk_n - 1
        Emit(Row(UPPER(wrk(j)), CmdSyntax(ns, wrk(j)), CmdDesc(ns, wrk(j))))
        tot_cmd = tot_cmd + 1
      NEXT j
      ' FUNZIONI
      CollectNames(1, ns)
      FOR j = 0 TO wrk_n - 1
        Emit(Row(UPPER(wrk(j)), FuncSyntax(ns, wrk(j)), FuncDesc(ns, wrk(j))))
        tot_func = tot_func + 1
      NEXT j
      ' TIPI
      CollectNames(2, ns)
      FOR j = 0 TO wrk_n - 1
        Emit(Row("TYPE:" + UPPER(wrk(j)), "TYPE " + wrk(j), TypeDesc(ns, wrk(j))))
        tot_type = tot_type + 1
      NEXT j
      ' COSTANTI
      IF ns = "CONSTANTS" THEN
        CollectNames(3, ns)
        FOR j = 0 TO wrk_n - 1
          Emit(Row("COSTANTE:%" + UPPER(wrk(j)), "%" + wrk(j), ConstDesc(ns, wrk(j))))
          tot_const = tot_const + 1
        NEXT j
      END IF
      Emit(BOX_CLOSE)
    END IF
  NEXT i
  ' APPENDICE (blocco informativo conservato dal file corrente)
  IF app_n > 0 THEN
    Emit("")
    FOR i = 0 TO app_n - 1
      Emit(app(i))
    NEXT i
  END IF
  ' RIEPILOGO
  Emit("")
  Emit(BOX_TOP)
  Emit("│" + PadStr("RIEPILOGO", 138) + "│")
  Emit(BOX_SEP)
  Emit(BOX_HDR)
  Emit(BOX_SEPH)
  Emit(Row("COMANDI", Num(tot_cmd), "Comandi del linguaggio"))
  Emit(Row("FUNZIONI", Num(tot_func), "Funzioni del linguaggio"))
  Emit(Row("TIPI", Num(tot_type), "Tipi del linguaggio"))
  Emit(Row("COSTANTI", Num(tot_const), "Costanti del linguaggio"))
  Emit(BOX_SEP)
  Emit(Row("TOTALE", Num(tot_cmd + tot_func + tot_type + tot_const), "Voci complessive"))
  Emit(BOX_CLOSE)
END SUB

SUB WriteOut(path AS STRING)
  DIM h AS LONG, i AS LONG, diff AS LONG, n_old AS LONG, n_new AS LONG
  DIM old$() AS STRING
  IF g_chk <> 0 THEN
    IF FILEEXISTS(path) = 0 THEN
      PRINT "DIVERGE (assente): "; path
      g_chk = 2
      EXIT SUB
    END IF
    old$ = IO.READLINES(path)
    diff = 0
    ' confronto tollerante alle righe vuote finali (IO.READLINES può restituirne una in più
    ' quando il file termina con il newline).
    n_old = UBOUND(old$) + 1
    DO WHILE n_old > 0
      IF LEN(TRIM(old$(n_old - 1))) = 0 THEN
        n_old = n_old - 1
      ELSE
        EXIT DO
      END IF
    LOOP
    n_new = out_n
    DO WHILE n_new > 0
      IF LEN(TRIM(out(n_new - 1))) = 0 THEN
        n_new = n_new - 1
      ELSE
        EXIT DO
      END IF
    LOOP
    IF n_old <> n_new THEN diff = 1
    IF diff = 0 THEN
      FOR i = 0 TO n_new - 1
        IF old$(i) <> out(i) THEN
          diff = 1
          PRINT "prima riga diversa: "; i + 1
          PRINT "  atteso: "; out(i)
          PRINT "  a disco: "; old$(i)
          EXIT FOR
        END IF
      NEXT i
    END IF
    IF diff <> 0 THEN
      PRINT "DIVERGE: "; path; " (righe a disco "; n_old; ", generate "; n_new; ")"
      g_chk = 2
    END IF
    EXIT SUB
  END IF
  h = IO.FCREATE(path)
  FOR i = 0 TO out_n - 1
    IO.FWRITE(h, out(i) + CHR$(10))
  NEXT i
  IO.FCLOSE(h)
  PRINT "scritto: "; path
END SUB

FUNCTION FindRoot() AS STRING
  DIM d AS STRING, i AS LONG
  d = IO.CWD()
  FOR i = 1 TO 8
    IF FILEEXISTS(d + "/GMBasicSharp/common/StdLib/StdLib.cs") THEN RETURN d
    IF FILEEXISTS(d + "/common/StdLib/StdLib.cs") THEN RETURN d
    d = IO.PARENTDIR(d)
  NEXT i
  RETURN ""
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM i AS LONG, p AS STRING, rep AS STRING, outfile AS STRING
  g_chk = 0 : outfile = ""
  FOR i = 1 TO COMMANDCOUNT()
    p = COMMAND$(i)
    IF p = "--check" THEN
      g_chk = 1
    ELSEIF p = "--out" THEN
      i = i + 1
      IF i <= COMMANDCOUNT() THEN outfile = COMMAND$(i)
    ELSEIF LEFT$(p, 1) = "-" THEN
      PRINT "opzione ignota: "; p
      RETURN 2
    END IF
  NEXT i
  g_root = FindRoot()
  IF LEN(g_root) = 0 THEN
    PRINT "monorepo non trovato risalendo da "; IO.CWD()
    RETURN 2
  END IF
  IF LEN(outfile) = 0 THEN outfile = g_root + "/COMMANDS.TXT"

  BuildBoxes()
  LoadRecon(g_root + "/docs/comparison/recon-stdlib.md")
  IF fn_n = 0 THEN
    PRINT "recon-stdlib.md assente o vuoto: lanciare prima tools/basic/recon_stdlib.bas"
    RETURN 2
  END IF
  LoadCatalog(g_root + "/docs/comparison/command_desc.tsv")
  ' Il testo CURATO e l'appendice si leggono SEMPRE da COMMANDS.TXT nella root, non dal
  ' percorso di output (con --out il file di destinazione può non esistere).
  LoadOld(g_root + "/COMMANDS.TXT")
  BuildNsList()
  BuildCommands()
  WriteOut(outfile)

  PRINT "gen_commands: " + Num(ns_n) + " namespace | comandi " + Num(tot_cmd) + " | funzioni " + Num(tot_func) + " | tipi " + Num(tot_type) + " | costanti " + Num(tot_const) + " | totale " + Num(tot_cmd + tot_func + tot_type + tot_const)
  IF g_chk <> 0 AND g_chk > 1 THEN
    PRINT "gen_commands: COMMANDS.TXT NON è aggiornato (rilanciare senza --check)"
    RETURN 1
  END IF
  IF g_chk <> 0 THEN PRINT "gen_commands: OK, COMMANDS.TXT allineato"
  RETURN 0
END FUNCTION
