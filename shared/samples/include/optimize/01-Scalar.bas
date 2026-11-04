REM FILE: 01-Scalar.bas — OPT (FFI): radici 1D e minimo scalare.
IMPORT OPT

SUB Main()
    DIM f = FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0
    DIM g = FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0
    DIM p = FN(x AS DOUBLE) AS DOUBLE => (x - 1.0) * (x - 1.0)
    DIM q = FN(x AS DOUBLE) AS DOUBLE => (x - 2.0) * (x - 2.0)

    PRINT "NEWTON      sqrt(2) ="; OPT.NEWTON(f, 1.0)
    PRINT "BISECTION   sqrt(2) ="; OPT.BISECTION(f, 1.0, 2.0)
    PRINT "BRENT       sqrt(2) ="; OPT.BRENT(f, 1.0, 2.0)
    PRINT "SECANT      sqrt(2) ="; OPT.SECANT(f, 1.0, 2.0)
    PRINT "BRENTQ      2       ="; OPT.BRENTQ(g, 1.0, 3.0)
    PRINT "BRENTH      2       ="; OPT.BRENTH(g, 1.0, 3.0)
    PRINT "RIDDER      2       ="; OPT.RIDDER(g, 1.0, 3.0)
    PRINT "TOMS748     2       ="; OPT.TOMS748(g, 1.0, 3.0)
    PRINT "GOLDENSECTION min   ="; OPT.GOLDENSECTION(q, 0.0, 5.0)
    PRINT "BRENTMIN      min   ="; OPT.BRENTMIN(p, 0.0, 3.0)
    PRINT "MINIMIZESCALAR min  ="; OPT.MINIMIZESCALAR(p, 0.0, 3.0)
    PRINT "MINIMIZESCALARM gold="; OPT.MINIMIZESCALARM(p, 0.0, 3.0, "golden")
END SUB
