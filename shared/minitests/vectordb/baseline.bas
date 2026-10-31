' R7 — VECTORDB.BASELINE(sample$): cattura baseline per monitoraggio.
' Accetta un sample JSON e lo salva come baseline corrente.
' EXPECT: bl= sample-ok
IMPORT VECTORDB

SUB Main()
    DIM s$ = "{""p_at"":0.8,""recall"":0.7,""latency_ms"":150}"
    DIM bl$ = VECTORDB.BASELINE(s$)
    IF INSTR(bl$, "pending") > 0 THEN
        PRINT "bl= pending"
    ELSE
        PRINT "bl= sample-ok"
    END IF
END SUB