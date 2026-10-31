' R1 AI OBSERVE — OBSERVELOG con tail predefinito.
' Usa path assoluto a fixture. Expect solo compilazione + esecuzione.
' EXPECT: ok= log-ok
IMPORT AI

SUB Main()
    DIM path AS STRING
    path = "../../samples/Fixtures/ai/fattura_it.txt"
    c0 = AI.OBSERVATIONCOUNT()
    AI OBSERVE LOG path INTO id
    c1 = AI.OBSERVATIONCOUNT()
    IF c1 > c0 THEN
        PRINT "ok= log-ok"
    ELSE
        PRINT "ok= log-fail"
    END IF
END SUB