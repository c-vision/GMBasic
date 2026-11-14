' R1 AI OBSERVE — OBSERVATIONLIST/HASH/COUNT strutturale.
' Il contratto di OBSERVATIONLIST$() è la lista compatta `id|tipo|hash` (non JSON): la
' verifica è che contenga l'hash dell'osservazione appena registrata.
' EXPECT: ok= list-ok
IMPORT AI

SUB Main()
    DIM path AS STRING
    path = "../../samples/Fixtures/ai/fattura_it.txt"
    AI OBSERVE FILE path INTO id
    n = AI.OBSERVATIONCOUNT()
    h$ = AI.OBSERVATIONHASH$(id)
    o$ = AI.OBSERVATION$(id)
    l$ = AI.OBSERVATIONLIST$()
    IF n > 0 AND LEN(h$) > 0 AND INSTR(l$, h$) > 0 AND INSTR(l$, "|") > 0 THEN
        PRINT "ok= list-ok"
    ELSE
        PRINT "ok= list-fail"
    END IF
END SUB
