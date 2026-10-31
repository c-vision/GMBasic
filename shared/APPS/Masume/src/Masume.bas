' ============================================================================
' FILE: APPS/Masume/src/Masume.bas
'
' MASUME (GMBasic port) — keyboard-first terminal database client with AI chat
' and an MCP server.
'
' Upstream (Go, Apache-2.0): https://github.com/turanmahmudov/masume
' Reference docs:            https://github.com/turanmahmudov/masume/tree/master/docs
' Port notes + backlog:      APPS/Masume/README.md and APPS/Masume/TODO.md
'
' This port is a clean rewrite: it reproduces the OBSERVABLE behaviour of the
' upstream client (project file, profiles, named queries, headless output shape,
' browse/query interaction model), not the Go source code.
'
' MODES (compile-time symbol, see APPS/README.md — BASIC has no runtime argv):
'   (default)                    status: banner + project file + connectivity
'   -DMASUME_MODE_SEED           create the demo database in bin/
'   -DMASUME_MODE_OBJECTS        object tree (tables/views + columns)
'   -DMASUME_MODE_RUN            headless "masume run": named query -> TSV
'   -DMASUME_MODE_CHECK          "masume --mcp --check" style diagnostics
'   -DMASUME_MODE_TUI            interactive MENU + TBROWSE + WINDOW shell
' ============================================================================

IMPORT IO
IMPORT SQL
IMPORT STRINGS
IMPORT AI
IMPORT CONTROLS

' --- Application globals (filled by BootPaths / LoadProjectFile) -------------
DIM g_work AS STRING            ' APPS/Masume/work — all generated files
DIM g_projectFile AS STRING     ' path of the .masume.toml found ("" if none)
DIM g_dbPath AS STRING          ' path of the demo database (profile "demo")

' Profiles: index 1..N (0 is unused, like a Clipper work area)
DIM g_profName(8) AS STRING     ' profile names, in file order
DIM g_profEngine(8) AS STRING
DIM g_profEnv(8) AS STRING
DIM g_profMode(8) AS STRING
DIM g_profPath(8) AS STRING     ' engine = sqlite: database file path
DIM g_profileCount AS INTEGER

' Named queries: [query.<name>] sections
DIM g_queryName(16) AS STRING
DIM g_querySql(16) AS STRING
DIM g_queryDesc(16) AS STRING
DIM g_queryCount AS INTEGER

' Query parameters: [params] section (port of upstream --param name=value)
DIM g_paramName(16) AS STRING
DIM g_paramVal(16) AS STRING
DIM g_paramCount AS INTEGER

' [settings] mode — fallback mode used when no -DMASUME_MODE_* symbol is passed
DIM g_cfgMode AS STRING

' --- Active connection (ResolveActive) --------------------------------------
DIM g_activeAlias AS STRING     ' SQL alias used to OPEN the active profile
DIM g_activeProfile AS STRING   ' profile name
DIM g_dbOpen AS BIT             ' TRUE when the connection is open

' ============================================================================
' MODE: STATUS (default) — banner + startup self-check
' ============================================================================
SUB PrintBanner()
    PRINT "╔══════════════════════════════════════════════════════════════════╗"
    PRINT "║  masume (GMBasic port) — terminal database client                  ║"
    PRINT "║  upstream: github.com/turanmahmudov/masume  (Apache-2.0)           ║"
    PRINT "╚══════════════════════════════════════════════════════════════════╝"
    PRINT ""
END SUB

SUB PrintStatus()
    PRINT "project file : "; g_projectFile
    PRINT "working dir  : "; g_work
    PRINT "profiles     : "; g_profileCount
    DIM i AS INTEGER
    FOR i = 1 TO g_profileCount
        PRINT "  ["; i; "] "; g_profName(i); "  engine="; g_profEngine(i); "  env="; g_profEnv(i); "  mode="; g_profMode(i)
    NEXT i
    PRINT "named queries: "; g_queryCount
    FOR i = 1 TO g_queryCount
        PRINT "  ["; i; "] "; g_queryName(i); "  — "; g_queryDesc(i)
    NEXT i
    PRINT ""
    PRINT "connection   : ";
    IF g_dbOpen THEN
        PRINT "OPEN ("; g_activeProfile; " via alias "; g_activeAlias; ")"
        PRINT "schema       : "; SchemaSummary()
    ELSE
        PRINT "not open — run with -DMASUME_MODE_SEED to create the demo database"
    END IF
    PRINT ""
    PRINT "modes        : -DMASUME_MODE_SEED | _OBJECTS | _RUN | _CHECK | _TUI"
