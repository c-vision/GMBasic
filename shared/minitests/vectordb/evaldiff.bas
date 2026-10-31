' R6 — VECTORDB.EVALDIFF(baseline$, current$): differenze per metrica.
' Confronta baseline e corrente, verifica delta presenti.
' EXPECT: diff= ok
IMPORT VECTORDB

SUB Main()
    DIM base$ = "{""p_at"":0.9,""recall"":0.8,""faithfulness"":0.9}"
    DIM curr$ = "{""p_at"":0.5,""recall"":0.4,""faithfulness"":0.9}"
    DIM d$ = VECTORDB.EVALDIFF(base$, curr$)
    IF INSTR(d$, "delta") > 0 OR INSTR(d$, "p_at") > 0 THEN
        PRINT "diff= ok"
    ELSE
        PRINT "diff= ERR: " + d$
    END IF
END SUB