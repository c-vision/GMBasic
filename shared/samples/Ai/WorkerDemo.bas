' Sample AI WORKER + AI STATE — worker lifecycle, workspace, state.
' Dimostra: WORKER NEW, WORKER RUN, STATE SAVE/LOAD, metriche run-level.
IMPORT AI

SUB Main()
    PRINT "=== AI WORKER demo ==="
    PRINT ""

    ' 1. Crea worker con workspace per-task
    AI WORKER NEW "solver", "{""workspace"":""work/solver"",""budget"":32000}"
    PRINT "1. Worker creato, rounds=" + STR$(AI.WORKERROUNDS())

    ' 2. Esegui il worker (invocazione pulita con vista curata)
    AI WORKER RUN "solver"
    AI WORKER RUN "solver"
    PRINT "2. Esecuzioni: " + STR$(AI.WORKERROUNDS())

    ' 3. Salva stato per-key
    AI STATE SAVE "solver", "task", "find max subarray"
    AI STATE SAVE "solver", "plan", "1. divide 2. conquer 3. combine"
    PRINT "3. Stato salvato"

    ' 4. Leggi stato
    DIM task$ = AI.STATELOAD("solver", "task")
    PRINT "4. Task loaded: " + task$

    ' 5. Metriche run-level
    PRINT "5. Rounds=" + STR$(AI.WORKERROUNDS()) + " rescued=" + STR$(AI.WORKERRESCUED()) + " spend=" + STR$(AI.WORKERSPEND())

    PRINT ""
    PRINT "=== Fine demo ==="
END SUB