' recon_stdlib.bas — ricognizione della StdLib (port BASIC di docs/comparison/recon_stdlib.py).
'
' Scansiona GMBasicSharp/common/StdLib/StdLib.*.cs e produce, in docs/comparison/:
'   recon-stdlib.json   dati strutturati, chiave canonica (NAMESPACE, NOME)
'   recon-stdlib.md     report globale (sintesi, per-namespace, polimorfici, elenchi)
'   audit-stdlib.md     audit per namespace (Comandi/Funzioni+ritorno/Tipi/Costanti)
' Con --headers aggiorna anche il blocco /* GMB-STDLIB-AUDIT-BEGIN ... END */ in testa a
' ogni file StdLib (idempotente, come faceva il Python).
'
' Perche': il Python e' stato rimosso dal monorepo (policy "niente Python") e i tre report
' erano FERMI. Qui la stessa estrazione usa solo IO/GREP del linguaggio: nessuna dipendenza
' esterna, nessuna variabile d'ambiente.
'
' Uso (dalla root del monorepo):
'   tools/basic/recon_stdlib.bas                  genera/aggiorna i tre report
'   tools/basic/recon_stdlib.bas --check          exit 1 se i report a disco divergono
'   tools/basic/recon_stdlib.bas --headers        aggiorna anche gli header nei .cs
'   tools/basic/recon_stdlib.bas --out-dir DIR    cartella di output (default docs/comparison)

IMPORT IO
IMPORT GREP

DIM g_std AS STRING
DIM g_out AS STRING
DIM g_hdr AS INTEGER
DIM g_chk AS INTEGER
DIM g_verbose AS INTEGER
DIM g_dirty AS INTEGER

' occorrenze (non deduplicate): servono anche per gli header per-file
DIM f_ns(8000) AS STRING
DIM f_nm(8000) AS STRING
DIM f_rt(8000) AS STRING
DIM nf AS LONG
DIM c_ns(1200) AS STRING
DIM c_nm(1200) AS STRING
DIM nc AS LONG
DIM t_ns(1200) AS STRING
DIM t_nm(1200) AS STRING
DIM nt AS LONG
DIM k_nm(1200) AS STRING
DIM nk AS LONG

DIM fi_path(600) AS STRING
DIM fi_ns(600) AS STRING
DIM fi_f0(600) AS LONG
DIM fi_f1(600) AS LONG
DIM fi_c0(600) AS LONG
DIM fi_c1(600) AS LONG
DIM fi_t0(600) AS LONG
DIM fi_t1(600) AS LONG
DIM fi_k0(600) AS LONG
DIM fi_k1(600) AS LONG
DIM nfi AS LONG

' ── utilità ───────────────────────────────────────────────────────────────────
FUNCTION ParenDelta(s AS STRING) AS LONG
  DIM i AS LONG, d AS LONG, c AS STRING
  d = 0
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c = "(" THEN d = d + 1
    IF c = ")" THEN d = d - 1
  NEXT i
  RETURN d
END FUNCTION

FUNCTION BraceDelta(s AS STRING) AS LONG
  DIM i AS LONG, d AS LONG, c AS STRING
  d = 0
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c = "{" THEN d = d + 1
    IF c = "}" THEN d = d - 1
  NEXT i
  RETURN d
END FUNCTION

FUNCTION QuotedAfter(s AS STRING, startAt AS LONG) AS STRING
  DIM a AS LONG, b AS LONG
  IF startAt < 1 THEN startAt = 1
  a = INSTR(s, CHR$(34), startAt)
  IF a = 0 THEN RETURN ""
  b = INSTR(s, CHR$(34), a + 1)
  IF b = 0 THEN RETURN ""
  RETURN MID$(s, a + 1, b - a - 1)
END FUNCTION

FUNCTION FirstQuoted(s AS STRING) AS STRING
  RETURN QuotedAfter(s, 1)
END FUNCTION

FUNCTION IsIdentChar(c AS STRING) AS INTEGER
  DIM n AS INTEGER
  n = ASC(c)
  IF n >= 65 AND n <= 90 THEN RETURN -1
  IF n >= 97 AND n <= 122 THEN RETURN -1
  IF n >= 48 AND n <= 57 THEN RETURN -1
  IF c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IdentAfter(s AS STRING, pos AS LONG) AS STRING
  DIM q AS LONG, r AS STRING
  q = pos
  r = ""
  DO WHILE q <= LEN(s)
    IF IsIdentChar(MID$(s, q, 1)) = 0 THEN EXIT DO
    r = r + MID$(s, q, 1)
    q = q + 1
  LOOP
  RETURN r
END FUNCTION

FUNCTION CommentLine(l AS STRING) AS INTEGER
  DIM t AS STRING
  t = TRIM(l)
  IF LEFT$(t, 2) = "//" THEN RETURN -1
  RETURN 0
END FUNCTION

' ── estrazione ────────────────────────────────────────────────────────────────
FUNCTION FileNamespace(ls$()) AS STRING
  DIM i AS LONG, l AS STRING, p AS LONG
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    p = INSTR(l, "const string")
    IF p > 0 THEN
      IF INSTR(l, "namespaceName") > 0 OR INSTR(l, " ns ") > 0 OR INSTR(l, "ns =") > 0 THEN
        IF LEN(FirstQuoted(l)) > 0 THEN RETURN FirstQuoted(l)
      END IF
    END IF
  NEXT i
  RETURN ""
END FUNCTION

FUNCTION BlockName(block AS STRING) AS STRING
  DIM p AS LONG
  p = INSTR(block, "Name: new Token(")
  IF p > 0 THEN RETURN QuotedAfter(block, p)
  p = INSTR(block, "T(TokenKind.")
  IF p = 0 THEN p = INSTR(block, "Tk(TokenKind.")
  IF p = 0 THEN RETURN ""
  RETURN QuotedAfter(block, p)
END FUNCTION

FUNCTION BlockReturnType(block AS STRING) AS STRING
  DIM p AS LONG, r AS STRING, q AS LONG, c AS STRING, best AS LONG
  ' Tipo di ritorno = il `new <Tipo>(` che segue la lista parametri `]`.
  ' Il blocco è ricomposto unendo righe con spazi, quindi fra `]`, `,` e `new` possono esserci
  ' più spazi: si cerca ogni "new ", si torna indietro saltando gli spazi fino a `,` e poi a `]`.
  best = 0
  p = INSTR(block, "new ")
  DO WHILE p > 0
    q = p - 1
    DO WHILE q > 0
      c = MID$(block, q, 1)
      IF c <> " " AND c <> CHR$(9) AND c <> CHR$(10) THEN EXIT DO
      q = q - 1
    LOOP
    IF q > 0 THEN
      IF MID$(block, q, 1) = "," THEN
        q = q - 1
        DO WHILE q > 0
          c = MID$(block, q, 1)
          IF c <> " " AND c <> CHR$(9) AND c <> CHR$(10) THEN EXIT DO
          q = q - 1
        LOOP
        IF q > 0 THEN
          IF MID$(block, q, 1) = "]" THEN best = p
        END IF
      END IF
    END IF
    p = INSTR(block, "new ", p + 4)
  LOOP
  IF best > 0 THEN
    r = IdentAfter(block, best + 4)
    IF LEN(r) > 0 THEN RETURN r
  END IF
  p = INSTR(block, "ReturnType: new ")
  IF p > 0 THEN
    r = IdentAfter(block, p + 15)
    IF LEN(r) > 0 THEN RETURN r
  END IF
  RETURN "?"
