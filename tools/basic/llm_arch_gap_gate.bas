' llm_arch_gap_gate.bas — gate "zero gap" LLM (HF universe vs GMBasic).
' Equivalente del vecchio tools/llm_arch_gap_gate.py, senza Python.
'
' Confronta l'universo dei causal-LM (snapshot hf_causal_lm_models.txt) con ciò che
' GMBasic DESCRIVE (LLMEnv/ABI/*.gmbarch) ed ESEGUE (ARCH_TABLE in
' <Projects>/LLMEnv/LLMEngine/mlx/llm_mlx_model_lifecycle.c), meno le esclusioni
' motivate in llm_arch_allowlist.tsv. Esce 1 se ci sono gap.
'
' Uso (dalla root del monorepo):
'   tools/basic/llm_arch_gap_gate.bas
IMPORT IO
IMPORT GREP

DIM g_uni(4000) AS STRING
DIM g_nu AS LONG
DIM g_des(2000) AS STRING
DIM g_nd AS LONG
DIM g_exe(2000) AS STRING
DIM g_ne AS LONG
DIM g_exc(2000) AS STRING
DIM g_nx AS LONG

SUB AddUni(s AS STRING)
  DIM i AS LONG
  s = TRIM$(s)
  IF LEN(s) = 0 THEN EXIT SUB
  IF LEFT$(s, 1) = "#" THEN EXIT SUB
  FOR i = 1 TO g_nu
    IF g_uni(i) = s THEN EXIT SUB
  NEXT i
  IF g_nu < 4000 THEN g_nu = g_nu + 1 : g_uni(g_nu) = s
END SUB

