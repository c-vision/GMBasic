' gen_command_desc.bas — catalogo descrizioni COMMANDS (port BASIC).
' Uso: gen_command_desc.bas [--out FILE] [--commands FILE] [--report] [--check]
' Rigenera il catalogo partendo dal TSV versionato + superficie canonica, preservando
' le righe curate/manuali; i simboli nuovi entrano come "scoperto".
IMPORT IO
IMPORT GREP
IMPORT SYSTEM

DIM r_ns(4000) AS STRING
DIM r_nm(4000) AS STRING
DIM r_kd(4000) AS STRING
DIM r_sx(4000) AS STRING
DIM r_ds(4000) AS STRING
DIM r_ft(4000) AS STRING
DIM r_mb(4000) AS STRING
DIM r_n AS LONG
DIM key_dict AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_std AS STRING
DIM m_sx AS STRING, m_ds AS STRING, m_ok AS LONG
DIM g_cmd AS STRING, c_sx AS STRING, c_ds AS STRING
DIM p_sx AS STRING, p_ds AS STRING
DIM t_ds AS STRING, t_mb AS STRING
DIM h_key$(5000) AS STRING, h_ds$(5000) AS STRING, h_n AS LONG, h_loaded AS LONG
DIM pn$(40) AS STRING, pt$(40) AS STRING, po(40) AS LONG

' ─── Indici one-shot (2026-10-28) ────────────────────────────────────────────
' Prima: CommentFor/ConstFor/MetaFor/TypeInfo facevano, PER OGNI SIMBOLO,
' `GREP.FILES(" ", g_std)` (una grep ricorsiva sul corpus: ~0.14 s) + IO.READLINES
' di tutti i file StdLib. Con ~2000 simboli erano ~6000 grep complete + decine di
' migliaia di letture: CPU al 100% per decine di minuti (la macchina sembrava bloccata).
' Il Python originale (parse_stdlib) legge il corpus UNA volta e indicizza; qui si fa
' lo stesso: un solo passaggio, poi lookup O(1). Semantica invariata ("primo che vince").
DIM g_ix_done AS LONG, g_cx_done AS LONG
DIM ix_cmt AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM ix_cst AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM ix_meta AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM ix_type AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM ix_cmd AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM seen_nm AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM h_idx AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING

FUNCTION FindUp(rel AS STRING) AS STRING
  DIM d AS STRING, p AS STRING, i AS LONG
  d = IO.CWD()
  FOR i = 1 TO 7
    p = d + "/" + rel
    IF FILEEXISTS(p) THEN RETURN p
    d = IO.PARENTDIR(d)
  NEXT i
  RETURN ""
END FUNCTION

SUB AddRow(ns AS STRING, nm AS STRING, kd AS STRING, sx AS STRING, ds AS STRING, ft AS STRING, mb AS STRING)
  DIM k AS STRING
  k = UPPER$(ns) + "|" + UPPER$(nm)
  IF k IN key_dict THEN EXIT SUB
  r_n = r_n + 1
  r_ns(r_n) = ns : r_nm(r_n) = nm : r_kd(r_n) = kd : r_sx(r_n) = sx
  r_ds(r_n) = ds : r_ft(r_n) = ft : r_mb(r_n) = mb
  key_dict(k) = r_n
  seen_nm(UPPER$(nm)) = 1
END SUB

FUNCTION Clean(s AS STRING) AS STRING
  RETURN REPLACE(REPLACE(s, CHR$(9), " "), CHR$(10), " ")
END FUNCTION

FUNCTION UsableComment(s AS STRING) AS INTEGER
  DIM t AS STRING, first AS STRING
  t = TRIM$(s)
  IF LEN(t) < 15 OR LEN(t) > 200 THEN RETURN 0
  IF INSTR(UPPER$(t), "TODO") > 0 OR INSTR(UPPER$(t), "PARSER.") > 0 THEN RETURN 0
  IF INSTR(t, "...") > 0 THEN RETURN 0
  IF RIGHT$(t, 1) = ":" THEN RETURN 0
  IF LEN(t) > 0 AND LEN(REPLACE(t, "(", "")) <> LEN(REPLACE(t, ")", "")) THEN RETURN 0
  first = LOWER$(TRIM$(LEFT$(t, INSTR(t + " ", " ") - 1)))
  IF first = "i" OR first = "il" OR first = "la" OR first = "lo" OR first = "gli" OR first = "le" THEN RETURN 0
  IF first = "con" OR first = "su" OR first = "per" OR first = "da" OR first = "in" OR first = "nel" THEN RETURN 0
  IF first = "e" OR first = "che" OR first = "come" OR first = "quando" OR first = "se" THEN RETURN 0
  IF first = "nota" OR first = "vedi" OR first = "explain" OR first = "solo" THEN RETURN 0
  IF first = "stessa" OR first = "stesso" OR first = "stessi" OR first = "stesse" THEN RETURN 0
  RETURN 1
END FUNCTION

