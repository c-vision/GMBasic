' FILE: shared/Apps/DigUp/src/DigUp.bas
' ============================================================================
' DigUp (GMBasic port) — "Describe it. Dig it up."
' Ricerca semantica nei file: indicizza il TESTO dei file scelti (M1) e trova i
' passaggi più vicini a una descrizione (embedding + parole esatte), come
' l'originale. L'indice è un SQLite (alias `DigDb`) con la collezione vettoriale
' gestita da AI RAG; la tabella `chunks` tiene i metadati (path/seq/testo).
'
' Port fedele dell'OSSATURA headless di https://github.com/ARahim3/DigUp (MIT):
'   folders -> reader -> embedding -> indice (vettori + testo) -> query -> rank
' M1: testo/documenti/codice. Immagini/PDF/audio/video (EmbeddingGemma 2
' multimodale, PDFium, ffmpeg nell'originale) sono in TODO.md: LLMEnv non espone
' oggi un embedding multimodale. Il modello di embedding è QUELLO CONFIGURATO
' NEI SETTINGS della app (`.digup.toml`, [llm]) — mai hardcoded.
'
' MODI (simboli compile-time; BASIC non ha argv):
'   (default)                    status: banner + config + statistiche indice
'   -DDIGUP_MODE_INDEX           indicizza le cartelle di [index] roots
'   -DDIGUP_MODE_SEARCH          cerca la query di [search] query
'   -DDIGUP_MODE_CHECK           diagnostica: provider embedding + indice
'
' RUN (dalla ROOT del monorepo):
'   tools/sh/harness.sh pcode shared/Apps/DigUp/src/DigUp.bas
'   # modo diverso: aggiungere in testa al file  #DEF DIGUP_MODE_INDEX
'   # (i runner di tools/runners/ non passano -D)
' ============================================================================
IMPORT IO
IMPORT STRINGS
IMPORT SQL
IMPORT AI
IMPORT JSON

' --- Application globals ----------------------------------------------------
DIM g_appDir AS STRING          ' cartella della app (padre di src/)
DIM g_outDir AS STRING          ' <app>/bin — indice e output (gitignored)
DIM g_dbPath AS STRING          ' indice SQLite
DIM g_projectFile AS STRING     ' .digup.toml trovato ("" se nessuno)
DIM g_cfgMode AS STRING         ' [settings] mode
DIM g_roots AS STRING           ' [index] roots (csv)
DIM g_exts AS STRING            ' [index] extensions (csv)
DIM g_reindex AS STRING         ' [index] reindex ("true" = da zero)
DIM g_chunk AS INTEGER          ' [index] chunk (caratteri)
DIM g_top AS INTEGER            ' [search] top
DIM g_query AS STRING           ' [search] query
' LLM: il modello si LEGGE dai settings (ADR-App-LLM-Modello-nei-Settings).
DIM g_llmModel AS STRING
DIM g_llmCapability AS STRING
DIM g_llmDir AS STRING
DIM g_dims AS INTEGER

' Contatori di indicizzazione (stato globale).
DIM g_nFiles AS LONG
DIM g_nChunks AS LONG

' Estensioni e roots su array fissi (niente liste/varianti: parità 4 gambe).
DIM g_extArr(32) AS STRING
DIM g_extN AS INTEGER
DIM g_rootArr(32) AS STRING
DIM g_rootN AS INTEGER

DIM g_coll AS STRING            ' nome della collezione vettoriale
DIM g_lastId AS STRING          ' ultimo sourceId risolto (lookup in `chunks`)

' ============================================================================
SUB Main()
    BootPaths()
    ConfigureDefaults()
    PrintBanner()
    LoadProjectFile()
    LlmActivate()

    DIM mode AS STRING
    mode = ComputeMode()
    IF mode = "STATUS" AND LEN(g_cfgMode) > 0 THEN mode = g_cfgMode

    IF mode = "INDEX" THEN
        RunIndex()
    ELSEIF mode = "SEARCH" THEN
        RunSearch()
    ELSEIF mode = "CHECK" THEN
        RunCheck()
    ELSE
        PrintStatus()
    END IF
END SUB

SUB BootPaths()
    DIM srcDir AS STRING, appDir AS STRING
    srcDir = PROJECTFOLDER()
    IF LEN(srcDir) = 0 THEN srcDir = IO.CWD()
    appDir = IO.PARENTDIR(RTrimSlash(srcDir))
    IF LEN(appDir) = 0 THEN appDir = srcDir
    g_appDir = RTrimSlash(appDir)
    g_outDir = g_appDir + "/bin"
    IF NOT IO.DIREXISTS(g_outDir) THEN MKDIR g_outDir
    g_dbPath = g_outDir + "/digup.sqlite"
