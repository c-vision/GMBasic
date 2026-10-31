' ==============================================================================
' FILE: Workflow/WorkflowRetryTimeout.bas
' WORKFLOW — retry e timeout per passo (A3, 2026-09-07)
' ==============================================================================
' Sintassi estesa (2026-09-07):
'   AI WORKFLOW STEP nome, azione [, retry [, timeoutMs]]
'     retry     = tentativi aggiuntivi dopo il primo fallimento (0 = nessun retry)
'               totale tentativi = retry + 1
'     timeoutMs = 0 = nessun limite; >0 = se il passo supera timeoutMs → TimeoutException e retry
'                     (se retry esauriti → workflow in stato "failed")
'
' Verificato: IL, PCodeVM, VM/C, C99, C99Native (parser + Compiler 4-arg + PCode syscalls +
'             WorkflowServer.Execute + gmb_workflow_step_ex + vm_sys_workflow_103)
' ==============================================================================
IMPORT AI

SUB Main()
    PRINT "=== WORKFLOW Retry / Timeout ==="
    PRINT

    ' ------------------------------------------------------------------
    ' 1) Retry — passo che fallisce 2× poi passa (retry=2 → 3 tentativi)
    '    In BASIC puro il passo è dichiarativo ("action" è stringa), quindi
    '    il retry si esercita via API C# con callback che lancia eccezioni.
    '    Qui dimostriamo la definizione e lo stato finale "completed".
    ' ------------------------------------------------------------------
    AI WORKFLOW PipelineRetry
    AI WORKFLOW STEP estrai, "EXTRACT db", 2, 0        ' 2 retry, nessun timeout
    AI WORKFLOW STEP trasforma, "TRANSFORM csv", 0, 0 ' nessun retry
    AI WORKFLOW STEP carica, "LOAD warehouse", 1, 500  ' 1 retry + timeout 500ms
    END WORKFLOW
    PRINT "Workflow 'PipelineRetry' definito: "; AI.WorkflowSteps("PipelineRetry")

    AI WORKFLOW EXECUTE PipelineRetry
    DIM s1$ AS STRING
    s1$ = AI.WorkflowStatus("PipelineRetry")
    PRINT "Stato PipelineRetry: "; s1$   ' atteso "completed"
    PRINT

    ' ------------------------------------------------------------------
    ' 2) Solo retry (senza timeout) — sintassi a 3 argomenti
    ' ------------------------------------------------------------------
    AI WORKFLOW SoloRetry
    AI WORKFLOW STEP s1, "task fragile", 1
    AI WORKFLOW STEP s2, "task stabile"
    END WORKFLOW
    PRINT "Workflow 'SoloRetry' definito: "; AI.WorkflowSteps("SoloRetry")
    AI WORKFLOW EXECUTE SoloRetry
    PRINT "Stato SoloRetry: "; AI.WorkflowStatus("SoloRetry")
    PRINT

    ' ------------------------------------------------------------------
    ' 3) Timeout — passo con limite di 2s
    ' ------------------------------------------------------------------
    AI WORKFLOW ConTimeout
    AI WORKFLOW STEP veloce, "quick op", 0, 2000       ' timeout 2s, nessun retry
    AI WORKFLOW STEP lentoConRetry, "slow op", 1, 100  ' 1 retry + timeout 100ms
    END WORKFLOW
    PRINT "Workflow 'ConTimeout' definito: "; AI.WorkflowSteps("ConTimeout")
    AI WORKFLOW EXECUTE ConTimeout
    PRINT "Stato ConTimeout: "; AI.WorkflowStatus("ConTimeout")
    PRINT

    ' ------------------------------------------------------------------
    ' 4) Lista e interrogazione
    ' ------------------------------------------------------------------
    DIM all$ AS STRING
    all$ = AI.WorkflowList()
    PRINT "Tutti i workflow: "; all$

    PRINT
    PRINT "Completato — retry/timeout verificati (stato/lista)."
    PRINT "Nota: il retry/timeout reale (eccezione/timeout) si verifica"
    PRINT "quando il workflow è eseguito con callback C# (WorkflowServer.Execute"
    PRINT "con onStep che lancia). In BASIC dichiarativo ogni EXECUTE va in"
    PRINT "'completed' perché non c'è onStep che fallisce."
END SUB

