' Surface HTTP /api/workflows* a PARITA' 4 backend (IL / P-Code / VM/C / C99Native):
' server AI AGENT SERVE implementato in ogni backend (VM/C via gmb_agent_http_start),
' client FETCH in-process senza deadlock (fix watchdog GPM gmb_c99gpm_extjob_*).
' FILE: samples/Community/AI/WebAgentWorkflowsDemo.bas
' =================================================================
' WEBAPP Workflows — demo dettagliata (BASIC + HTTP API)
' Mostra il ciclo completo da browser (Log tab) e da BASIC:
'   WORKFLOW → STEP → END WORKFLOW → EXECUTE → API /api/workflows
' =================================================================
IMPORT HTTP
IMPORT JSON
IMPORT AI

SUB Main()
    PRINT "=== WEBAPP Workflows Demo (dettagliato) ==="
    PRINT ""

    ' ----------------------------------------------------------
    ' 1. Definizione via BASIC (AI WORKFLOW) — come in
    '    samples/Community/Workflow/WorkflowBasics.bas
    '    Sintassi: AI WORKFLOW nome / AI WORKFLOW STEP nome, "azione" [,retry [,timeoutMs]] / END WORKFLOW
    ' ----------------------------------------------------------
    PRINT "1) Definizione BASIC: FlowA con 2 step"
    AI WORKFLOW FlowA
    AI WORKFLOW STEP s1, "EXTRACT db", 1, 0
    AI WORKFLOW STEP s2, "TRANSFORM csv", 0, 500
    END WORKFLOW
    PRINT "   FlowA status: " + AI.WorkflowStatus("FlowA")
    PRINT "   FlowA steps:  " + AI.WorkflowSteps("FlowA")
    PRINT ""

    ' ----------------------------------------------------------
    ' 2. Esecuzione via BASIC (AI WORKFLOW EXECUTE)
    '    Ogni step logga in ExecutionLog: [0] s1: EXTRACT db, [1] s2: TRANSFORM csv
    ' ----------------------------------------------------------
    PRINT "2) Esecuzione BASIC: AI WORKFLOW EXECUTE FlowA"
    AI WORKFLOW EXECUTE FlowA
    PRINT "   Dopo EXECUTE status: " + AI.WorkflowStatus("FlowA")
    DIM log$ = AI.WorkflowSteps("FlowA")
    PRINT "   Steps: " + log$
    PRINT ""

    ' ----------------------------------------------------------
    ' 3. Definizione via HTTP API (come fa la dashboard JS)
    '    POST /api/workflows {"name":"FlowB"} → 201 {name,status:"empty",steps:[]}
    '    Usato da webapp/pages/index.bsp: wfCreate() → fetch /api/workflows
    ' ----------------------------------------------------------
    AI AGENT SERVE 8766
    SLEEP 500
    PRINT "3) HTTP API: POST /api/workflows → Create FlowB"
    DIM o AS HTTPOPTIONS = NEW HTTPOPTIONS
    o.Method = "POST"
    o.Headers.Add("Content-Type", "application/json")
    o.Body = "{""name"":""FlowB""}"
    DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows", o)
    PRINT "   " + LTRIM$(STR$(r.StatusCode)) + " " + r.Body

    ' 3b. Aggiunta step via HTTP (dashboard: wfAddStep)
    '     POST /api/workflows/FlowB/steps {"name":"step1","action":"echo hello","retryCount":1}
    o.Body = "{""name"":""step1"",""action"":""echo hello"",""retryCount"":1,""timeoutMs"":2000}"
    DIM r2 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows/FlowB/steps", o)
    PRINT "   add step1: " + LTRIM$(STR$(r2.StatusCode)) + " " + r2.Body
    o.Body = "{""name"":""step2"",""action"":""echo world""}"
    DIM r3 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows/FlowB/steps", o)
    PRINT "   add step2: " + LTRIM$(STR$(r3.StatusCode)) + " " + r3.Body

    ' 3c. Dettaglio via GET /api/workflows/FlowB
    DIM d AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows/FlowB")
    PRINT "   detail: " + d.Body

    ' 3d. Esecuzione via POST /api/workflows/FlowB/execute → {ok:true,status:"completed",log:[...]}
    '     Dashboard: wfExecute() → fetch /api/workflows/{n}/execute
    DIM e AS HTTPOPTIONS = NEW HTTPOPTIONS
    e.Method = "POST"
    e.Headers.Add("Content-Type", "application/json")
    e.Body = "{}"
    DIM r4 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows/FlowB/execute", e)
    PRINT "   execute: " + LTRIM$(STR$(r4.StatusCode)) + " " + r4.Body
    IF INSTR(r4.Body, "completed") > 0 THEN PRINT "   → execute: OK" ELSE PRINT "   → execute: FAIL"

    ' 3e. Lista completa via GET /api/workflows → [{name,status,steps}]
    DIM r5 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows")
    PRINT "   list: " + LEFT$(r5.Body, 300)

    ' 3f. Delete via DELETE /api/workflows/FlowB → {deleted:"FlowB"}
    '     Dashboard: wfDelete() → fetch DELETE
    DIM del AS HTTPOPTIONS = NEW HTTPOPTIONS
    del.Method = "DELETE"
    DIM r6 AS FETCHRESULT = FETCH("http://127.0.0.1:8766/api/workflows/FlowB", del)
    PRINT "   delete: " + LTRIM$(STR$(r6.StatusCode)) + " " + r6.Body
    AI AGENT STOP SERVER
    PRINT ""

    ' ----------------------------------------------------------
    ' 4. Workflow con retry/timeout (come WorkflowRetryTimeout.bas)
    '    Utile per task fragili: retry 1 + timeout 500ms
    ' ----------------------------------------------------------
    PRINT "4) Workflow con retry/timeout: FlowRetry"
    AI WORKFLOW FlowRetry
    AI WORKFLOW STEP fragile, "task fragile", 1
    AI WORKFLOW STEP stabile, "task stabile", 0, 0
    END WORKFLOW
    AI WORKFLOW EXECUTE FlowRetry
    PRINT "   FlowRetry status: " + AI.WorkflowStatus("FlowRetry")
    PRINT ""

    PRINT "Demo completata — apri webapp/pages/index.bsp Log tab → Workflows per CRUD da browser."
    PRINT "  wfPick/wfCreate/wfAddStep/wfExecute/wfDelete in JS chiamano le stesse API qui testate."
END SUB

