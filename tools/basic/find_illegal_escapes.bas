' find_illegal_escapes.bas — GATE: backslash DENTRO i letterali stringa BASIC.
' Equivalente del vecchio tools/find_illegal_escapes.py, senza Python.
'
' GMBasic non ha escape nelle stringhe: la stringa finisce alla prima virgoletta,
' quindi un letterale con una barra prima della virgoletta vale UN carattere (la barra).
' Le forme con cui si scrive un carattere speciale sono CHR$(34) e CHR$(92).
'
' Classificazione (per OGNI backslash visto dentro un letterale):
'   RISK    -> sequenza barra-quote-barra-quote: chiude la stringa, lascia la barra come
'              operatore e produce div fra stringhe -> IL invalido
'              ("Invalid operand types in BinaryInst").
'   escape  -> barra seguita da virgoletta MA la stringa non finisce davvero li' (dopo
'              viene un identificativo, o la riga resta aperta): escape C-style sbagliato.
'   json    -> barra ULTIMO carattere di un letterale vero e dopo la virgoletta riprende la
'              sintassi: barra letterale, idioma dell'escaping JSON. BENIGNO.
'   trail   -> barra letterale prima della chiusura (es. path): legale ma da verificare.
'
' Uso (CWD = tools/, come imposta l'harness):
'   test_ilrun --engine=il tools/basic/find_illegal_escapes.bas [root ...]
'   root = directory o singolo file .bas; senza argomenti stampa l'uso ed esce 2.
'
' Exit: 2 se c'e' almeno un RISK, se una root non esiste o se non si e' scansionato
'       nessun file (un path sbagliato non deve passare in silenzio);
'       1 se ci sono escape; 0 se pulito.
IMPORT IO
IMPORT GREP
IMPORT COLLECTIONS

DIM g_risk AS LONG
DIM g_esc AS LONG
DIM g_json AS LONG
DIM g_trail AS LONG
DIM g_files AS LONG
DIM g_scanned AS LONG
DIM g_badroot AS LONG
DIM g_hdr AS INTEGER
DIM g_seen AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
' DICT, non SORTED: l'ordine di iterazione e' l'ordine di inserimento su TUTTE le gambe
' (SORTED non ordina su VM/C), e l'ordine alfabetico lo garantisce il sort per-directory
' in Walk. Cosi' l'output e' identico sulle 4 gambe.
DIM g_pend AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
' Stack di visita: 4096 directory in coda (usi il letterale, `DIM CONST` non e' sintassi).
DIM g_stack(4096) AS STRING

' ---------------------------------------------------------------- helper stringhe
FUNCTION Ltrim(s AS STRING) AS STRING
  DIM i AS LONG
  i = 1
  DO WHILE i <= LEN(s) AND MID$(s, i, 1) = " "
    i = i + 1
  LOOP
  RETURN MID$(s, i)
END FUNCTION

FUNCTION Rtrim(s AS STRING) AS STRING
  DIM n AS LONG
  n = LEN(s)
  DO WHILE n > 0 AND MID$(s, n, 1) = " "
    n = n - 1
  LOOP
  RETURN LEFT$(s, n)
END FUNCTION

FUNCTION Trim(s AS STRING) AS STRING
  RETURN Rtrim(Ltrim(s))
END FUNCTION

FUNCTION Lpad(s AS STRING, w AS LONG) AS STRING
  DIM r AS STRING
  r = s
  DO WHILE LEN(r) < w
    r = " " + r
  LOOP
  RETURN r
END FUNCTION

FUNCTION Rpad(s AS STRING, w AS LONG) AS STRING
  DIM r AS STRING
  r = s
  DO WHILE LEN(r) < w
    r = r + " "
  LOOP
  RETURN r
END FUNCTION

