' make_lora_dataset.bas — dataset di fine-tune LoRA (binario) dal corpus di documentazione.
'
' Port BASIC di `samples/Fixtures/make_lora_dataset.py` (repo GMBasic, deprecato).
' Niente Python e niente `tokenizers`: gli id vengono da `LLM.TOKENS(model, testo)`
' (superficie BASIC del tokenizer, 4 backend). Formato identico al .py (little-endian int32):
'
'   <out>.train.bin : int32 n_sample | per sample: int32 len, int32 ids[len], int32 target
'   <out>.hold.bin  : idem, sul 10% finale del corpus (holdout)
'
' Modalita':
'   rule (default) : classe A = finestra con keyword di dominio negli ULTIMI 4 token -> target "vault"
'                    classe B = finestra senza keyword in tutta la finestra     -> target ":"
'   docs           : target = token successivo alla finestra (dato "reale" dal corpus)
'
' Uso:
'   tools/basic/make_lora_dataset.bas <model_dir> <out> [rule|docs] [T] [nA] [nB] [seed] [--repo DIR]
'     model_dir : cartella HF o file tokenizer.json (relativo = dalla radice del monorepo)
'     out       : prefisso dei due file .bin
'     T         : lunghezza finestra (default 8)
'     nA, nB    : quanti sample di classe A e B (default 300 e 60)
'     seed      : seme del PRNG (default 20260909)
'     --repo    : radice del corpus (default: due livelli sopra la CWD, cioè la radice del monorepo)
'
' Differenze dichiarate rispetto al .py:
'   1) PRNG: LCG deterministico (il .py usa Mersenne Twister) → stesso seme dà lo stesso dataset,
'      ma non byte-identico a Python per costruzione;
'   2) keyword e target sono RICAVATI dal tokenizer (il .py li aveva hardcoded per un modello):
'      funziona con qualunque tokenizer, fixture compresa;
'   3) il corpus è parametrico (`--repo`).
'
' Note di implementazione:
'   - `ReadText` usa `ON ERROR GOTO` con etichetta dentro la FUNCTION (idioma canonico di
'     lettura file): fino al 2026-10-30 rompeva il body su IL (Bugs/(solved) - IL-OnError-…),
'     oggi funziona e resta coperto dal MiniTest constructs/on_error_label_in_function.bas;
'   - `ParseIdsInto` riceve l'array GLOBALE come parametro `a() AS LONG`: fino al 2026-10-30
'     dava NullReferenceException solo su IL (Bugs/(solved) - IL-Array-Globale-…), oggi
'     funziona ed è coperto da constructs/array_global_param.bas;
'   - la scrittura del .bin è incrementale (un int32 per volta): `PUT #` scrive TUTTI gli
'     elementi dell'array, quindi un buffer grande verrebbe scritto per intero a ogni PUT.
IMPORT LLM
IMPORT IO

DIM g_ids(1000000) AS LONG      ' id del corpus
DIM g_tok AS LONG
DIM g_seed AS DOUBLE
DIM g_cand(500000) AS LONG      ' posizioni candidate (mescolate)
DIM g_start(500000) AS LONG     ' posizioni selezionate (A poi B)
DIM g_tgt(500000) AS LONG
DIM g_used(1000000) AS BYTE     ' stride >= 3
DIM g_i32(3) AS BYTE             ' int32 di scrittura (4 byte)
DIM g_cap AS LONG
DIM c_namespace AS LONG, c_runtime AS LONG, c_vault AS LONG, c_colon AS LONG

' ── PRNG deterministico (LCG), indipendente dal runtime ───────────────────────────────────
SUB SeedRnd(n AS DOUBLE)
  g_seed = n
  IF g_seed < 1 THEN g_seed = 1
END SUB
FUNCTION RandNext() AS DOUBLE
  g_seed = (1664525.0 * g_seed + 1013904223.0) MOD 2147483647.0
  RandNext = g_seed / 2147483647.0
END FUNCTION
FUNCTION RandBelow(n AS LONG) AS LONG
  DIM v AS LONG
  v = INT(RandNext() * n)
  IF v >= n THEN v = n - 1
  IF v < 0 THEN v = 0
  RandBelow = v
END FUNCTION

' ── IO ────────────────────────────────────────────────────────────────────────────────────
FUNCTION ReadText(p AS STRING) AS STRING
  DIM s AS STRING, ln AS STRING
  s = ""
  ON ERROR GOTO NoFile
  OPEN p FOR INPUT AS #7
  DO WHILE NOT EOF(7)
    LINE INPUT #7, ln
    IF s = "" THEN
      s = ln
    ELSE
      s = s + CHR$(10) + ln
    END IF
  LOOP
  CLOSE #7
