' R7 — VECTORDB.DRIFT(baseline$, window$): Population Stability Index.
' Distribuzione omogenea window vs baseline → PSI ~0 (nessun drift).
' EXPECT: psi= no-drift
IMPORT VECTORDB

SUB Main()
    ' Due distribuzioni molto simili → PSI basso
    DIM base$ = "{""values"":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]}"
    DIM wind$ = "{""values"":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]}"
    DIM psi = VECTORDB.DRIFT(base$, wind$)
    IF psi >= 0 AND psi < 0.1 THEN
        PRINT "psi= no-drift"
    ELSE
        PRINT "psi= ERR: " + STR$(psi)
    END IF
END SUB