' ---------------------------------------------------------------- parsing riga
' Ritorna il codice senza il commento ('...), rispettando i letterali stringa.
FUNCTION StripComment(s AS STRING) AS STRING
  DIM i AS LONG, n AS LONG, c AS STRING, inStr AS INTEGER
  n = LEN(s)
  inStr = 0
  FOR i = 1 TO n
    c = MID$(s, i, 1)
    IF c = CHR$(34) THEN inStr = 1 - inStr
    IF c = "'" AND inStr = 0 THEN RETURN LEFT$(s, i - 1)
  NEXT i
  RETURN s
END FUNCTION

' Nome della procedura dichiarata dalla riga ("" se non e' una dichiarazione).
FUNCTION ProcName(code AS STRING) AS STRING
  DIM s AS STRING, up AS STRING, rest AS STRING, kw AS STRING, nm AS STRING
  DIM i AS LONG, p AS LONG
  s = Trim(code)
  up = UPPER$(s)
  IF LEFT$(up, 8) = "END SUB " OR up = "END SUB" THEN RETURN ""
  IF LEFT$(up, 13) = "END FUNCTION " OR up = "END FUNCTION" THEN RETURN ""
  IF LEFT$(up, 14) = "END PROCEDURE " OR up = "END PROCEDURE" THEN RETURN ""
  IF LEFT$(up, 7) = "PUBLIC " THEN
    s = MID$(s, 8)
  ELSEIF LEFT$(up, 8) = "PRIVATE " THEN
    s = MID$(s, 9)
  ELSEIF LEFT$(up, 7) = "STATIC " THEN
    s = MID$(s, 8)
  END IF
  up = UPPER$(s)
  FOR i = 1 TO 5
    IF i = 1 THEN kw = "SUB "
    IF i = 2 THEN kw = "FUNCTION "
    IF i = 3 THEN kw = "PROCEDURE "
    IF i = 4 THEN kw = "METHOD "
    IF i = 5 THEN kw = "PROPERTY "
    IF LEFT$(up, LEN(kw)) = kw THEN
      rest = Trim(MID$(s, LEN(kw) + 1))
      p = INSTR(rest, "(")
      IF p > 0 THEN rest = LEFT$(rest, p - 1)
      p = INSTR(rest, " ")
      IF p > 0 THEN rest = LEFT$(rest, p - 1)
      nm = Rtrim(rest)
      DO WHILE RIGHT$(nm, 1) = ","
        nm = LEFT$(nm, LEN(nm) - 1)
      LOOP
      RETURN nm
    END IF
  NEXT i
  RETURN ""
END FUNCTION

' 1 se dopo la virgoletta di chiusura riprende la sintassi e il resto della riga ha
' un numero PARI di virgolette: la barra e' l'ultimo carattere di un letterale vero.
FUNCTION JsonLiteralEndsHere(code AS STRING, p AS LONG) AS INTEGER
  DIM j AS LONG, n AS LONG, c AS STRING, qn AS LONG
  n = LEN(code)
  j = p + 2
  DO WHILE j <= n
    c = MID$(code, j, 1)
    IF c <> " " AND c <> CHR$(9) THEN EXIT DO
    j = j + 1
  LOOP
  IF j <= n THEN
    c = MID$(code, j, 1)
    IF INSTR(",)]};+", c) = 0 THEN RETURN 0
  END IF
  qn = 0
  FOR j = p + 2 TO n
    IF MID$(code, j, 1) = CHR$(34) THEN qn = qn + 1
  NEXT j
  IF qn MOD 2 = 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB RegisterHit(kind AS STRING, path AS STRING, lineno AS LONG, col AS LONG, proc AS STRING, snip AS STRING)
  DIM where AS STRING
  IF kind = "RISK" THEN g_risk = g_risk + 1
  IF kind = "escape" THEN g_esc = g_esc + 1
  IF kind = "json" THEN g_json = g_json + 1
  IF kind = "trail" THEN g_trail = g_trail + 1
  IF g_hdr = 0 THEN
    g_files = g_files + 1        ' g_files = file CON hit ("in N/M file scansionati")
    PRINT ""
    PRINT path
    g_hdr = 1
  END IF
  where = "top-level"
  IF proc <> "" THEN where = "in " + proc
  PRINT "  "; Lpad(LTRIM$(STR(lineno)), 4); ":"; Rpad(LTRIM$(STR(col)), 3); " "; Rpad(kind, 6); " "; Rpad(where, 28); " "; snip
