' FILE: Optimization/OptimizeUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: OPTIMIZE - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni OPTIMIZE che non comparivano in alcun esempio .bas:
'   CHOLESKYSOLVE, CONJUGATEGRADIENT, LEVENBERGMARQUARDTVEC.
'
' COPERTURA:
' - OPTIMIZE.CHOLESKYSOLVE(A, b)       : risolve Ax=b via fattorizzazione di Cholesky.
' - OPTIMIZE.CONJUGATEGRADIENT(fn, guess): minimizza una funzione multivariata.
' - OPTIMIZE.LEVENBERGMARQUARDT(fn, guess, inputs, targets): fit L-M scalare.
' - OPTIMIZE.LEVENBERGMARQUARDTVEC(fn, guess, inputs, targets): fit L-M vettoriale.
' =================================================================================================
IMPORT OPTIMIZE
IMPORT MATH

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "OPTIMIZE - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' 1. CHOLESKYSOLVE / CONJUGATEGRADIENT: A = [[4,1],[1,3]], b = [1,2]
    PRINT "-- CHOLESKYSOLVE / CONJUGATEGRADIENT --"
    DIM A(1) AS ARRAY OF DOUBLE
    A(0) = {4, 1}
    A(1) = {1, 3}
    DIM b(1) = {1, 2}
    DIM SolCh() = OPTIMIZE.CHOLESKYSOLVE(A, b)
    PRINT "CHOLESKYSOLVE x0="; MATH.ROUND(SolCh(0), 4); " x1="; MATH.ROUND(SolCh(1), 4)
    PRINT

    ' 1b. CONJUGATEGRADIENT: minimizzatore di funzione (Funzione, StimaIniziale).
    ' NOTA BUG: il blocco audit documenta CONJUGATEGRADIENT(A, b) come se risolvesse
    ' un sistema lineare, ma il runtime lo implementa come minimizzatore (come
    ' NELDERMEAD): argomenti = (Funzione, StimaIniziale()[, tol][, maxIter]).
    PRINT "-- CONJUGATEGRADIENT (minimizzatore) --"
    DIM Minim = FN(v() AS DOUBLE) AS DOUBLE => (v(0)-3)^2 + 1
    DIM CgInit(0) AS DOUBLE = {0.0}
    DIM SolCg() = OPTIMIZE.CONJUGATEGRADIENT(Minim, CgInit)
    PRINT "CONJUGATEGRADIENT x0="; MATH.ROUND(SolCg(0), 4)
    PRINT

    ' 2. LEVENBERGMARQUARDT / LEVENBERGMARQUARDTVEC (fit y = a + b*x)
    PRINT "-- LEVENBERGMARQUARDT --"
    DIM PG(1) AS DOUBLE = {1.0, 0.0}
    DIM Inputs(3) AS DOUBLE = {0, 1, 2, 3}
    DIM Targets(3) AS DOUBLE = {1, 3, 5, 7}
    DIM LinearModel = FN(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE => p(0) + p(1) * x
    DIM SolLm() = OPTIMIZE.LEVENBERGMARQUARDT(LinearModel, PG, Inputs, Targets)
    PRINT "LEVENBERGMARQUARDT a="; MATH.ROUND(SolLm(0), 4); " b="; MATH.ROUND(SolLm(1), 4)

    PRINT "-- LEVENBERGMARQUARDTVEC --"
    DIM VectorModel = FN(p() AS DOUBLE, xs() AS DOUBLE) AS ARRAY OF DOUBLE:
        DIM predicted(3) AS DOUBLE
        predicted(0) = p(0) + p(1) * xs(0)
        predicted(1) = p(0) + p(1) * xs(1)
        predicted(2) = p(0) + p(1) * xs(2)
        predicted(3) = p(0) + p(1) * xs(3)
        RETURN predicted
    END FN
    DIM SolLmVec() = OPTIMIZE.LEVENBERGMARQUARDTVEC(VectorModel, PG, Inputs, Targets)
    PRINT "LEVENBERGMARQUARDTVEC a="; MATH.ROUND(SolLmVec(0), 4); " b="; MATH.ROUND(SolLmVec(1), 4)
    PRINT

    PRINT "Test Completato."
END SUB