END FUNCTION

SUB AddFunc(ns AS STRING, nm AS STRING, rt AS STRING)
  IF nf >= 8000 THEN EXIT SUB
  f_ns(nf) = ns : f_nm(nf) = nm : f_rt(nf) = rt
  nf = nf + 1
END SUB

SUB AddCmd(ns AS STRING, nm AS STRING)
  IF nc >= 1200 THEN EXIT SUB
  c_ns(nc) = ns : c_nm(nc) = nm
  nc = nc + 1
END SUB

' nomi dei tipi già visti: chiave UPPER(ns)+TAB+UPPER(nome) -> indice+1.
' Serve ad accoppiare il nome LINGUAGGIO (`HttpOptions`, chiave del dizionario `types`) con la
' classe C# (`HttpOptionsType`, da RegisterTypeSchema): è lo STESSO tipo, si tiene un nome solo
' (la classe C#, quella usata in COMMANDS.TXT). Senza, il conteggio raddoppia.
DIM type_seen AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER

SUB AddType(ns AS STRING, nm AS STRING)
  DIM u AS STRING, key AS STRING, short AS STRING, idx AS LONG
  IF nt >= 1200 THEN EXIT SUB
  IF LEN(nm) = 0 THEN EXIT SUB
  u = UPPER(nm)
  key = UPPER(ns) + CHR$(9) + u
  IF RIGHT$(u, 4) = "TYPE" AND LEN(u) > 5 THEN
    short = UPPER(ns) + CHR$(9) + LEFT$(u, LEN(u) - 4)
    IF short IN type_seen THEN
      idx = type_seen(short)
      t_nm(idx - 1) = nm
      type_seen(key) = idx
      EXIT SUB
    END IF
  ELSE
    IF (UPPER(ns) + CHR$(9) + u + "TYPE") IN type_seen THEN EXIT SUB
  END IF
  IF key IN type_seen THEN EXIT SUB
  t_ns(nt) = ns
  t_nm(nt) = nm
  type_seen(key) = nt + 1
  nt = nt + 1
END SUB

SUB AddConst(nm AS STRING)
  IF nk >= 1200 THEN EXIT SUB
  k_nm(nk) = nm
  nk = nk + 1
END SUB

' Tabella dei dispatcher (identica a recon_stdlib.py / gen_commands.py): il verbo registrato
' e' solo il namespace, il parser dispaccia a sottocomandi reali. La voce 'NS.NS' e' un
' artefatto e viene sostituita dai sottocomandi veri.
FUNCTION DispatchList(verb AS STRING) AS STRING
  DIM v AS STRING
  v = UPPER(verb)
  IF v = "HTTP" THEN RETURN "SERVER,ROUTE,USE,CONFIG,STATIC"
  IF v = "WINDOW" THEN RETURN "DEFINE,SHOW,SELECT,THEME,DESKTOP,FRAME,SHADOW,SET,PRINT,TEXT,CLEAR,SCROLL,EDIT"
  IF v = "MOUSE" THEN RETURN "ON,OFF,SHOW,HIDE,SET,LIMIT,UNLIMIT,CURSOR,WAIT"
  IF v = "MATRIX" THEN RETURN "IDENTITY,TRANSLATE,ROTATE,SCALE,MULTIPLY,PUSH,POP,WORLD,VIEW,PROJECTION"
  IF v = "VECTOR" THEN RETURN "ADD,ADD WRAP,FUNCTION,MUL,DIV,SET,WRAP,FILL RANDOM,BOUNCE2D"
  IF v = "CONVERT" THEN RETURN "EXTRACT,RENDER"
  IF v = "CHART" THEN RETURN "PLOT,FINANCIAL,CLEAR,CLOSE,SAVE,SHOW"
  IF v = "JSON" THEN RETURN "SAVE,WRITEVALUE,WRITESECTION,OPEN"
  RETURN ""
END FUNCTION

SUB AddCmdExpanded(ns AS STRING, verb AS STRING)
  DIM lst AS STRING, parts$() AS STRING, i AS LONG
  lst = DispatchList(verb)
  IF LEN(lst) = 0 THEN
    AddCmd(ns, TRIM(verb))
    EXIT SUB
  END IF
  parts$ = SPLIT(lst, ",")
  FOR i = 0 TO UBOUND(parts$)
    AddCmd(ns, TRIM(parts$(i)))
  NEXT i
END SUB

