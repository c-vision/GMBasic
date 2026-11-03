' MiniTest: rubrica SCORE 0..10 (proiezione T6) — singola (`SCORE:0:10`) e batch
' (`SCORE 0 TO 10`). Rules mappa critical→10, high→8; senza match prima opzione 0.5.
' EXPECT: score10-single=10/ 1|score10-batch=8/ 1|score10-mock=0/ 1
IMPORT AI

SUB Main()
    DIM s AS DECISION
    AI DECIDE "critical incident", "SCORE:0:10" INTO s
    PRINT "score10-single="; s.VALUE; "/"; s.PROBABILITY

    DIM b AS DECISION
    AI DECIDE
        b AS SCORE 0 TO 10
    FROM "high risk"
    END DECIDE
    PRINT "score10-batch="; b.VALUE; "/"; b.PROBABILITY

    AI DECIDE "x", "SCORE:0:10", "mock" INTO s
    PRINT "score10-mock="; s.VALUE; "/"; s.PROBABILITY
END SUB