END SUB

SUB ConfigureDefaults()
    g_cfgMode = ""
    g_roots = "fixtures"
    g_exts = ".txt,.md,.csv"
    g_reindex = "true"
    g_chunk = 300
    g_top = 5
    g_query = ""
    g_llmModel = ""
    g_llmCapability = "embedding"
    g_llmDir = ""
    g_dims = 8
    g_coll = "digup"
END SUB

SUB PrintBanner()
    PRINT "╔══════════════════════════════════════════════════════════════════╗"
    PRINT "║  DigUp (GMBasic port) — describe it. dig it up.                    ║"
    PRINT "║  upstream: github.com/ARahim3/DigUp  (MIT)                         ║"
    PRINT "╚══════════════════════════════════════════════════════════════════╝"
    PRINT ""
END SUB

' ============================================================================
' Config: .digup.toml — lettore minimale (stile TOML: [sezione], key = value)
' ============================================================================
SUB LoadProjectFile()
    DIM dir AS STRING, candidate AS STRING
    DIM i AS INTEGER
    dir = IO.CWD()
    FOR i = 1 TO 8
        candidate = RTrimSlash(dir) + "/.digup.toml"
        IF IO.FILEEXISTS(candidate) THEN
            g_projectFile = candidate
            ParseProjectFile(candidate)
            i = 8
        ELSE
            dir = IO.PARENTDIR(dir)
            IF LEN(dir) = 0 THEN i = 8
        END IF
    NEXT i
END SUB

FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF STRINGS.ENDSWITH(s, "/") THEN s = STRINGS.LEFT(s, LEN(s) - 1)
    RTrimSlash = s
END FUNCTION

SUB ParseProjectFile(path AS STRING)
    DIM lines
    DIM raw AS STRING, section AS STRING, key AS STRING, val AS STRING
    DIM eq AS INTEGER, i AS LONG
    lines = IO.READLINES(path)
    section = ""
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            IF STRINGS.STARTSWITH(raw, "[") THEN
                section = STRINGS.UPPER(BracketInner(raw))
            ELSE
                eq = INSTR(raw, "=")
                IF eq > 1 THEN
                    key = STRINGS.UPPER(STRINGS.TRIM(LEFT$(raw, eq - 1)))
                    val = Unquote(STRINGS.TRIM(MID$(raw, eq + 1)))
                    IF section = "SETTINGS" AND key = "MODE" THEN g_cfgMode = STRINGS.UPPER(val)
                    IF section = "INDEX" THEN
                        IF key = "ROOTS" THEN g_roots = val
                        IF key = "DB" THEN g_dbPath = g_appDir + "/" + StripLeadSlash(val)
                        IF key = "EXTENSIONS" THEN g_exts = val
                        IF key = "CHUNK" THEN g_chunk = CINT(VAL(val))
                        IF key = "REINDEX" THEN g_reindex = STRINGS.UPPER(val)
                    END IF
                    IF section = "SEARCH" THEN
                        IF key = "TOP" THEN g_top = CINT(VAL(val))
                        IF key = "QUERY" THEN g_query = val
                    END IF
                    IF section = "LLM" THEN
                        IF key = "MODEL" THEN g_llmModel = val
                        IF key = "CAPABILITY" THEN g_llmCapability = val
                        IF key = "DIR" THEN g_llmDir = val
                        IF key = "DIMS" THEN g_dims = CINT(VAL(val))
                    END IF
                END IF
            END IF
        END IF
    NEXT i
END SUB

FUNCTION BracketInner(s AS STRING) AS STRING
    BracketInner = STRINGS.TRIM(MID$(s, 2, LEN(s) - 2))
END FUNCTION

FUNCTION Unquote(s AS STRING) AS STRING
    DIM t AS STRING
    DIM q AS STRING
    q = CHR$(34)
    t = s
    IF LEN(t) >= 2 THEN
        IF STRINGS.STARTSWITH(t, q) AND STRINGS.ENDSWITH(t, q) THEN
            t = MID$(t, 2, LEN(t) - 2)
        END IF
    END IF
    Unquote = t
END FUNCTION

FUNCTION StripLeadSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF STRINGS.STARTSWITH(s, "/") THEN s = MID$(s, 2, LEN(s) - 1)
    StripLeadSlash = s
END FUNCTION

