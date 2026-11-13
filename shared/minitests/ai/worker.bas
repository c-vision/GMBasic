' P3 AI WORKER — lifecycle + workspace + state save/load.
' EXPECT: new= ok|run= ok|save= ok|load= nota|rounds=  1|spend=  0|rescued=  0
' NOTA: `spend`/`rescued` sono 0 sulle gambe managed (il completer FIXTURE non conta token) e
' `251`/`1` su C99Native, che invece li conta: DIVERGENZA APERTA (contabilita' token del
' worker fra le gambe), non un oracolo da aggiustare.
IMPORT AI
IMPORT STRINGS

SUB Main()
  CHDIR PROJECTFOLDER()
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
