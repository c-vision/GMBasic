' EXPECT: fsolve= 1.4142 , 1.4142|broyden= 1.4142 , 1.4142|lsfit= 1 , 2|lm= 1 , 0.5|lmvec= 1 , 0.5|curvefit= 1 , 0.5
' OPTIMIZE via FFI — sistemi e curve fitting (6 funzioni).
IMPORT OPT
IMPORT MATH

SUB Main()
    ' sistema: x^2 + y^2 = 4, x = y  ->  (sqrt2, sqrt2)
    DIM sys = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE => {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
    DIM x0(1) AS DOUBLE
    x0(0) = 1.0 : x0(1) = 0.0
    DIM out(1) AS DOUBLE

    CALL OPT.FSOLVE(sys, x0, out)
    PRINT "fsolve="; MATH.ROUND(out(0), 4); ","; MATH.ROUND(out(1), 4)
    CALL OPT.BROYDEN(sys, x0, out)
    PRINT "broyden="; MATH.ROUND(out(0), 4); ","; MATH.ROUND(out(1), 4)

    ' minimi quadrati non lineari: y = a + b*x  (residui m=5, parametri n=2)
    DIM res = FN(v() AS DOUBLE) AS ARRAY OF DOUBLE => {v(0) - 1.0, v(0) + v(1) - 3.0, v(0) + 2.0 * v(1) - 5.0, v(0) + 3.0 * v(1) - 7.0, v(0) + 4.0 * v(1) - 9.0}
    DIM p0(1) AS DOUBLE
    p0(0) = 0.0 : p0(1) = 0.0
    DIM r5(4) AS DOUBLE
    CALL OPT.LEASTSQUARESFIT(res, p0, r5)
    PRINT "lsfit="; MATH.ROUND(r5(0), 4); ","; MATH.ROUND(r5(1), 4)

    ' curve fitting y = a * exp(b*x) sui dati esatti
    DIM xs(4) AS DOUBLE
    xs(0) = 0.0 : xs(1) = 1.0 : xs(2) = 2.0 : xs(3) = 3.0 : xs(4) = 4.0
    DIM ys(4) AS DOUBLE
    ys(0) = 1.0 : ys(1) = MATH.EXP(0.5) : ys(2) = MATH.EXP(1.0) : ys(3) = MATH.EXP(1.5) : ys(4) = MATH.EXP(2.0)
    DIM model = FN(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE => p(0) * MATH.EXP(p(1) * x)
    DIM vmodel = FN(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * MATH.EXP(p(1) * x(0)), p(0) * MATH.EXP(p(1) * x(1)), p(0) * MATH.EXP(p(1) * x(2)), p(0) * MATH.EXP(p(1) * x(3)), p(0) * MATH.EXP(p(1) * x(4))}
    DIM q0(1) AS DOUBLE
    q0(0) = 0.5 : q0(1) = 0.5
    DIM fit(1) AS DOUBLE
    CALL OPT.LEVENBERGMARQUARDT(model, q0, xs, ys, fit)
    PRINT "lm="; MATH.ROUND(fit(0), 4); ","; MATH.ROUND(fit(1), 4)
    CALL OPT.LEVENBERGMARQUARDTVEC(vmodel, q0, xs, ys, fit)
    PRINT "lmvec="; MATH.ROUND(fit(0), 4); ","; MATH.ROUND(fit(1), 4)
    CALL OPT.CURVEFIT(model, xs, ys, q0, fit)
    PRINT "curvefit="; MATH.ROUND(fit(0), 4); ","; MATH.ROUND(fit(1), 4)
END SUB
