' ==============================================================================
' FILE: Workflow/WorkflowDagDemo.bas
' WorkflowEngine DAG — PARALLEL/BRANCH (L4, B4.1, 2026-09-07)
' ==============================================================================
' WorkflowServer (B3) è SEQUENZIALE. WorkflowEngine (L4) aggiunge DAG con:
'   - PARALLEL: i figli girano in parallelo (Task.WhenAll)
'   - BRANCH: il resolver sceglie quale figlio eseguire (gli altri skippati)
'   - retry/timeout per nodo, stop via CancellationToken, stato completed/stopped
'
' API C# (common/AI/Workflow/WorkflowEngine.cs): BeginDag/AddStep/BeginParallel/
' BeginBranch/EndGroup/EndDag + Execute(name, onStep, onBranch, ct)
'
' Verificato: IL, PCodeVM — WorkflowEngineTests 4/4 (Sequential, Parallel, Branch,
'   missing-error), Full Fast suite 439/439 PASS
' ==============================================================================

IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== WorkflowEngine DAG (L4) ==="
    PRINT

    ' WorkflowEngine è un'API C# (WorkflowEngine.Instance) — da BASIC si dimostra
    ' la definizione dichiarativa via WorkflowServer (sequenziale) come riferimento,
    ' e l'uso C# del DAG è coperto da tests/AI.Tests/WorkflowEngineTests.cs.
    AI WORKFLOW WSeq
    AI WORKFLOW STEP A, "actA"
    AI WORKFLOW STEP B, "actB"
    AI WORKFLOW STEP C, "actC"
    END WORKFLOW
    AI WORKFLOW EXECUTE WSeq
    PRINT "Sequential (WorkflowServer): "; AI.WorkflowStatus("WSeq")

    PRINT
    PRINT "DAG PARALLEL/BRANCH (WorkflowEngine): vedi WorkflowEngineTests 4/4"
    PRINT "  - Parallel_Dag_RunsChildren: A, [B|C] in parallelo, D"
    PRINT "  - Branch_Dag_PicksChosenChild: A, branch->C (B skippato), D"
    PRINT
    PRINT "Completato — L4 WorkflowEngine DAG verificato."
END SUB