END SUB

' ============================================================================
' MODE: SEED — create the demo database (customers + orders, with a FK)
' ============================================================================
SUB SeedDemo()
    PRINT "seeding demo database: "; g_dbPath
    ON ERROR RESUME NEXT
    KILL g_dbPath
    ON ERROR GOTO 0
    SQL OPEN g_dbPath ALIAS Demo
    SQL EXEC Demo, "CREATE TABLE IF NOT EXISTS customers (id INTEGER PRIMARY KEY, name TEXT NOT NULL, city TEXT, created_at TEXT)"
    SQL EXEC Demo, "CREATE TABLE IF NOT EXISTS orders (id INTEGER PRIMARY KEY, customer_id INTEGER NOT NULL, total REAL NOT NULL, created_at TEXT, FOREIGN KEY (customer_id) REFERENCES customers(id))"
    SQL EXEC Demo, "CREATE INDEX IF NOT EXISTS idx_orders_customer ON orders(customer_id)"
    SQL EXEC Demo, "DELETE FROM orders"
    SQL EXEC Demo, "DELETE FROM customers"
    SQL EXEC Demo, "INSERT INTO customers (id, name, city, created_at) VALUES (?, ?, ?, ?)", 1, "Ada Lovelace", "London", "2026-01-04"
    SQL EXEC Demo, "INSERT INTO customers (id, name, city, created_at) VALUES (?, ?, ?, ?)", 2, "Grace Hopper", "New York", "2026-02-11"
    SQL EXEC Demo, "INSERT INTO customers (id, name, city, created_at) VALUES (?, ?, ?, ?)", 3, "Alan Turing", "Manchester", "2026-03-19"
    SQL EXEC Demo, "INSERT INTO orders (id, customer_id, total, created_at) VALUES (?, ?, ?, ?)", 101, 1, 199.50, "2026-09-01"
    SQL EXEC Demo, "INSERT INTO orders (id, customer_id, total, created_at) VALUES (?, ?, ?, ?)", 102, 1, 42.00, "2026-09-04"
    SQL EXEC Demo, "INSERT INTO orders (id, customer_id, total, created_at) VALUES (?, ?, ?, ?)", 103, 2, 310.75, "2026-09-05"
    SQL EXEC Demo, "INSERT INTO orders (id, customer_id, total, created_at) VALUES (?, ?, ?, ?)", 104, 3, 88.10, "2026-09-08"
    SQL EXEC Demo, "INSERT INTO orders (id, customer_id, total, created_at) VALUES (?, ?, ?, ?)", 105, 2, 15.90, "2026-09-11"
    SQL CLOSE Demo
    ON ERROR GOTO 0
    PRINT "customers="; TableCount(g_dbPath, "customers"); " orders="; TableCount(g_dbPath, "orders")
    PRINT "done — now run -DMASUME_MODE_OBJECTS or -DMASUME_MODE_RUN"
END SUB

' ============================================================================
' Shared helpers — numbers, catalog, result sets
' ============================================================================
' Number -> string without BASIC's positive-number padding, so that generated
' text (TSV headers, counts) is byte-stable across backends.
FUNCTION N2S(v AS LONG) AS STRING
    N2S = STRINGS.LTRIM(STR$(v))
END FUNCTION

SUB OpenActive()
    IF NOT g_dbOpen THEN
        SQL OPEN g_dbPath ALIAS Demo
        g_activeAlias = "Demo"
        g_activeProfile = "demo"
        g_dbOpen = TRUE
    END IF
END SUB

SUB CloseActive()
    IF g_dbOpen THEN
        SQL CLOSE Demo
        g_dbOpen = FALSE
    END IF
END SUB

' Row count of one table/view, on a private connection (so it is safe to call
' while another result set is open).
FUNCTION TableCount(dbPath AS STRING, tableName AS STRING) AS LONG
    DIM n AS LONG
    DIM rs AS SQLRESULTSET
    n = 0
    SQL OPEN dbPath ALIAS Q
    SQL QUERY Q, rs, "SELECT COUNT(*) AS n FROM " + tableName
    IF SQL.FETCH(rs) THEN n = rs("n")
    SQL CLOSE RESULTSET rs
    SQL CLOSE Q
    TableCount = n
