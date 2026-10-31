' FILE: Workflow/WorkflowFullCoverage.bas
' Copertura completa delle funzioni Workflow non ancora coperte.
IMPORT AI

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Workflow - Copertura Completa"
    PRINT "============================================================"
    PRINT

    PRINT "-- AI.WorkflowList --"
    DIM l$ = AI.WorkflowList()
    PRINT "LIST len: "; LEN(l$)
    PRINT "-- AI.WorkflowStatus --"
    DIM s$ = AI.WorkflowStatus("Piano")
    PRINT "STATUS len: "; LEN(s$)
    PRINT "-- AI.WorkflowSteps --"
    DIM p$ = AI.WorkflowSteps("Piano")
    PRINT "STEPS len: "; LEN(p$)
    PRINT

    PRINT "Test Completato."
END SUB
