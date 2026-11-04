REM FILE: 01-Scalar.bas — OPT (FFI): radici 1D e minimo scalare.
IMPORT OPTIMIZE

SUB Main()
    DIM f = FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0
    DIM g = FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0
    DIM p = FN(x AS DOUBLE) AS DOUBLE => (x - 1.0) * (x - 1.0)
    DIM q = FN(x AS DOUBLE) AS DOUBLE => (x - 2.0) * (x - 2.0)

    PRINT "NEWTON      sqrt(2) ="; OPTIMIZE.NEWTON(f, 1.0)
    PRINT "BISECTION   sqrt(2) ="; OPTIMIZE.BISECTION(f, 1.0, 2.0)
    PRINT "BRENT       sqrt(2) ="; OPTIMIZE.BRENT(f, 1.0, 2.0)
    PRINT "SECANT      sqrt(2) ="; OPTIMIZE.SECANT(f, 1.0, 2.0)
    PRINT "BRENTQ      2       ="; OPTIMIZE.BRENTQ(g, 1.0, 3.0)
    PRINT "BRENTH      2       ="; OPTIMIZE.BRENTH(g, 1.0, 3.0)
    PRINT "RIDDER      2       ="; OPTIMIZE.RIDDER(g, 1.0, 3.0)
    PRINT "TOMS748     2       ="; OPTIMIZE.TOMS748(g, 1.0, 3.0)
    PRINT "GOLDENSECTION min   ="; OPTIMIZE.GOLDENSECTION(q, 0.0, 5.0)
    PRINT "BRENTMIN      min   ="; OPTIMIZE.BRENTMIN(p, 0.0, 3.0)
    PRINT "MINIMIZESCALAR min  ="; OPTIMIZE.MINIMIZESCALAR(p, 0.0, 3.0)
    PRINT "MINIMIZESCALARM gold="; OPTIMIZE.MINIMIZESCALARM(p, 0.0, 3.0, "golden")
END SUB