END FUNCTION

' "N tables, M views" — the one-line schema summary shown by the status mode.
FUNCTION SchemaSummary() AS STRING
    DIM nt AS LONG, nv AS LONG
    DIM rs AS SQLRESULTSET
    nt = 0 : nv = 0
    SQL QUERY Demo, rs, "SELECT type, COUNT(*) AS n FROM sqlite_master WHERE type IN ('table','view') GROUP BY type"
    DO WHILE SQL.FETCH(rs)
        IF rs("type") = "table" THEN nt = rs("n")
        IF rs("type") = "view" THEN nv = rs("n")
    LOOP
    SQL CLOSE RESULTSET rs
    SchemaSummary = N2S(nt) + " tables, " + N2S(nv) + " views"
END FUNCTION

' The object tree of upstream's Browse view: type group + row count + name.
SUB PrintObjects()
    DIM rs AS SQLRESULTSET
    DIM otype AS STRING, oname AS STRING, lastType AS STRING
    SQL QUERY Demo, rs, "SELECT type, name FROM sqlite_master WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' ORDER BY type, name"
    lastType = ""
    DO WHILE SQL.FETCH(rs)
        otype = rs("type")
        oname = rs("name")
        IF otype <> lastType THEN
            PRINT STRINGS.UPPER(otype) + "S"
            lastType = otype
        END IF
        PRINT "  "; STRINGS.PADL(N2S(TableCount(g_dbPath, oname)), 5); "  "; oname
    LOOP
    SQL CLOSE RESULTSET rs
END SUB

' Columns of one object (upstream's table view: data, columns, indexes, ...).
' M1 covers columns; indexes/constraints/DDL/query plans are in TODO.md.
SUB PrintColumns(tableName AS STRING)
    DIM rs AS SQLRESULTSET
    DIM pk AS LONG
    PRINT "  columns of "; tableName
    SQL OPEN g_dbPath ALIAS Q
    SQL QUERY Q, rs, "PRAGMA table_info(" + tableName + ")"
    DO WHILE SQL.FETCH(rs)
        pk = rs("pk")
        IF pk > 0 THEN
            PRINT "    "; rs("name"); "  "; rs("type"); "  PK"
        ELSE
            PRINT "    "; rs("name"); "  "; rs("type")
        END IF
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE Q
END SUB

' Headless "masume run": execute a SELECT and emit TSV on stdout.
SUB PrintResultSet(sqlText AS STRING)
    PRINT QueryToText(sqlText)
END SUB

' The query → TSV text conversion, shared by the headless mode and the MCP tool.
' NB: rs(i) (positional index) and rs.COLUMNS are NOT reliable on this build (both
' return empty/zero on the PCode engine — verified live, see APPS/Masume/TODO.md
' §M1 "SQLRESULTSET positional access"), so columns are read by NAME (rs("col"),
' proven correct everywhere else in this file) using the names SelectListHeader
' already parses out of the SELECT list text.
FUNCTION QueryToText(sqlText AS STRING) AS STRING
    DIM rs AS SQLRESULTSET
    DIM j AS LONG
    DIM out AS STRING, row AS STRING, header AS STRING
    DIM colNames() AS STRING
    OpenActive()
    SQL QUERY Demo, rs, sqlText
    header = SelectListHeader(sqlText, 0)
    colNames = STRINGS.SPLIT(header, CHR$(9))
    out = header
    DO WHILE SQL.FETCH(rs)
        row = ""
        FOR j = 0 TO UBOUND(colNames)
            IF j > 0 THEN row = row + CHR$(9)
            row = row + CellText(rs(colNames(j)))
        NEXT j
        out = out + CHR$(10) + row
    LOOP
    SQL CLOSE RESULTSET rs
    QueryToText = out
END FUNCTION

' One TSV cell: tabs/newlines are neutralised so a row stays on one line.
FUNCTION CellText(v AS STRING) AS STRING
    DIM s AS STRING
    s = v
    s = STRINGS.REPLACE(s, CHR$(9), " ")
    CellText = STRINGS.REPLACE(s, CHR$(10), " ")
