' ==============================================================================
' AgentMemoryAndPermissions.bas — AI.MEMORYASK + AI PERMISSION (parità 4 backend)
' ==============================================================================
' Copre: AI.MEMORYASK (ricerca nel brain, query-based), AI AGENT CREATE,
'        AI PERMISSION (livello Sandbox/Supervised/YOLO + policy per-tool),
'        AI.AGENTGET (permission/toolpolicies).
' ==============================================================================
IMPORT AI

SUB Main()
    PRINT "=== Memoria e permessi AGENT ==="

    ' MEMORYASK: ricerca nel brain. Query senza corrispondenze → stringa vuota.
    PRINT "MemoryAsk: ["; AI.MEMORYASK("query-inesistente-xyz-123", 3); "]"

    ' Modello permessi: livello + override per-tool.
    AI PLUGINS ENABLE "browser"
    AI AGENT CREATE "P"
    AI PERMISSION P, sandbox
    PRINT "level:    "; AI.AGENTGET("P", "permission")
    AI PERMISSION P, TOOL browser_set_cookie, allow
    PRINT "policies: "; AI.AGENTGET("P", "toolpolicies")
    AI PERMISSION P, yolo
    PRINT "level2:   "; AI.AGENTGET("P", "permission")
    PRINT
    PRINT "Completato."
END SUB