' ============================================================================
' Liste di stringhe su array fissi (MID$/INSTR: niente liste/varianti)
' ============================================================================
SUB LoadExts()
    DIM s AS STRING, i AS INTEGER, start AS INTEGER, comma AS INTEGER, piece AS STRING
    g_extN = 0
    s = STRINGS.LOWER(g_exts)
    start = 1
    DO WHILE start <= LEN(s) AND g_extN < 32
        comma = INSTR(start, s, ",")
        IF comma = 0 THEN
            piece = MID$(s, start, LEN(s) - start + 1)
            start = LEN(s) + 1
        ELSE
            piece = MID$(s, start, comma - start)
            start = comma + 1
        END IF
        piece = STRINGS.TRIM(piece)
        IF LEN(piece) > 0 THEN
            g_extArr(g_extN) = piece
            g_extN = g_extN + 1
        END IF
    LOOP
END SUB

SUB LoadRoots()
    DIM s AS STRING, i AS INTEGER, start AS INTEGER, comma AS INTEGER, piece AS STRING
    g_rootN = 0
    s = g_roots
    start = 1
    DO WHILE start <= LEN(s) AND g_rootN < 32
        comma = INSTR(start, s, ",")
        IF comma = 0 THEN
            piece = MID$(s, start, LEN(s) - start + 1)
            start = LEN(s) + 1
        ELSE
            piece = MID$(s, start, comma - start)
            start = comma + 1
        END IF
        piece = STRINGS.TRIM(piece)
        IF LEN(piece) > 0 THEN
            g_rootArr(g_rootN) = piece
            g_rootN = g_rootN + 1
        END IF
    LOOP
END SUB

' ============================================================================
' LLM: attiva il modello di embedding configurato nei settings (mai hardcoded)
' ============================================================================
SUB LlmActivate()
    IF LEN(g_llmDir) > 0 THEN
        AI REGISTER "digup-embed", %EMBEDDING, g_llmDir, ""
        AI SET ACTIVE %EMBEDDING, "digup-embed"
    ELSEIF LEN(g_llmModel) > 0 THEN
        AI SET ACTIVE %EMBEDDING, g_llmModel
    END IF
END SUB

FUNCTION EmbedDims() AS INTEGER
    IF g_dims > 0 THEN
        EmbedDims = g_dims
    ELSE
        EmbedDims = 8
    END IF
END FUNCTION

' ============================================================================
' MODE: STATUS
' ============================================================================
SUB PrintStatus()
    PRINT "project file : "; g_projectFile
    PRINT "app dir      : "; g_appDir
    PRINT "index        : "; g_dbPath; "  ("; DbFileState(); ")"
    PRINT "embedding    : "; g_llmModel; "  dims="; EmbedDims()
    IF LEN(g_llmDir) > 0 THEN PRINT "             : dir="; g_llmDir
    PRINT "roots        : "; g_roots
    PRINT "extensions   : "; g_exts
    PRINT "chunk        : "; g_chunk
    PRINT "rank         : top "; g_top
    CountIndex()
    PRINT ""
    PRINT "modes        : -DDIGUP_MODE_INDEX | _SEARCH | _CHECK"
END SUB

FUNCTION DbFileState() AS STRING
    IF IO.FILEEXISTS(g_dbPath) THEN
        DbFileState = "presente"
    ELSE
        DbFileState = "assente (esegui index)"
    END IF
END FUNCTION

SUB CountIndex()
    DIM rs AS SQLRESULTSET
    DIM n$
    IF NOT IO.FILEEXISTS(g_dbPath) THEN
        PRINT "chunks       : 0 (indice assente)"
        EXIT SUB
    END IF
    SQL OPEN g_dbPath ALIAS DigDb
    SQL QUERY DigDb, rs, "SELECT COUNT(*) AS n FROM chunks"
    n$ = "0"
    IF SQL.FETCH(rs) THEN n$ = rs("n")
    SQL CLOSE RESULTSET rs
    SQL CLOSE DigDb
    PRINT "chunks       : "; n$
END SUB

' ============================================================================
' MODE: INDEX — cartelle -> testo -> chunk -> indice (AI RAG + tabella chunks)
' ============================================================================
SUB RunIndex()
    DIM i AS INTEGER, root AS STRING
    IF g_reindex = "TRUE" THEN
        IF IO.FILEEXISTS(g_dbPath) THEN
            KILL g_dbPath
        END IF
    END IF
    OpenIndex()
    AI RAG ENSURE COLLECTION "DigDb", g_coll, EmbedDims()
    g_nFiles = 0
    g_nChunks = 0
    LoadRoots()
    FOR i = 0 TO g_rootN - 1
        root = g_rootArr(i)
        IF LEN(root) > 0 THEN
            IF NOT STRINGS.STARTSWITH(root, "/") THEN root = g_appDir + "/" + root
            WalkDir(root)
        END IF
    NEXT i
    CloseIndex()
    PRINT "files indexed : "; g_nFiles
    PRINT "chunks        : "; g_nChunks
    PRINT "index         : "; g_dbPath