END SUB

' ------------------------------------------------------------------ scansione file
SUB ScanFile(path AS STRING)
  DIM text AS STRING, raw AS STRING, code AS STRING, c AS STRING, nxt AS STRING
  DIM lines$() AS STRING, proc AS STRING, kind AS STRING, snip AS STRING
  DIM i AS LONG, li AS LONG, p AS LONG, n AS LONG, inStr AS INTEGER
  DIM endsJson AS INTEGER
  text = IO.READFILE(path)
  IF LEN(text) = 0 THEN EXIT SUB
  g_hdr = 0
  lines$ = SPLIT(text, CHR$(10))
  proc = ""
  IF UBOUND(lines$) < 0 THEN EXIT SUB
  FOR li = 0 TO UBOUND(lines$)
    raw = lines$(li)
    IF LEN(raw) > 0 THEN
      IF RIGHT$(raw, 1) = CHR$(13) THEN raw = LEFT$(raw, LEN(raw) - 1)
    END IF
    code = StripComment(raw)
    nxt = ProcName(code)
    IF nxt <> "" THEN proc = nxt
    ' Fast-path: senza backslash la riga non puo' produrre hit; il loop per-carattere
    ' (lento su IL: un MID$ per carattere) gira solo sulle righe che ne hanno uno.
    IF INSTR(code, CHR$(92)) = 0 THEN
      ' niente da fare su questa riga
    ELSE
    snip = LEFT$(Trim(code), 110)
    n = LEN(code)
    inStr = 0
    FOR p = 1 TO n
      c = MID$(code, p, 1)
      IF c = CHR$(34) THEN
        inStr = 1 - inStr
      ELSEIF c = CHR$(92) AND inStr = 1 THEN
        IF MID$(code, p, 4) = CHR$(92) + CHR$(34) + CHR$(92) + CHR$(34) THEN
          kind = "RISK"
        ELSE
          nxt = MID$(code, p + 1, 1)
          IF nxt = CHR$(34) THEN
            endsJson = JsonLiteralEndsHere(code, p)
            IF endsJson = -1 THEN kind = "json" ELSE kind = "escape"
          ELSE
            kind = "trail"
          END IF
        END IF
        RegisterHit(kind, path, li + 1, p, proc, snip)
      END IF
    NEXT p
    END IF
  NEXT li
END SUB

FUNCTION SkipPath(p AS STRING) AS INTEGER
  IF INSTR(p, "/Resources/libs/") > 0 THEN RETURN -1
  IF INSTR(p, "/GMBasic-Deprecated/") > 0 THEN RETURN -1
  IF INSTR(p, "/build/") > 0 THEN RETURN -1
  IF INSTR(p, "/Build/") > 0 THEN RETURN -1
  IF INSTR(p, "/build-c99/") > 0 THEN RETURN -1
  IF INSTR(p, "/bin/") > 0 THEN RETURN -1
  IF INSTR(p, "/obj/") > 0 THEN RETURN -1
  IF INSTR(p, "/.git/") > 0 THEN RETURN -1
  IF INSTR(p, "/node_modules/") > 0 THEN RETURN -1
  IF INSTR(p, "/packages/") > 0 THEN RETURN -1
  IF INSTR(p, "/_deps/") > 0 THEN RETURN -1
  IF INSTR(p, "/work/") > 0 THEN RETURN -1
  IF INSTR(p, "/Work/") > 0 THEN RETURN -1
  ' Feed di mercato esterni: symlink all'SSD Armor5TB (AGENTS §9.3), non contengono .bas.
  IF INSTR(p, "/Fixtures/markets") > 0 THEN RETURN -1
  IF INSTR(p, "/Fixtures/option_chains") > 0 THEN RETURN -1
  IF INSTR(p, "/Fixtures/rates") > 0 THEN RETURN -1
  IF INSTR(p, "/Fixtures/darkpool") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB ScanOne(path AS STRING)
  IF path IN g_seen THEN EXIT SUB
  g_seen.Set(path, 1)
  g_scanned = g_scanned + 1
  ScanFile(path)
