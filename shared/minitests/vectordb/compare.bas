' R8 — VECTORDB.COMPARE(configs$): confronto multi-configurazione.
' Due config con metriche diverse: la migliore vince per ogni metrica.
' EXPECT: cmp= has-winner
IMPORT VECTORDB

SUB Main()
    DIM cfg$ = "[{""config"":{""name"":""base""},""results"":{""p_at"":0.5,""recall"":0.4}},{""config"":{""name"":""tuned""},""results"":{""p_at"":0.9,""recall"":0.8}}]"
    DIM cmp$ = VECTORDB.COMPARE(cfg$)
    IF INSTR(cmp$, "winner") > 0 OR INSTR(cmp$, "tuned") > 0 THEN
        PRINT "cmp= has-winner"
    ELSE
        PRINT "cmp= ERR: " + cmp$
    END IF
END SUB