' llm_ns_census.bas — censimento namespace AI -> LLM (equivalente dry-run del vecchio
' tools/llm_ns_census.py, senza Python). Il move LLM e' completato: --apply non e' portato.
'
' Scansiona i .bas di <root> (default shared) e classifica:
'   llm-only       usa solo superficie engine che sta in LLM
'   ai+llm         usa superficie engine E altro AI
'   local-register AI REGISTER con indizi di engine locale (candidato LLM)
'   ai-only        nessuna superficie LLM (non si tocca)
'
' Uso: gmb llm_ns_census.bas [--root DIR] [--quiet]
IMPORT IO
IMPORT GREP

DIM g_n AS LONG
DIM g_ip(5000) AS STRING
DIM g_ic(5000) AS LONG
DIM g_if(5000) AS LONG
DIM g_ir(5000) AS LONG
DIM g_irt(5000) AS LONG
DIM g_isa(5000) AS LONG
DIM g_iia(5000) AS LONG
DIM g_iil(5000) AS LONG
DIM g_io(5000) AS LONG
DIM g_iv(5000) AS LONG

FUNCTION IsWord(c AS STRING) AS INTEGER
  IF c >= "A" AND c <= "Z" THEN RETURN -1
  IF c >= "a" AND c <= "z" THEN RETURN -1
  IF c >= "0" AND c <= "9" THEN RETURN -1
  IF c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION SkipSp(line AS STRING, p AS LONG) AS LONG
  DO WHILE p <= LEN(line)
    IF MID$(line, p, 1) <> " " AND MID$(line, p, 1) <> CHR$(9) THEN EXIT DO
    p = p + 1
  LOOP
  RETURN p
END FUNCTION

' conteggio della sequenza di parole a + b (spazi arbitrari, confini di parola)
FUNCTION CountSeq(line AS STRING, a AS STRING, b AS STRING) AS LONG
  DIM p AS LONG, q AS LONG, la AS LONG, lb AS LONG, n AS LONG
  la = LEN(a) : lb = LEN(b) : n = 0 : p = 1
  DO WHILE p <= LEN(line)
    p = INSTR(p, line, a)
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(line, p - 1, 1)) = 0) AND (p + la > LEN(line) OR IsWord(MID$(line, p + la, 1)) = 0) THEN
      q = SkipSp(line, p + la)
      IF q + lb - 1 <= LEN(line) THEN
        IF MID$(line, q, lb) = b THEN
          IF q + lb > LEN(line) OR IsWord(MID$(line, q + lb, 1)) = 0 THEN n = n + 1
        END IF
      END IF
    END IF
    p = p + la
  LOOP
  RETURN n
END FUNCTION

FUNCTION CountSeq3(line AS STRING, a AS STRING, b AS STRING, c AS STRING) AS LONG
  DIM p AS LONG, q AS LONG, la AS LONG, lb AS LONG, lc AS LONG, n AS LONG
  la = LEN(a) : lb = LEN(b) : lc = LEN(c) : n = 0 : p = 1
  DO WHILE p <= LEN(line)
    p = INSTR(p, line, a)
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(line, p - 1, 1)) = 0) AND (p + la > LEN(line) OR IsWord(MID$(line, p + la, 1)) = 0) THEN
      q = SkipSp(line, p + la)
      IF q + lb - 1 <= LEN(line) AND MID$(line, q, lb) = b THEN
        IF q + lb > LEN(line) OR IsWord(MID$(line, q + lb, 1)) = 0 THEN
          q = SkipSp(line, q + lb)
          IF q + lc - 1 <= LEN(line) AND MID$(line, q, lc) = c THEN
            IF q + lc > LEN(line) OR IsWord(MID$(line, q + lc, 1)) = 0 THEN n = n + 1
          END IF
        END IF
      END IF
    END IF
    p = p + la
  LOOP
  RETURN n
END FUNCTION

FUNCTION InSet(w AS STRING, set AS STRING) AS INTEGER
  IF INSTR("|" + set + "|", "|" + w + "|") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION LlmVerb(v AS STRING) AS INTEGER
  RETURN InSet(v, "MODEL|FINETUNE|LORA|BENCHMARK|PREFILL|DECODE|CACHE|SERVER")
END FUNCTION