NoFile:
  ReadText = s
END FUNCTION

' int32 little-endian: riempie g_i32 (4 byte) e lo scrive sul file 9 (posizione corrente)
SUB PutI32Stream(v AS LONG)
  DIM u AS DOUBLE
  u = v
  IF u < 0 THEN u = u + 4294967296.0
  g_i32(0) = INT(u MOD 256)
  g_i32(1) = INT((u / 256.0) MOD 256)
  g_i32(2) = INT((u / 65536.0) MOD 256)
  g_i32(3) = INT((u / 16777216.0) MOD 256)
  PUT #9, , g_i32()
END SUB

' come sopra, ma su una posizione esplicita (1-based): serve a riscrivere l header
SUB PutI32At(pos AS LONG, v AS LONG)
  DIM u AS DOUBLE
  u = v
  IF u < 0 THEN u = u + 4294967296.0
  g_i32(0) = INT(u MOD 256)
  g_i32(1) = INT((u / 256.0) MOD 256)
  g_i32(2) = INT((u / 65536.0) MOD 256)
  g_i32(3) = INT((u / 16777216.0) MOD 256)
  PUT #9, pos, g_i32()
END SUB

' "74,71,78" → array passato (a()); ritorna quanti (cap = dimensione dell'array)
FUNCTION ParseIdsInto(s AS STRING, a() AS LONG, cap AS LONG) AS LONG
  DIM i AS LONG, n AS LONG, d AS LONG, started AS INTEGER
  DIM v AS DOUBLE
  n = 0 : v = 0 : started = 0
  FOR i = 1 TO LEN(s)
    d = ASC(MID$(s, i, 1))
    IF d >= 48 AND d <= 57 THEN
      v = v * 10 + (d - 48) : started = 1
    ELSE
      IF started = 1 THEN
        IF n < cap THEN a(n) = v
        n = n + 1 : v = 0 : started = 0
        IF n >= cap THEN EXIT FOR
      END IF
    END IF
  NEXT i
  IF started = 1 AND n < cap THEN
    a(n) = v : n = n + 1
  END IF
  ParseIdsInto = n
END FUNCTION

' primo numero di una stringa CSV ("74,71" → 74); -1 se non c'è
FUNCTION FirstId(s AS STRING) AS LONG
  DIM i AS LONG, d AS LONG, started AS INTEGER
  DIM v AS DOUBLE
  v = 0 : started = 0
  FOR i = 1 TO LEN(s)
    d = ASC(MID$(s, i, 1))
    IF d >= 48 AND d <= 57 THEN
      v = v * 10 + (d - 48) : started = 1
    ELSE
      IF started = 1 THEN RETURN v
    END IF
  NEXT i
  IF started = 1 THEN RETURN v
  RETURN -1
END FUNCTION

' keyword/target ricavati dal tokenizer del modello
FUNCTION IdOfWord(model AS STRING, parola AS STRING) AS LONG
  DIM s AS STRING
  s = LLM.TOKENS(model, parola)
  RETURN FirstId(s)
END FUNCTION

' keyword negli ULTIMI 4 token della finestra (regola della classe A)
FUNCTION HasKwTail(s AS LONG) AS INTEGER
  DIM i AS LONG, ini AS LONG
  ini = s + g_cap - 4
  IF ini < s THEN ini = s
  FOR i = ini TO s + g_cap - 1
    IF g_ids(i) = c_namespace OR g_ids(i) = c_runtime THEN RETURN 1
  NEXT i
  RETURN 0
END FUNCTION

' keyword ovunque nella finestra (esclusione della classe B)
FUNCTION HasKwAny(s AS LONG) AS INTEGER
  DIM i AS LONG
  FOR i = s TO s + g_cap - 1
    IF g_ids(i) = c_namespace OR g_ids(i) = c_runtime THEN RETURN 1
  NEXT i
  RETURN 0
END FUNCTION

' Fisher-Yates deterministico su g_cand
SUB ShuffleCand(n AS LONG)
  DIM i AS LONG, j AS LONG, t AS LONG
  IF n < 2 THEN EXIT SUB
  FOR i = n - 1 TO 1 STEP -1
    j = RandBelow(i + 1)
    t = g_cand(i) : g_cand(i) = g_cand(j) : g_cand(j) = t
  NEXT i
END SUB

' raccoglie le posizioni candidate della classe (0=A, 1=B, 2=docs) in g_cand
FUNCTION Collect(fromIdx AS LONG, toIdx AS LONG, classe AS LONG, mode AS STRING) AS LONG
  DIM i AS LONG, n AS LONG
  n = 0
  FOR i = fromIdx TO toIdx - g_cap - 1
    IF mode = "docs" THEN
      g_cand(n) = i : n = n + 1
    ELSEIF classe = 0 THEN
      IF HasKwTail(i) = 1 THEN g_cand(n) = i : n = n + 1
    ELSE
      IF HasKwAny(i) = 0 THEN g_cand(n) = i : n = n + 1
    END IF
    IF n >= 500000 THEN EXIT FOR
  NEXT i
  Collect = n