SUB ScanFile(path AS STRING)
  DIM ls$() AS STRING
  DIM ns AS STRING
  DIM l AS STRING, block AS STRING, nm AS STRING, rt AS STRING, vrb AS STRING, cmd_ns AS STRING
  DIM i AS LONG, j AS LONG, k AS LONG, d AS LONG, p AS LONG, q1 AS LONG, q2 AS LONG
  DIM cpos AS LONG, nspos AS LONG
  DIM f0 AS LONG, c0 AS LONG, t0 AS LONG, k0 AS LONG
  DIM depth AS LONG, seen AS LONG, ch AS STRING

  f0 = nf : c0 = nc : t0 = nt : k0 = nk
  ls$ = IO.READLINES(path)
  ns = FileNamespace(ls$)
  ' NB: i file SENZA namespace (es. StdLib.Polymorfic.cs) hanno comunque comandi registrati
  ' con namespace LETTERALE: si scansionano lo stesso (come faceva il Python).

  ' 1) funzioni: new *ProcedureDefinition( ... ) a parentesi bilanciate
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = ls$(i)
    IF LEN(ns) > 0 AND INSTR(l, "ProcedureDefinition") > 0 AND INSTR(l, "new ") > 0 THEN
      block = l
      d = ParenDelta(l)
      j = i
      DO WHILE d > 0 AND j < UBOUND(ls$)
        j = j + 1
        block = block + " " + ls$(j)
        d = d + ParenDelta(ls$(j))
      LOOP
      nm = BlockName(block)
      IF LEN(nm) > 0 THEN
        rt = BlockReturnType(block)
        AddFunc(ns, nm, rt)
      END IF
      i = j
    END IF
    i = i + 1
  LOOP

  ' 2) comandi: RegisterCommand(TokenKind.X, "VERBO", <"NS"|namespaceName>, ...)
  '    Il namespace puo' essere un LETTERALE diverso da quello del file (varianti forzate,
  '    es. CPALETTE registrata con "CONSOLE" da StdLib.Graphics.cs): si usa quello.
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    IF CommentLine(l) = 0 THEN
      p = INSTR(l, "RegisterCommand(")
      DO WHILE p > 0
        vrb = QuotedAfter(l, p)
        IF LEN(vrb) > 0 THEN
          q1 = INSTR(l, CHR$(34), p)
          q2 = 0
          IF q1 > 0 THEN q2 = INSTR(l, CHR$(34), q1 + 1)
          cmd_ns = ns
          IF q2 > 0 THEN
            cpos = INSTR(l, ",", q2 + 1)
            IF cpos > 0 THEN
              nspos = 0
              FOR k = cpos + 1 TO LEN(l)
                IF MID$(l, k, 1) <> " " THEN
                  nspos = k
                  EXIT FOR
                END IF
              NEXT k
              IF nspos > 0 THEN
                IF MID$(l, nspos, 1) = CHR$(34) THEN cmd_ns = QuotedAfter(l, nspos)
              END IF
            END IF
          END IF
          IF LEN(cmd_ns) > 0 THEN AddCmdExpanded(cmd_ns, vrb)
        END IF
        p = INSTR(l, "RegisterCommand(", p + 16)
      LOOP
    END IF
  NEXT i

  ' 3a) tipi: RegisterTypeSchema(typeof(X), ...)
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    p = INSTR(l, "RegisterTypeSchema")
    IF p > 0 THEN
      q = INSTR(l, "typeof(")
      IF q > 0 THEN
        nm = IdentAfter(l, q + 7)
        IF LEN(nm) > 0 THEN AddType(ns, nm)
      END IF
    END IF
  NEXT i
  ' 3b) tipi: chiavi del dizionario `types` (nomi LINGUAGGIO, es. STRINGBUILDER, LIST)
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = ls$(i)
    IF INSTR(l, "var types") > 0 AND INSTR(l, "Dictionary<string, DataType>") > 0 THEN
      block = ""
      depth = 0
      seen = 0
      j = i
      DO WHILE j <= UBOUND(ls$)
        block = block + " " + ls$(j)
        d = BraceDelta(ls$(j))
        depth = depth + d
        IF d > 0 THEN seen = 1
        j = j + 1
        IF seen = -1 OR (seen = 1 AND depth <= 0) THEN EXIT DO
        IF seen = 0 AND j > i + 3 THEN EXIT DO
      LOOP
      ' chiavi nella forma `{ "NOME"`: da ogni graffa, salta gli spazi, poi la virgoletta
      FOR k = 1 TO LEN(block)
        IF MID$(block, k, 1) = "{" THEN
          q = k + 1
          DO WHILE q <= LEN(block)
            IF MID$(block, q, 1) <> " " THEN EXIT DO
            q = q + 1
          LOOP
          IF q <= LEN(block) THEN
            IF MID$(block, q, 1) = CHR$(34) THEN
              nm = QuotedAfter(block, q)
              IF LEN(nm) > 0 THEN AddType(ns, nm)
            END IF
          END IF
        END IF
      NEXT k
      i = j
    END IF
    i = i + 1
  LOOP

  ' 4a) costanti: AddConst("NOME") — piu' chiamate possono stare sulla stessa riga
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    IF LEN(ns) > 0 THEN
      p = INSTR(l, "AddConst(")
      DO WHILE p > 0
        nm = QuotedAfter(l, p)
        IF LEN(nm) > 0 THEN AddConst(nm)
        p = INSTR(l, "AddConst(", p + 9)
      LOOP
    END IF
  NEXT i
  ' 4b) costanti: new ConstDeclaration(..., T(TokenKind, "NOME"))
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = ls$(i)
    IF INSTR(l, "ConstDeclaration") > 0 THEN
      block = l
      d = ParenDelta(l)
      j = i
      DO WHILE d > 0 AND j < UBOUND(ls$)
        j = j + 1
        block = block + " " + ls$(j)
        d = d + ParenDelta(ls$(j))
      LOOP
      p = INSTR(block, "T(TokenKind.")
      IF p > 0 THEN
        nm = QuotedAfter(block, p)
        IF LEN(nm) > 0 THEN
          IF LEFT$(nm, 1) = "%" THEN nm = MID$(nm, 2)
          IF LEN(nm) > 0 THEN AddConst(nm)
        END IF
      END IF
      i = j
    END IF
    i = i + 1
  LOOP

  fi_path(nfi) = path : fi_ns(nfi) = ns
  fi_f0(nfi) = f0 : fi_f1(nfi) = nf
  fi_c0(nfi) = c0 : fi_c1(nfi) = nc
  fi_t0(nfi) = t0 : fi_t1(nfi) = nt
  fi_k0(nfi) = k0 : fi_k1(nfi) = nk
  nfi = nfi + 1
END SUB

' ── ordinamento ───────────────────────────────────────────────────────────────
SUB Sort3(ka$(), kb$(), kc$(), n AS LONG)
  DIM i AS LONG, j AS LONG
  DIM ta AS STRING, tb AS STRING, tc AS STRING
  FOR i = 1 TO n - 1
    j = i
    DO WHILE j > 0
      IF UPPER(ka$(j - 1)) > UPPER(ka$(j)) OR (UPPER(ka$(j - 1)) = UPPER(ka$(j)) AND UPPER(kb$(j - 1)) > UPPER(kb$(j))) THEN
        ta = ka$(j - 1) : ka$(j - 1) = ka$(j) : ka$(j) = ta
        tb = kb$(j - 1) : kb$(j - 1) = kb$(j) : kb$(j) = tb
        tc = kc$(j - 1) : kc$(j - 1) = kc$(j) : kc$(j) = tc
      ELSE
        EXIT DO
      END IF
      j = j - 1
    LOOP
  NEXT i
END SUB

SUB Sort1(ka$(), n AS LONG)
  DIM i AS LONG, j AS LONG, t AS STRING
  FOR i = 1 TO n - 1
    j = i
    DO WHILE j > 0
      IF UPPER(ka$(j - 1)) > UPPER(ka$(j)) THEN
        t = ka$(j - 1) : ka$(j - 1) = ka$(j) : ka$(j) = t
      ELSE
        EXIT DO
      END IF
      j = j - 1
    LOOP
  NEXT i
END SUB

' copia i nomi di uno slice [a..b) nell'array di lavoro h_nm (per gli header)
DIM h_nm(2000) AS STRING
DIM h_n AS LONG

SUB CollectNames(kind AS INTEGER, a AS LONG, b AS LONG)
  DIM i AS LONG
  h_n = 0
  FOR i = a TO b - 1
    IF kind = 0 THEN h_nm(h_n) = f_nm(i)
    IF kind = 1 THEN h_nm(h_n) = c_nm(i)
    IF kind = 2 THEN h_nm(h_n) = t_nm(i)
    IF kind = 3 THEN h_nm(h_n) = k_nm(i)
    h_n = h_n + 1
  NEXT i
END SUB

' ── scrittura / verifica ──────────────────────────────────────────────────────
SUB WriteOrCheck(path AS STRING, ls$(), n AS LONG)
  DIM h AS LONG, i AS LONG, diff AS LONG
  DIM old$() AS STRING
  IF g_chk <> 0 THEN
    IF FILEEXISTS(path) = 0 THEN
      PRINT "DIVERGE (assente): "; path
      g_chk = 2
      EXIT SUB
    END IF
    old$ = IO.READLINES(path)
    diff = 0
    IF UBOUND(old$) <> n - 1 THEN diff = 1
    IF diff = 0 THEN
      FOR i = 0 TO n - 1
        IF old$(i) <> ls$(i) THEN diff = 1 : EXIT FOR
      NEXT i
    END IF
    IF diff <> 0 THEN
      PRINT "DIVERGE: "; path
      g_chk = 2
    END IF
    EXIT SUB
  END IF
  h = IO.FCREATE(path)
  FOR i = 0 TO n - 1
    IO.FWRITE(h, ls$(i) + CHR$(10))
  NEXT i
  IO.FCLOSE(h)
  PRINT "scritto: "; path
END SUB

' ── liste uniche (chiave (NAMESPACE, NOME), ordinate case-insensitive) ───────
DIM u_ns(8000) AS STRING
DIM u_nm(8000) AS STRING
DIM u_rt(8000) AS STRING
DIM u_n AS LONG
DIM v_ns(1200) AS STRING
DIM v_nm(1200) AS STRING
DIM v_rt(1200) AS STRING
DIM v_n AS LONG
DIM w_ns(1200) AS STRING
DIM w_nm(1200) AS STRING
DIM w_rt(1200) AS STRING
DIM w_n AS LONG
DIM z_nm(1200) AS STRING
DIM z_n AS LONG
DIM ns_list(4000) AS STRING
DIM ns_n AS LONG
DIM out(60000) AS STRING
DIM out_n AS LONG
DIM poly_f AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM poly_c AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM poly_fk(8000) AS STRING
DIM poly_fkn AS LONG
DIM poly_ck(1200) AS STRING
DIM poly_ckn AS LONG
DIM rt_of AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_root AS STRING

FUNCTION Num(n AS LONG) AS STRING
  RETURN TRIM(STR$(n))
END FUNCTION

FUNCTION CommaIf(n AS LONG) AS STRING
  ' virgola di separazione JSON quando restano `n` elementi dopo questo.
  IF n > 0 THEN RETURN ","
  RETURN ""
END FUNCTION

SUB Emit(s AS STRING)
  IF out_n >= 59000 THEN EXIT SUB
  out(out_n) = s
  out_n = out_n + 1
END SUB

SUB LoadUnique()
  DIM i AS LONG, k AS LONG
  ' funzioni
  u_n = 0
  FOR i = 0 TO nf - 1
    u_ns(u_n) = f_ns(i) : u_nm(u_n) = f_nm(i) : u_rt(u_n) = f_rt(i)
    u_n = u_n + 1
  NEXT i
  Sort3(u_ns(), u_nm(), u_rt(), u_n)
  k = 0
  FOR i = 0 TO u_n - 1
    IF k = 0 OR UPPER(u_ns(i)) <> UPPER(u_ns(k - 1)) OR UPPER(u_nm(i)) <> UPPER(u_nm(k - 1)) THEN
      u_ns(k) = u_ns(i) : u_nm(k) = u_nm(i) : u_rt(k) = u_rt(i)
      k = k + 1
    END IF
  NEXT i
  u_n = k
  ' comandi
  v_n = 0
  FOR i = 0 TO nc - 1
    v_ns(v_n) = c_ns(i) : v_nm(v_n) = c_nm(i) : v_rt(v_n) = ""
    v_n = v_n + 1
  NEXT i
  Sort3(v_ns(), v_nm(), v_rt(), v_n)
  k = 0
  FOR i = 0 TO v_n - 1
    IF k = 0 OR UPPER(v_ns(i)) <> UPPER(v_ns(k - 1)) OR UPPER(v_nm(i)) <> UPPER(v_nm(k - 1)) THEN
      v_ns(k) = v_ns(i) : v_nm(k) = v_nm(i)
      k = k + 1
    END IF
  NEXT i
  v_n = k
  ' tipi
  w_n = 0
  FOR i = 0 TO nt - 1
    w_ns(w_n) = t_ns(i) : w_nm(w_n) = t_nm(i) : w_rt(w_n) = ""
    w_n = w_n + 1
  NEXT i
  Sort3(w_ns(), w_nm(), w_rt(), w_n)
  k = 0
  FOR i = 0 TO w_n - 1
    IF k = 0 OR UPPER(w_ns(i)) <> UPPER(w_ns(k - 1)) OR UPPER(w_nm(i)) <> UPPER(w_nm(k - 1)) THEN
      w_ns(k) = w_ns(i) : w_nm(k) = w_nm(i)
      k = k + 1
    END IF
  NEXT i
  w_n = k
  ' costanti (namespace sempre CONSTANTS)
  z_n = 0
  FOR i = 0 TO nk - 1
    z_nm(z_n) = k_nm(i)
    z_n = z_n + 1
  NEXT i
  Sort1(z_nm(), z_n)
  k = 0
  FOR i = 0 TO z_n - 1
    IF k = 0 OR UPPER(z_nm(i)) <> UPPER(z_nm(k - 1)) THEN
      z_nm(k) = z_nm(i)
      k = k + 1
    END IF
  NEXT i
  z_n = k
END SUB

SUB BuildNsList()
  DIM i AS LONG, k AS LONG, ns AS STRING
  ns_n = 0
  FOR i = 0 TO u_n - 1
    ns = UPPER(u_ns(i))
    IF ns = "?" OR ns = "" THEN ns = ""
    IF LEN(ns) > 0 THEN ns_list(ns_n) = ns : ns_n = ns_n + 1
  NEXT i
  FOR i = 0 TO v_n - 1
    ns = UPPER(v_ns(i))
    IF LEN(ns) > 0 THEN ns_list(ns_n) = ns : ns_n = ns_n + 1
  NEXT i
  FOR i = 0 TO w_n - 1
    ns = UPPER(w_ns(i))
    IF LEN(ns) > 0 THEN ns_list(ns_n) = ns : ns_n = ns_n + 1
  NEXT i
  IF z_n > 0 THEN ns_list(ns_n) = "CONSTANTS" : ns_n = ns_n + 1
  Sort1(ns_list(), ns_n)
  IF g_verbose <> 0 THEN PRINT "ns prima del dedup: " + Num(ns_n)
  k = 0
  FOR i = 0 TO ns_n - 1
    IF k = 0 OR ns_list(i) <> ns_list(k - 1) THEN
      ns_list(k) = ns_list(i)
      k = k + 1
    END IF
  NEXT i
  ns_n = k
  IF g_verbose <> 0 THEN PRINT "ns dopo il dedup: " + Num(ns_n)
END SUB

FUNCTION CountNs(kind AS INTEGER, ns AS STRING) AS LONG
  DIM i AS LONG, n AS LONG
  n = 0
  IF kind = 0 THEN
    FOR i = 0 TO u_n - 1
      IF UPPER(u_ns(i)) = ns THEN n = n + 1
    NEXT i
  ELSEIF kind = 1 THEN
    FOR i = 0 TO v_n - 1
      IF UPPER(v_ns(i)) = ns THEN n = n + 1
    NEXT i
  ELSEIF kind = 2 THEN
    FOR i = 0 TO w_n - 1
      IF UPPER(w_ns(i)) = ns THEN n = n + 1
    NEXT i
  ELSEIF kind = 3 THEN
    IF ns = "CONSTANTS" THEN n = z_n
  END IF
  RETURN n
END FUNCTION

FUNCTION CountNamespaces(kind AS INTEGER) AS LONG
  DIM i AS LONG, n AS LONG
  FOR i = 0 TO ns_n - 1
    IF CountNs(kind, ns_list(i)) > 0 THEN n = n + 1
  NEXT i
  RETURN n
END FUNCTION

SUB BuildPoly()
  DIM i AS LONG, key AS STRING, cur AS STRING
  poly_f = NEW DICT OF STRING, STRING
  poly_c = NEW DICT OF STRING, STRING
  poly_fkn = 0 : poly_ckn = 0
  FOR i = 0 TO u_n - 1
    key = UPPER(u_nm(i))
    IF key IN poly_f THEN
      cur = poly_f(key)
      IF INSTR("," + cur + ",", "," + u_ns(i) + ",") = 0 THEN cur = cur + "," + u_ns(i)
      poly_f(key) = cur
    ELSE
      poly_f(key) = u_ns(i)
      poly_fk(poly_fkn) = key
      poly_fkn = poly_fkn + 1
    END IF
  NEXT i
  FOR i = 0 TO v_n - 1
    key = UPPER(v_nm(i))
    IF key IN poly_c THEN
      cur = poly_c(key)
      IF INSTR("," + cur + ",", "," + v_ns(i) + ",") = 0 THEN cur = cur + "," + v_ns(i)
      poly_c(key) = cur
    ELSE
      poly_c(key) = v_ns(i)
      poly_ck(poly_ckn) = key
      poly_ckn = poly_ckn + 1
    END IF
  NEXT i
  Sort1(poly_fk(), poly_fkn)
  Sort1(poly_ck(), poly_ckn)
END SUB

FUNCTION NsListOf(s AS STRING) AS STRING
  ' "MATH,TORCH" -> '"MATH", "TORCH"'
  DIM p$() AS STRING, i AS LONG, r AS STRING
  p$ = SPLIT(s, ",")
  r = ""
  FOR i = 0 TO UBOUND(p$)
    IF i > 0 THEN r = r + ", "
    r = r + CHR$(34) + TRIM(p$(i)) + CHR$(34)
  NEXT i
  RETURN r
END FUNCTION

SUB BuildJson(path AS STRING)
  DIM i AS LONG, n AS LONG, cn AS LONG
  out_n = 0
  Emit("{")
  Emit(" " + CHR$(34) + "fonte" + CHR$(34) + ": " + CHR$(34) + "common/StdLib/StdLib.*.cs" + CHR$(34) + ",")
  Emit(" " + CHR$(34) + "chiave" + CHR$(34) + ": " + CHR$(34) + "(NAMESPACE, NOME)" + CHR$(34) + ",")
  ' funzioni
  Emit(" " + CHR$(34) + "funzioni" + CHR$(34) + ": {")
  Emit("  " + CHR$(34) + "occorrenze" + CHR$(34) + ": " + Num(nf) + ",")
  Emit("  " + CHR$(34) + "uniche" + CHR$(34) + ": " + Num(u_n) + ",")
  Emit("  " + CHR$(34) + "per_namespace" + CHR$(34) + ": {")
  cn = CountNamespaces(0)
  n = 0
  FOR i = 0 TO ns_n - 1
    IF CountNs(0, ns_list(i)) > 0 THEN
      n = n + 1
      Emit("   " + CHR$(34) + ns_list(i) + CHR$(34) + ": " + Num(CountNs(0, ns_list(i))) + CommaIf(cn - n))
    END IF
  NEXT i
  Emit("  }")
  Emit(" },")
  ' comandi
  Emit(" " + CHR$(34) + "comandi" + CHR$(34) + ": {")
  Emit("  " + CHR$(34) + "occorrenze" + CHR$(34) + ": " + Num(nc) + ",")
  Emit("  " + CHR$(34) + "uniche" + CHR$(34) + ": " + Num(v_n) + ",")
  Emit("  " + CHR$(34) + "per_namespace" + CHR$(34) + ": {")
  cn = CountNamespaces(1)
  n = 0
  FOR i = 0 TO ns_n - 1
    IF CountNs(1, ns_list(i)) > 0 THEN
      n = n + 1
      Emit("   " + CHR$(34) + ns_list(i) + CHR$(34) + ": " + Num(CountNs(1, ns_list(i))) + CommaIf(cn - n))
    END IF
  NEXT i
  Emit("  }")
  Emit(" },")
  ' tipi
  Emit(" " + CHR$(34) + "tipi" + CHR$(34) + ": {")
  Emit("  " + CHR$(34) + "occorrenze" + CHR$(34) + ": " + Num(nt) + ",")
  Emit("  " + CHR$(34) + "uniche" + CHR$(34) + ": " + Num(w_n) + ",")
  Emit("  " + CHR$(34) + "per_namespace" + CHR$(34) + ": {")
  cn = CountNamespaces(2)
  n = 0
  FOR i = 0 TO ns_n - 1
    IF CountNs(2, ns_list(i)) > 0 THEN
      n = n + 1
      Emit("   " + CHR$(34) + ns_list(i) + CHR$(34) + ": " + Num(CountNs(2, ns_list(i))) + CommaIf(cn - n))
    END IF
  NEXT i
  Emit("  }")
  Emit(" },")
  ' costanti
  Emit(" " + CHR$(34) + "costanti" + CHR$(34) + ": {")
  Emit("  " + CHR$(34) + "occorrenze" + CHR$(34) + ": " + Num(nk) + ",")
  Emit("  " + CHR$(34) + "uniche" + CHR$(34) + ": " + Num(z_n))
  Emit(" },")
  ' polimorfiche_funzioni
  Emit(" " + CHR$(34) + "polimorfiche_funzioni" + CHR$(34) + ": {")
  n = 0
  FOR i = 0 TO poly_fkn - 1
    IF INSTR(poly_f(poly_fk(i)), ",") > 0 THEN n = n + 1
  NEXT i
  FOR i = 0 TO poly_fkn - 1
    IF INSTR(poly_f(poly_fk(i)), ",") > 0 THEN
      n = n - 1
      Emit("  " + CHR$(34) + poly_fk(i) + CHR$(34) + ": [" + NsListOf(poly_f(poly_fk(i))) + "]" + CommaIf(n))
    END IF
  NEXT i
  Emit(" },")
  ' polimorfici_comandi
  Emit(" " + CHR$(34) + "polimorfici_comandi" + CHR$(34) + ": {")
  n = 0
  FOR i = 0 TO poly_ckn - 1
    IF INSTR(poly_c(poly_ck(i)), ",") > 0 THEN n = n + 1
  NEXT i
  FOR i = 0 TO poly_ckn - 1
    IF INSTR(poly_c(poly_ck(i)), ",") > 0 THEN
      n = n - 1
      Emit("  " + CHR$(34) + poly_ck(i) + CHR$(34) + ": [" + NsListOf(poly_c(poly_ck(i))) + "]" + CommaIf(n))
    END IF
  NEXT i
  Emit(" },")
  ' elenchi
  Emit(" " + CHR$(34) + "elenco_funzioni" + CHR$(34) + ": [")
  FOR i = 0 TO u_n - 1
    Emit("  " + CHR$(34) + u_ns(i) + "." + u_nm(i) + CHR$(34) + CommaIf(u_n - 1 - i))
  NEXT i
  Emit(" ],")
  Emit(" " + CHR$(34) + "elenco_comandi" + CHR$(34) + ": [")
  FOR i = 0 TO v_n - 1
    Emit("  " + CHR$(34) + v_ns(i) + "." + v_nm(i) + CHR$(34) + CommaIf(v_n - 1 - i))
  NEXT i
  Emit(" ],")
  Emit(" " + CHR$(34) + "elenco_tipi" + CHR$(34) + ": [")
  FOR i = 0 TO w_n - 1
    Emit("  " + CHR$(34) + w_ns(i) + "." + w_nm(i) + CHR$(34) + CommaIf(w_n - 1 - i))
  NEXT i
  Emit(" ],")
  Emit(" " + CHR$(34) + "elenco_costanti" + CHR$(34) + ": [")
  FOR i = 0 TO z_n - 1
    Emit("  [")
    Emit("   " + CHR$(34) + "CONSTANTS" + CHR$(34) + ",")
    Emit("   " + CHR$(34) + z_nm(i) + CHR$(34))
    Emit("  ]" + CommaIf(z_n - 1 - i))
  NEXT i
  Emit(" ]")
  Emit("}")
  WriteOrCheck(path, out(), out_n)
END SUB

' ── report globale (recon-stdlib.md) ─────────────────────────────────────────
SUB BuildMd(path AS STRING)
  DIM i AS LONG, n AS LONG, tot_o AS LONG, tot_u AS LONG
  out_n = 0
  Emit("# GMBASIC — Complete StdLib survey")
  Emit("")
  Emit("_Generated by `tools/basic/recon_stdlib.bas` (from the code `common/StdLib/StdLib.*.cs`)._")
  Emit("**Canonical key:** (NAMESPACE, NAME). Same names in different namespaces = distinct registrations.")
  Emit("")
  Emit("## 1. Summary")
  Emit("")
  Emit("| Surface | Occurrences | Unique (ns, name) |")
  Emit("|---|---|---|")
  Emit("| Functions | " + Num(nf) + " | " + Num(u_n) + " |")
  Emit("| Commands | " + Num(nc) + " | " + Num(v_n) + " |")
  Emit("| Types | " + Num(nt) + " | " + Num(w_n) + " |")
  Emit("| Constants | " + Num(nk) + " | " + Num(z_n) + " |")
  tot_o = nf + nc + nt + nk
  tot_u = u_n + v_n + w_n + z_n
  Emit("| **Total** | " + Num(tot_o) + " | " + Num(tot_u) + " |")
  Emit("")
  Emit("Namespaces: " + Num(ns_n))
  Emit("")
  Emit("## 2. Summary per namespace")
  Emit("")
  Emit("| Namespace | Functions | Commands | Types | Constants | Total |")
  Emit("|---|---|---|---|---|---|")
  FOR i = 0 TO ns_n - 1
    Emit("| " + ns_list(i) + " | " + Num(CountNs(0, ns_list(i))) + " | " + Num(CountNs(1, ns_list(i))) + " | " + Num(CountNs(2, ns_list(i))) + " | " + Num(CountNs(3, ns_list(i))) + " | " + Num(CountNs(0, ns_list(i)) + CountNs(1, ns_list(i)) + CountNs(2, ns_list(i)) + CountNs(3, ns_list(i))) + " |")
  NEXT i
  Emit("")
  Emit("## 3. Polymorphic functions (same name in several namespaces)")
  Emit("")
  n = 0
  FOR i = 0 TO poly_fkn - 1
    IF INSTR(poly_f(poly_fk(i)), ",") > 0 THEN n = n + 1
  NEXT i
  IF n = 0 THEN
    Emit("_none_")
  ELSE
    Emit("| Name | Namespace |")
    Emit("|---|---|")
    FOR i = 0 TO poly_fkn - 1
      IF INSTR(poly_f(poly_fk(i)), ",") > 0 THEN
        Emit("| " + poly_fk(i) + " | " + Replace(poly_f(poly_fk(i)), ",", ", ") + " |")
      END IF
    NEXT i
  END IF
  Emit("")
  Emit("## 4. Polymorphic commands (same name in several namespaces)")
  Emit("")
  n = 0
  FOR i = 0 TO poly_ckn - 1
    IF INSTR(poly_c(poly_ck(i)), ",") > 0 THEN n = n + 1
  NEXT i
  IF n = 0 THEN
    Emit("_none_")
  ELSE
    Emit("| Name | Namespace |")
    Emit("|---|---|")
    FOR i = 0 TO poly_ckn - 1
      IF INSTR(poly_c(poly_ck(i)), ",") > 0 THEN
        Emit("| " + poly_ck(i) + " | " + Replace(poly_c(poly_ck(i)), ",", ", ") + " |")
      END IF
    NEXT i
  END IF
  Emit("")
  Emit("## 5. Function list")
  Emit("")
  Emit("```")
  FOR i = 0 TO u_n - 1
    Emit(u_ns(i) + "." + u_nm(i))
  NEXT i
  Emit("```")
  Emit("")
  Emit("## 6. Command list")
  Emit("")
  Emit("```")
  FOR i = 0 TO v_n - 1
    Emit(v_ns(i) + "." + v_nm(i))
  NEXT i
  Emit("```")
  Emit("")
  Emit("## 7. Type list")
  Emit("")
  Emit("```")
  FOR i = 0 TO w_n - 1
    Emit(w_ns(i) + "." + w_nm(i))
  NEXT i
  Emit("```")
  Emit("")
  Emit("## 8. Constant list")
  Emit("")
  Emit("```")
  FOR i = 0 TO z_n - 1
    Emit("%" + z_nm(i))
  NEXT i
  Emit("```")
  Emit("")
  WriteOrCheck(path, out(), out_n)
END SUB

' ── audit per namespace (audit-stdlib.md) ────────────────────────────────────
SUB BuildRtMap()
  DIM i AS LONG, key AS STRING
  rt_of = NEW DICT OF STRING, STRING
  FOR i = 0 TO nf - 1
    key = UPPER(f_ns(i)) + CHR$(9) + UPPER(f_nm(i))
    IF key IN rt_of THEN
      ' primo tipo "vero" vince: ? non sovrascrive
    ELSE
      rt_of(key) = f_rt(i)
    END IF
  NEXT i
END SUB

DIM aud_nm(2000) AS STRING
DIM aud_n AS LONG

' ordina con confronto CASE-SENSITIVE (ordine alfabetico ASCII, come il sorted() del Python
' usato nel report audit: le maiuscole precedono le minuscole).
SUB SortCase(a$(), n AS LONG)
  DIM i AS LONG, j AS LONG, t AS STRING
  FOR i = 1 TO n - 1
    j = i
    DO WHILE j > 0
      IF a$(j - 1) > a$(j) THEN
        t = a$(j - 1) : a$(j - 1) = a$(j) : a$(j) = t
      ELSE
        EXIT DO
      END IF
      j = j - 1
    LOOP
  NEXT i
END SUB

SUB CollectForNs(kind AS INTEGER, ns AS STRING)
  DIM i AS LONG
  aud_n = 0
  IF kind = 0 THEN
    FOR i = 0 TO u_n - 1
      IF UPPER(u_ns(i)) = ns THEN aud_nm(aud_n) = u_nm(i) : aud_n = aud_n + 1
    NEXT i
  ELSEIF kind = 1 THEN
    FOR i = 0 TO v_n - 1
      IF UPPER(v_ns(i)) = ns THEN aud_nm(aud_n) = v_nm(i) : aud_n = aud_n + 1
    NEXT i
  ELSEIF kind = 2 THEN
    FOR i = 0 TO w_n - 1
      IF UPPER(w_ns(i)) = ns THEN aud_nm(aud_n) = w_nm(i) : aud_n = aud_n + 1
    NEXT i
  END IF
  SortCase(aud_nm(), aud_n)
END SUB

SUB BuildAudit(path AS STRING)
  DIM i AS LONG, j1 AS LONG, n AS LONG, ns AS STRING, key AS STRING, rt AS STRING
  out_n = 0
  Emit("# GMBASIC — StdLib audit per namespace (commands, functions, constants, types)")
  Emit("")
  Emit("_Generated by `tools/basic/recon_stdlib.bas` from the code `common/StdLib/StdLib.*.cs`._")
  Emit("**Key:** (NAMESPACE, NAME). Polymorphic names are listed in EVERY namespace that exposes them.")
  Emit("")
  FOR i = 0 TO ns_n - 1
    ns = ns_list(i)
    Emit("## Namespace " + ns)
    Emit("")
    ' comandi
    CollectForNs(1, ns)
    Emit("### Commands (" + Num(aud_n) + ")")
    Emit("")
    Emit("| Command |")
    Emit("|---|---|")
    FOR j1 = 0 TO aud_n - 1
      Emit("| `" + aud_nm(j1) + "` |")
    NEXT j1
    Emit("")
    ' funzioni
    CollectForNs(0, ns)
    Emit("### Functions (" + Num(aud_n) + ")")
    Emit("")
    Emit("| Function | Returns |")
    Emit("|---|---|")
    FOR j1 = 0 TO aud_n - 1
      key = ns + CHR$(9) + UPPER(aud_nm(j1))
      rt = "?"
      IF key IN rt_of THEN rt = rt_of(key)
      Emit("| `" + aud_nm(j1) + "` | " + rt + " |")
    NEXT j1
    Emit("")
    ' tipi
    CollectForNs(2, ns)
    Emit("### Types (" + Num(aud_n) + ")")
    Emit("")
    Emit("| Type |")
    Emit("|---|---|")
    FOR j1 = 0 TO aud_n - 1
      Emit("| `" + aud_nm(j1) + "` |")
    NEXT j1
    Emit("")
    ' costanti (solo nel namespace CONSTANTS)
    n = 0
    IF ns = "CONSTANTS" THEN n = z_n
    Emit("### Constants (" + Num(n) + ")")
    Emit("")
    Emit("| Constant |")
    Emit("|---|---|")
    IF ns = "CONSTANTS" THEN
      FOR j1 = 0 TO z_n - 1
        Emit("| `" + z_nm(j1) + "` |")
      NEXT j1
    END IF
    Emit("")
  NEXT i
  WriteOrCheck(path, out(), out_n)
END SUB

' ── header /* GMB-STDLIB-AUDIT */ per file ───────────────────────────────────
FUNCTION HeaderText(idx AS LONG) AS STRING
  DIM nf_l AS LONG, nc_l AS LONG, nt_l AS LONG, nk_l AS LONG
  DIM fn_base AS STRING, txt AS STRING, prev AS STRING
  DIM n AS LONG, i AS LONG
  nf_l = fi_f1(idx) - fi_f0(idx)
  nc_l = fi_c1(idx) - fi_c0(idx)
  nt_l = fi_t1(idx) - fi_t0(idx)
  nk_l = fi_k1(idx) - fi_k0(idx)
  fn_base = fi_path(idx)
  i = INSTR(fn_base, "StdLib.")
  IF i > 0 THEN fn_base = MID$(fn_base, i + 7)
  IF RIGHT$(fn_base, 3) = ".cs" THEN fn_base = LEFT$(fn_base, LEN(fn_base) - 3)
  txt = "/* ==== GMB-STDLIB-AUDIT-BEGIN ====" + CHR$(10)
  txt = txt + " * FILE: " + fn_base + " — namespace '" + fi_ns(idx) + "'" + CHR$(10)
  txt = txt + " * " + Num(nf_l) + " funzioni | " + Num(nc_l) + " comandi | " + Num(nt_l) + " tipi | " + Num(nk_l) + " costanti" + CHR$(10)
  txt = txt + " *" + CHR$(10)
  ' funzioni (nomi unici, ordinati)
  CollectNames(0, fi_f0(idx), fi_f1(idx))
  n = h_n
  IF n > 0 THEN
    SortCase(h_nm(), n)
    txt = txt + " * FUNZIONI (" + Num(nf_l) + "):" + CHR$(10)
    prev = ""
    FOR i = 0 TO n - 1
      IF UPPER(h_nm(i)) <> UPPER(prev) THEN txt = txt + " *   " + h_nm(i) + CHR$(10)
      prev = h_nm(i)
    NEXT i
  END IF
  ' comandi
  CollectNames(1, fi_c0(idx), fi_c1(idx))
  n = h_n
  IF n > 0 THEN
    SortCase(h_nm(), n)
    txt = txt + " * COMANDI (" + Num(nc_l) + "):" + CHR$(10)
    prev = ""
    FOR i = 0 TO n - 1
      IF UPPER(h_nm(i)) <> UPPER(prev) THEN txt = txt + " *   " + h_nm(i) + CHR$(10)
      prev = h_nm(i)
    NEXT i
  END IF
  ' tipi
  CollectNames(2, fi_t0(idx), fi_t1(idx))
  n = h_n
  IF n > 0 THEN
    SortCase(h_nm(), n)
    txt = txt + " * TIPI (" + Num(nt_l) + "):" + CHR$(10)
    prev = ""
    FOR i = 0 TO n - 1
      IF UPPER(h_nm(i)) <> UPPER(prev) THEN txt = txt + " *   " + h_nm(i) + CHR$(10)
      prev = h_nm(i)
    NEXT i
  END IF
  ' costanti
  CollectNames(3, fi_k0(idx), fi_k1(idx))
  n = h_n
  IF n > 0 THEN
    SortCase(h_nm(), n)
    txt = txt + " * COSTANTI (" + Num(nk_l) + "):" + CHR$(10)
    prev = ""
    FOR i = 0 TO n - 1
      IF UPPER(h_nm(i)) <> UPPER(prev) THEN txt = txt + " *   %" + h_nm(i) + CHR$(10)
      prev = h_nm(i)
    NEXT i
  END IF
  txt = txt + "    ==== GMB-STDLIB-AUDIT-END ==== */"
  RETURN txt
END FUNCTION

SUB UpsertHeaders()
  DIM idx AS LONG, p AS LONG, q AS LONG, h AS LONG
  DIM txt AS STRING, hdr AS STRING, neu AS STRING
  FOR idx = 0 TO nfi - 1
    IF LEN(fi_ns(idx)) > 0 THEN
      txt = IO.READFILE(fi_path(idx))
      hdr = HeaderText(idx)
      p = INSTR(txt, "/* ==== GMB-STDLIB-AUDIT-BEGIN ====")
      IF p > 0 THEN
        q = INSTR(txt, "    ==== GMB-STDLIB-AUDIT-END ==== */", p)
        IF q = 0 THEN
          PRINT "header non chiuso: "; fi_path(idx)
        ELSE
          neu = LEFT$(txt, p - 1) + hdr + MID$(txt, q + 37)
          IF neu <> txt THEN
            h = IO.FCREATE(fi_path(idx))
            IO.FWRITE(h, neu)
            IO.FCLOSE(h)
            PRINT "header aggiornato: "; fi_path(idx)
          END IF
        END IF
      END IF
    END IF
  NEXT idx
END SUB

' ── main ─────────────────────────────────────────────────────────────────────
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
  DIM i AS LONG, j AS LONG, k AS LONG
  DIM p AS STRING, outdir AS STRING
  DIM fs$() AS STRING
  DIM files(600) AS STRING
  DIM nfs AS LONG, t AS STRING

  g_chk = 0 : g_hdr = 0 : outdir = ""
  FOR i = 1 TO COMMANDCOUNT()
    p = COMMAND$(i)
    IF p = "--check" THEN
      g_chk = 1
    ELSEIF p = "--headers" THEN
      g_hdr = 1
    ELSEIF p = "--verbose" THEN
      g_verbose = 1
    ELSEIF p = "--out-dir" THEN
      i = i + 1
      IF i <= COMMANDCOUNT() THEN outdir = COMMAND$(i)
    ELSEIF LEFT$(p, 1) = "-" THEN
      PRINT "opzione ignota: "; p
      RETURN 2
    END IF
  NEXT i

  g_root = FindRoot()
  IF LEN(g_root) = 0 THEN
    PRINT "StdLib non trovata risalendo da "; IO.CWD()
    RETURN 2
  END IF
  IF FILEEXISTS(g_root + "/GMBasicSharp/common/StdLib/StdLib.cs") THEN
    g_std = g_root + "/GMBasicSharp/common/StdLib"
  ELSE
    g_std = g_root + "/common/StdLib"
  END IF
  IF LEN(outdir) = 0 THEN outdir = g_root + "/docs/comparison"
  g_out = outdir

  ' elenco file: solo StdLib.*.cs, ordine alfabetico (deterministico)
  fs$ = GREP.FILES(" ", g_std)
  nfs = 0
  FOR i = 0 TO UBOUND(fs$)
    IF RIGHT$(fs$(i), 3) = ".cs" AND INSTR(fs$(i), "StdLib") > 0 THEN
      IF INSTR(fs$(i), "/obj/") = 0 AND INSTR(fs$(i), "/bin/") = 0 THEN
        files(nfs) = fs$(i)
        nfs = nfs + 1
      END IF
    END IF
  NEXT i
  FOR i = 1 TO nfs - 1
    k = i
    DO WHILE k > 0
      IF files(k - 1) > files(k) THEN
        t = files(k - 1) : files(k - 1) = files(k) : files(k) = t
      ELSE
        EXIT DO
      END IF
      k = k - 1
    LOOP
  NEXT i

  nf = 0 : nc = 0 : nt = 0 : nk = 0 : nfi = 0
  FOR i = 0 TO nfs - 1
    ScanFile(files(i))
    IF g_verbose <> 0 THEN
      PRINT files(i) + "  ns='" + fi_ns(nfi - 1) + "' f=" + Num(fi_f1(nfi - 1) - fi_f0(nfi - 1)) + " c=" + Num(fi_c1(nfi - 1) - fi_c0(nfi - 1)) + " t=" + Num(fi_t1(nfi - 1) - fi_t0(nfi - 1)) + " k=" + Num(fi_k1(nfi - 1) - fi_k0(nfi - 1))
    END IF
  NEXT i

  LoadUnique()
  BuildNsList()
  BuildPoly()
  BuildRtMap()

  BuildJson(g_out + "/recon-stdlib.json")
  BuildMd(g_out + "/recon-stdlib.md")
  BuildAudit(g_out + "/audit-stdlib.md")

  IF g_hdr <> 0 THEN UpsertHeaders()

  PRINT "recon_stdlib: " + Num(nfi) + " file | funzioni " + Num(nf) + " occ/" + Num(u_n) + " uniche | comandi " + Num(nc) + "/" + Num(v_n) + " | tipi " + Num(nt) + "/" + Num(w_n) + " | costanti " + Num(nk) + "/" + Num(z_n) + " | namespaces " + Num(ns_n)
  IF g_chk <> 0 AND g_chk > 1 THEN
    PRINT "recon_stdlib: DERIVA rispetto ai file a disco (rilanciare senza --check)"
    RETURN 1
  END IF
  IF g_chk <> 0 THEN PRINT "recon_stdlib: OK, report allineati"
  RETURN 0
END FUNCTION
