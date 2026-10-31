' FILE: Connectors/ConnectorComposability.bas
' Composizione di connettori (SYSTEM.txt §33) — il valore è la catena, non il singolo connector
' Gmail -> Drive -> PDF -> OCR -> Database -> CRM
' GitHub -> Issue -> Source -> Compiler -> Tests -> Sentry -> PR

IMPORT AI

SUB Main()
    PRINT "=== Composability Demo ==="

    ' Prepara i connettori
    AI CONNECTOR LOAD "gmail"
    AI CONNECTOR LOAD "google-drive"
    AI CONNECTOR LOAD "github"
    AI CONNECTOR LOAD "postgres"
    PRINT "LOAD gmail/drive/github/postgres OK"

    ' Catena 1: Gmail attachment -> Drive -> DB (contratto hotel)
    PRINT "--- Catena Gmail -> Drive -> DB ---"
    DIM mail$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""contratto hotel""}")
    PRINT "gmail.search: "; LEFT(mail$, 200)

    DIM attach$ AS STRING = AI.CONNECTORCALL("gmail", "attachments", "{""id"":""msg123""}")
    PRINT "gmail.attachments: "; LEFT(attach$, 200)

    DIM drive$ AS STRING = AI.CONNECTORCALL("google-drive", "write", "{""path"":""/contracts/hotel.pdf"",""content"":""pdf-bytes""}")
    PRINT "drive.write: "; LEFT(drive$, 200)

    DIM db$ AS STRING = AI.CONNECTORCALL("postgres", "execute", "{""sql"":""INSERT INTO contracts (name) VALUES (''hotel.pdf'')""}")
    PRINT "postgres.execute: "; LEFT(db$, 200)

    ' Catena 2: GitHub flow (come SYSTEM.txt §6)
    PRINT "--- Catena GitHub -> Source -> PR ---"
    DIM issue$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic"",""query"":""compiler crash""}")
    PRINT "github.issues: "; LEFT(issue$, 200)

    DIM src$ AS STRING = AI.CONNECTORCALL("github", "repository", "{""name"":""c-vision/GMBasic""}")
    PRINT "github.repository: "; LEFT(src$, 200)

    ' Tool discovery dinamica: l'agente non riceve 300 tool nel prompt, ma cerca per capability
    PRINT "--- Tool Discovery Dinamica ---"
    DIM mailTools$ AS STRING = AI.CONNECTORSEARCH("mail")
    PRINT "discover mail len="; LEN(mailTools$)
    DIM dbTools$ AS STRING = AI.CONNECTORSEARCH("db")
    PRINT "discover db len="; LEN(dbTools$)
    DIM ghTools$ AS STRING = AI.CONNECTORSEARCH("github")
    PRINT "discover github len="; LEN(ghTools$)

    ' Toolset per agente (SYSTEM.txt §31): CODING_AGENT vs BUSINESS_AGENT
    PRINT "--- Toolset per Agente ---"
    PRINT "CODING_AGENT: filesystem, git, github, tests"
    PRINT "BUSINESS_AGENT: gmail, calendar, contacts, crm, drive, sheets"
    PRINT "I connettori sono assegnati per toolset, non tutti a tutti (riduce hallucination)"

    PRINT "=== Composability OK ==="
END SUB