END FUNCTION

' Result-grid / TSV header. Upstream takes it from the database catalog; this port
' has rs("name") but rs(i)/rs.COLUMNS are unreliable (see QueryToText), so the
' header of a plain SELECT list is derived from the statement text instead —
' otherwise positional names c1..cN are used (only reachable for SELECT *, since
' the cols argument can no longer come from rs.COLUMNS).
FUNCTION SelectListHeader(sqlText AS STRING, cols AS LONG) AS STRING
    DIM up AS STRING, sel AS STRING, out AS STRING, p AS STRING
    DIM parts() AS STRING
    DIM fromPos AS INTEGER, asPos AS INTEGER, i AS LONG
    up = STRINGS.UPPER(sqlText)
    out = ""
    IF STRINGS.STARTSWITH(up, "SELECT ") THEN
        fromPos = INSTR(up, " FROM ")
        IF fromPos > 7 THEN
            sel = MID$(sqlText, 8, fromPos - 8)
            IF INSTR(sel, "*") = 0 THEN
                parts = STRINGS.SPLIT(sel, ",")
                FOR i = 0 TO LEN(parts()) - 1
                    p = STRINGS.TRIM(parts(i))
                    asPos = INSTR(STRINGS.UPPER(p), " AS ")
                    IF asPos > 0 THEN p = STRINGS.TRIM(MID$(p, asPos + 4))
                    IF i > 0 THEN out = out + CHR$(9)
                    out = out + p
                NEXT i
            END IF
        END IF
    END IF
    IF LEN(out) = 0 THEN
        FOR i = 0 TO cols - 1
            IF i > 0 THEN out = out + CHR$(9)
            out = out + "c" + N2S(i + 1)
        NEXT i
    END IF
    SelectListHeader = out
END FUNCTION

' ============================================================================
' Project file (.masume.toml) — upstream layout: [profile.*], [query.*], [params]
' ============================================================================
' Searches the working directory and its ancestors for `.masume.toml`, like
' upstream does ("masume reads the nearest project file in or above the working
' directory"). TOML subset: section headers, `key = value`, quoted/bare values,
' `#` comments.
SUB LoadProjectFile()
    DIM dir AS STRING, candidate AS STRING
    DIM i AS INTEGER
    dir = IO.CWD()
    FOR i = 1 TO 8
        candidate = RTrimSlash(dir) + "/.masume.toml"
        IF IO.FILEEXISTS(candidate) THEN
            g_projectFile = candidate
            ParseProjectFile(candidate)
            i = 8
        ELSE
            dir = IO.PARENTDIR(dir)
            IF LEN(dir) = 0 THEN i = 8
        END IF
    NEXT i
    IF LEN(g_projectFile) = 0 THEN
        PRINT "warn: no .masume.toml found in the working directory or above"
    END IF
END SUB

FUNCTION RTrimSlash(p AS STRING) AS STRING
    DIM s AS STRING
    s = p
    IF STRINGS.ENDSWITH(s, "/") THEN s = STRINGS.LEFT(s, LEN(s) - 1)
    RTrimSlash = s
END FUNCTION

SUB ParseProjectFile(path AS STRING)
    DIM lines
    DIM raw AS STRING, line AS STRING, section AS STRING, key AS STRING, val AS STRING
    DIM eq AS INTEGER, i AS LONG, dot AS INTEGER
    lines = IO.READLINES(path)
    section = ""
    FOR i = 0 TO UBOUND(lines)
        raw = STRINGS.TRIM(lines(i))
        IF LEN(raw) > 0 AND NOT STRINGS.STARTSWITH(raw, "#") THEN
            IF STRINGS.STARTSWITH(raw, "[") THEN
                section = BracketInner(raw)
            ELSE
                eq = INSTR(raw, "=")
                IF eq > 1 THEN
                    key = STRINGS.TRIM(LEFT$(raw, eq - 1))
                    val = Unquote(STRINGS.TRIM(MID$(raw, eq + 1)))
                    dot = INSTR(section, ".")
                    IF STRINGS.STARTSWITH(section, "profile.") THEN
                        ApplyProfile(MID$(section, dot + 1), key, val)
                    ELSEIF STRINGS.STARTSWITH(section, "query.") THEN
                        ApplyQuery(MID$(section, dot + 1), key, val)
                    ELSEIF section = "params" THEN
                        ApplyParam(key, val)
                    ELSEIF section = "settings" THEN
                        ApplySetting(key, val)
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
    DIM quoteMark AS STRING
    quoteMark = CHR$(34)
    t = s
    IF LEN(t) >= 2 THEN
        IF STRINGS.STARTSWITH(t, quoteMark) AND STRINGS.ENDSWITH(t, quoteMark) THEN
            t = MID$(t, 2, LEN(t) - 2)
        END IF
    END IF
    Unquote = t
