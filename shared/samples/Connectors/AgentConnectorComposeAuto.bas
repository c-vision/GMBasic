' FILE: Connectors/AgentConnectorComposeAuto.bas
' Agente che compone più connettori (SYSTEM.txt §33) — Gmail → Drive → Postgres → GitHub
' Senza REGISTER esplicito: l'agente vede tutti i 52 via discovery automatica

IMPORT AI

SUB Main()
    PRINT "=== Agent Compose Auto ==="

    ' Carica i connettori una volta (globali)
    AI CONNECTOR LOAD "gmail"
    AI CONNECTOR LOAD "google-drive"
    AI CONNECTOR LOAD "postgres"
    AI CONNECTOR LOAD "github"
    PRINT "LOAD gmail/drive/postgres/github OK"

    ' Configura i segreti una volta (persistiti)
    AI CONNECTOR SECRET SET "postgres", "path", "work/agent_compose.db"
    AI CONNECTOR SECRET SET "github", "api_key", "ghp_demo"
    PRINT "SECRET postgres/github OK"

    AI AGENT CREATE "ComposeAgent"
    AI AGENT SET "ComposeAgent" "systemPrompt" "You are a business agent. Compose gmail, drive, postgres and github to automate workflows."
    PRINT "Agent ComposeAgent creato (vede gmail+drive+postgres+github automaticamente)"

    ' Catena 1: Gmail → Drive → Postgres (contratto hotel)
    PRINT "--- Catena Gmail → Drive → Postgres ---"
    DIM mail$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""contratto hotel""}")
    PRINT "gmail.search: "; LEFT(mail$, 150)

    DIM drive$ AS STRING = AI.CONNECTORCALL("google-drive", "search", "{""query"":""contratto hotel""}")
    PRINT "drive.search: "; LEFT(drive$, 150)

    ' Postgres reale (F1): crea tabella e inserisce (via Connector.Database.cs:30)
    DIM cr$ AS STRING = AI.CONNECTORCALL("postgres", "execute", "{""sql"":""CREATE TABLE IF NOT EXISTS compose_demo (id SERIAL PRIMARY KEY, name TEXT)"",""path"":""work/agent_compose.db""}")
    PRINT "postgres CREATE: "; LEFT(cr$, 150)
    DIM ins$ AS STRING = AI.CONNECTORCALL("postgres", "execute", "{""sql"":""INSERT INTO compose_demo (name) VALUES ('hotel.pdf') ON CONFLICT DO NOTHING"",""path"":""work/agent_compose.db""}")
    PRINT "postgres INSERT: "; LEFT(ins$ , 150)
    DIM sel$ AS STRING = AI.CONNECTORCALL("postgres", "query", "{""sql"":""SELECT * FROM compose_demo LIMIT 2"",""path"":""work/agent_compose.db""}")
    PRINT "postgres SELECT: "; LEFT(sel$, 300)

    ' Catena 2: GitHub flow (come SYSTEM.txt §6: issue → source → PR)
    PRINT "--- Catena GitHub ---"
    DIM iss$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic"",""state"":""open""}")
    PRINT "github.issues: "; LEFT(iss$, 200)

    ' Tool discovery dinamica (§32): l'agente non ha 300 tool nel prompt, ma cerca
    PRINT "--- Discovery per capability ---"
    DIM d1$ AS STRING = AI.CONNECTORSEARCH("mail")
    PRINT "SEARCH mail: "; LEFT(d1$, 200)
    DIM d2$ AS STRING = AI.CONNECTORSEARCH("db")
    PRINT "SEARCH db: "; LEFT(d2$, 200)

    ' Con LLM reale: AI.AGENTASKSYNC("ComposeAgent", "prendi le fatture da Gmail, salvale su Drive e registrale in Postgres")
    ' → tool_calls: gmail.search → google-drive.write → postgres.execute → risposta
    ' Il bridge Connector.AgentBridge.cs:30 gestisce policy/rate-limit/approval per ogni tool_call

    AI AGENT DELETE "ComposeAgent"
    PRINT "=== Compose Auto OK ==="
END SUB
