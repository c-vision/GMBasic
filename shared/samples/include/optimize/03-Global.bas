REM FILE: 03-Global.bas — OPT (FFI): ottimizzatori globali.
IMPORT OPTIMIZE

SUB Main()
    DIM quad = FN(x() AS DOUBLE) AS DOUBLE => (x(0) - 1.0) * (x(0) - 1.0) + (x(1) + 2.0) * (x(1) + 2.0)
    DIM half = FN(x() AS DOUBLE) AS DOUBLE => (x(0) - 0.5) * (x(0) - 0.5)

    ' bounds (2*n) e ranges (2*n)
    DIM bnd(3) AS DOUBLE
    bnd(0) = -5.0 : bnd(1) = 5.0 : bnd(2) = -5.0 : bnd(3) = 5.0
    DIM out(1) AS DOUBLE
    CALL OPTIMIZE.DUALANNEALING(quad, bnd, out)
    PRINT "DUALANNEALING         ="; out(0); ","; out(1)
    CALL OPTIMIZE.DIFFERENTIALEVOLUTION(quad, bnd, out)
    PRINT "DIFFERENTIALEVOLUTION ="; out(0); ","; out(1)

    DIM x0(1) AS DOUBLE
    x0(0) = -1.0 : x0(1) = 1.0
    CALL OPTIMIZE.BASINHOPPING(quad, x0, out)
    PRINT "BASINHOPPING          ="; out(0); ","; out(1)

    DIM rng(1) AS DOUBLE
    rng(0) = -1.0 : rng(1) = 1.0
    DIM o1(0) AS DOUBLE
    CALL OPTIMIZE.BRUTE(half, rng, o1)
    PRINT "BRUTE                 ="; o1(0)
END SUB