FUNCTION WordChar(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN RETURN 1
  RETURN 0
END FUNCTION

FUNCTION HasWord(s AS STRING, w AS STRING) AS INTEGER
  DIM p AS LONG, q AS LONG
  p = 1
  DO
    q = INSTR(p, UPPER$(s), UPPER$(w))
    IF q = 0 THEN RETURN 0
    IF q = 1 THEN
      IF q + LEN(w) > LEN(s) THEN RETURN 1
      IF WordChar(MID$(s, q + LEN(w), 1)) = 0 THEN RETURN 1
    ELSE
      IF WordChar(MID$(s, q - 1, 1)) = 0 THEN
        IF q + LEN(w) > LEN(s) THEN RETURN 1
        IF WordChar(MID$(s, q + LEN(w), 1)) = 0 THEN RETURN 1
      END IF
    END IF
    p = q + 1
  LOOP
END FUNCTION

FUNCTION CommentTail(l AS STRING) AS STRING
  DIM p AS LONG, q AS LONG, t AS STRING, sep AS STRING
  p = INSTR(l, "//")
  IF p = 0 THEN RETURN ""
  t = TRIM$(MID$(l, p + 2, LEN(l) - p - 1))
  ' Il separatore può essere multibyte (— è 3 byte, – 2-3): l'offset NON si scrive a mano.
  ' `p + 5` era corretto solo a BYTE: dove gli indici sono per CARATTERE (backend IL) il
  ' taglio slittava di 2 caratteri ("loop" → "op"), con output divergenti fra le gambe.
  ' LEN(sep) è la lunghezza nella semantica del motore corrente.
  sep = ""
  p = INSTR(t, " — ")
  IF p > 0 THEN sep = " — "
  IF p = 0 THEN
    p = INSTR(t, " – ")
    IF p > 0 THEN sep = " – "
  END IF
  IF p = 0 THEN
    p = INSTR(t, " - ")
    IF p > 0 THEN sep = " - "
  END IF
  IF p = 0 THEN
    p = INSTR(t, ": ")
    IF p > 0 THEN sep = ": "
  END IF
  IF p = 0 THEN RETURN ""
  q = p + LEN(sep)
  t = TRIM$(MID$(t, q, LEN(t) - q + 1))
  IF UsableComment(t) = 0 THEN RETURN ""
  RETURN t
END FUNCTION

FUNCTION CommentFor(nm AS STRING) AS STRING
  DIM k AS STRING
  IF LEN(g_std) = 0 THEN RETURN ""
  BuildStdIndex()
  k = UPPER$(nm)
  IF k IN ix_cmt THEN RETURN ix_cmt(k)
  RETURN ""
END FUNCTION

FUNCTION NameChars(s AS STRING) AS INTEGER
  DIM i AS LONG, c AS STRING
  IF LEN(TRIM$(s)) = 0 THEN RETURN 0
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" OR c = "." OR c = " " OR c = "%" THEN
      ' ok
    ELSE
      RETURN 0
    END IF
  NEXT i
  RETURN 1
END FUNCTION

FUNCTION IsGeneric(d AS STRING) AS INTEGER
  DIM s AS STRING, low AS STRING, tail AS STRING
  s = TRIM$(d) : low = LOWER$(s)
  IF LEFT$(s, 7) = "Comando" THEN
    tail = MID$(s, 8, LEN(s) - 7)
    IF NameChars(tail) <> 0 THEN RETURN 1
  END IF
  IF LEFT$(s, 8) = "Funzione" THEN
    tail = MID$(s, 9, LEN(s) - 8)
    IF NameChars(tail) <> 0 THEN RETURN 1
  END IF
  IF LEFT$(s, 4) = "Tipo" THEN
    tail = TRIM$(MID$(s, 5, LEN(s) - 4))
    IF INSTR(tail, " ") = 0 AND NameChars(tail) <> 0 THEN RETURN 1
  END IF
  IF LEFT$(s, 8) = "Costante" THEN
    tail = TRIM$(MID$(s, 9, LEN(s) - 8))
    IF INSTR(tail, " ") = 0 AND NameChars(tail) <> 0 THEN RETURN 1
  END IF
  IF LEFT$(low, 8) = "comando " THEN
    IF INSTR(low, "comando grafico") = 1 OR INSTR(low, "comando principale") = 1 THEN RETURN 1
    IF INSTR(low, "comando del namespace") = 1 OR INSTR(low, "comando nativo") = 1 THEN RETURN 1
  END IF
  RETURN 0
END FUNCTION

' Un namespace plausibile: solo lettere/cifre/_/. (scarta le righe spazzatura che il vecchio
' catalogo si trascinava dietro, es. un NS "indow.cs:55" nato dal parsing di un commento).
FUNCTION PlausibleNs(ns AS STRING) AS INTEGER
  DIM i AS LONG, c AS STRING
  ns = TRIM$(ns)
  IF LEN(ns) = 0 THEN RETURN 0
  FOR i = 1 TO LEN(ns)
    c = MID$(ns, i, 1)
    IF NOT ((c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" OR c = ".") THEN RETURN 0
  NEXT i
  RETURN 1
END FUNCTION

' Nomi della tabella RIEPILOGO di COMMANDS.TXT: non sono simboli del linguaggio.
FUNCTION IsSummaryName(nm AS STRING) AS INTEGER
  DIM u AS STRING
  u = UPPER$(TRIM$(nm))
  IF u = "COMANDI" OR u = "FUNZIONI" OR u = "TIPI" OR u = "COSTANTI" THEN RETURN 1
  IF u = "RIEPILOGO" OR u = "TOTALE" OR u = "NOME" THEN RETURN 1
  RETURN 0
END FUNCTION

SUB LoadCatalog(p AS STRING)
  DIM ls$() AS STRING
  DIM f$() AS STRING
  DIM l AS STRING, i AS LONG, d AS STRING
  IF NOT FILEEXISTS(p) THEN EXIT SUB
  ls$ = IO.READLINES(p)
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    IF LEN(l) > 0 AND LEFT$(l, 1) <> "#" THEN
      f$ = SPLIT(l, CHR$(9))
      IF UBOUND(f$) >= 5 THEN
        IF IsGeneric(f$(4)) <> 0 AND LOWER$(TRIM$(f$(5))) <> "manuale" THEN
          IF PinFor(f$(0), f$(1)) <> 0 THEN
            f$(3) = p_sx
            f$(4) = p_ds
            f$(5) = "manuale"
          ELSE
            d = CommentFor(f$(1))
            IF LEN(d) > 0 THEN
              f$(4) = d
              f$(5) = "commento"
            END IF
          END IF
        END IF
        IF UBOUND(f$) >= 6 THEN
          IF PlausibleNs(f$(0)) <> 0 AND IsSummaryName(f$(1)) = 0 THEN
            AddRow(f$(0), f$(1), f$(2), f$(3), f$(4), f$(5), f$(6))
          END IF
        ELSE
          IF PlausibleNs(f$(0)) <> 0 AND IsSummaryName(f$(1)) = 0 THEN
            AddRow(f$(0), f$(1), f$(2), f$(3), f$(4), f$(5), "")
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

FUNCTION ParenDelta(s AS STRING) AS LONG
  DIM i AS LONG, c AS STRING, d AS LONG
  d = 0
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c = "(" THEN d = d + 1
    IF c = ")" THEN d = d - 1
  NEXT i
  RETURN d
END FUNCTION

' Registra il blocco di registrazione sotto ogni nome quotato che contiene (max 4
' blocchi per nome, in ordine di scansione: MetaFor li prova in sequenza, come prima).
SUB AddMetaBlock(b AS STRING)
  DIM f$() AS STRING
  DIM i AS LONG, s AS LONG, tok AS STRING, kk AS STRING
  f$ = SPLIT(b, CHR$(34))
  FOR i = 1 TO UBOUND(f$) - 1 STEP 2
    tok = f$(i)
    IF LEN(tok) > 0 THEN
      FOR s = 1 TO 8
        kk = tok + "|" + CHR$(48 + s)
        IF NOT (kk IN ix_meta) THEN
          ix_meta(kk) = b
          EXIT FOR
        END IF
      NEXT s
    END IF
  NEXT i
END SUB

' Registra il blocco RegisterTypeSchema(typeof(<cls>)) → testo dei campi.
SUB AddTypeBlock(b AS STRING)
  DIM p AS LONG, q AS LONG, cls AS STRING, out AS STRING, s AS STRING
  DIM f$() AS STRING
  DIM i AS LONG, n AS LONG
  p = INSTR(b, "typeof(")
  IF p = 0 THEN EXIT SUB
  q = INSTR(p + 7, b, ")")
  IF q <= p + 7 THEN EXIT SUB
  cls = TRIM$(MID$(b, p + 7, q - p - 7))
  IF LEN(cls) = 0 OR (cls IN ix_type) THEN EXIT SUB
  f$ = SPLIT(b, CHR$(34))
  out = "" : n = 0
  FOR i = 1 TO UBOUND(f$) - 1 STEP 2
    s = TRIM$(f$(i))
    IF LEN(s) > 0 AND s <> cls AND n < 12 THEN
      IF LEN(out) > 0 THEN out = out + ", "
      out = out + s : n = n + 1
    END IF
  NEXT i
  IF LEN(out) > 0 THEN ix_type(cls) = out
END SUB

' Un solo passaggio sul corpus StdLib: commenti, costanti, blocchi di registrazione/tipi.
SUB BuildStdIndex()
  DIM fs$() AS STRING
  DIM ls$() AS STRING
  DIM l AS STRING, b AS STRING
  DIM i AS LONG, j AS LONG, k AS LONG, d AS LONG
  DIM p AS LONG, q AS LONG, e AS LONG, t AS STRING, tok AS STRING, c AS STRING
  IF g_ix_done <> 0 THEN EXIT SUB
  g_ix_done = 1
  IF LEN(g_std) = 0 THEN EXIT SUB
  fs$ = GREP.FILES(" ", g_std)
  FOR i = 0 TO UBOUND(fs$)
    IF INSTR(fs$(i), "StdLib") > 0 AND RIGHT$(fs$(i), 3) = ".cs" THEN
      ls$ = IO.READLINES(fs$(i))
      FOR j = 0 TO UBOUND(ls$)
        l = ls$(j)
        ' 1) commenti: per ogni token della riga vale la PRIMA coda usabile
        IF INSTR(l, "//") > 0 THEN
          t = CommentTail(l)
          IF LEN(t) > 0 THEN
            tok = ""
            FOR k = 1 TO LEN(l) + 1
              c = ""
              IF k <= LEN(l) THEN c = MID$(l, k, 1)
              IF k <= LEN(l) AND WordChar(c) <> 0 THEN
                tok = tok + c
              ELSE
                IF LEN(tok) > 0 THEN
                  IF NOT (UPPER$(tok) IN ix_cmt) THEN ix_cmt(UPPER$(tok)) = t
                  tok = ""
                END IF
              END IF
            NEXT k
          END IF
        END IF
        ' 2) costanti: AddConst("NOME", valore) / AddKeyConstant("NOME", valore)
        p = INSTR(l, "AddConst(")
        IF p = 0 THEN p = INSTR(l, "AddKeyConstant(")
        IF p > 0 THEN
          q = INSTR(p, l, CHR$(34))
          IF q > 0 THEN
            e = INSTR(q + 1, l, CHR$(34))
            IF e > q THEN
              tok = MID$(l, q + 1, e - q - 1)
              d = INSTR(p, l, ",")
              IF d > 0 AND LEN(tok) > 0 AND NOT (tok IN ix_cst) THEN
                e = INSTR(d + 1, l, ")")
                IF e > d THEN
                  t = TRIM$(MID$(l, d + 1, e - d - 1))
                  IF INSTR(t, "ConsoleKey.") > 0 OR INSTR(t, "ConsoleColor.") > 0 THEN
                    p = INSTR(t, ".")
                    IF p > 0 THEN t = MID$(t, p + 1, LEN(t) - p)
                  END IF
                  IF LEN(t) > 0 THEN ix_cst(tok) = t
                END IF
              END IF
            END IF
          END IF
        END IF
        ' 3) blocchi (registrazioni e schemi tipo)
        IF INSTR(l, "NativeProcedureDefinition") > 0 OR INSTR(l, "FunctionDefinition") > 0 THEN
          b = l : d = ParenDelta(b) : k = j
          DO WHILE d > 0 AND k < UBOUND(ls$)
            k = k + 1
            b = b + CHR$(10) + ls$(k)
            d = d + ParenDelta(ls$(k))
          LOOP
          AddMetaBlock(b)
          j = k
        ELSEIF INSTR(l, "RegisterTypeSchema(typeof(") > 0 THEN
          b = l : d = ParenDelta(b) : k = j
          DO WHILE d > 0 AND k < UBOUND(ls$)
            k = k + 1
            b = b + CHR$(10) + ls$(k)
            d = d + ParenDelta(ls$(k))
          LOOP
          AddTypeBlock(b)
          j = k
        END IF
      NEXT j
    END IF
  NEXT i
END SUB

FUNCTION TypeName(s AS STRING) AS STRING
  DIM t AS STRING, p AS LONG, w AS STRING, c AS STRING, i AS LONG
  p = INSTR(s, "new ")
  IF p = 0 THEN RETURN ""
  t = MID$(s, p + 4, LEN(s) - p - 3)
  w = ""
  FOR i = 1 TO LEN(t)
    c = MID$(t, i, 1)
    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
      w = w + c
    ELSE
      EXIT FOR
    END IF
  NEXT i
  IF RIGHT$(w, 4) = "Type" THEN w = LEFT$(w, LEN(w) - 4)
  w = UPPER$(w)
  IF w = "STRING" THEN RETURN "STRING"
  IF w = "INTEGER" OR w = "LONG" THEN RETURN "LONG"
  IF w = "BOOLEAN" OR w = "BIT" THEN RETURN "BIT"
  IF w = "OBJECT" OR w = "VARIANT" THEN RETURN "VARIANT"
  IF w = "ARRAY" THEN RETURN "ARRAY"
  IF w = "MAP" THEN RETURN "MAP"
  IF w = "TUPLE" THEN RETURN "TUPLE"
  RETURN w
END FUNCTION

SUB ParseMetaArgs(block AS STRING, ns AS STRING, nm AS STRING)
  DIM i AS LONG, j AS LONG, k AS LONG, d AS LONG, p AS LONG, q AS LONG
  DIM inner AS STRING, piece AS STRING, c AS STRING, t AS STRING, qn AS STRING
  DIM retpart AS STRING, req AS STRING, opt AS STRING
  DIM depth AS LONG
  m_sx = "" : m_ds = "" : m_ok = 0
  p = INSTR(block, "[")
  IF p = 0 THEN EXIT SUB
  d = 0 : q = 0
  FOR i = p TO LEN(block)
    c = MID$(block, i, 1)
    IF c = "[" THEN d = d + 1
    IF c = "]" THEN
      d = d - 1
      IF d = 0 THEN q = i : EXIT FOR
    END IF
  NEXT i
  IF q = 0 THEN EXIT SUB
  inner = MID$(block, p + 1, q - p - 1)
  retpart = MID$(block, q + 1, LEN(block) - q)
  np = 0
  depth = 0 : piece = ""
  FOR i = 1 TO LEN(inner) + 1
    c = MID$(inner, i, 1)
    IF c = "(" OR c = "[" OR c = "{" THEN depth = depth + 1
    IF c = ")" OR c = "]" OR c = "}" THEN depth = depth - 1
    IF (c = "," AND depth = 0) OR i > LEN(inner) THEN
      piece = TRIM$(piece)
      IF LEN(piece) > 0 AND np < 40 THEN
        qn = "" : t = "" : po(np) = 0
        p = INSTR(piece, "P(")
        IF p > 0 THEN
          p = INSTR(piece, CHR$(34))
          IF p > 0 THEN
            q = INSTR(p + 1, piece, CHR$(34))
            IF q > p THEN qn = MID$(piece, p + 1, q - p - 1)
          END IF
          IF INSTR(LOWER$(piece), ", true") > 0 THEN po(np) = 1
        ELSE
          p = INSTR(piece, "T(")
          IF p > 0 THEN
            k = INSTR(p + 2, piece, CHR$(34))
            IF k > 0 THEN
              j = INSTR(k + 1, piece, CHR$(34))
              IF j > k THEN qn = MID$(piece, k + 1, j - k - 1)
            END IF
          END IF
          t = TypeName(piece)
          IF INSTR(piece, "IsOptional") > 0 THEN po(np) = 1
        END IF
        pn$(np) = qn : pt$(np) = t : np = np + 1
      END IF
      piece = ""
    ELSE
      piece = piece + c
    END IF
  NEXT i
  req = "" : opt = ""
  FOR i = 0 TO np - 1
    IF po(i) = 0 THEN
      IF LEN(req) > 0 THEN req = req + ", "
      req = req + pn$(i)
    ELSE
      IF LEN(opt) > 0 THEN opt = opt + ", "
      opt = opt + pn$(i)
    END IF
  NEXT i
  m_sx = "x = " + ns + "." + UPPER$(nm) + "(" + req
  IF LEN(opt) > 0 THEN
    IF LEN(req) > 0 THEN m_sx = m_sx + " "
    m_sx = m_sx + "[, " + opt + "]"
  END IF
  m_sx = m_sx + ")"
  t = TypeName(retpart)
  IF LEN(t) > 0 THEN m_ds = "-> " + t ELSE m_ds = ""
  IF np > 0 THEN
    IF LEN(m_ds) > 0 THEN m_ds = m_ds + " · arg "
    FOR i = 0 TO np - 1
      IF i > 0 THEN m_ds = m_ds + ", "
      m_ds = m_ds + pn$(i)
      IF LEN(pt$(i)) > 0 THEN m_ds = m_ds + ":" + pt$(i)
      IF po(i) <> 0 THEN m_ds = m_ds + "?"
    NEXT i
  END IF
  m_ok = 1
END SUB

FUNCTION MetaFor(ns AS STRING, nm AS STRING) AS INTEGER
  DIM s AS LONG, k AS STRING
  m_sx = "" : m_ds = "" : m_ok = 0
  IF LEN(g_std) = 0 THEN RETURN 0
  BuildStdIndex()
  FOR s = 1 TO 8
    k = nm + "|" + CHR$(48 + s)
    IF k IN ix_meta THEN
      ParseMetaArgs(ix_meta(k), ns, nm)
      IF m_ok <> 0 THEN RETURN 1
    END IF
  NEXT s
  RETURN 0
END FUNCTION

' Indice una-tantum di COMMANDS.TXT: nome → "sintassi<TAB>descrizione", primo usabile.
SUB BuildCmdIndex()
  DIM ls$() AS STRING
  DIM f$() AS STRING
  DIM l AS STRING, i AS LONG, nm AS STRING, ds AS STRING
  DIM in_sum AS INTEGER
  IF g_cx_done <> 0 THEN EXIT SUB
  g_cx_done = 1
  IF LEN(g_cmd) = 0 OR NOT FILEEXISTS(g_cmd) THEN EXIT SUB
  ls$ = IO.READLINES(g_cmd)
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    ' La tabella RIEPILOGO (COMANDI/FUNZIONI/TIPI/COSTANTI/TOTALE) NON è un elenco di comandi:
    ' prima finiva nell'indice per nome e i suoi numeri diventavano "descrizioni" di comandi
    ' inesistenti (voci COMANDI/COSTANTI nel catalogo).
    IF INSTR(l, "RIEPILOGO") > 0 THEN in_sum = 1
    IF in_sum = 0 AND INSTR(l, "│") > 0 THEN
      f$ = SPLIT(l, "│")
      IF UBOUND(f$) >= 3 THEN
        ds = TRIM$(f$(3))
        IF LEN(ds) > 0 AND IsGeneric(ds) = 0 THEN
          nm = UPPER$(TRIM$(f$(1)))
          IF IsSummaryName(nm) = 0 THEN
            IF NOT (nm IN ix_cmd) THEN ix_cmd(nm) = TRIM$(f$(2)) + CHR$(9) + ds
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

FUNCTION LookupCommand(nm AS STRING) AS INTEGER
  DIM k AS STRING, v AS STRING, p AS LONG
  c_sx = "" : c_ds = ""
  IF LEN(g_cmd) = 0 OR NOT FILEEXISTS(g_cmd) THEN RETURN 0
  BuildCmdIndex()
  k = UPPER$(nm)
  IF k IN ix_cmd THEN
    v = ix_cmd(k)
    p = INSTR(v, CHR$(9))
    IF p > 0 THEN
      c_sx = LEFT$(v, p - 1)
      c_ds = MID$(v, p + 1, LEN(v) - p)
    END IF
    RETURN 1
  END IF
  RETURN 0
END FUNCTION

FUNCTION NameExists(nm AS STRING) AS INTEGER
  IF UPPER$(nm) IN seen_nm THEN RETURN 1
  RETURN 0
END FUNCTION

SUB HistAdd(ns AS STRING, nm AS STRING, ds AS STRING)
  DIM k AS STRING, i AS LONG, old AS LONG
  ds = TRIM$(ds)
  IF LEN(ds) < 5 OR IsGeneric(ds) <> 0 THEN EXIT SUB
  IF INSTR(LOWER$(ds), "non implementato") > 0 OR INSTR(LOWER$(ds), "non ancora") > 0 THEN EXIT SUB
  k = UPPER$(ns) + "|" + UPPER$(nm)
  old = 0
  FOR i = 1 TO h_n
    IF h_key$(i) = k THEN old = i : EXIT FOR
  NEXT i
  IF old = 0 THEN
    IF h_n >= 5000 THEN EXIT SUB
    h_n = h_n + 1 : old = h_n : h_key$(old) = k
  END IF
  IF LEN(ds) > LEN(h_ds$(old)) THEN h_ds$(old) = ds
  h_idx(k) = h_ds$(old)
END SUB

SUB LoadHistory()
  DIM revs$() AS STRING
  DIM ls$() AS STRING
  DIM f$() AS STRING
  DIM txt AS STRING, l AS STRING, ns AS STRING, nm AS STRING, ds AS STRING
  DIM root AS STRING, cmd AS STRING
  DIM i AS LONG, j AS LONG, n AS LONG
  IF h_loaded <> 0 THEN EXIT SUB
  h_loaded = 1 : h_n = 0
  IF LEN(g_cmd) = 0 OR NOT FILEEXISTS(g_cmd) THEN EXIT SUB
  root = IO.PARENTDIR(g_cmd)
  cmd = "git -C " + CHR$(34) + root + CHR$(34) + " log --all --format=%h -- COMMANDS.TXT"
  txt = SYSTEM.SHELLOUT(cmd)
  revs$ = SPLIT(txt, CHR$(10))
  n = 0
  FOR i = 0 TO UBOUND(revs$)
    l = TRIM$(revs$(i))
    IF LEN(l) >= 6 THEN
      n = n + 1
      IF n > 120 THEN EXIT FOR
      txt = SYSTEM.SHELLOUT("git -C " + CHR$(34) + root + CHR$(34) + " show " + l + ":COMMANDS.TXT")
      ls$ = SPLIT(txt, CHR$(10))
      ns = ""
      FOR j = 0 TO UBOUND(ls$)
        l = ls$(j)
        IF INSTR(l, "│Namespace ") > 0 THEN
          f$ = SPLIT(l, "│")
          IF UBOUND(f$) >= 1 THEN ns = UPPER$(TRIM$(f$(1)))
        ELSEIF INSTR(l, "│") > 0 AND LEN(ns) > 0 THEN
          f$ = SPLIT(l, "│")
          IF UBOUND(f$) >= 3 THEN
            nm = TRIM$(f$(1)) : ds = TRIM$(f$(3))
            IF LEN(nm) > 0 AND nm <> "NOME" AND LEFT$(nm, 5) <> "TYPE:" AND LEFT$(nm, 10) <> "COSTANTE:%" THEN
              HistAdd(ns, nm, ds)
            END IF
          END IF
        END IF
      NEXT j
    END IF
  NEXT i
END SUB

FUNCTION HistoryFor(ns AS STRING, nm AS STRING) AS STRING
  DIM k AS STRING
  LoadHistory()
  k = UPPER$(ns) + "|" + UPPER$(nm)
  IF k IN h_idx THEN RETURN h_idx(k)
  RETURN ""
END FUNCTION

FUNCTION TypeInfo(cls AS STRING) AS INTEGER
  t_ds = "" : t_mb = ""
  IF LEN(g_std) = 0 THEN RETURN 0
  BuildStdIndex()
  IF cls IN ix_type THEN
    t_ds = "Campi: " + ix_type(cls)
    t_mb = t_ds
    RETURN 1
  END IF
  RETURN 0
END FUNCTION
SUB AddType(spec AS STRING)
  DIM f$() AS STRING, ns AS STRING, nm AS STRING, i AS LONG
  spec = TRIM$(spec)
  IF LEN(spec) = 0 THEN EXIT SUB
  f$ = SPLIT(spec, ".")
  IF UBOUND(f$) < 1 THEN EXIT SUB
  nm = f$(UBOUND(f$))
  ns = f$(0)
  FOR i = 1 TO UBOUND(f$) - 1
    ns = ns + "." + f$(i)
  NEXT i
  IF TypeInfo(nm) <> 0 THEN
    AddRow(ns, "TYPE:" + nm, "tipo", "TYPE " + nm, t_ds, "metadati:membri", t_mb)
  ELSE
    AddRow(ns, "TYPE:" + nm, "tipo", "TYPE " + nm, "Tipo definito dal runtime.", "derivato:tipo", "")
  END IF
END SUB

SUB LoadRecon()
  DIM ls$() AS STRING
  DIM p AS STRING, txt AS STRING, l AS STRING
  DIM f$() AS STRING, ns AS STRING, nm AS STRING, d AS STRING
  DIM i AS LONG, j AS LONG, k AS LONG, sec AS LONG, got AS LONG, cs AS LONG
  p = FindUp("docs/comparison/recon-stdlib.json")
  IF LEN(p) = 0 OR NOT FILEEXISTS(p) THEN EXIT SUB
  txt = IO.READFILE(p)
  ls$ = SPLIT(txt, CHR$(10))
  sec = 0 : got = 0
  FOR i = 0 TO UBOUND(ls$)
    l = ls$(i)
    IF INSTR(l, CHR$(34) + "elenco_tipi" + CHR$(34)) > 0 THEN sec = 1 : got = 0
    IF sec = 1 AND INSTR(l, CHR$(34)) > 0 THEN
      f$ = SPLIT(l, CHR$(34))
      IF UBOUND(f$) >= 1 THEN
        IF LEN(TRIM$(f$(1))) > 0 AND INSTR(TRIM$(f$(1)), "elenco") = 0 THEN
          IF got = 1 THEN AddType(TRIM$(f$(1)))
        END IF
      END IF
    END IF
    IF sec = 1 AND got = 0 AND INSTR(l, "[") > 0 THEN got = 1
    IF sec = 1 AND got = 1 AND INSTR(l, CHR$(34) + "elenco_costanti" + CHR$(34)) > 0 THEN sec = 2
    IF sec = 2 AND INSTR(l, CHR$(34)) > 0 THEN
      f$ = SPLIT(l, CHR$(34))
      IF UBOUND(f$) >= 1 THEN
        nm = TRIM$(f$(1))
        IF LEN(nm) > 0 AND INSTR(nm, "elenco") = 0 THEN
          IF cs = 0 THEN
            ns = nm : cs = 1
          ELSE
            cs = 0
            d = ConstFor(nm)
            IF LEN(d) > 0 THEN AddRow(ns, "%" + nm, "costante", "%" + nm, "= " + d, "metadati:valore", "")
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB
FUNCTION PinFor(ns AS STRING, nm AS STRING) AS INTEGER
  DIM k AS STRING
  k = UPPER$(ns) + "|" + UPPER$(nm)
  p_sx = "" : p_ds = ""
  IF k = "CONCURRENCY|CHANNEL" THEN
      p_sx = "CHANNEL CLOSE c"
      p_ds = "Chiude un canale GPM."
    RETURN 1
  END IF
  IF k = "CONCURRENCY|MUTEX" THEN
      p_sx = "MUTEX LOCK m / MUTEX UNLOCK m"
      p_ds = "Lock/unlock di un mutex."
    RETURN 1
  END IF
  IF k = "CONCURRENCY|ONCE" THEN
      p_sx = "ONCE RUN once, proc"
      p_ds = "Esegue la procedura una sola volta."
    RETURN 1
  END IF
  IF k = "CONSOLE|MAKE" THEN
      p_sx = "x = KEY.MAKE(key, modifiers)"
      p_ds = "Metodo statico KEY.MAKE: codifica tasto -> LONG."
    RETURN 1
  END IF
  IF k = "CONVERT|CONVBAT" THEN
      p_sx = "CONVBAT a [, b...] INTO x"
      p_ds = "Converte gli argomenti e li assegna a target."
    RETURN 1
  END IF
  IF k = "CONVERT|WEB2MD" THEN
      p_sx = "WEB2MD url INTO x"
      p_ds = "Scarica una pagina web e la converte in Markdown."
    RETURN 1
  END IF
  IF k = "GRAPHICS|TEXTMODE" THEN
      p_sx = "TEXTMODE mode"
      p_ds = "Forza il target dell'output (testo/renderer)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PIXEL" THEN
      p_sx = "PIXEL BUFFER w, h, buffer"
      p_ds = "Disegna un buffer di pixel w x h (legacy, GRAFX-03)."
    RETURN 1
  END IF
  IF k = "IO|MMAPPEEK" THEN
      p_sx = "x = MEMORYMAP.MMAPPEEK(handle, offset)"
      p_ds = "Legge un byte mappato; nome reale MEMORYMAP."
    RETURN 1
  END IF
  IF k = "IO|MMAPREADBYTES" THEN
      p_sx = "x = MEMORYMAP.MMAPREADBYTES(handle, offset, count)"
      p_ds = "Legge N byte mappati; nome reale MEMORYMAP."
    RETURN 1
  END IF
  IF k = "GRAPHICS|BLOAD" THEN
      p_sx = "BLOAD file$ [, (x!,y!)]"
      p_ds = "Carica un'immagine da file."
    RETURN 1
  END IF
  IF k = "GRAPHICS|CIRCLE" THEN
      p_sx = "CIRCLE (x!, y!), r! [, col%]"
      p_ds = "Disegna un cerchio."
    RETURN 1
  END IF
  IF k = "GRAPHICS|CLOCATE" THEN
      p_sx = "CLOCATE riga%, colonna%"
      p_ds = "Posiziona il cursore di testo nel renderer."
    RETURN 1
  END IF
  IF k = "GRAPHICS|CPALETTE" THEN
      p_sx = "CPALETTE col%, r%, g%, b%"
      p_ds = "Colore della palette testo del renderer."
    RETURN 1
  END IF
  IF k = "GRAPHICS|CPRINT" THEN
      p_sx = "CPRINT [lista]"
      p_ds = "Stampa testo nel renderer grafico."
    RETURN 1
  END IF
  IF k = "GRAPHICS|CRESTORESCREEN" THEN
      p_sx = "RESTORE SCREEN var$"
      p_ds = "Ripristina lo screen salvato (alias)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|CSAVESCREEN" THEN
      p_sx = "SAVE SCREEN var$"
      p_ds = "Salva lo screen in var$ (alias)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|DRAW" THEN
      p_sx = "DRAW cmd$"
      p_ds = "Esegue una sequenza di disegno da stringa."
    RETURN 1
  END IF
  IF k = "GRAPHICS|FONT" THEN
      p_sx = "FONT file$ [, size%]"
      p_ds = "Carica il font usato da CPRINT."
    RETURN 1
  END IF
  IF k = "GRAPHICS|IMAGEGET" THEN
      p_sx = "IMAGEGET (x1,y1)-(x2,y2), var$"
      p_ds = "Legge un rettangolo di pixel in var$."
    RETURN 1
  END IF
  IF k = "GRAPHICS|IMAGEPUT" THEN
      p_sx = "IMAGEPUT (x!,y!), var$ [, mode%]"
      p_ds = "Disegna l'immagine contenuta in var$."
    RETURN 1
  END IF
  IF k = "GRAPHICS|IMAGEPUTS" THEN
      p_sx = "IMAGEPUTS (x!,y!), var$ [, scala!]"
      p_ds = "Disegna piu' immagini/posizioni (sprite)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|LINE" THEN
      p_sx = "LINE [(x1,y1)]-(x2,y2) [, col%] [, B|BF]"
      p_ds = "Disegna linea o rettangolo (B/BF)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PAINT" THEN
      p_sx = "PAINT (x!, y!) [, fill%] [, border%]"
      p_ds = "Riempie un'area (flood fill)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PALETTE" THEN
      p_sx = "PALETTE col%, r%, g%, b%"
      p_ds = "Imposta un colore della palette RGB."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PCOPY" THEN
      p_sx = "PCOPY sorgente%, destinazione%"
      p_ds = "Copia una pagina video in un'altra."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PLAY" THEN
      p_sx = "PLAY nota$ [, durata%]"
      p_ds = "Suona una nota/sequenza audio."
    RETURN 1
  END IF
  IF k = "GRAPHICS|POLYGON" THEN
      p_sx = "POLYGON n%, (x!,y!)... [, col%]"
      p_ds = "Disegna un poligono di n vertici."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PRESET" THEN
      p_sx = "PRESET (x!, y!) [, col%]"
      p_ds = "Disegna un pixel col colore di default."
    RETURN 1
  END IF
  IF k = "GRAPHICS|PSET" THEN
      p_sx = "PSET (x!, y!) [, col%]"
      p_ds = "Disegna un pixel del colore indicato."
    RETURN 1
  END IF
  IF k = "GRAPHICS|SCREENSET" THEN
      p_sx = "SCREENSET pagina%, visibile%"
      p_ds = "Seleziona pagina video e pagina visibile."
    RETURN 1
  END IF
  IF k = "GRAPHICS|SOUND" THEN
      p_sx = "SOUND file$"
      p_ds = "Carica e riproduce un suono."
    RETURN 1
  END IF
  IF k = "GRAPHICS|SPRITE" THEN
      p_sx = "SPRITE id% <clausola>"
      p_ds = "Sprite: LOAD/ON/OFF/MOVE/SETPOSITION."
    RETURN 1
  END IF
  IF k = "GRAPHICS|SPRITEDEF" THEN
      p_sx = "SPRITEDEF id%, ..."
      p_ds = "Definisce uno sprite."
    RETURN 1
  END IF
  IF k = "GRAPHICS|SYNC" THEN
      p_sx = "SYNC"
      p_ds = "Presenta il frame (double buffer)."
    RETURN 1
  END IF
  IF k = "GRAPHICS|TILEMAP" THEN
      p_sx = "TILEMAP ..."
      p_ds = "Definisce e disegna una tilemap."
    RETURN 1
  END IF
  IF k = "CONSOLE|LINE INPUT" THEN
      p_sx = "LINE INPUT var$"
      p_ds = "Legge una riga intera di input."
    RETURN 1
  END IF
  IF k = "IO|LINE INPUT" THEN
      p_sx = "LINE INPUT var$"
      p_ds = "Legge una riga intera dal file aperto."
    RETURN 1
  END IF
  RETURN 0
END FUNCTION
FUNCTION ConstFor(nm AS STRING) AS STRING
  IF LEN(g_std) = 0 THEN RETURN ""
  BuildStdIndex()
  IF nm IN ix_cst THEN RETURN ix_cst(nm)
  RETURN ""
END FUNCTION

SUB LoadSurface(p AS STRING)
  DIM ls$() AS STRING
  DIM f$() AS STRING
  DIM l AS STRING, i AS LONG, ns AS STRING, nm AS STRING, kd AS STRING
  DIM sx AS STRING, ds AS STRING, ft AS STRING, d AS STRING
  IF NOT FILEEXISTS(p) THEN EXIT SUB
  ls$ = IO.READLINES(p)
  FOR i = 0 TO UBOUND(ls$)
    l = TRIM$(ls$(i))
    IF LEN(l) > 0 AND LEFT$(l, 1) <> "#" AND LEFT$(l, 4) <> "kind" THEN
      f$ = SPLIT(l, ",")
      IF UBOUND(f$) >= 2 THEN
        kd = f$(0) : ns = f$(1) : nm = f$(2)
        IF NOT ((UPPER$(ns) + "|" + UPPER$(nm)) IN key_dict) THEN
          IF LEN(ns) > 0 OR NameExists(nm) = 0 THEN
          sx = "" : ds = "" : ft = ""
          IF kd = "procedure" THEN kd = "funzione"
          IF kd = "command" THEN kd = "comando"
          IF kd = "constant" THEN kd = "costante"
          IF kd = "comando" AND LEN(ns) = 0 THEN
            IF LookupCommand(nm) <> 0 THEN
              sx = c_sx : ds = c_ds : ft = "storico"
            END IF
          END IF
          IF PinFor(ns, nm) <> 0 THEN
            sx = p_sx : ds = p_ds : ft = "manuale"
          END IF
          IF kd = "costante" AND LEN(ds) = 0 THEN
            d = ConstFor(nm)
            IF LEN(d) > 0 THEN
              ds = "= " + d : ft = "metadati:valore"
            END IF
          END IF
          IF LEN(ds) = 0 THEN
            d = HistoryFor(ns, nm)
            IF LEN(d) > 0 THEN ds = d : ft = "storico"
          END IF
          IF LEN(ds) = 0 THEN d = CommentFor(nm)
          IF LEN(d) > 0 AND LEN(ds) = 0 THEN
            ds = d : ft = "commento"
          ELSEIF LEN(ds) = 0 AND MetaFor(ns, nm) <> 0 THEN
            sx = m_sx : ds = m_ds : ft = "metadati"
          END IF
          ' SINTASSI e DESCRIZIONE si risolvono INDIPENDENTEMENTE: quando la descrizione arriva
          ' da un commento, prima la sintassi restava vuota (riga MESSAGE.DELIVERY senza
          ' parametri nel manuale). Se la sintassi manca si prende dai metadati della
          ' registrazione (MetaFor), che ha i parametri veri.
          IF LEN(TRIM$(sx)) = 0 AND MetaFor(ns, nm) <> 0 THEN sx = m_sx
          IF LEN(ds) = 0 AND ns = "CONTROLS" THEN
            ds = "Evento UI generato dal runtime."
            ft = "derivato:evento"
          END IF
          IF LEN(ds) = 0 AND ns = "ATOMIC" THEN
            ds = "Operazione atomica sul valore."
            ft = "derivato:handle"
          END IF
          IF LEN(ds) = 0 AND ns = "CONTEXT" THEN
            ds = "Handle di contesto asincrono."
            ft = "derivato:handle"
          END IF
          IF LEN(ds) = 0 AND ns = "SYNCMAP" THEN
            ds = "Mappa sincronizzata."
            ft = "derivato:handle"
          END IF
          IF LEN(ds) = 0 AND ns = "CONCURRENCY" AND UPPER$(nm) = "SLEEPASYNC" THEN
            ds = "Attesa asincrona cooperativa."
            ft = "derivato:handle"
          END IF
          IF LEN(ds) = 0 AND kd = "comando" AND LEN(ns) = 0 THEN
            ds = "Dispatcher del namespace " + UPPER$(nm) + "."
            ft = "derivato:dispatcher"
          END IF
          IF LEN(ds) = 0 THEN
            ds = "Nessun testo: " + kd + " senza metadati."
            ft = "scoperto"
          END IF
          AddRow(ns, nm, kd, sx, ds, ft, "")
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

SUB WriteCatalog(p AS STRING)
  DIM h AS LONG, i AS LONG
  h = IO.FCREATE(p)
  IO.FWRITE(h, "# GMBASIC — catalogo descrizioni per COMMANDS.TXT" + CHR$(10))
  IO.FWRITE(h, "# Generato da tools/basic/gen_command_desc.bas (fonts: StdLib + commenti + storico + metadati)." + CHR$(10))
  IO.FWRITE(h, "# Colonne: NS \t NOME \t KIND \t SINTASSI \t DESCRIZIONE \t FONTE \t MEMBRI" + CHR$(10))
  FOR i = 1 TO r_n
    IO.FWRITE(h, Clean(r_ns(i)) + CHR$(9) + Clean(r_nm(i)) + CHR$(9) + Clean(r_kd(i)) + CHR$(9) + Clean(r_sx(i)) + CHR$(9) + Clean(r_ds(i)) + CHR$(9) + Clean(r_ft(i)) + CHR$(9) + Clean(r_mb(i)) + CHR$(10))
  NEXT i
  IO.FCLOSE(h)
END SUB

FUNCTION Main() AS INTEGER
  DIM out AS STRING, commands AS STRING, surface AS STRING
  DIM rep AS INTEGER, chk AS INTEGER, i AS LONG, l AS LONG
  DIM ls$() AS STRING
  DIM f$() AS STRING
  DIM src(30) AS STRING, cnt(30) AS LONG
  DIM sc AS LONG, gen AS LONG, tgen AS LONG, fr AS LONG, c AS LONG
  SET SHELL ON
  out = "" : commands = "" : surface = "" : rep = 0 : chk = 0
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--out" AND i < COMMANDCOUNT() THEN i = i + 1 : out = COMMAND$(i)
    IF COMMAND$(i) = "--commands" AND i < COMMANDCOUNT() THEN i = i + 1 : commands = COMMAND$(i)
    IF COMMAND$(i) = "--surface" AND i < COMMANDCOUNT() THEN i = i + 1 : surface = COMMAND$(i)
    IF COMMAND$(i) = "--report" THEN rep = 1
    IF COMMAND$(i) = "--check" THEN chk = 1
    i = i + 1
  LOOP
  g_std = FindUp("GMBasicSharp/common/StdLib/StdLib.AI.cs")
  IF LEN(g_std) = 0 THEN g_std = FindUp("common/StdLib/StdLib.AI.cs")
  IF LEN(g_std) > 0 THEN g_std = IO.PARENTDIR(g_std)
  IF LEN(out) = 0 THEN
    out = FindUp("docs/comparison/command_desc.tsv")
  END IF
  IF LEN(commands) = 0 THEN commands = FindUp("COMMANDS.TXT")
  g_cmd = commands
  IF LEN(surface) = 0 THEN surface = FindUp("shared/spec/registry/surface.csv")
  IF LEN(surface) = 0 THEN surface = FindUp("spec/registry/surface.csv")
  IF LEN(out) = 0 THEN PRINT "command_desc.tsv non trovato" : RETURN 2
  r_n = 0 : key_dict = NEW DICT OF STRING, INTEGER
  LoadCatalog(out)
  LoadSurface(surface)
  LoadRecon()
  FOR i = 1 TO r_n
    fr = -1
    FOR c = 0 TO 29
      IF src(c) = r_ft(i) THEN fr = c : EXIT FOR
    NEXT c
    IF fr < 0 THEN
      FOR c = 0 TO 29
        IF LEN(src(c)) = 0 THEN src(c) = r_ft(i) : fr = c : EXIT FOR
      NEXT c
    END IF
    IF fr >= 0 THEN cnt(fr) = cnt(fr) + 1
    IF r_ft(i) = "scoperto" THEN sc = sc + 1
  NEXT i
  IF chk <> 0 THEN
    gen = 0 : tgen = 0
    IF FILEEXISTS(commands) THEN
      ls$ = IO.READLINES(commands)
      FOR i = 0 TO UBOUND(ls$)
        l = ls$(i)
        IF INSTR(l, "│") > 0 THEN
          f$ = SPLIT(l, "│")
          IF UBOUND(f$) >= 3 THEN
            IF IsGeneric(f$(3)) <> 0 THEN
              IF LEFT$(TRIM$(f$(1)), 5) = "TYPE:" OR LEFT$(TRIM$(f$(1)), 10) = "COSTANTE:%" THEN
                tgen = tgen + 1
              ELSE
                gen = gen + 1
                IF gen <= 20 THEN PRINT "  generica: "; TRIM$(f$(1))
              END IF
            END IF
          END IF
        END IF
      NEXT i
    END IF
    PRINT "catalogo: "; r_n; " righe | scoperte: "; sc; " | generiche comando/funzione: "; gen; " | generiche tipo/costante: "; tgen
    IF sc > 0 OR gen > 0 OR tgen > 0 THEN RETURN 1
    RETURN 0
  END IF
  WriteCatalog(out)
  PRINT "catalogo: "; out; " ("; r_n; " righe)"
  FOR i = 0 TO 29
    IF LEN(src(i)) > 0 THEN PRINT "  "; src(i); ": "; cnt(i)
  NEXT i
  IF rep <> 0 AND sc > 0 THEN PRINT "ancora scoperte: "; sc
  RETURN 0
END FUNCTION
