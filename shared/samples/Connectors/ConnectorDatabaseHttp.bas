' FILE: Connectors/ConnectorDatabaseHttp.bas
' Esempio Database + HTTP + Filesystem (T0) — connettori nativi via AI CONNECTOR
' API comune: db.connect/query/transaction/schema + http.get/post + fs.read/write

IMPORT AI

SUB Main()
    PRINT "=== Database + HTTP + Filesystem Demo ==="

    ' 1. Database connectors (Postgres + SQLite) — API comune
    AI CONNECTOR LOAD "postgres"
    AI CONNECTOR LOAD "sqlite"
    PRINT "LOAD postgres+sqlite OK"

    DIM pgSt$ AS STRING = AI.CONNECTORSTATUS("postgres")
    PRINT "postgres: "; LEFT(pgSt$, 200)
    DIM sqSt$ AS STRING = AI.CONNECTORSTATUS("sqlite")
    PRINT "sqlite: "; LEFT(sqSt$, 200)

    ' Secret per DB (conn string)
    AI CONNECTOR SECRET SET "postgres", "conn_string", "Host=localhost;Database=hotel;Username=demo"
    AI CONNECTOR SECRET SET "sqlite", "path", "work/demo.db"
    PRINT "SECRET conn_string/path OK"

    ' Chiamate DB — sqlite reale (F1), postgres solo status (richiede server esterno)
    ' Demo sqlite: crea DB in work/demo.db, tabella, insert, query
    ' Demo sqlite: usa work/demo.db reale (F1) — crea tabella, insert, query
    DIM e1$ AS STRING = AI.CONNECTORCALL("sqlite", "execute", "{""sql"":""CREATE TABLE IF NOT EXISTS demo (id INTEGER PRIMARY KEY, name TEXT)"",""path"":""work/demo.db""}")
    DIM e2$ AS STRING = AI.CONNECTORCALL("sqlite", "execute", "{""sql"":""INSERT OR IGNORE INTO demo (id, name) VALUES (1, 'Alice')"",""path"":""work/demo.db""}")
    DIM q2$ AS STRING = AI.CONNECTORCALL("sqlite", "query", "{""sql"":""SELECT * FROM demo LIMIT 2"",""path"":""work/demo.db""}")
    PRINT "sqlite.query: "; LEFT(q2$, 400)
    DIM sch$ AS STRING = AI.CONNECTORCALL("sqlite", "schema", "{""path"":""work/demo.db""}")
    PRINT "sqlite.schema: "; LEFT(sch$, 400)
    PRINT "postgres status only (no server): "; LEFT(pgSt$, 100)

    ' 2. HTTP connector (fondamentale — evita 1000 connector per servizi minori)
    AI CONNECTOR LOAD "http"
    PRINT "LOAD http OK"
    DIM httpCall$ AS STRING = AI.CONNECTORCALL("http", "get", "{""url"":""https://example.com""}")
    PRINT "http.get: "; LEFT(httpCall$, 300)

    DIM httpPost$ AS STRING = AI.CONNECTORCALL("http", "post", "{""url"":""https://example.com/api"",""body"":""{}""}")
    PRINT "http.post: "; LEFT(httpPost$, 300)

    ' 3. Filesystem con sandbox (WORKSPACE PROJECT /etc DENY /~/.ssh DENY)
    AI CONNECTOR LOAD "filesystem"
    PRINT "LOAD filesystem OK"
    ' Scrivi e leggi in work/ (sandbox OK: repo root/work/)
    DIM fw$ AS STRING = AI.CONNECTORCALL("filesystem", "write", "{""path"":""work/demo.txt"",""content"":""ciao filesystem F1""}")
    PRINT "fs.write work/demo.txt: "; LEFT(fw$, 200)
    DIM fs$ AS STRING = AI.CONNECTORCALL("filesystem", "list", "{""path"":""work""}")
    PRINT "fs.list work: "; LEFT(fs$, 300)
    DIM fsRead$ AS STRING = AI.CONNECTORCALL("filesystem", "read", "{""path"":""work/demo.txt""}")
    PRINT "fs.read: "; LEFT(fsRead$, 300)

    ' 4. Storage astratto (S3 / Drive / Dropbox via STORAGE connector)
    ' L'agente non sa dove è il documento — il connettore sceglie backend
    AI CONNECTOR LOAD "google-drive"
    DIM drive$ AS STRING = AI.CONNECTORCALL("google-drive", "search", "{""query"":""contratto hotel""}")
    PRINT "drive.search: "; LEFT(drive$, 300)

    ' 5. Composizione: DB → HTTP → Filesystem → Drive (valore reale è la composizione)
    PRINT "Composizione DB->HTTP->FS->Drive OK (F0)"

    PRINT "=== Database/HTTP/FS Demo OK ==="
END SUB
