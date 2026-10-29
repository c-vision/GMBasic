' clipper_dead.bas — analisi di raggiungibilità per il PRUNE di ClipperLib.
' Equivalente del vecchio tools/clipper_dead.py, senza Python.
'
' Namespace-aware: ogni tipo ha full-name <namespace>.<Nome>. Un riferimento (nome corto)
' in un file viene risolto verso <ns_file>.<Nome>, poi via `using X;` (X.<Nome>), poi verso
' qualunque <...>.<Nome> (conservativo, solo file senza namespace). Radici = file .cs FUORI
' da GMBasicSharp/common/ClipperLib. Tipi ClipperLib NON raggiunti = morti.
'
' Prestazioni (VM/C): GREP fa le ricerche (regex PCRE2 in C) su TUTTO l'albero; in GMBasic
' si parsano solo le righe restituite (namespace/using/tipi per i file coinvolti; righe che
' citano un tipo ClipperLib per i riferimenti). Nessuna scansione char-by-char dell'intero
' corpus.
IMPORT IO
IMPORT GREP

DIM g_def AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_short AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_clip AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_reach AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_nsd AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_usd AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_ftd AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_clipHits AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
DIM g_isclip AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_proc AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_ct(8000) AS STRING
DIM g_nct AS LONG
DIM g_nreach AS LONG
DIM g_q(20000) AS STRING
DIM g_qh AS LONG
DIM g_qt AS LONG
DIM g_cf(20000) AS STRING
DIM g_ncf AS LONG


DIM g_pos AS LONG
DIM g_tok AS STRING
DIM g_alt AS STRING
DIM g_cur_ns AS STRING
DIM g_cur_us AS STRING
DIM g_cur_ft AS STRING

FUNCTION Before1stColon(h AS STRING) AS STRING
  DIM p AS LONG
  p = INSTR(h, ":")
  IF p = 0 THEN RETURN h
  RETURN MID$(h, 1, p - 1)
END FUNCTION

FUNCTION After3rdColon(h AS STRING) AS STRING
  DIM p1 AS LONG, p2 AS LONG, p3 AS LONG
  p1 = INSTR(h, ":")
  IF p1 = 0 THEN RETURN ""
  p2 = INSTR(p1 + 1, h, ":")
  IF p2 = 0 THEN RETURN ""
  p3 = INSTR(p2 + 1, h, ":")
  IF p3 = 0 THEN RETURN ""
  RETURN MID$(h, p3 + 1)
END FUNCTION

SUB SkipSp(s AS STRING, q AS LONG)
  DIM c AS STRING
  DO WHILE q <= LEN(s)
    c = MID$(s, q, 1)
    IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN
      q = q + 1
    ELSE
      EXIT DO
    END IF
  LOOP
  g_pos = q
END SUB

SUB ReadWord(s AS STRING, q AS LONG)
  DIM st AS LONG, c AS STRING
  st = q
  DO WHILE q <= LEN(s)
    c = MID$(s, q, 1)
    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
      q = q + 1
    ELSE
      EXIT DO
    END IF
  LOOP
  g_tok = MID$(s, st, q - st)
  g_pos = q
END SUB

SUB ReadDotted(s AS STRING, q AS LONG)
  DIM st AS LONG, c AS STRING
  st = q
  DO WHILE q <= LEN(s)
    c = MID$(s, q, 1)
    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" OR c = "." THEN
      q = q + 1
    ELSE
      EXIT DO
    END IF
  LOOP
  g_tok = MID$(s, st, q - st)
  g_pos = q
END SUB

SUB AddReach(f AS STRING)
  IF LEN(f) = 0 THEN EXIT SUB
  IF f IN g_reach THEN EXIT SUB
  g_reach(f) = 1
  g_nreach = g_nreach + 1
  IF g_qt < 20000 THEN g_qt = g_qt + 1 : g_q(g_qt) = f
END SUB

