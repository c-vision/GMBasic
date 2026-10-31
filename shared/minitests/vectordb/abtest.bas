' R7 — VECTORDB.ABTEST(variantA$, variantB$): test A/B z-test.
' Due proporzioni identiche → z-score ~0.
' EXPECT: ab= zero-diff
IMPORT VECTORDB

SUB Main()
    DIM a$ = "{""n"":50,""success"":25}"
    DIM b$ = "{""n"":50,""success"":25}"
    DIM ab$ = VECTORDB.ABTEST(a$, b$)
    IF INSTR(ab$, "z_score") > 0 THEN
        IF INSTR(ab$, "0") > 0 OR INSTR(ab$, "0.0") > 0 OR INSTR(ab$, "pending") > 0 THEN
            PRINT "ab= zero-diff"
        ELSE
            PRINT "ab= nonzero: " + ab$
        END IF
    ELSE
        PRINT "ab= ERR: " + ab$
    END IF
END SUB