' EXPECT: newton= 1.4142135623731|bisection= 1.41421356237288|brent= 1.41421356237314|secant= 1.4142135623731|golden= 1.99999999999519|brentq= 2|brenth= 2|ridder= 2|toms748= 2|brentmin= 1|minscalar= 1|minscalarG= 1.0000000000058
' OPTIMIZE via FFI — 1D: radici e minimo scalare (11 funzioni).
IMPORT OPT

SUB Main()
    PRINT "newton="; OPT.NEWTON(FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0, 1.0)
    PRINT "bisection="; OPT.BISECTION(FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0, 1.0, 2.0)
    PRINT "brent="; OPT.BRENT(FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0, 1.0, 2.0)
    PRINT "secant="; OPT.SECANT(FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0, 1.0, 2.0)
    PRINT "golden="; OPT.GOLDENSECTION(FN(x AS DOUBLE) AS DOUBLE => (x - 2.0) * (x - 2.0), 0.0, 5.0)
    PRINT "brentq="; OPT.BRENTQ(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0, 1.0, 3.0)
    PRINT "brenth="; OPT.BRENTH(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0, 1.0, 3.0)
    PRINT "ridder="; OPT.RIDDER(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0, 1.0, 3.0)
    PRINT "toms748="; OPT.TOMS748(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0, 1.0, 3.0)
    PRINT "brentmin="; OPT.BRENTMIN(FN(x AS DOUBLE) AS DOUBLE => (x - 1.0) * (x - 1.0), 0.0, 3.0)
    PRINT "minscalar="; OPT.MINIMIZESCALAR(FN(x AS DOUBLE) AS DOUBLE => (x - 1.0) * (x - 1.0), 0.0, 3.0)
    PRINT "minscalarG="; OPT.MINIMIZESCALARM(FN(x AS DOUBLE) AS DOUBLE => (x - 1.0) * (x - 1.0), 0.0, 3.0, "golden")
END SUB