SUB AddDes(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 1 TO g_nd
    IF g_des(i) = s THEN EXIT SUB
  NEXT i
  IF g_nd < 2000 THEN g_nd = g_nd + 1 : g_des(g_nd) = s
END SUB

SUB AddExe(s AS STRING)
  DIM i AS LONG
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 1 TO g_ne
    IF g_exe(i) = s THEN EXIT SUB
  NEXT i
  IF g_ne < 2000 THEN g_ne = g_ne + 1 : g_exe(g_ne) = s
END SUB

SUB AddExc(s AS STRING)
  DIM i AS LONG
  s = TRIM$(s)
  IF LEN(s) = 0 THEN EXIT SUB
  FOR i = 1 TO g_nx
    IF g_exc(i) = s THEN EXIT SUB
  NEXT i
  IF g_nx < 2000 THEN g_nx = g_nx + 1 : g_exc(g_nx) = s
END SUB

FUNCTION InUni(s AS STRING) AS INTEGER
  DIM i AS LONG
  FOR i = 1 TO g_nu
    IF g_uni(i) = s THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION InDes(s AS STRING) AS INTEGER
  DIM i AS LONG
  FOR i = 1 TO g_nd
    IF g_des(i) = s THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION InExe(s AS STRING) AS INTEGER
  DIM i AS LONG
  FOR i = 1 TO g_ne
    IF g_exe(i) = s THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION InExc(s AS STRING) AS INTEGER
  DIM i AS LONG
  FOR i = 1 TO g_nx
    IF g_exc(i) = s THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION IsLowerId(c AS STRING) AS INTEGER
  IF (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

' Descritte: nomi file *.gmbarch in <abi>.
SUB LoadDescribed(abi AS STRING)
  DIM f = IO.DIR(abi + "/*")
  DIM n AS STRING
  DIM i AS LONG, j AS LONG, p AS LONG
  FOR i = 0 TO UBOUND(f)
    n = f(i)
    p = 0
    FOR j = 1 TO LEN(n)
      IF MID$(n, j, 1) = "/" THEN p = j
    NEXT j
    IF p > 0 THEN n = MID$(n, p + 1)
    IF LEN(n) > 8 THEN
      IF RIGHT$(n, 8) = ".gmbarch" THEN AddDes(MID$(n, 1, LEN(n) - 8))
    END IF
  NEXT i
END SUB

' Eseguibili: nomi in ARCH_TABLE, pattern { "model_type",  (come il regex del .py).
SUB LoadExecutable(life AS STRING)
  DIM s AS STRING, c AS STRING, w AS STRING
  DIM i AS LONG, st AS LONG, q AS LONG
  s = IO.READFILE(life)
  IF LEN(s) = 0 THEN EXIT SUB
  i = 1
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF c = "{" THEN
      q = i + 1
      DO WHILE q <= LEN(s)
        c = MID$(s, q, 1)
        IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN
          q = q + 1
        ELSE
          EXIT DO
        END IF
      LOOP
      IF q <= LEN(s) AND MID$(s, q, 1) = CHR$(34) THEN
        st = q + 1 : q = st
        DO WHILE q <= LEN(s)
          c = MID$(s, q, 1)
          IF IsLowerId(c) THEN
            q = q + 1
          ELSE
            EXIT DO
          END IF
        LOOP
        IF q > st AND q <= LEN(s) AND MID$(s, q, 1) = CHR$(34) THEN
          w = MID$(s, st, q - st)
          q = q + 1
          DO WHILE q <= LEN(s)
            c = MID$(s, q, 1)
            IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN
              q = q + 1
            ELSE
              EXIT DO
            END IF
          LOOP
          IF q <= LEN(s) AND MID$(s, q, 1) = "," THEN AddExe(w)
        END IF
        i = q
      ELSE
        i = i + 1
      END IF
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, tools AS STRING, abi AS STRING, life AS STRING
  DIM snap AS STRING, allow AS STRING
  DIM lines
  DIM i AS LONG, up AS LONG, p AS LONG
  DIM md AS LONG, me AS LONG, gaps AS LONG
  DIM u AS STRING
  root = IO.CWD()
  FOR up = 1 TO 6
    IF DIREXISTS(root + "/tools/basic") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up
  tools = root + "/tools"
  abi = IO.PARENTDIR(root) + "/LLMEnv/ABI"   ' descrittori: UNICA sede = cartella ABI di LLMEnv (siblings)
  life = IO.PARENTDIR(root) + "/LLMEnv/LLMEngine/mlx/llm_mlx_model_lifecycle.c"
  snap = tools + "/data/hf_causal_lm_models.txt"
  allow = tools + "/data/llm_arch_allowlist.tsv"
  IF NOT FILEEXISTS(snap) THEN PRINT "ERRORE: snapshot mancante ("; snap; ")" : RETURN 2
  IF NOT FILEEXISTS(life) THEN PRINT "ERRORE: lifecycle LLMEnv mancante ("; life; ")" : RETURN 2

  lines$ = IO.READLINES(snap)
  FOR i = 0 TO UBOUND(lines)
    AddUni(lines(i))
  NEXT i
  LoadDescribed(abi)
  LoadExecutable(life)
  IF FILEEXISTS(allow) THEN
    lines$ = IO.READLINES(allow)
    FOR i = 0 TO UBOUND(lines)
      u = lines(i)
      IF LEN(u) > 0 AND LEFT$(u, 1) <> "#" THEN
        p = INSTR(u, CHR$(9))
        IF p > 0 THEN
          AddExc(LEFT$(u, p - 1))
        ELSE
          AddExc(u)
        END IF
      END IF
    NEXT i
  END IF

  PRINT "=== GATE GAP ARCH LLM ==="
  PRINT "universo causal-LM : "; g_nu
  PRINT "descritte (.gmbarch): "; g_nd; "   eseguibili (ARCH_TABLE): "; g_ne
  PRINT "escluse (allowlist) : "; g_nx
  PRINT ""

  md = 0
  FOR i = 1 TO g_nu
    IF NOT InDes(g_uni(i)) AND NOT InExc(g_uni(i)) THEN md = md + 1
  NEXT i
  me = 0
  FOR i = 1 TO g_nd
    IF InUni(g_des(i)) AND NOT InExe(g_des(i)) THEN me = me + 1
  NEXT i

  PRINT "--- NON descritte ("; md; ") ---"
  IF md = 0 THEN
    PRINT "(nessuna)"
  ELSE
    FOR i = 1 TO g_nu
      IF NOT InDes(g_uni(i)) AND NOT InExc(g_uni(i)) THEN PRINT g_uni(i); " ";
    NEXT i
    PRINT ""
  END IF
  PRINT "--- descritte ma NON eseguibili ("; me; ") ---"
  IF me = 0 THEN
    PRINT "(nessuna)"
  ELSE
    FOR i = 1 TO g_nd
      IF InUni(g_des(i)) AND NOT InExe(g_des(i)) THEN PRINT g_des(i); " ";
    NEXT i
    PRINT ""
  END IF
  gaps = md + me
  PRINT ""
  IF gaps = 0 THEN
    PRINT "GAP TOTALI: 0  ->  PULITO"
  ELSE
    PRINT "GAP TOTALI: "; gaps; "  ->  DA CHIUDERE"
  END IF
  IF gaps = 0 THEN RETURN 0
  RETURN 1
END FUNCTION