END SUB

' una cartella: file indicizzati + sotto-cartelle visitate (ricorsione)
SUB WalkDir(path AS STRING)
    DIM entries AS LIST, e AS STRING, p AS STRING, nm AS STRING
    DIM isDir AS INTEGER, ok AS INTEGER
    entries = IO.DIR(path + "/")
    FOR EACH e IN entries
        IF LEN(e) > 0 THEN
            nm = e
            p = path + "/" + nm
            isDir = IO.DIREXISTS(p)
            IF isDir THEN
                WalkDir(p)
            ELSE
                ok = AllowedFile(nm)
                IF ok THEN
                    IndexFile(p)
                    g_nFiles = g_nFiles + 1
                END IF
            END IF
        END IF
    NEXT
END SUB

FUNCTION AllowedFile(name AS STRING) AS INTEGER
    DIM n AS STRING, ext AS STRING
    DIM dot AS INTEGER, i AS INTEGER
    n = STRINGS.LOWER(name)
    dot = 0
    FOR i = 1 TO LEN(n)
        IF MID$(n, i, 1) = "." THEN dot = i
    NEXT i
    IF dot = 0 THEN
        AllowedFile = 0
        EXIT FUNCTION
    END IF
    ext = MID$(n, dot, LEN(n) - dot + 1)
    LoadExts()
    FOR i = 0 TO g_extN - 1
        IF g_extArr(i) = ext THEN
            AllowedFile = 1
            EXIT FUNCTION
        END IF
    NEXT i
    AllowedFile = 0
END FUNCTION

SUB OpenIndex()
    SQL OPEN g_dbPath ALIAS DigDb
    SQL EXEC DigDb, "CREATE TABLE IF NOT EXISTS chunks (id TEXT PRIMARY KEY, path TEXT, seq INTEGER, text TEXT)"
END SUB

SUB CloseIndex()
    SQL CLOSE DigDb
END SUB

' quoting per SQL: raddoppia gli apici singoli
FUNCTION SqlQuote(s AS STRING) AS STRING
    SqlQuote = STRINGS.REPLACE(s, "'", "''")
END FUNCTION