SUB Resolve(short AS STRING, ns AS STRING, us AS STRING)
  DIM i AS LONG, u AS STRING, f AS STRING
  DIM parts$() AS STRING
  IF LEN(short) = 0 THEN EXIT SUB
  IF LEN(ns) > 0 THEN
    f = ns + "." + short
    IF f IN g_clip THEN
      AddReach(f)
    END IF
  END IF
  IF LEN(us) > 0 THEN
    parts$ = SPLIT(us, ";")
    FOR i = 0 TO UBOUND(parts$)
      u = parts$(i)
      IF LEN(u) > 0 THEN
        f = u + "." + short
        IF f IN g_clip THEN
          AddReach(f)
        END IF
      END IF
    NEXT i
  END IF
  IF LEN(ns) = 0 THEN
    IF short IN g_short THEN
      parts$ = SPLIT(g_short(short), ";")
      FOR i = 0 TO UBOUND(parts$)
        f = parts$(i)
        IF LEN(f) > 0 AND INSTR(f, ".") = 0 THEN
          IF f IN g_clip THEN
              AddReach(f)
          END IF
        END IF
      NEXT i
    END IF
  END IF
END SUB

' Idents (maiuscola, len>=2) e catene qualificate A.B(.C) su UNA riga.
SUB ScanIdLine(s AS STRING, ns AS STRING, us AS STRING)
  DIM i AS LONG, st AS LONG, q AS LONG, q2 AS LONG
  DIM c AS STRING, w AS STRING, comp AS STRING, cand AS STRING
  i = 1
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF c >= "A" AND c <= "Z" THEN
      st = i
      DO WHILE i <= LEN(s)
        c = MID$(s, i, 1)
        IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
          i = i + 1
        ELSE
          EXIT DO
        END IF
      LOOP
      w = MID$(s, st, i - st)
      IF LEN(w) >= 2 THEN Resolve(w, ns, us)
      q = i
      IF q <= LEN(s) AND MID$(s, q, 1) = "." AND q + 1 <= LEN(s) THEN
        IF MID$(s, q + 1, 1) >= "A" AND MID$(s, q + 1, 1) <= "Z" THEN
          cand = w
          DO WHILE q <= LEN(s) AND MID$(s, q, 1) = "."
            IF q + 1 > LEN(s) THEN EXIT DO
            IF NOT (MID$(s, q + 1, 1) >= "A" AND MID$(s, q + 1, 1) <= "Z") THEN EXIT DO
            IF cand IN g_clip THEN AddReach(cand)
            q2 = q + 1
            st = q2
            DO WHILE q2 <= LEN(s)
              c = MID$(s, q2, 1)
              IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                q2 = q2 + 1
              ELSE
                EXIT DO
              END IF
            LOOP
            comp = MID$(s, st, q2 - st)
            cand = cand + "." + comp
            IF cand IN g_clip THEN AddReach(cand)
            q = q2
          LOOP
          i = q
        END IF
      END IF
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

FUNCTION InList(list AS STRING, f AS STRING) AS INTEGER
  DIM i AS LONG
  DIM parts$() AS STRING
  IF LEN(list) = 0 THEN RETURN 0
  parts$ = SPLIT(list, ";")
  FOR i = 0 TO UBOUND(parts$)
    IF parts$(i) = f THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