END SUB

' Ricorsione propria su DIR() invece di GREP.FILES: su un albero grande la lista JSON
' di GREP.FILES si tronca e torna 0 file (gate che direbbe "pulito" senza aver letto
' nulla). I nomi raccolti finiscono in g_pend (SORTED = dedup + ordine alfabetico).
' NB: il parametro NON si chiama `dir` — DIR e' un TokenKind (funzione), un identificativo
' con quel nome non e' un target/espressione validi.
SUB Walk(root AS STRING)
  ' Array SENZA suffisso `$`: con `names$ = DIR(...)` l'emitter C99 assegna una lista a
  ' un gmb_string_t* e la build muore ("incompatible type gmb_list_t").
  DIM names() AS STRING
  DIM i AS LONG, j AS LONG, sp AS LONG, cur AS STRING, full AS STRING, kk AS STRING
  ' ITERATIVO, con stack esplicito: la ricorsione NON e' portabile. Su PCode e VM/C i
  ' locali di SUB sono CONDIVISI fra le chiamate, quindi la discesa ricorsiva si
  ' corrompe e si ferma al primo sottodirectory (misurato: 1 file su 1.208).
  sp = 0
  cur = root
  DO WHILE RIGHT$(cur, 1) = "/"
    cur = LEFT$(cur, LEN(cur) - 1)
  LOOP
  sp = 1
  g_stack(sp) = cur
  DO WHILE sp > 0
    cur = g_stack(sp)
    sp = sp - 1
    names = DIR(cur + "/*")
    IF UBOUND(names) >= 0 THEN
      ' Insertion sort della singola directory: l'ordine di DIR() non e' specificato
      ' e cambia fra le gambe; cosi' l'ordine finale e' alfabetico ovunque.
      FOR i = 1 TO UBOUND(names)
        kk = names(i)
        j = i - 1
        DO WHILE j >= 0
          IF UPPER$(names(j)) <= UPPER$(kk) THEN EXIT DO
          names(j + 1) = names(j)
          j = j - 1
        LOOP
        names(j + 1) = kk
      NEXT i
      ' 1) i FILE di questa directory, in ordine alfabetico
      FOR i = 0 TO UBOUND(names)
        full = cur + "/" + names(i)
        IF DIREXISTS(full) = 0 THEN
          IF RIGHT$(UPPER$(names(i)), 4) = ".BAS" THEN
            IF SkipPath(full) = 0 THEN
              IF FILEEXISTS(full) THEN g_pend.Set(full, 1)
            END IF
          END IF
        END IF
      NEXT i
      ' 2) le SOTTOdirectory, in ordine INVERSO: lo stack le ripesca in ordine
      '    alfabetico -> DFS identico alla visita ricorsiva.
      FOR i = UBOUND(names) TO 0 STEP -1
        full = cur + "/" + names(i)
        IF DIREXISTS(full) THEN
          IF SkipPath(full + "/") = 0 THEN
            IF sp >= 4096 THEN
              PRINT "!! troppe directory in coda (> 4096): alzare g_stack"
              g_badroot = g_badroot + 1
              EXIT SUB
            END IF
            sp = sp + 1
            g_stack(sp) = full
          END IF
        END IF
      NEXT i
    END IF
  LOOP
END SUB

