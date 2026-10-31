' FILE: Connectors/AgentConnectorAutoDiscovery.bas
' Agente con discovery automatica — ogni agente creato conosce i 52 connettori senza AI AGENT REGISTER TOOL
' Dimostra: AI CONNECTOR LOAD → nuovo agente auto-vede i tool (ResolveAgentTools, ConnectorRegistry → McpToolRegistry)

IMPORT AI

FUNCTION GmailSearch(query AS STRING) AS STRING
    ' Mock locale per test senza LLM reale: il connettore reale farebbe Gmail API
    DIM q$ AS STRING = "{" + CHR(34) + "query" + CHR(34) + ":" + CHR(34) + query + CHR(34) + "}"
    GmailSearch = AI.CONNECTORCALL("gmail", "search", q$)
END FUNCTION

SUB Main()
    PRINT "=== Agent Auto-Discovery ==="

    ' 1. Carica i connettori (una volta, globali — persistiti in config/connectors.json)
    AI CONNECTOR LOAD "gmail"
    AI CONNECTOR LOAD "filesystem"
    AI CONNECTOR LOAD "github"
    PRINT "LOAD gmail/filesystem/github OK"

    ' 2. Verifica discovery: l'agente non ha bisogno di REGISTER esplicito
    DIM found$ AS STRING = AI.CONNECTORSEARCH("mail")
    PRINT "SEARCH mail (discovery): "; LEFT(found$, 300)
    ' → trova gmail (mail.read) anche se nessun agente ha fatto REGISTER

    ' 3. Crea agente SENZA configurazione connettori — li vede automaticamente
    AI AGENT CREATE "AutoAgent"
    AI AGENT SET "AutoAgent" "systemPrompt" "You are a helpful assistant with access to gmail, filesystem and github."
    PRINT "Agent AutoAgent creato (senza REGISTER)"

    ' 4. L'agente ora ha gmail.search, filesystem.list, github.issues nel prompt
    '    (AgentToolLoop.ResolveAgentTools aggiunge tutti i ConnectorRegistry.ListEnabled)
    '    Con un LLM reale, il prompt "cerca le fatture da ACME" farebbe:
    '    → tool_call gmail.search {"query":"from:ACME fatture"}
    '    Qui dimostriamo la chiamata diretta equivalente (ciò che l'agente farebbe):
    DIM r1$ AS STRING = AI.CONNECTORCALL("gmail", "search", "{""query"":""from:ACME fatture""}")
    PRINT "gmail.search (ciò che farebbe l'agente): "; LEFT(r1$, 200)

    DIM r2$ AS STRING = AI.CONNECTORCALL("filesystem", "list", "{""path"":""work""}")
    PRINT "filesystem.list: "; LEFT(r2$, 200)

    DIM r3$ AS STRING = AI.CONNECTORCALL("github", "issues", "{""repo"":""GMBasic""}")
    PRINT "github.issues: "; LEFT(r3$, 200)

    ' 5. Con un LLM reale: AI.AGENTASKSYNC("AutoAgent", "cerca le fatture da ACME e lista i file in work/")
    '    → il modello vede nel system prompt:
    '      Available tools:
    '      - gmail.search: Gmail — email search
    '      - filesystem.list: Filesystem — sandbox
    '      - github.issues: GitHub — issues
    '    → genera tool_calls → AgentToolLoop.InvokeToolCall → ConnectorRuntimeHelper.CallForAgent
    '    (conversione già in Connector.AgentBridge.cs:30, policy/rate-limit inclusi)

    AI AGENT DELETE "AutoAgent"
    PRINT "=== Auto-Discovery OK (nessun REGISTER necessario) ==="
END SUB