' namespace/using/tipi su UNA riga (accumula in g_cur_*).
SUB ScanMetaLine(s AS STRING)
  DIM c AS STRING, w AS STRING, t AS STRING
  DIM i AS LONG, st AS LONG, q AS LONG
  i = 1
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR c = "_" THEN
      st = i
      DO WHILE i <= LEN(s)
        c = MID$(s, i, 1)
        IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
          i = i + 1
        ELSE
          EXIT DO
        END IF
      LOOP
      w = MID$(s, st, i - st)
      IF w = "namespace" THEN
        SkipSp(s, i)
        ReadDotted(s, g_pos)
        IF LEN(g_tok) > 0 THEN g_cur_ns = g_tok
        i = g_pos
      ELSEIF w = "using" THEN
        q = i
        SkipSp(s, q)
        ReadWord(s, g_pos)
        IF g_tok = "static" THEN
          SkipSp(s, g_pos)
          ReadDotted(s, g_pos)
        ELSE
          ReadDotted(s, q)
        END IF
        IF LEN(g_tok) > 0 THEN
          IF InList(g_cur_us, g_tok) = 0 THEN
            IF LEN(g_cur_us) = 0 THEN g_cur_us = g_tok ELSE g_cur_us = g_cur_us + ";" + g_tok
          END IF
        END IF
        i = g_pos
      ELSEIF w = "class" OR w = "struct" OR w = "interface" OR w = "enum" OR w = "record" THEN
        SkipSp(s, i)
        ReadWord(s, g_pos)
        t = g_tok
        IF t = "struct" OR t = "class" THEN
          SkipSp(s, g_pos)
          ReadWord(s, g_pos)
          t = g_tok
        END IF
        IF LEN(t) > 0 THEN
          c = MID$(t, 1, 1)
          IF c >= "A" AND c <= "Z" THEN
            IF InList(g_cur_ft, t) = 0 THEN
              IF LEN(g_cur_ft) = 0 THEN g_cur_ft = t ELSE g_cur_ft = g_cur_ft + ";" + t
            END IF
          END IF
        END IF
        i = g_pos
      END IF
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

