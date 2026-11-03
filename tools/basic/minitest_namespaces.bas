' minitest_namespaces.bas — CENSIMENTO dei MiniTest PER NAMESPACE.
'
' Modello (deciso dall'utente): un MiniTest per namespace che copre TUTTI i
' comandi/funzioni/costanti del namespace, più la struttura base del linguaggio.
' Scopo = verificare che i 4 backend supportino la STESSA versione della superficie,
' NON un'applicazione. File canonico:
'
'     shared/minitests/<ns-minuscolo>/<ns-minuscolo>_all.bas
'     es. BITS   -> shared/minitests/bits/bits_all.bas
'         SPEED  -> shared/minitests/speed/speed_all.bas
'         TRADING.PORTFOLIO -> shared/minitests/trading.portfolio/trading_portfolio_all.bas
'         (la directory mantiene il punto; il nome FILE usa '_' al posto dei '.')
'
' La struttura base del linguaggio vive in shared/minitests/constructs/base.bas.
'
' La copertura è misurata SOLO sul file canonico del namespace (non sull'intero
' corpus): un nome che compare in un MiniTest applicativo di un altro dominio non
' conta. Così il censimento dice davvero quali namespace hanno il loro test.
'
' Uso (dalla root del monorepo):
'   tools/basic/minitest_namespaces.bas                 elenco + riepilogo
'   tools/basic/minitest_namespaces.bas --missing       elenca i nomi mancanti
'   tools/basic/minitest_namespaces.bas --ns SPEED      solo un namespace
'   tools/basic/minitest_namespaces.bas --strict        exit 1 se manca un file,
'                                                       se la superficie non è coperta
'                                                       o se manca constructs/base.bas
'
' Nota: i COMANDI (kind=command in surface.csv) hanno ns vuoto nel registro e non
' sono attribuibili a un namespace. Molte procedure ARRAYS/DBF/SPEED hanno ANCHE una
' forma-comando (ACOPY a, b / ADEL a, i): la copertura conta quindi una procedura sia
' come chiamata con "(" sia come parola nel codice (la forma-comando non ha parentesi).
IMPORT IO

DIM g_kind(4000) AS STRING
DIM g_nsv(4000) AS STRING
DIM g_name(4000) AS STRING
DIM g_n AS INTEGER
DIM g_ns(200) AS STRING
DIM g_ns_n AS INTEGER
DIM g_calls AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_words AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_missing AS INTEGER
DIM g_strict AS INTEGER
DIM g_only AS STRING

