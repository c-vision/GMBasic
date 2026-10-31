' ==============================================================================
' FILE: AI/AgentHttpRoutes.bas
' Agent HTTP routes — chat / execute / workflow / memory (A5, 2026-09-07)
' ==============================================================================
' AI AGENT SERVE port  → avvia HttpListener su http://*:<port>/
' Route verificate (IlRuntime.HandleAgentHttpRequestAsync):
'   POST /chat     {"agent":"name","prompt":"..."}                → {"answer":"..."}
'   POST /execute  {"agent":"name","prompt":"..."}                → {"answer":"..."}
'                  {"agent":"name","tool":"calc_sum","args":{}}   → {"tool":"...","result":"..."}
'   POST /workflow {"workflow":"name"}                             → {"workflow":"...","status":"...","log":[...]}
'   POST /memory   {"agent":"name"}  (o GET /memory?agent=name)   → {"agent":"...","status":"...","history":[...]}
'
' Verificato: IL (AgentHttpRoutes.bas → IlRuntime → WorkflowServer/AgentServer)
' Curl:
'   curl -X POST http://127.0.0.1:8765/chat -H "Content-Type: application/json" -d '{"agent":"Demo","prompt":"Hello"}'
'   curl -X POST http://127.0.0.1:8765/execute -d '{"agent":"Demo","prompt":"Hello"}'
'   curl -X POST http://127.0.0.1:8765/workflow -d '{"workflow":"DemoWF"}'
'   curl "http://127.0.0.1:8765/memory?agent=Demo"
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== Agent HTTP Routes (chat/execute/workflow/memory) ==="
    PRINT

    ' 1) Prepara agent e workflow
    AI AGENT CREATE Demo
    AI AGENT SET Demo systemPrompt "You are a demo agent."
    PRINT "Agent Demo creato"

    AI WORKFLOW DemoWF
    AI WORKFLOW STEP s1, "step one"
    AI WORKFLOW STEP s2, "step two"
    END WORKFLOW
    PRINT "Workflow DemoWF definito: "; AI.WorkflowSteps("DemoWF")

    ' 2) Avvia server HTTP agent su 8765
    AI AGENT SERVE 8765
    PRINT "Agent HTTP server avviato su http://127.0.0.1:8765/"
    PRINT "Route:"
    PRINT "  POST /chat     {""agent"":""Demo"",""prompt"":""Hello""}"
    PRINT "  POST /execute  {""agent"":""Demo"",""prompt"":""Hello""}"
    PRINT "  POST /workflow {""workflow"":""DemoWF""}"
    PRINT "  GET  /memory?agent=Demo"
    PRINT
    PRINT "Curl esempi (esegui in un altro terminale):"
    PRINT "  curl -X POST http://127.0.0.1:8765/chat -H ""Content-Type: application/json"" -d '{""agent"":""Demo"",""prompt"":""Hello""}'"
    PRINT "  curl -X POST http://127.0.0.1:8765/execute -d '{""agent"":""Demo"",""prompt"":""Hello execute""}'"
    PRINT "  curl -X POST http://127.0.0.1:8765/workflow -d '{""workflow"":""DemoWF""}'"
    PRINT "  curl ""http://127.0.0.1:8765/memory?agent=Demo"""
    PRINT
    PRINT "Server attivo per 5s — prova i curl ora..."
    SLEEP 5
    AI AGENT STOP SERVER
    PRINT "Server fermato"

    AI AGENT DELETE Demo
    PRINT "Completato."
END SUB

