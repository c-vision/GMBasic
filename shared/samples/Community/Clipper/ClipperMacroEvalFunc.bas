' FILE: Clipper/ClipperMacroEvalFunc.bas
' =================================================================================================
'   CANARINO — EVAL("F(x)") / &("F(x)") chiamano una FUNCTION utente del programma.
'   C99: jump table gmb_host_expr_call (Fase 7, hook call).
'   VM/C: vm_expr_call sulla tabella procedure del chunk.
' =================================================================================================

GLOBAL X AS INTEGER

FUNCTION DoubleIt(n AS INTEGER) AS INTEGER
    RETURN n * 2
END FUNCTION

FUNCTION AddToX(n AS INTEGER) AS INTEGER
    RETURN X + n
END FUNCTION

SUB Main()
    X = 10

    DIM a AS INTEGER
    a = EVAL("DoubleIt(21)")
    PRINT "test10-eval-func:"; a
    IF a = 42 THEN PRINT "test10-eval-func-ok" ELSE PRINT "test10-eval-func-FAIL"

    DIM b AS INTEGER
    b = EVAL("AddToX(5)")
    PRINT "test11-eval-func-env:"; b
    IF b = 15 THEN PRINT "test11-eval-func-env-ok" ELSE PRINT "test11-eval-func-env-FAIL"

    DIM c AS INTEGER
    c = &("DoubleIt(3)")
    PRINT "test12-macro-func:"; c
    IF c = 6 THEN PRINT "test12-macro-func-ok" ELSE PRINT "test12-macro-func-FAIL"
END SUB
