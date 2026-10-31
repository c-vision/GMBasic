' R1 AI OBSERVE — OBSERVATIONLIST/HASH/COUNT strutturale.
' Expect: ok= list-ok
' EXPECT: ok= list-fail
IMPORT AI

SUB Main()
    DIM path AS STRING
    path = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/fattura_it.txt"
    AI OBSERVE FILE path INTO id
    n = AI.OBSERVATIONCOUNT()
    h$ = AI.OBSERVATIONHASH$(id)
    o$ = AI.OBSERVATION$(id)
    l$ = AI.OBSERVATIONLIST$()
    IF n > 0 AND LEN(h$) > 0 AND INSTR(l$, "{") AND INSTR(l$, "|") THEN
        PRINT "ok= list-ok"
    ELSE
        PRINT "ok= list-fail"
    END IF
END SUB