FUNCTION IsIdStart(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsIdChar(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

SUB NsAdd(ns AS STRING)
  DIM j AS INTEGER
  FOR j = 1 TO g_ns_n
    IF g_ns(j) = ns THEN EXIT SUB
  NEXT j
  IF g_ns_n >= 200 THEN EXIT SUB
  g_ns_n = g_ns_n + 1
  g_ns(g_ns_n) = ns
END SUB

' Path canonico del MiniTest di un namespace ("" se non calcolabile).
FUNCTION NsFile(root AS STRING, ns AS STRING) AS STRING
  DIM d AS STRING
  DIM f AS STRING
  d = LOWER(ns)
  ' Il nome FILE non può contenere '.', altrimenti il runner IL deriva l'assembly
  ' dal primo segmento (trading.bonds_all -> trading.dll) e non lo trova.
  f = REPLACE(d, ".", "_")
  RETURN root + "/shared/minitests/" + d + "/" + f + "_all.bas"
END FUNCTION

' Scansiona UN file raccogliendo chiamate (nome seguito da "(") e parole.
' I commenti ' e le stringhe sono esclusi: un nome citato in un commento non conta.
SUB ScanFile(path AS STRING)
  DIM s AS STRING, c AS STRING, w AS STRING
  DIM i AS LONG, st AS LONG
  DIM inStr AS INTEGER, inComment AS INTEGER
  s = IO.READFILE(path)
  IF LEN(s) = 0 THEN EXIT SUB
  i = 1 : inStr = 0 : inComment = 0
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF inComment THEN
      IF c = CHR$(10) THEN inComment = 0
      i = i + 1
    ELSEIF inStr THEN
      IF c = CHR$(34) THEN inStr = 0
      i = i + 1
    ELSEIF c = CHR$(39) THEN
      inComment = 1 : i = i + 1
    ELSEIF c = CHR$(34) THEN
      inStr = 1 : i = i + 1
    ELSEIF IsIdStart(c) THEN
      st = i
      DO WHILE i <= LEN(s)
        c = MID$(s, i, 1)
        IF IsIdChar(c) THEN i = i + 1 ELSE EXIT DO
      LOOP
      w = UPPER(MID$(s, st, i - st))
      IF NOT (w IN g_words) THEN g_words(w) = 1
      IF i <= LEN(s) THEN
        IF MID$(s, i, 1) = "(" THEN
          IF NOT (w IN g_calls) THEN g_calls(w) = 1
        END IF
      END IF
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

' Copertura del namespace: ritorna -1 se il file esiste, 0 altrimenti.
' g_tot/g_cov sono variabili globali (li usa anche il chiamante per i totali).
DIM g_tot AS LONG
DIM g_cov AS LONG

FUNCTION NsCover(root AS STRING, ns AS STRING) AS INTEGER
  DIM f AS STRING
  DIM j AS INTEGER
  f = NsFile(root, ns)
  g_tot = 0 : g_cov = 0
  FOR j = 1 TO g_n
    IF g_nsv(j) = ns THEN g_tot = g_tot + 1
  NEXT j
  IF NOT FILEEXISTS(f) THEN RETURN 0
  ' Dizionari freschi: la copertura è solo del file canonico.
  g_calls = NEW DICT OF STRING, INTEGER
  g_words = NEW DICT OF STRING, INTEGER
  ScanFile(f)
  FOR j = 1 TO g_n
    IF g_nsv(j) = ns THEN
      IF g_kind(j) = "procedure" THEN
        IF (g_name(j) IN g_calls) OR (g_name(j) IN g_words) THEN g_cov = g_cov + 1
      ELSE
        IF g_name(j) IN g_words THEN g_cov = g_cov + 1
      END IF
    END IF
  NEXT j
  RETURN -1
END FUNCTION

SUB PrintMissing(root AS STRING, ns AS STRING)
  DIM f AS STRING
  DIM j AS INTEGER
  f = NsFile(root, ns)
  IF NOT FILEEXISTS(f) THEN
    PRINT "MANCA FILE  "; ns; " -> "; f
    EXIT SUB
  END IF
  FOR j = 1 TO g_n
    IF g_nsv(j) = ns THEN
      IF g_kind(j) = "procedure" THEN
        IF NOT ((g_name(j) IN g_calls) OR (g_name(j) IN g_words)) THEN PRINT "  MANCA "; ns; "."; g_name(j)
      ELSE
        IF NOT (g_name(j) IN g_words) THEN PRINT "  MANCA "; ns; "."; g_name(j)
      END IF
    END IF
  NEXT j
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, surf AS STRING
  DIM lines, flds$() AS STRING
  DIM i AS LONG, j AS LONG, up AS INTEGER
  DIM ln AS STRING, k AS STRING, ns AS STRING, nm AS STRING
  DIM seen AS INTEGER
  DIM tot AS LONG, cov AS LONG, withfile AS LONG, complete AS LONG
  DIM f AS STRING, fl AS STRING
  DIM a AS INTEGER
  DIM arg AS STRING
  DIM t AS LONG, c AS LONG
  DIM BASE AS STRING
  DIM exists AS INTEGER
  DIM nsel AS INTEGER

  ' --- argomenti
  a = 1
  DO WHILE a <= COMMANDCOUNT()
    arg = UPPER(COMMAND$(a))
    IF arg = "--MISSING" THEN g_missing = -1
    IF arg = "--STRICT" THEN g_strict = -1
    IF arg = "--NS" AND a < COMMANDCOUNT() THEN
      a = a + 1
      g_only = UPPER(COMMAND$(a))
    END IF
    a = a + 1
  LOOP

  root = IO.CWD()
  FOR up = 1 TO 6
    IF FILEEXISTS(root + "/shared/spec/registry/surface.csv") THEN EXIT FOR
    root = IO.PARENTDIR(root)
  NEXT up
  surf = root + "/shared/spec/registry/surface.csv"
  IF NOT FILEEXISTS(surf) THEN PRINT "ERRORE: surface.csv non trovato ("; surf; ")" : RETURN 1

  ' --- superficie (dedup per ns+nome)
  lines$ = IO.READLINES(surf)
  g_n = 0
  FOR i = 0 TO UBOUND(lines$)
    ln = lines$(i)
    IF LEN(ln) > 0 AND LEFT$(ln, 1) <> "#" THEN
      flds$ = SPLIT(ln, ",")
      IF UBOUND(flds$) >= 3 THEN
        k = flds$(0)
        IF k = "procedure" OR k = "constant" THEN
          ns = flds$(1) : nm = UPPER(flds$(2))
          seen = 0
          FOR j = 1 TO g_n
            IF g_nsv(j) = ns AND g_name(j) = nm THEN seen = -1 : EXIT FOR
          NEXT j
          IF NOT seen THEN
            g_n = g_n + 1
            g_kind(g_n) = k : g_nsv(g_n) = ns : g_name(g_n) = nm
            NsAdd(ns)
          END IF
        END IF
      END IF
    END IF
  NEXT i

  ' --- base del linguaggio
  BASE = root + "/shared/minitests/constructs/base.bas"

  PRINT "MiniTest PER NAMESPACE — superficie: "; g_n; " voci (procedure+costanti), "; g_ns_n; " namespace"
  PRINT "file canonico: shared/minitests/<ns>/<ns>_all.bas"
  PRINT
  PRINT "namespace                          tot  cop  manca  file"
  FOR i = 1 TO g_ns_n
    IF g_only <> "" AND g_ns(i) <> g_only THEN
      ' salta i namespace non richiesti
    ELSE
      exists = NsCover(root, g_ns(i))
      t = g_tot : c = g_cov
      f = NsFile(root, g_ns(i))
      fl = "NO"
      IF exists THEN fl = "si"
      IF g_missing THEN PrintMissing(root, g_ns(i))
      IF t > c OR NOT exists THEN
        PRINT LEFT$(g_ns(i) + SPACE$(32), 32); RIGHT$(SPACE$(3) + STR$(t), 4); _
              RIGHT$(SPACE$(3) + STR$(c), 4); RIGHT$(SPACE$(5) + STR$(t - c), 6); "   "; fl
      END IF
      tot = tot + t : cov = cov + c
      IF exists THEN withfile = withfile + 1
      IF exists AND t > 0 AND c = t THEN complete = complete + 1
    END IF
  NEXT i

  PRINT
  PRINT "namespace con file canonico: "; withfile; "/"; g_ns_n
  PRINT "namespace completi (file + superficie): "; complete; "/"; g_ns_n
  PRINT "superficie coperta: "; cov; "/"; tot; "  (mancano "; tot - cov; ")"
  PRINT "struttura base del linguaggio: "; BASE; " -> ";
  fl = "MANCANTE"
  IF FILEEXISTS(BASE) THEN fl = "presente"
  PRINT fl

  IF g_strict THEN
    IF g_only <> "" THEN
      nsel = 0
      FOR i = 1 TO g_ns_n
        IF g_ns(i) = g_only THEN
          nsel = 1
          exists = NsCover(root, g_ns(i))
          IF NOT exists OR g_tot > g_cov THEN
            PRINT "STRICT: FALLITO ("; g_only; ")"
            RETURN 1
          END IF
        END IF
      NEXT i
      IF nsel = 0 THEN
        PRINT "STRICT: FALLITO (namespace sconosciuto: "; g_only; ")"
        RETURN 1
      END IF
    ELSE
      IF withfile < g_ns_n OR cov < tot OR NOT FILEEXISTS(BASE) THEN
        PRINT "STRICT: FALLITO"
        RETURN 1
      END IF
    END IF
    PRINT "STRICT: ok"
  END IF
  RETURN 0
END FUNCTION
