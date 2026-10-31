' R4 — VECTORDB.DIAGNOSEREPORT(): report aggregato diagnosi.
' Lo stato aggregato viene accumulato dalle DIAGNOSE precedenti nello stesso processo.
' Se nessuna DIAGNOSE prima → total 0.
' EXPECT: ok= report-ok
IMPORT VECTORDB

SUB Main()
    DIM d$ = VECTORDB.DIAGNOSEREPORT()
    IF INSTR(d$, "{") > 0 THEN
        PRINT "ok= report-ok"
    ELSE
        PRINT "ok= ERR: " + d$
    END IF
END SUB