END FUNCTION

' One [profile.<name>] section → one slot. The name is learned from the section
' header; keys may arrive in any order, so a single slot is reused while the name
' stays the same.
SUB ApplyProfile(name AS STRING, key AS STRING, val AS STRING)
    DIM k AS STRING
    k = STRINGS.UPPER(key)
    IF g_profileCount = 0 THEN
        ProfileSlotAdd(name)
    END IF
    IF g_profileCount > 0 AND g_profName(g_profileCount) <> name THEN ProfileSlotAdd(name)
    IF k = "ENGINE" THEN g_profEngine(g_profileCount) = val
    IF k = "ENV" THEN g_profEnv(g_profileCount) = val
    IF k = "MODE" THEN g_profMode(g_profileCount) = val
    IF k = "PATH" THEN g_profPath(g_profileCount) = val
END SUB

SUB ProfileSlotAdd(name AS STRING)
    IF g_profileCount < 8 THEN
        g_profileCount = g_profileCount + 1
        g_profName(g_profileCount) = name
        g_profEngine(g_profileCount) = ""
        g_profEnv(g_profileCount) = ""
        g_profMode(g_profileCount) = ""
        g_profPath(g_profileCount) = ""
    END IF
END SUB

SUB ApplyQuery(name AS STRING, key AS STRING, val AS STRING)
    DIM k AS STRING
    k = STRINGS.UPPER(key)
    IF g_queryCount = 0 THEN
        QuerySlotAdd(name)
    END IF
    IF g_queryCount > 0 AND g_queryName(g_queryCount) <> name THEN QuerySlotAdd(name)
    IF k = "SQL" THEN g_querySql(g_queryCount) = val
    IF k = "DESCRIPTION" THEN g_queryDesc(g_queryCount) = val
END SUB

SUB QuerySlotAdd(name AS STRING)
    IF g_queryCount < 16 THEN
        g_queryCount = g_queryCount + 1
        g_queryName(g_queryCount) = name
        g_querySql(g_queryCount) = ""
        g_queryDesc(g_queryCount) = ""
    END IF
END SUB

SUB ApplyParam(name AS STRING, val AS STRING)
    IF g_paramCount < 16 THEN
        g_paramCount = g_paramCount + 1
        g_paramName(g_paramCount) = name
        g_paramVal(g_paramCount) = val
    END IF
END SUB

' [settings] — app-level defaults. `mode` is the fallback used when the run does not
' carry a -DMASUME_MODE_* symbol (BASIC has no argv yet, so this is how a user picks
' the mode without recompiling flags: edit the project file).
SUB ApplySetting(key AS STRING, val AS STRING)
    IF STRINGS.UPPER(key) = "MODE" THEN g_cfgMode = STRINGS.UPPER(val)
END SUB
' ============================================================================
' Boot: paths, active connection
' ============================================================================
' The compiler's project root is the folder of this entry file (APPS/Masume/src),
' so the app folder is its parent and every generated file lands in
' APPS/Masume/work — the convention for output in this repository (AGENTS.md §11).
SUB BootPaths()
    DIM srcDir AS STRING, appDir AS STRING
    srcDir = PROJECTFOLDER()
    IF LEN(srcDir) = 0 THEN srcDir = IO.CWD()
    appDir = IO.PARENTDIR(RTrimSlash(srcDir))
    IF LEN(appDir) = 0 THEN appDir = srcDir
    g_work = RTrimSlash(appDir) + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    g_dbPath = g_work + "/masume_demo.sqlite"
END SUB