SUB IndexFile(path AS STRING)
    DIM text AS STRING, body AS STRING, piece AS STRING, cid AS STRING
    DIM pos AS INTEGER, seq AS INTEGER, lenAll AS INTEGER, take AS INTEGER
    ' Nota (parità backend): una chiamata a FUNCTION utente DENTRO gli argomenti di
    ' uno statement (SQL EXEC/QUERY) non è ancora emessa dall'IL ("Label N has not
    ' been marked"). Si precalcola su variabili locali: stesso risultato su 4 gambe.
    DIM qPath AS STRING, qCid AS STRING, qPiece AS STRING, sSeq AS STRING

    text = IO.READFILE(path)
    IF LEN(text) = 0 THEN EXIT SUB
    IF LEN(text) > 200000 THEN text = LEFT$(text, 200000)
    body = STRINGS.REPLACE(text, CHR$(13), " ")
    body = STRINGS.REPLACE(body, CHR$(10), " ")

    qPath = SqlQuote(path)
    SQL EXEC DigDb, "DELETE FROM chunks WHERE path = '" + qPath + "'"

    lenAll = LEN(body)
    pos = 1
    seq = 0
    DO WHILE pos <= lenAll
        take = g_chunk
        IF take < 1 THEN take = 300
        IF pos + take - 1 > lenAll THEN take = lenAll - pos + 1
        piece = STRINGS.TRIM(MID$(body, pos, take))
        IF LEN(piece) >= 24 THEN
            cid = path + "#" + LTRIM$(STR$(seq))
            AI RAG INDEX TEXT "DigDb", g_coll, cid, piece
            qCid = SqlQuote(cid)
            qPiece = SqlQuote(piece)
            sSeq = LTRIM$(STR$(seq))
            SQL EXEC DigDb, "INSERT OR REPLACE INTO chunks (id, path, seq, text) VALUES ('" + qCid + "', '" + qPath + "', " + sSeq + ", '" + qPiece + "')"
            g_nChunks = g_nChunks + 1
            seq = seq + 1
        END IF
        pos = pos + take
    LOOP
END SUB

' ============================================================================
' MODE: SEARCH — AI.SEARCH ibrida (embedding + parole esatte) -> metadati
' ============================================================================
SUB RunSearch()
    DIM q$, hits$
    DIM i AS INTEGER, cid$, scs$
    DIM res AS JSON

    q$ = STRINGS.TRIM(g_query)
    IF LEN(q$) = 0 THEN
        PRINT "error: nessuna query — imposta [search] query nel .digup.toml"
        EXIT SUB
    END IF
    IF NOT IO.FILEEXISTS(g_dbPath) THEN
        PRINT "error: indice assente ("; g_dbPath; ") — esegui prima il modo index"
        EXIT SUB
    END IF

    SQL OPEN g_dbPath ALIAS DigDb
    hits$ = AI.SEARCH("DigDb", g_coll, q$, g_top, 1)
    res = JSON.PARSE(hits$)
    PRINT "query        : "; q$
    PRINT "results      : "; AI.COUNT("DigDb", g_coll)
    FOR i = 0 TO g_top - 1
        cid$ = JSON.Extract(res, "hits[" + LTRIM$(STR$(i)) + "].sourceId")
        IF LEN(cid$) = 0 THEN EXIT FOR
        scs$ = JSON.Extract(res, "hits[" + LTRIM$(STR$(i)) + "].score")
        PrintHit(i + 1, scs$, cid$)
    NEXT i
    SQL CLOSE DigDb
END SUB

SUB PrintHit(rank AS INTEGER, scs$, cid$)
    DIM rs AS SQLRESULTSET
    DIM p$, t$, sq$
    DIM qCid2 AS STRING
    qCid2 = SqlQuote(cid$)
    SQL QUERY DigDb, rs, "SELECT path, seq, text FROM chunks WHERE id = '" + qCid2 + "'"
    IF SQL.FETCH(rs) THEN
        p$ = rs("path")
        t$ = rs("text")
        sq$ = rs("seq")
        SQL CLOSE RESULTSET rs
        PRINT rank; "  "; scs$; "  "; RelPath(p$); "#"; sq$
        PRINT "     "; Snippet(t$)
    ELSE
        SQL CLOSE RESULTSET rs
        PRINT rank; "  "; scs$; "  "; cid$
    END IF
END SUB

FUNCTION Format3(x AS SINGLE) AS STRING
    DIM v AS LONG, s AS STRING, neg AS INTEGER
    v = CLNG(x * 1000)
    s = STRINGS.TRIM(STR$(v))
    neg = 0
    IF STRINGS.STARTSWITH(s, "-") THEN
        neg = 1
        s = MID$(s, 2, LEN(s) - 1)
    END IF
    DO WHILE LEN(s) < 4
        s = "0" + s
    LOOP
    s = LEFT$(s, LEN(s) - 3) + "." + MID$(s, LEN(s) - 2, 3)
    IF neg = 1 THEN s = "-" + s
    Format3 = s
END FUNCTION

FUNCTION RelPath(p AS STRING) AS STRING
    DIM pre AS STRING
    pre = g_appDir + "/"
    IF STRINGS.STARTSWITH(p, pre) THEN
        RelPath = MID$(p, LEN(pre) + 1, LEN(p) - LEN(pre))
    ELSE
        RelPath = p
    END IF
END FUNCTION

FUNCTION Snippet(t AS STRING) AS STRING
    DIM s AS STRING
    s = STRINGS.TRIM(t)
    IF LEN(s) > 90 THEN s = LEFT$(s, 87) + "..."
    Snippet = s
END FUNCTION

' ============================================================================
' MODE: CHECK — provider di embedding e indice
' ============================================================================
SUB RunCheck()
    DIM active AS STRING
    active = AI.ACTIVE("embedding")
    PRINT "embedding    : "; active
    IF IO.FILEEXISTS(g_dbPath) THEN
        PRINT "index        : "; g_dbPath; " (presente)"
        SQL OPEN g_dbPath ALIAS DigDb
        PRINT "embeddings   : "; AI.COUNT("DigDb", g_coll)
        SQL CLOSE DigDb
        CountIndex()
    ELSE
        PRINT "index        : assente (esegui index)"
    END IF
    PRINT "check        : done"
END SUB

' ============================================================================
FUNCTION ComputeMode() AS STRING
    DIM m AS STRING
    m = "STATUS"
#IFDEF DIGUP_MODE_INDEX
    m = "INDEX"
#ENDIF
#IFDEF DIGUP_MODE_SEARCH
    m = "SEARCH"
#ENDIF
#IFDEF DIGUP_MODE_CHECK
    m = "CHECK"
#ENDIF
    ComputeMode = m
END FUNCTION