FUNCTION IsLlmFn(n AS STRING) AS INTEGER
  IF InSet(n, "STREAMSTATS|PREFILLSTATS|DECODESTATS|CACHESTATS|BENCHRESULT") <> 0 THEN RETURN -1
  IF LEFT$(n, 8) = "FINETUNE" THEN RETURN -1
  IF LEFT$(n, 5) = "MODEL" THEN RETURN -1
  RETURN 0
END FUNCTION

' conta AI <VERBO> con verbo [A-Z]{3,} NON nella superficie LLM/REGISTER/SET
FUNCTION CountAiOther(line AS STRING) AS LONG
  DIM p AS LONG, q AS LONG, st AS LONG, w AS STRING, n AS LONG, c AS STRING
  n = 0 : p = 1
  DO WHILE p <= LEN(line)
    p = INSTR(p, line, "AI")
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(line, p - 1, 1)) = 0) AND (p + 2 > LEN(line) OR IsWord(MID$(line, p + 2, 1)) = 0) THEN
      q = SkipSp(line, p + 2)
      IF q > p + 2 THEN
        st = q
        DO WHILE q <= LEN(line)
          c = MID$(line, q, 1)
          IF c >= "A" AND c <= "Z" THEN
            q = q + 1
          ELSE
            EXIT DO
          END IF
        LOOP
        w = MID$(line, st, q - st)
        IF LEN(w) >= 3 THEN
          IF LlmVerb(w) = 0 AND w <> "REGISTER" AND w <> "SET" THEN n = n + 1
        END IF
      END IF
    END IF
    p = p + 2
  LOOP
  RETURN n
END FUNCTION