' Carica namespace/using/tipi di UN file (GREP sulla sola riga-keyword) e, se isClip,
' registra defines/short2full/g_clip.
SUB LoadMetaFile(path AS STRING, isClip AS LONG)
  DIM hits$() AS STRING
  DIM h AS LONG, k AS LONG, t AS STRING, full AS STRING
  DIM parts$() AS STRING
  g_cur_ns = "" : g_cur_us = "" : g_cur_ft = ""
  hits$ = GREP.MATCHES("\b(namespace|using|class|struct|interface|enum|record)\b", path)
  FOR h = 0 TO UBOUND(hits$)
    ScanMetaLine(After3rdColon(hits$(h)))
  NEXT h
  g_nsd(path) = g_cur_ns
  g_usd(path) = g_cur_us
  IF isClip <> 0 THEN
    g_ftd(path) = g_cur_ft
    IF LEN(g_cur_ft) > 0 THEN
      parts$ = SPLIT(g_cur_ft, ";")
      FOR k = 0 TO UBOUND(parts$)
        t = parts$(k)
        IF LEN(g_cur_ns) > 0 THEN full = g_cur_ns + "." + t ELSE full = t
        IF NOT (full IN g_def) THEN g_def(full) = path
        IF t IN g_short THEN
          IF InList(g_short(t), full) = 0 THEN g_short(t) = g_short(t) + ";" + full
        ELSE
          g_short(t) = full
        END IF
        IF NOT (full IN g_clip) THEN
          g_clip(full) = 1
          IF g_nct < 8000 THEN g_nct = g_nct + 1 : g_ct(g_nct) = full
        END IF
      NEXT k
    END IF
  END IF
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, gs AS STRING, clipDir AS STRING
  DIM clip$() AS STRING
  DIM i AS LONG, up AS LONG, n AS LONG, j AS LONG, p AS LONG
  DIM f AS STRING, full AS STRING, alt AS STRING, short AS STRING
  DIM ns AS STRING, us AS STRING
  DIM dead AS LONG, ndf AS LONG, allDead AS LONG
  DIM parts$() AS STRING, k AS LONG
  DIM altset AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  DIM ex AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  root = IO.CWD()
  FOR up = 1 TO 6
    IF DIREXISTS(root + "/GMBasicSharp/common") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up
  gs = root + "/GMBasicSharp"
  clipDir = gs + "/common/ClipperLib"
  IF NOT DIREXISTS(gs) THEN
    PRINT "ERRORE: GMBasicSharp non trovato ("; gs; ")"
    RETURN 2
  END IF

  ' 1) ClipperLib: elenco + meta (una chiamata GREP sulla dir)
  clip$ = GREP.FILES(" ", clipDir)
  g_ncf = 0
  FOR i = 0 TO UBOUND(clip$)
    f = clip$(i)
    IF RIGHT$(UPPER(f), 3) = ".CS" THEN
      IF INSTR(f, "/obj/") = 0 AND INSTR(f, "/bin/") = 0 AND INSTR(f, "/.git/") = 0 AND INSTR(f, "graphify-out") = 0 THEN
        IF g_ncf < 20000 THEN
          g_ncf = g_ncf + 1 : g_cf(g_ncf) = f
          g_isclip(f) = 1
        END IF
      END IF
    END IF
  NEXT i
  FOR i = 1 TO g_ncf
    LoadMetaFile(g_cf(i), -1)
  NEXT i

  ' 2) regex dei nomi corti + hit su tutto l'albero (una chiamata GREP)
  alt = ""
  FOR i = 1 TO g_nct
    full = g_ct(i)
    p = 0
    FOR j = 1 TO LEN(full)
      IF MID$(full, j, 1) = "." THEN p = j
    NEXT j
    IF p > 0 THEN short = MID$(full, p + 1) ELSE short = full
    IF NOT (short IN altset) THEN
      altset(short) = 1
      IF LEN(alt) = 0 THEN alt = short ELSE alt = alt + "|" + short
    END IF
  NEXT i
  IF LEN(alt) = 0 THEN
    PRINT "Tipi ClipperLib: 0  raggiunti: 0  morti: 0"
    PRINT "File ClipperLib interamente morti: 0"
    RETURN 0
  END IF
  g_alt = "\b(" + alt + ")\b"
  g_qh = 1 : g_qt = 0 : g_nreach = 0
  ' 2b) scansione per directory con coda iterativa (niente ricorsione: le locali di
  '     SUB non sono per-frame su VM/C). Una dir troppo grande viene divisa.
  ScanAll(gs)

  ' 3) BFS sui file ClipperLib raggiunti (le righe sono già in g_clipHits)
  DO WHILE g_qh <= g_qt
    full = g_q(g_qh)
    g_qh = g_qh + 1
    IF full IN g_def THEN
      f = g_def(full)
      IF NOT (f IN g_proc) THEN
        g_proc(f) = 1
        IF f IN g_clipHits THEN
          parts$ = SPLIT(g_clipHits(f), CHR$(10))
          IF f IN g_nsd THEN ns = g_nsd(f) ELSE ns = ""
          IF f IN g_usd THEN us = g_usd(f) ELSE us = ""
          FOR k = 0 TO UBOUND(parts$)
            IF LEN(parts$(k)) > 0 THEN ScanIdLine(parts$(k), ns, us)
          NEXT k
        END IF
      END IF
    END IF
  LOOP

  dead = 0
  FOR i = 1 TO g_nct
    IF NOT (g_ct(i) IN g_reach) THEN dead = dead + 1
  NEXT i
  ndf = 0
  FOR i = 1 TO g_ncf
    f = g_cf(i)
    IF f IN g_ftd THEN
      IF LEN(g_ftd(f)) > 0 THEN
        parts$ = SPLIT(g_ftd(f), ";")
        allDead = -1
        IF f IN g_nsd THEN ns = g_nsd(f) ELSE ns = ""
        FOR k = 0 TO UBOUND(parts$)
          IF LEN(ns) > 0 THEN full = ns + "." + parts$(k) ELSE full = parts$(k)
          IF full IN g_reach THEN allDead = 0
        NEXT k
        IF allDead <> 0 THEN ndf = ndf + 1
      END IF
    END IF
  NEXT i

  PRINT "Tipi ClipperLib: "; g_nct; "  raggiunti: "; g_nreach; "  morti: "; dead
  PRINT "File ClipperLib interamente morti: "; ndf
  n = LEN(gs) + 1
  FOR i = 1 TO g_ncf
    f = g_cf(i)
    IF f IN g_ftd THEN
      IF LEN(g_ftd(f)) > 0 THEN
        parts$ = SPLIT(g_ftd(f), ";")
        allDead = -1
        IF f IN g_nsd THEN ns = g_nsd(f) ELSE ns = ""
        FOR k = 0 TO UBOUND(parts$)
          IF LEN(ns) > 0 THEN full = ns + "." + parts$(k) ELSE full = parts$(k)
          IF full IN g_reach THEN allDead = 0
        NEXT k
        IF allDead <> 0 THEN PRINT "  "; MID$(f, n)
      END IF
    END IF
  NEXT i
  RETURN 0
