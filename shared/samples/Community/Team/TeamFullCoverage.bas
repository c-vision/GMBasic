' FILE: Team/TeamFullCoverage.bas
' Copertura completa delle funzioni Team (TEAM vive dentro AI, non è namespace).
IMPORT AI WITH ALIAS

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "Team - Copertura Completa"
    PRINT "============================================================"
    PRINT

    AI TEAM CREATE Gruppo1
    PRINT "-- AI.TEAMLIST --"
    DIM list$ = AI.TEAMLIST()
    PRINT "AI.TEAMLIST len: "; LEN(list$)
    PRINT "-- AI.TEAMAGENTS --"
    DIM ag$ = AI.TEAMAGENTS("Gruppo1")
    PRINT "AI.TEAMAGENTS len: "; LEN(ag$)
    PRINT

    PRINT "Test Completato."
END SUB
