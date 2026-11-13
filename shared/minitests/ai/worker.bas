' P3 AI WORKER — lifecycle + workspace + state save/load.
' EXPECT: new= ok|run= ok|save= ok|load= nota|rounds=  1|spend=  7|rescued=  1
' NOTA: `AI WORKER RUN` è un ROUND DI LAVORO (come `AI WORKER "<TASK>"`, senza testo di task):
' costruisce la vista curata dal solo stato del workspace e la manda al completer. Qui non c'è
' provider → sentinella "Error: nessun provider LLM configurato" ⇒ rescued=1, e spend = 7 =
' lunghezza della vista "TASK: \n" (per questo il test CANCELLA il workspace: senza la pulizia la
' vista includerebbe SOLUTION/STATE del run precedente e spend non sarebbe deterministico).
' rounds/spend/rescued sono per-processo: identici su tutte e 4 le gambe (semantica unica nel
' runtime: wkr_round in C, InvokeWithView nel managed).
IMPORT AI
IMPORT STRINGS

SUB Main()
  CHDIR PROJECTFOLDER()
    ' Workspace ERMETICO: il run precedente lascia solution/last_error/state/*, che entrerebbero
    ' nella vista curata (spend dipenderebbe dalla storia del checkout).
    IF IO.DIREXISTS("work/w1") THEN RMDIR "work/w1", TRUE
    ' AI WORKER NEW name, {opts}
    AI WORKER NEW "w1", "{""workspace"":""work/w1"",""budget"":16000}"
    IF AI.WORKERROUNDS() >= 0 THEN
        PRINT "new= ok"
    ELSE
        PRINT "new= ERR"
    END IF

    ' AI WORKER RUN name
    AI WORKER RUN "w1"
    PRINT "run= ok"

    ' AI STATE SAVE worker, key, value — comando legacy a spazio
    AI STATE SAVE "w1", "notes", "greedy algorithm"
    PRINT "save= ok"

    ' AI.STATELOAD(worker, key) — funzione qualificata
    DIM v$ = AI.STATELOAD("w1", "notes")
    IF v$ = "greedy algorithm" THEN
        PRINT "load= nota"
    ELSE
        PRINT "load= ERR: " + v$
    END IF

    PRINT "rounds= "; STR$(AI.WORKERROUNDS())
    PRINT "spend= "; STR$(AI.WORKERSPEND())
    PRINT "rescued= "; STR$(AI.WORKERRESCUED())
END SUB