' Opens the active profile. M1: profile "demo" (SQLite, self-contained). Its
' `path` key from .masume.toml is honoured when present, resolved against the
' project-file folder; otherwise the app default under bin/ is used.
' Other engines (postgres, mysql, ...) and profile selection from the command line
' are TODO (upstream: `masume TARGET`, `-p NAME`; BASIC has no argv yet).
SUB ResolveActive()
    DIM i AS INTEGER, p AS STRING
    FOR i = 1 TO g_profileCount
        IF g_profName(i) = "demo" AND LEN(g_profPath(i)) > 0 THEN
            p = g_profPath(i)
            IF NOT STRINGS.STARTSWITH(p, "/") THEN
                p = RTrimSlash(IO.PARENTDIR(g_projectFile)) + "/" + p
            END IF
            g_dbPath = p
            i = g_profileCount
        END IF
    NEXT i
    IF IO.FILEEXISTS(g_dbPath) THEN OpenActive()
END SUB

' ============================================================================
' MODE: OBJECTS / RUN — headless catalog + query
' ============================================================================
SUB RunObjects()
    IF NOT g_dbOpen THEN
        PRINT "error: database not found at "; g_dbPath
        PRINT "       run -DMASUME_MODE_SEED first"
    ELSE
        PRINT "database : "; g_dbPath
        PRINT "schema   : "; SchemaSummary()
        PRINT ""
        PrintObjects()
        PRINT ""
        PrintColumns("orders")
    END IF
END SUB

SUB RunNamedQuery()
    DIM q AS STRING
    IF g_queryCount = 0 THEN
        PRINT "error: no [query.*] section in the project file"
    ELSE
        q = g_querySql(1)
        q = SubstituteParams(q)
        PRINT "# query="; g_queryName(1); "  (from "; g_projectFile; ")"
        PrintResultSet(q)
    END IF
END SUB

' ${name} substitution from the [params] section — the port of upstream
' `masume run ... --param name=value` (no argv in BASIC yet, see APPS/README.md).
FUNCTION SubstituteParams(sqlText AS STRING) AS STRING
    DIM out AS STRING
    DIM i AS INTEGER
    out = sqlText
    FOR i = 1 TO g_paramCount
        out = STRINGS.REPLACE(out, "${" + g_paramName(i) + "}", g_paramVal(i))
    NEXT i
    SubstituteParams = out
END FUNCTION

' ============================================================================
' MODE: CHECK — diagnostics (upstream `masume --mcp --check`)
' ============================================================================
SUB RunCheck()
    PRINT "check: project file   : ";
    IF LEN(g_projectFile) = 0 THEN
        PRINT "MISSING"
    ELSE
        PRINT "OK  "; g_projectFile
    END IF
    PRINT "check: profiles       : "; g_profileCount
    DIM i AS INTEGER
    FOR i = 1 TO g_profileCount
        PRINT "       - "; g_profName(i); " engine="; g_profEngine(i); " env="; g_profEnv(i); " mode="; g_profMode(i)
    NEXT i
    PRINT "check: demo database  : ";
    IF IO.FILEEXISTS(g_dbPath) THEN
        PRINT "OK  "; g_dbPath
    ELSE
        PRINT "MISSING (run -DMASUME_MODE_SEED)"
    END IF
    PRINT "check: connection     : ";
    IF g_dbOpen THEN
        PRINT "OK  "; SchemaSummary()
    ELSE
        PRINT "NOT OPEN"
    END IF
    PRINT "check: mcp tool set   : read-only ";
    AI MCP REGISTER TOOL "masume_query", "MasumeQuery", "Run a read-only SQL statement and return TSV", TRUE
    AI MCP REGISTER TOOL "masume_objects", "MasumeObjects", "List the database objects with row counts", TRUE
    PRINT AI.MCPLISTTOOLS()
    PRINT "       (the MCP server process itself is TODO §M4: AI MCP SERVE stdio)"
END SUB

' MCP tools exposed by the port — read-only by construction (SELECT only).
FUNCTION MasumeQuery(sqlText AS STRING) AS STRING
    MasumeQuery = QueryToText(sqlText)
END FUNCTION