' conta TUTTE le funzioni AI.<Lettera> (case-insensitive: U e' gia' maiuscolo)
FUNCTION CountAiFns(u AS STRING) AS LONG
  DIM p AS LONG, q AS LONG, st AS LONG, n AS LONG, c AS STRING
  n = 0 : p = 1
  DO WHILE p <= LEN(u)
    p = INSTR(p, u, "AI.")
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(u, p - 1, 1)) = 0) AND p + 3 <= LEN(u) THEN
      c = MID$(u, p + 3, 1)
      IF c >= "A" AND c <= "Z" THEN
        st = p + 3
        q = st
        DO WHILE q <= LEN(u)
          c = MID$(u, q, 1)
          IF (c >= "A" AND c <= "Z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
            q = q + 1
          ELSE
            EXIT DO
          END IF
        LOOP
        n = n + 1
      END IF
    END IF
    p = p + 3
  LOOP
  RETURN n
END FUNCTION

' conta le AI.<FN> della superficie LLM (nome in maiuscolo)
FUNCTION CountLlmFns(u AS STRING) AS LONG
  DIM p AS LONG, q AS LONG, st AS LONG, n AS LONG, c AS STRING, w AS STRING
  n = 0 : p = 1
  DO WHILE p <= LEN(u)
    p = INSTR(p, u, "AI.")
    IF p = 0 THEN EXIT DO
    IF (p = 1 OR IsWord(MID$(u, p - 1, 1)) = 0) AND p + 3 <= LEN(u) THEN
      c = MID$(u, p + 3, 1)
      IF c >= "A" AND c <= "Z" THEN
        st = p + 3
        q = st
        DO WHILE q <= LEN(u)
          c = MID$(u, q, 1)
          IF (c >= "A" AND c <= "Z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
            q = q + 1
          ELSE
            EXIT DO
          END IF
        LOOP
        w = MID$(u, st, q - st)
        IF IsLlmFn(w) <> 0 THEN n = n + 1
      END IF
    END IF
    p = p + 3
  LOOP
  RETURN n
END FUNCTION

FUNCTION HasImport(line AS STRING, ns AS STRING) AS INTEGER
  DIM p AS LONG
  p = 1
  DO WHILE p <= LEN(line) AND (MID$(line, p, 1) = " " OR MID$(line, p, 1) = CHR$(9)) : p = p + 1 : LOOP
  IF MID$(line, p, 6) <> "IMPORT" THEN RETURN 0
  IF p + 6 <= LEN(line) AND IsWord(MID$(line, p + 6, 1)) <> 0 THEN RETURN 0
  p = SkipSp(line, p + 6)
  IF MID$(line, p, LEN(ns)) <> ns THEN RETURN 0
  IF p + LEN(ns) <= LEN(line) AND IsWord(MID$(line, p + LEN(ns), 1)) <> 0 THEN RETURN 0
  RETURN -1
END FUNCTION

FUNCTION HasLocalHint(u AS STRING) AS INTEGER
  IF INSTR(u, "MLX") > 0 THEN RETURN -1
  IF INSTR(u, "GGUF") > 0 THEN RETURN -1
  IF INSTR(u, "OLLAMA") > 0 THEN RETURN -1
  IF INSTR(u, "LLAMACPP") > 0 THEN RETURN -1
  IF INSTR(u, "MOESTREAM") > 0 THEN RETURN -1
  IF INSTR(u, "/USERS/") > 0 AND INSTR(u, "/MODELS") > 0 THEN RETURN -1
  IF INSTR(u, "~/AI/MODELS") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB AddInvolved(path AS STRING, cmds AS LONG, fns AS LONG, rl AS LONG, rt AS LONG, sa AS LONG, ia AS LONG, il AS LONG, oth AS LONG, verdict AS LONG)
  IF g_n >= 5000 THEN EXIT SUB
  g_n = g_n + 1
  g_ip(g_n) = path : g_ic(g_n) = cmds : g_if(g_n) = fns : g_ir(g_n) = rl
  g_irt(g_n) = rt : g_isa(g_n) = sa : g_iia(g_n) = ia : g_iil(g_n) = il
  g_io(g_n) = oth : g_iv(g_n) = verdict
END SUB

SUB ScanFile(f AS STRING)
  DIM txt AS STRING, line AS STRING, u AS STRING
  DIM parts$() AS STRING
  DIM k AS LONG, cmds AS LONG, fns AS LONG, llmf AS LONG, allf AS LONG
  DIM rt AS LONG, sa AS LONG, rl AS LONG, rline AS LONG
  DIM ia AS LONG, il AS LONG, oth AS LONG, othercmd AS LONG
  DIM v AS LONG, i AS LONG
  txt = IO.READFILE(f)
  IF LEN(txt) = 0 THEN EXIT SUB
  parts$ = SPLIT(txt, CHR$(10))
  cmds = 0 : fns = 0 : llmf = 0 : allf = 0 : rt = 0 : sa = 0 : rl = 0
  ia = 0 : il = 0 : othercmd = 0
  FOR k = 0 TO UBOUND(parts$)
    line = parts$(k)
    u = UPPER(line)
    cmds = cmds + CountSeq(line, "AI", "MODEL") + CountSeq(line, "AI", "FINETUNE")
    cmds = cmds + CountSeq(line, "AI", "LORA") + CountSeq(line, "AI", "BENCHMARK")
    cmds = cmds + CountSeq(line, "AI", "PREFILL") + CountSeq(line, "AI", "DECODE")
    cmds = cmds + CountSeq(line, "AI", "CACHE") + CountSeq(line, "AI", "SERVER")
    rline = CountSeq(line, "AI", "REGISTER")
    rt = rt + rline
    sa = sa + CountSeq3(line, "AI", "SET", "ACTIVE")
    IF rline > 0 AND HasLocalHint(u) <> 0 THEN rl = rl + 1
    othercmd = othercmd + CountAiOther(line)
    allf = allf + CountAiFns(u)
    llmf = llmf + CountLlmFns(u)
    IF HasImport(line, "AI") <> 0 THEN ia = -1
    IF HasImport(line, "LLM") <> 0 THEN il = -1
  NEXT k
  fns = llmf
  oth = othercmd + allf - llmf
  IF oth < 0 THEN oth = 0
  IF cmds = 0 AND fns = 0 AND rl = 0 THEN EXIT SUB
  v = 1
  IF rl > 0 THEN
    v = 2
  ELSEIF oth = 0 THEN
    v = 0
  END IF
  AddInvolved(f, cmds, fns, rl, rt, sa, ia, il, oth, v)
END SUB

SUB SortInvolved()
  DIM i AS LONG, j AS LONG, p AS STRING
  DIM c AS LONG, f AS LONG, r AS LONG, rt AS LONG, sa AS LONG
  DIM ia AS LONG, il AS LONG, oth AS LONG, v AS LONG
  FOR i = 2 TO g_n
    p = g_ip(i) : c = g_ic(i) : f = g_if(i) : r = g_ir(i) : rt = g_irt(i)
    sa = g_isa(i) : ia = g_iia(i) : il = g_iil(i) : oth = g_io(i) : v = g_iv(i)
    j = i - 1
    DO WHILE j >= 1 AND g_ip(j) > p
      g_ip(j + 1) = g_ip(j) : g_ic(j + 1) = g_ic(j) : g_if(j + 1) = g_if(j)
      g_ir(j + 1) = g_ir(j) : g_irt(j + 1) = g_irt(j) : g_isa(j + 1) = g_isa(j)
      g_iia(j + 1) = g_iia(j) : g_iil(j + 1) = g_iil(j) : g_io(j + 1) = g_io(j) : g_iv(j + 1) = g_iv(j)
      j = j - 1
    LOOP
    g_ip(j + 1) = p : g_ic(j + 1) = c : g_if(j + 1) = f : g_ir(j + 1) = r : g_irt(j + 1) = rt
    g_isa(j + 1) = sa : g_iia(j + 1) = ia : g_iil(j + 1) = il : g_io(j + 1) = oth : g_iv(j + 1) = v
  NEXT i
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, arg AS STRING, f AS STRING, rel AS STRING, repo AS STRING
  DIM up AS LONG
  DIM quiet AS LONG, i AS LONG, nfiles AS LONG, ntot AS LONG
  DIM files$() AS STRING
  DIM c AS LONG, v AS LONG
  DIM iaS AS STRING, ilS AS STRING
  DIM cat(2) AS LONG
  repo = IO.CWD()
  FOR up = 1 TO 6
    IF DIREXISTS(repo + "/GMBasicSharp/common") THEN EXIT FOR
    repo = IO.PARENTDIR(repo)
  NEXT up
  root = repo + "/shared" : quiet = 0
  DO WHILE i < COMMANDCOUNT()
    i = i + 1
    arg = COMMAND$(i)
    IF arg = "--quiet" THEN
      quiet = -1
    ELSEIF arg = "--root" AND i < COMMANDCOUNT() THEN
      i = i + 1 : root = COMMAND$(i)
    ELSEIF arg = "--apply" THEN
      PRINT "--apply non supportato (hard move LLM completato)"
      RETURN 2
    END IF
  LOOP
  IF NOT DIREXISTS(root) THEN
    PRINT "ERRORE: root non trovato ("; root; ")"
    RETURN 2
  END IF
  files$ = GREP.FILES(" ", root)
  ntot = 0
  FOR i = 0 TO UBOUND(files$)
    f = files$(i)
    IF RIGHT$(UPPER(f), 4) = ".BAS" THEN
      IF INSTR(f, "/build/") = 0 AND INSTR(f, "/.git/") = 0 AND INSTR(f, "/node_modules/") = 0 AND INSTR(f, "/_deps/") = 0 THEN
        ntot = ntot + 1
        ScanFile(f)
      END IF
    END IF
  NEXT i
  SortInvolved()
  IF quiet <> 0 THEN RETURN 0
  PRINT "# Censimento namespace AI -> LLM  (root="; root; ")"
  PRINT "File .bas totali: "; ntot; " | coinvolti: "; g_n
  PRINT ""
  cat(0) = 0 : cat(1) = 0 : cat(2) = 0
  FOR i = 1 TO g_n
    v = g_iv(i)
    IF v >= 0 AND v <= 2 THEN cat(v) = cat(v) + 1
  NEXT i
  FOR c = 0 TO 2
    IF c = 0 THEN PRINT "## llm-only  ("; cat(c); ")"
    IF c = 1 THEN PRINT "## ai+llm  ("; cat(c); ")"
    IF c = 2 THEN PRINT "## local-register  ("; cat(c); ")"
    FOR i = 1 TO g_n
      IF g_iv(i) = c THEN
        rel = g_ip(i)
        IF LEFT$(rel, LEN(root) + 1) = root + "/" THEN rel = MID$(rel, LEN(root) + 2)
        iaS = "False" : ilS = "False"
        IF g_iia(i) <> 0 THEN iaS = "True"
        IF g_iil(i) <> 0 THEN ilS = "True"
        PRINT "  "; rel; "  [cmds="; g_ic(i); " fns="; g_if(i); " reg_local="; g_ir(i); "/"; g_irt(i); " setactive="; g_isa(i); " import_ai="; iaS; " import_llm="; ilS; " other_ai="; g_io(i); "]"
      END IF
    NEXT i
    PRINT ""
  NEXT c
  PRINT "ai-only (NON si toccano): "; ntot - g_n
  RETURN 0
END FUNCTION