END FUNCTION

' seleziona fino a `want` posizioni con stride minimo 3 (|s-u| >= 3), come il .py
FUNCTION SelectCand(fromIdx AS LONG, nCand AS LONG, want AS LONG, mode AS STRING, classe AS LONG, _
                    nskip AS LONG) AS LONG
  DIM k AS LONG, s AS LONG, c AS LONG, n AS LONG, ok AS INTEGER
  n = nskip
  FOR k = 0 TO nCand - 1
    IF n - nskip >= want THEN EXIT FOR
    s = g_cand(k)
    ok = 1
    FOR c = 0 TO 2
      IF s - fromIdx + c < g_tok AND g_used(s - fromIdx + c) <> 0 THEN ok = 0
      IF s - fromIdx - c >= 0 AND g_used(s - fromIdx - c) <> 0 THEN ok = 0
    NEXT c
    IF ok = 1 THEN
      g_start(n) = s
      IF mode = "docs" THEN
        g_tgt(n) = g_ids(s + g_cap)
      ELSEIF classe = 0 THEN
        g_tgt(n) = c_vault
      ELSE
        g_tgt(n) = c_colon
      END IF
      FOR c = 0 TO 2
        IF s - fromIdx + c < g_tok THEN g_used(s - fromIdx + c) = 1
      NEXT c
      n = n + 1
    END IF
  NEXT k
  SelectCand = n
END FUNCTION

' scrive n sample di g_start/g_tgt nel file: int32 per volta (header riscritto alla fine).
' NIENTE buffer grande: PUT di un array scrive TUTTI i suoi elementi, quindi si usa un int32/volta.
FUNCTION WriteBin(path AS STRING, n AS LONG) AS LONG
  DIM i AS LONG, s AS LONG
  ' il file va TRONCATO: OPEN FOR BINARY non tronca (resterebbe la coda di un run precedente)
  IF FILEEXISTS(path) THEN KILL path
  OPEN path FOR BINARY AS #9
  PutI32Stream(0)                       ' segnaposto: conteggio vero scritto alla fine
  FOR i = 0 TO n - 1
    PutI32Stream(g_cap)
    FOR s = 0 TO g_cap - 1
      PutI32Stream(g_ids(g_start(i) + s))
    NEXT s
    PutI32Stream(g_tgt(i))
  NEXT i
  PutI32At(1, n)                        ' riscrive il conteggio in testa (pos 1-based)
  CLOSE #9
  WriteBin = n
END FUNCTION