END FUNCTION

SUB ProcessLine(f AS STRING, line AS STRING)
  DIM ns AS STRING, us AS STRING
  IF f IN g_isclip THEN
    IF f IN g_clipHits THEN
      g_clipHits(f) = g_clipHits(f) + CHR$(10) + line
    ELSE
      g_clipHits(f) = line
    END IF
  ELSE
    IF NOT (f IN g_nsd) THEN
      LoadMetaFile(f, 0)
    END IF
    IF f IN g_nsd THEN ns = g_nsd(f) ELSE ns = ""
    IF f IN g_usd THEN us = g_usd(f) ELSE us = ""
    ScanIdLine(line, ns, us)
  END IF
END SUB

SUB ScanAll(root AS STRING)
  DIM q(20000) AS STRING, qh AS LONG, qt AS LONG
  DIM d AS STRING, cd AS STRING, f AS STRING
  DIM hits$() AS STRING, kids$() AS STRING
  DIM k AS LONG, h AS LONG
  qh = 1 : qt = 0
  kids$ = IO.DIR(root + "/*")
  FOR k = 0 TO UBOUND(kids$)
    cd = root + "/" + kids$(k)
    IF DIREXISTS(cd) THEN
      IF INSTR(cd, "/.git") = 0 AND INSTR(cd, "/obj") = 0 AND INSTR(cd, "/bin") = 0 AND INSTR(cd, "graphify-out") = 0 THEN
        IF qt < 20000 THEN qt = qt + 1 : q(qt) = cd
      END IF
    ELSE
      IF RIGHT$(UPPER(cd), 3) = ".CS" THEN
        ScanFile(cd)
      END IF
    END IF
  NEXT k
  DO WHILE qh <= qt
    d = q(qh)
    qh = qh + 1
    hits$ = GREP.MATCHES(g_alt, d)
    IF UBOUND(hits$) >= 0 AND UBOUND(hits$) <= 30000 THEN
      FOR h = 0 TO UBOUND(hits$)
        f = Before1stColon(hits$(h))
        IF RIGHT$(UPPER(f), 3) = ".CS" THEN
          IF INSTR(f, "/obj/") = 0 AND INSTR(f, "/bin/") = 0 AND INSTR(f, "/.git/") = 0 AND INSTR(f, "graphify-out") = 0 THEN
            ProcessLine(f, After3rdColon(hits$(h)))
          END IF
        END IF
      NEXT h
    ELSE
      kids$ = IO.DIR(d + "/*")
      FOR k = 0 TO UBOUND(kids$)
        cd = d + "/" + kids$(k)
        IF DIREXISTS(cd) THEN
          IF INSTR(cd, "/.git") = 0 AND INSTR(cd, "/obj") = 0 AND INSTR(cd, "/bin") = 0 AND INSTR(cd, "graphify-out") = 0 THEN
            IF qt < 20000 THEN qt = qt + 1 : q(qt) = cd
          END IF
        ELSE
          IF RIGHT$(UPPER(cd), 3) = ".CS" THEN
            ScanFile(cd)
          END IF
        END IF
      NEXT k
    END IF
  LOOP
END SUB

SUB ScanFile(f AS STRING)
  DIM hits$() AS STRING, h AS LONG
  hits$ = GREP.MATCHES(g_alt, f)
  FOR h = 0 TO UBOUND(hits$)
    ProcessLine(f, After3rdColon(hits$(h)))
  NEXT h
END SUB
