' FILE: AI/WebAgentDashboard.bas
' =================================================================
' Sample minimale per la WEBAPP Agents Dashboard (C1b, 5 tab)
' Avvia lo stesso stack di webapp/app.bas in forma compatta (60 righe).
' Verificato: compila su IL/PCodeVM/VM/C/C99/C99Native (Conformance).
' Per la webapp completa (Canvas + WASM + proxy), vedi webapp/app.bas
' e webapp/pages/index.bsp (editabili in include/AgentsDashboard.inc).
' =================================================================
IMPORT HTTP
IMPORT JSON
IMPORT AI WITH ALIAS
IMPORT DASHBOARD

SUB Main()
    PRINT "=== WebAgentDashboard — C1b sample ==="
    PRINT

    ' --- seed se vuoto ---
    DIM nms() = AI.AGENTLIST()
    IF UBOUND(nms) < 0 THEN
        AI AGENT CREATE "Demo"
        AI AGENT SET "Demo" systemPrompt "You are a demo agent."
        PRINT "Agent Demo creato"
    END IF
    AI AGENT TRACE VERBOSE

    ' --- verifica helper DASHBOARD (include) ---
    PRINT "Badge Idle: " + DASHBOARD.StatusBadgeHtml("Idle")
    PRINT "Badge YOLO: " + DASHBOARD.PermissionBadgeHtml("YOLO")
    PRINT "Title: " + DASHBOARD.TITLE$
    PRINT

    ' --- avvia API agents (C1) + dashboard HTTP ---
    AI AGENT SERVE 8765
    PRINT "Agent API: http://localhost:8765/api/agents"

    DIM cfg AS HTTPCONFIG = NEW HTTPCONFIG
    cfg.Port = 8091
    HTTP CONFIG cfg
    HTTP STATIC "/", "./webapp/pages"
    HTTP STATIC "/static", "./webapp/static"
    PRINT "Dashboard: http://localhost:8091/index.bsp (5 tab)"
    PRINT "Canvas:    http://localhost:8091/canvas.bsp"
    PRINT "WASM:      http://localhost:8091/wasm.bsp"
    HTTP SERVER cfg.Port

    PRINT
    PRINT "Server attivo — apri il browser o fai curl:"
    PRINT "  curl http://localhost:8091/index.bsp | grep Agents"
    PRINT "  curl http://localhost:8765/api/agents"
    SLEEP 3000

    HTTP SERVER STOP
    AI AGENT STOP SERVER
    PRINT "Server fermati — sample completato."
END SUB

