REM FILE: 05-Fitting.bas — OPT (FFI): curve fitting (Levenberg-Marquardt).
IMPORT OPTIMIZE

SUB Main()
    ' dati esatti y = 1 * exp(0.5 * x)
    DIM xs(4) AS DOUBLE
    xs(0) = 0.0 : xs(1) = 1.0 : xs(2) = 2.0 : xs(3) = 3.0 : xs(4) = 4.0
    DIM ys(4) AS DOUBLE
    ys(0) = 1.0 : ys(1) = MATH.EXP(0.5) : ys(2) = MATH.EXP(1.0) : ys(3) = MATH.EXP(1.5) : ys(4) = MATH.EXP(2.0)

    DIM model = FN(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE => p(0) * MATH.EXP(p(1) * x)
    DIM vmodel = FN(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * MATH.EXP(p(1) * x(0)), p(0) * MATH.EXP(p(1) * x(1)), p(0) * MATH.EXP(p(1) * x(2)), p(0) * MATH.EXP(p(1) * x(3)), p(0) * MATH.EXP(p(1) * x(4))}

    DIM p0(1) AS DOUBLE
    p0(0) = 0.5 : p0(1) = 0.5
    DIM fit(1) AS DOUBLE

    CALL OPTIMIZE.LEVENBERGMARQUARDT(model, p0, xs, ys, fit)
    PRINT "LEVENBERGMARQUARDT    a,b ="; fit(0); ","; fit(1)
    CALL OPTIMIZE.LEVENBERGMARQUARDTVEC(vmodel, p0, xs, ys, fit)
    PRINT "LEVENBERGMARQUARDTVEC a,b ="; fit(0); ","; fit(1)
    CALL OPTIMIZE.CURVEFIT(model, xs, ys, p0, fit)
    PRINT "CURVEFIT              a,b ="; fit(0); ","; fit(1)
END SUB