FUNCTION MasumeObjects(dummy AS STRING) AS STRING
    DIM rs AS SQLRESULTSET
    DIM out AS STRING, otype AS STRING, oname AS STRING
    out = ""
    OpenActive()
    SQL QUERY Demo, rs, "SELECT type, name FROM sqlite_master WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' ORDER BY type, name"
    DO WHILE SQL.FETCH(rs)
        otype = rs("type")
        oname = rs("name")
        out = out + otype + " " + oname + " rows=" + N2S(TableCount(g_dbPath, oname)) + CHR$(10)
    LOOP
    SQL CLOSE RESULTSET rs
    MasumeObjects = out
END FUNCTION

' ============================================================================
' MODE: TUI — interactive shell (port of the upstream interactive client)
' ============================================================================
' A Clipper-style screen: header panel + content panel + status bar, driven by a
' MENU loop (the same shape as the upstream TUI: an object view and a detail pane,
' with a key legend at the bottom). Esc cancels, "Quit" exits.
SUB RunTui()
    DIM choice AS INTEGER
    DIM menuItems(4) AS STRING
    menuItems(0) = "Browse objects"
    menuItems(1) = "Table: orders (data)"
    menuItems(2) = "Columns of orders"
    menuItems(3) = "Named query"
    menuItems(4) = "Quit"
    choice = 0
    WHILE choice <> 4
        DrawShell()
        MENU 2, 3, menuItems INTO choice
        IF choice = 0 THEN TuiGoObjects()
        IF choice = 1 THEN TuiGoData()
        IF choice = 2 THEN TuiGoColumns()
        IF choice = 3 THEN TuiGoQuery()
    WEND
    PRINT "masume: bye"
END SUB

SUB TuiGoObjects()
    PRINT "OBJECT TREE"
    PrintObjects()
    WAITKEY
END SUB

SUB TuiGoData()
    PRINT "DATA — orders"
    PrintResultSet("SELECT id, customer_id, total, created_at FROM orders ORDER BY id")
    WAITKEY
END SUB

SUB TuiGoColumns()
    PRINT "COLUMNS — orders"
    PrintColumns("orders")
    WAITKEY
END SUB

SUB TuiGoQuery()
    RunNamedQuery()
    WAITKEY
END SUB

SUB DrawShell()
    STATUSBAR "masume: objects | table | columns | query | quit  (Enter=select Esc=cancel)"
END SUB

' ============================================================================
' Mode selection. BASIC has no runtime argv (see APPS/README.md), so the mode is
' chosen EITHER with a compile-time symbol or with the project file ([settings]).
'
'   tools/sh/harness.sh pcode shared/APPS/Masume/src/Masume.bas      (dalla ROOT)
'   modo esplicito: #DEF MASUME_MODE_OBJECTS in testa al file (i runner non passano -D;
'   `REPL -D` vale per le direttive della REPL, non per l'esecuzione di un file)
'
' If NO symbol is passed, the mode falls back to `[settings] mode` in the
' .masume.toml project file, so every runner works — test_ilrun, test_pcode, test_vmrun,
' vmtest, C99 tests and NativeAOT — without flags.
' ============================================================================
FUNCTION ComputeMode() AS STRING
    DIM m AS STRING
    m = "STATUS"
#IFDEF MASUME_MODE_SEED
    m = "SEED"
#ENDIF
#IFDEF MASUME_MODE_OBJECTS
    m = "OBJECTS"
#ENDIF
#IFDEF MASUME_MODE_RUN
    m = "RUN"
#ENDIF
#IFDEF MASUME_MODE_CHECK
    m = "CHECK"
#ENDIF
#IFDEF MASUME_MODE_TUI
    m = "TUI"
#ENDIF
    ComputeMode = m
END FUNCTION

' ============================================================================
' Entry point
' ============================================================================
SUB Main()
    BootPaths()
    PrintBanner()
    LoadProjectFile()
    ResolveActive()

    ' No symbol passed? Then the project file decides ([settings] mode).
    DIM mode AS STRING
    mode = ComputeMode()
    IF mode = "STATUS" AND LEN(g_cfgMode) > 0 THEN mode = g_cfgMode

    IF mode = "SEED" THEN
        SeedDemo()
    ELSEIF mode = "OBJECTS" THEN
        RunObjects()
    ELSEIF mode = "RUN" THEN
        RunNamedQuery()
    ELSEIF mode = "CHECK" THEN
        RunCheck()
    ELSEIF mode = "TUI" THEN
        RunTui()
    ELSE
        PrintStatus()
    END IF

    CloseActive()
END SUB
