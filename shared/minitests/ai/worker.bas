' P3 AI WORKER — lifecycle + workspace + state save/load.
' EXPECT: new= ok|run= ok|save= ok|load= nota|rounds=  1|spend=  0|rescued=  0
' NOTA: `AI WORKER RUN` registra SOLO un round (rounds++); `spend` (caratteri della vista di
' contesto) e `rescued` (fallimenti recuperati) appartengono all'invocazione pulita
' `AI WORKER "<TASK>"` e al LOOP: qui restano 0 su tutte e 4 le gambe. Prima su C99Native
' valevano 251|1 perche' il parser C non riconosceva il sub-verbo RUN (soft keyword) e
' emetteva `gmb_worker_invoke("RUN")` con il nome del worker perso: bug chiuso nel parser
' (`gmb_parser_stmt_domain.c`), non nell'oracolo.
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
