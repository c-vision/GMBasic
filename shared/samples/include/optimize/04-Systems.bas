REM FILE: 04-Systems.bas — OPT (FFI): sistemi non lineari e minimi quadrati.
IMPORT OPTIMIZE

SUB Main()
    ' x^2 + y^2 = 4, x = y  ->  (sqrt(2), sqrt(2))
    DIM sys = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE => {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
    DIM x0(1) AS DOUBLE
    x0(0) = 1.0 : x0(1) = 0.0
    DIM out(1) AS DOUBLE
    CALL OPTIMIZE.FSOLVE(sys, x0, out)
    PRINT "FSOLVE  ="; out(0); ","; out(1)
    CALL OPTIMIZE.BROYDEN(sys, x0, out)
    PRINT "BROYDEN ="; out(0); ","; out(1)

    ' minimi quadrati non lineari: residui y = a + b*x (m=5) -> (1, 2)
    DIM res = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE => {v(0) - 1.0, v(0) + v(1) - 3.0, v(0) + 2.0 * v(1) - 5.0, v(0) + 3.0 * v(1) - 7.0, v(0) + 4.0 * v(1) - 9.0}
    DIM p0(1) AS DOUBLE
    p0(0) = 0.0 : p0(1) = 0.0
    DIM r(4) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARESFIT(res, p0, r)
    PRINT "LEASTSQUARESFIT ="; r(0); ","; r(1)
END SUB