SUB Main()
  DIM model AS STRING, out AS STRING, mode AS STRING, repo AS STRING, dumpFile AS STRING
  DIM wantA AS LONG, wantB AS LONG, i AS LONG, k AS LONG, pos AS LONG
  DIM corpus AS STRING, chunk AS STRING, csv AS STRING
  DIM nFiles AS LONG, nCand AS LONG, selA AS LONG, selB AS LONG, split AS LONG

  model = "" : out = "work/lora" : mode = "rule" : repo = ""
  g_cap = 8 : wantA = 300 : wantB = 60 : SeedRnd(20260909)

  pos = 0
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    DIM a AS STRING
    a = COMMAND$(i)
    IF a = "--repo" AND i < COMMANDCOUNT() THEN
      i = i + 1 : repo = COMMAND$(i)
    ELSEIF a = "--dump-ids" AND i < COMMANDCOUNT() THEN
      i = i + 1 : dumpFile = COMMAND$(i)
    ELSE
      pos = pos + 1
      IF pos = 1 THEN model = a
      IF pos = 2 THEN out = a
      IF pos = 3 THEN mode = a
      IF pos = 4 THEN g_cap = VAL(a)
      IF pos = 5 THEN wantA = VAL(a)
      IF pos = 6 THEN wantB = VAL(a)
      IF pos = 7 THEN SeedRnd(VAL(a))
    END IF
    i = i + 1
  LOOP
  IF LEN(model) = 0 THEN
    PRINT "uso: make_lora_dataset.bas <model_dir> <out> [rule|docs] [T] [nA] [nB] [seed] [--repo DIR]"
    RETURN
  END IF
  IF g_cap < 2 OR g_cap > 64 THEN g_cap = 8
  ' CWD = <root>/tools/basic (il runner ci si posiziona) → radice due livelli sopra
  IF LEN(repo) = 0 THEN repo = IO.PARENTDIR(IO.PARENTDIR(IO.CWD()))
  IF LEFT$(model, 1) <> "/" THEN model = repo + "/" + model
  IF NOT FILEEXISTS(model + "/tokenizer.json") AND NOT FILEEXISTS(model) THEN
    PRINT "[ds] tokenizer non trovato: "; model
    RETURN
  END IF

  ' 1) corpus: README/AGENTS + docs (implemented/audit, backend-dispatch, comparison)
  corpus = ReadText(repo + "/README.md")
  chunk = ReadText(repo + "/AGENTS.md")
  IF LEN(chunk) > 0 THEN
    corpus = corpus + CHR$(10) + CHR$(10) + chunk
    nFiles = 1
  END IF
  DIM dirs(3) AS STRING
  dirs(0) = "/docs/implemented/audit"
  dirs(1) = "/docs/implemented/audit/backend-dispatch"
  dirs(2) = "/docs/comparison"
  FOR k = 0 TO 2
    DIM files$ = IO.DIR(repo + dirs(k) + "/*.md")
    FOR i = 0 TO UBOUND(files$)
      IF LEN(files$(i)) > 0 AND nFiles < 60 THEN
        chunk = ReadText(files$(i))
        IF LEN(chunk) > 0 THEN
          corpus = corpus + CHR$(10) + CHR$(10) + chunk
          nFiles = nFiles + 1
        END IF
      END IF
    NEXT i
  NEXT k
  IF LEN(corpus) = 0 THEN
    PRINT "[ds] corpus vuoto in "; repo
    RETURN
  END IF
  PRINT "[ds] file="; nFiles; " caratteri="; LEN(corpus)

  ' 2) tokenizzazione (fonte unica: LLM.TOKENS)
  csv = LLM.TOKENS(model, corpus)
  g_tok = ParseIdsInto(csv, g_ids(), 1000000)
  PRINT "[ds] tokens="; g_tok
  IF g_tok < g_cap + 2 THEN
    PRINT "[ds] corpus troppo piccolo per finestre di "; g_cap; " token"
    RETURN
  END IF

  IF LEN(dumpFile) > 0 THEN
    IF FILEEXISTS(dumpFile) THEN KILL dumpFile
    OPEN dumpFile FOR OUTPUT AS #8
    FOR i = 0 TO g_tok - 1
      PRINT #8, LTRIM$(STR$(g_ids(i)))
    NEXT i
    CLOSE #8
    PRINT "[ds] ids esportati in "; dumpFile
  END IF

  ' 3) keyword/target dal tokenizer
  c_namespace = IdOfWord(model, "NAMESPACE")
  c_runtime = IdOfWord(model, "runtime")
  c_vault = IdOfWord(model, "vault")
  c_colon = IdOfWord(model, ":")
  PRINT "[ds] kw NAMESPACE="; c_namespace; " runtime="; c_runtime; " target vault="; c_vault; " ':'="; c_colon

  ' 4) train (primi 90%) e hold (ultimi 10%)
  split = INT(g_tok * 0.9)
  PRINT "[ds] "; out; ".train.bin sample="; BuildPart(out + ".train.bin", 0, split, wantA, wantB, mode)
  PRINT "[ds] "; out; ".hold.bin sample="; BuildPart(out + ".hold.bin", split, g_tok, INT(wantA / 10), INT(wantB / 10), mode)
END SUB

' costruisce una partizione: raccoglie, mescola, seleziona (A poi B) e scrive
FUNCTION BuildPart(path AS STRING, fromIdx AS LONG, toIdx AS LONG, wantA AS LONG, wantB AS LONG, _
                   mode AS STRING) AS LONG
  DIM i AS LONG, n AS LONG, selA AS LONG, selB AS LONG
  FOR i = 0 TO toIdx - fromIdx : g_used(i) = 0 : NEXT i

  IF mode = "docs" THEN
    n = Collect(fromIdx, toIdx, 2, mode)
    ShuffleCand(n)
    n = SelectCand(fromIdx, n, 500000, mode, 2, 0)
    BuildPart = WriteBin(path, n)
    RETURN BuildPart
  END IF

  ' SelectCand ritorna l INDICE FINALE (assoluto) nell array, non il conteggio del giro:
  ' A parte da 0, B riparte da dove A si è fermato (lo stride tiene conto di A).
  n = Collect(fromIdx, toIdx, 0, mode)
  ShuffleCand(n)
  selA = SelectCand(fromIdx, n, wantA, mode, 0, 0)
  n = Collect(fromIdx, toIdx, 1, mode)
  ShuffleCand(n)
  selB = SelectCand(fromIdx, n, wantB, mode, 1, selA)
  BuildPart = WriteBin(path, selB)
END FUNCTION