SUB ScanDir(root AS STRING)
  g_pend.Clear()
  Walk(root)
  ' NB: `FOR EACH k, v IN <SORTED>` non e' portabile (su PCode lega k all'indice e v
  ' all'intera entry); la forma con e.Key/e.Value e' identica sulle 4 gambe.
  FOR EACH e IN g_pend
    ScanOne(e.Key)
  NEXT
END SUB

FUNCTION Usage() AS INTEGER
  PRINT "find_illegal_escapes.bas — backslash dentro i letterali stringa BASIC"
  PRINT "uso: test_ilrun --engine=il tools/basic/find_illegal_escapes.bas [root ...]"
  PRINT "  root = directory (ricorsiva) o singolo file .bas; almeno una root e' obbligatoria"
  PRINT "exit: 2 RISK/root inesistente/nessun file, 1 escape, 0 pulito"
  RETURN 0
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM i AS LONG, argc AS LONG, a AS STRING
  DIM roots(64) AS STRING
  DIM nroot AS LONG
  g_risk = 0 : g_esc = 0 : g_json = 0 : g_trail = 0
  g_files = 0 : g_scanned = 0 : g_badroot = 0
  argc = COMMANDCOUNT()
  nroot = 0
  FOR i = 1 TO argc
    a = COMMAND(i)
    IF a = "-h" OR a = "--help" THEN RETURN Usage()
    IF LEFT$(a, 1) = "-" THEN
      PRINT "!! opzione sconosciuta: "; a
      RETURN 2
    END IF
    IF nroot < 64 THEN
      roots(nroot) = a
      nroot = nroot + 1
    END IF
  NEXT i
  IF nroot = 0 THEN RETURN Usage()   ' come l'originale: nessuna root non e' "pulito"

  FOR i = 0 TO nroot - 1
    PRINT "== "; roots(i)
    ' DIREXISTS PRIMA di FILEEXISTS: su VM/C FILEEXISTS(dir) e' vero e la directory
    ' verrebbe trattata come un file ("0/1 file scansionati" e gate falsamente verde).
    IF DIREXISTS(roots(i)) THEN
      ScanDir(roots(i))
    ELSEIF FILEEXISTS(roots(i)) THEN
      ScanOne(roots(i))
    ELSE
      PRINT "!! root inesistente: "; roots(i)
      g_badroot = g_badroot + 1
    END IF
  NEXT i

  PRINT ""
  PRINT "TOTALE: " + LTRIM$(STR(g_risk)) + " RISK (IL invalido), " + LTRIM$(STR(g_esc)) + " escape (semantica sbagliata), " + LTRIM$(STR(g_json)) + " json (barra letterale: idioma JSON, benigno), " + LTRIM$(STR(g_trail)) + " trail (legale) in " + LTRIM$(STR(g_files)) + "/" + LTRIM$(STR(g_scanned)) + " file .bas scansionati"
  PRINT ""
  IF g_risk > 0 THEN
    PRINT "GATE ROSSO: " + LTRIM$(STR(g_risk)) + " occorrenza/e producono IL invalido (div fra stringhe)."
    RETURN 2
  END IF
  IF g_badroot > 0 THEN
    PRINT "GATE ROSSO: " + LTRIM$(STR(g_badroot)) + " root inesistente/i: il path va corretto, non si puo' dire pulito."
    RETURN 2
  END IF
  IF g_scanned = 0 THEN
    PRINT "GATE ROSSO: nessun file .bas scansionato (root vuota o filtro sbagliato)."
    RETURN 2
  END IF
  IF g_esc > 0 THEN
    PRINT "GATE ROSSO: " + LTRIM$(STR(g_esc)) + " escape C-style da verificare."
    RETURN 1
  END IF
  PRINT "GATE VERDE: 0 RISK, 0 escape (le barre letterali in idioma JSON sono ammesse)."
  RETURN 0
END FUNCTION
