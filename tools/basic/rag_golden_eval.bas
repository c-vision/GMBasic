' rag_golden_eval.bas — valutazione del retrieval RAG con golden set (port BASIC).
'
' Port di tools/eval_rag_golden.py (repo GMBasic, deprecato). Per ogni query (titolo di un
' documento) verifica se il retrieval restituisce in top-k un chunk del DOCUMENTO ATTESO.
' Metriche: Precision@1/3/5 e MRR. Un hit e' "corretto" se il suo SourceId contiene il basename
' del documento atteso (primi 20 caratteri) oppure i primi 30 caratteri del path atteso:
' esattamente la regola del .py. Il confronto e' CASE-SENSITIVE (INSTR del linguaggio ignora il
' caso), quindi si usa HasCS.
'
' Uso:
'   tools/basic/rag_golden_eval.bas [--golden FILE] [--rag DB] [--name COLL] [--topk K]
'                                   [--max N] [--detail FILE] [--quiet]
'     --golden  file JSON: [[query, documento_atteso], ...]   (default /tmp/golden_set.json)
'     --rag     database RAG (dir o ":memory:")               (default ":memory:")
'     --name    nome della collezione                          (default "golden")
'     --topk    quanti hit chiedere                            (default 5)
'     --max     0 = tutte le query                             (default 0)
'     --detail  file di dettaglio (JSON)                       (default /tmp/golden_eval_detail.json)
'     --embedding NOME  provider di embedding da attivare (es. "fixture-embedding" nei test);
'
'                       se assente si usa quello configurato
'
'     --quiet   non stampa l'avanzamento ogni 20 query
'
' Exit: 0 valutazione eseguita (anche con miss), 2 uso/IO/JSON errati, 3 se il retrieval
' risponde con errore su TUTTE le query (nessuna misura possibile).
'
' Equivalenza col .py: stesse metriche, stessa regola di match, stesso file di dettaglio.
' Unica differenza: il .py invocava la CLI `dotnet run --project REPL -- rag query`; qui si usa
' direttamente la superficie BASIC `AI.SEARCH(db, coll, query, topk, useEmbedding)`.
IMPORT AI
IMPORT JSON
IMPORT IO

DIM g_q(20000) AS STRING
DIM g_a(20000) AS STRING

' ── IO: lettura/scrittura byte-esatta (idioma di tools/basic/gen_c_tokenkind.bas) ──────────
FUNCTION ReadAll(p AS STRING) AS STRING
  DIM s AS STRING, ln AS STRING
  s = ""
  OPEN p FOR INPUT AS #1
  DO WHILE NOT EOF(1)
    LINE INPUT #1, ln
    IF s = "" THEN
      s = ln
    ELSE
      s = s + CHR$(10) + ln
    END IF
  LOOP
  CLOSE #1
  ReadAll = s
END FUNCTION

FUNCTION WriteAll(p AS STRING, s AS STRING) AS INTEGER
  DIM h AS LONG, w AS LONG
  h = IO.FCreate(p)
  IF h <= 0 THEN
    WriteAll = 0
  ELSE
    w = IO.FWrite(h, s)
    w = IO.FClose(h)
    WriteAll = 1
  END IF
END FUNCTION

FUNCTION BaseName(p AS STRING) AS STRING
  DIM i AS LONG, c AS LONG
  c = 0
  FOR i = 1 TO LEN(p)
    IF MID$(p, i, 1) = "/" THEN c = i
  NEXT i
  IF c = 0 THEN RETURN p
  RETURN MID$(p, c + 1, LEN(p) - c)
END FUNCTION

' contains case-SENSITIVE
FUNCTION HasCS(hay AS STRING, needle AS STRING) AS INTEGER
  DIM i AS LONG, j AS LONG, ok AS INTEGER
  IF LEN(needle) = 0 THEN RETURN 0
  IF LEN(needle) > LEN(hay) THEN RETURN 0
  FOR i = 1 TO LEN(hay) - LEN(needle) + 1
    ok = 1
    FOR j = 1 TO LEN(needle)
      IF MID$(hay, i + j - 1, 1) <> MID$(needle, j, 1) THEN
        ok = 0
        EXIT FOR
      END IF
    NEXT j
    IF ok = 1 THEN RETURN 1
  NEXT i
  RETURN 0
END FUNCTION

' percentuale con 1 decimale, da aritmetica intera (niente FORMAT$ nel linguaggio)
FUNCTION Pct1(num AS LONG, den AS LONG) AS STRING
  DIM x AS LONG
  IF den = 0 THEN RETURN "n/d"
  x = INT((num * 1000.0 + den / 2.0) / den)
  RETURN LTRIM$(STR$(INT(x / 10))) + "," + LTRIM$(STR$(x - INT(x / 10) * 10)) + "%"
END FUNCTION

' valore con 3 decimali
FUNCTION F3(v AS DOUBLE, den AS LONG) AS STRING
  DIM x AS LONG
  IF den = 0 THEN RETURN "n/d"
  x = INT((v * 1000.0 / den) + 0.5)
  RETURN LTRIM$(STR$(INT(x / 1000))) + "," + RIGHT$("00" + LTRIM$(STR$(x - INT(x / 1000) * 1000)), 3)
END FUNCTION

SUB Main()
  DIM goldenFile AS STRING, ragDb AS STRING, coll AS STRING, detailFile AS STRING
  DIM embProv AS STRING
  DIM topk AS LONG, maxN AS LONG, i AS LONG, n AS LONG, k AS LONG
  DIM errors AS LONG, found AS LONG, rankFound AS LONG
  DIM h1 AS LONG, h3 AS LONG, h5 AS LONG, miss AS LONG
  DIM txt AS STRING, q AS STRING, atteso AS STRING, key1 AS STRING, key2 AS STRING
  DIM hits AS STRING, src AS STRING, det AS STRING
  DIM g AS JSON, hp AS JSON
  DIM rr AS DOUBLE
  DIM quiet AS INTEGER

  goldenFile = "/tmp/golden_set.json"
  ragDb = ":memory:"
  coll = "golden"
  topk = 5
  maxN = 0
  detailFile = "/tmp/golden_eval_detail.json"
  quiet = 0

  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--golden" AND i < COMMANDCOUNT() THEN i = i + 1 : goldenFile = COMMAND$(i)
    IF COMMAND$(i) = "--rag" AND i < COMMANDCOUNT() THEN i = i + 1 : ragDb = COMMAND$(i)
    IF COMMAND$(i) = "--name" AND i < COMMANDCOUNT() THEN i = i + 1 : coll = COMMAND$(i)
    IF COMMAND$(i) = "--topk" AND i < COMMANDCOUNT() THEN i = i + 1 : topk = VAL(COMMAND$(i))
    IF COMMAND$(i) = "--max" AND i < COMMANDCOUNT() THEN i = i + 1 : maxN = VAL(COMMAND$(i))
    IF COMMAND$(i) = "--detail" AND i < COMMANDCOUNT() THEN i = i + 1 : detailFile = COMMAND$(i)
    IF COMMAND$(i) = "--embedding" AND i < COMMANDCOUNT() THEN i = i + 1 : embProv = COMMAND$(i)
    IF COMMAND$(i) = "--quiet" THEN quiet = 1
    i = i + 1
  LOOP

  IF topk < 1 THEN topk = 5
  IF LEN(embProv) > 0 THEN AI SET ACTIVE %EMBEDDING, embProv
  IF NOT FILEEXISTS(goldenFile) THEN
    PRINT "golden set non trovato: "; goldenFile
    PRINT "uso: rag_golden_eval.bas [--golden FILE] [--rag DB] [--name COLL] [--topk K] [--max N]"
    RETURN
  END IF

  txt = ReadAll(goldenFile)
  g = JSON.PARSE(txt)

  n = 0
  DO WHILE n < 20000
    q = JSON.EXTRACT(g, "[" + LTRIM$(MID$(STR$(n), 1)) + "][0]")
    IF LEN(q) = 0 THEN EXIT DO
    g_q(n) = q
    g_a(n) = JSON.EXTRACT(g, "[" + LTRIM$(MID$(STR$(n), 1)) + "][1]")
    n = n + 1
  LOOP
  IF maxN > 0 AND n > maxN THEN n = maxN
  IF n = 0 THEN
    PRINT "golden set vuoto o malformato (atteso [[query, documento], ...]): "; goldenFile
    RETURN
  END IF

  PRINT "== rag_golden_eval: "; n; " query (top-k="; topk; ", db="; ragDb; ", coll="; coll; ")"

  h1 = 0 : h3 = 0 : h5 = 0 : rr = 0.0 : miss = 0 : errors = 0
  det = "["

  FOR i = 0 TO n - 1
    q = g_q(i)
    atteso = g_a(i)
    key1 = LEFT$(BaseName(atteso), 20)
    key2 = LEFT$(atteso, 30)
    found = -1 : rankFound = 0

    hits = AI.SEARCH(ragDb, coll, q, topk, 1)
    IF INSTR(hits, "Error") > 0 AND INSTR(hits, "hits") = 0 THEN
      errors = errors + 1
    ELSE
      hp = JSON.PARSE(hits)
      FOR k = 0 TO topk - 1
        src = JSON.EXTRACT(hp, "hits[" + LTRIM$(MID$(STR$(k), 1)) + "].sourceId")
        IF LEN(src) = 0 THEN src = JSON.EXTRACT(hp, "hits[" + LTRIM$(MID$(STR$(k), 1)) + "].SourceId")
        IF LEN(src) = 0 THEN EXIT FOR
        IF HasCS(src, key1) = 1 THEN
          found = k : rankFound = k + 1
          EXIT FOR
        END IF
        IF LEN(key2) > 0 AND HasCS(src, key2) = 1 THEN
          found = k : rankFound = k + 1
          EXIT FOR
        END IF
      NEXT k
    END IF

    IF found = 0 THEN h1 = h1 + 1
    IF found >= 0 AND found < 3 THEN h3 = h3 + 1
    IF found >= 0 AND found < 5 THEN h5 = h5 + 1
    IF found >= 0 THEN
      rr = rr + 1.0 / (found + 1)
    ELSE
      miss = miss + 1
    END IF

    IF i > 0 THEN det = det + ","
    det = det + "[" + JSON.STRINGIFY(q) + "," + JSON.STRINGIFY(atteso) + "," + _
          JSON.STRINGIFY(IIF(found >= 0, "HIT", "MISS")) + "," + LTRIM$(MID$(STR$(rankFound), 1)) + "]"

    IF quiet = 0 AND (i + 1) MOD 20 = 0 THEN PRINT "  "; i + 1; "/"; n; " ..."
  NEXT i
  det = det + "]"

  PRINT ""
  PRINT "=== Risultati golden set: "; n; " query (top-k="; topk; ") ==="
  PRINT "Precision@1: "; h1; "/"; n; " = "; Pct1(h1, n)
  PRINT "Precision@3: "; h3; "/"; n; " = "; Pct1(h3, n)
  PRINT "Precision@5: "; h5; "/"; n; " = "; Pct1(h5, n)
  PRINT "MRR: "; F3(rr, n)
  PRINT "errori di esecuzione: "; errors
  PRINT "miss: "; miss

  IF WriteAll(detailFile, det) = 1 THEN PRINT "dettaglio: "; detailFile

  IF errors = n THEN PRINT "NESSUN retrieval riuscito: il RAG non risponde (db vuoto o provider non attivo)."
END SUB

