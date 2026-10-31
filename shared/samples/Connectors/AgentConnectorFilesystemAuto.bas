' FILE: Connectors/AgentConnectorFilesystemAuto.bas
' Agente che usa filesystem senza configurazione esplicita — sandbox automatica

IMPORT AI

SUB Main()
    PRINT "=== Agent Filesystem Auto ==="

    AI CONNECTOR LOAD "filesystem"
    PRINT "LOAD filesystem OK (sandbox: work/ READ+WRITE, /etc DENY, ~/.ssh DENY)"

    ' Agente creato SENZA REGISTER — vede filesystem.list/read/write automaticamente
    AI AGENT CREATE "FsAgent"
    AI AGENT SET "FsAgent" "systemPrompt" "You are a filesystem assistant. Use filesystem tools to manage work/."
    PRINT "Agent FsAgent creato"

    ' Ciò che l'agente farebbe con LLM reale per "crea work/note.txt e leggilo":
    ' 1. filesystem.write
    DIM w$ AS STRING = AI.CONNECTORCALL("filesystem", "write", "{""path"":""work/agent_note.txt"",""content"":""ciao da agente""}")
    PRINT "write work/agent_note.txt: "; LEFT(w$, 150)

    ' 2. filesystem.list
    DIM l$ AS STRING = AI.CONNECTORCALL("filesystem", "list", "{""path"":""work""}")
    PRINT "list work: "; LEFT(l$, 300)

    ' 3. filesystem.read
    DIM r$ AS STRING = AI.CONNECTORCALL("filesystem", "read", "{""path"":""work/agent_note.txt""}")
    PRINT "read: "; LEFT(r$, 200)

    ' 4. filesystem.hash e metadata
    DIM h$ AS STRING = AI.CONNECTORCALL("filesystem", "hash", "{""path"":""work/agent_note.txt""}")
    PRINT "hash: "; LEFT(h$, 150)

    DIM m$ AS STRING = AI.CONNECTORCALL("filesystem", "metadata", "{""path"":""work/agent_note.txt""}")
    PRINT "metadata: "; LEFT(m$, 200)

    ' Con LLM reale: AI.AGENTASKSYNC("FsAgent", "crea work/report.md con 'ok' e verifica")
    ' → tool_calls: filesystem.write → filesystem.read → risposta

    AI AGENT DELETE "FsAgent"
    PRINT "=== Filesystem Auto OK ==="
END SUB
