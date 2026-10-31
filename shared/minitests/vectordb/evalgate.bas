' R6 — VECTORDB.EVALGATE(baseline$, current$): gate CI intelligente.
' Baseline buona, corrente con drop metriche → FAIL.
' EXPECT: verdict= fail
IMPORT VECTORDB

SUB Main()
    DIM base$ = "{""p_at"":0.9,""recall"":0.9,""faithfulness"":0.9}"
    DIM curr$ = "{""p_at"":0.4,""recall"":0.3,""faithfulness"":0.9}"
    DIM g$ = VECTORDB.EVALGATE(base$, curr$)
    IF INSTR(g$, "fail") > 0 THEN
        PRINT "verdict= fail"
    ELSE
        PRINT "verdict= ERR: " + g$
    END IF
END SUB