' EXPECT: nm= 1 , 1|bfgs= 1 , 1|fit= 1 , 0.5|root= 1.414 , 1.414
' OPT via FFI con CALLBACK NOMINATI (non lambda) su tutti i 4 backend.
IMPORT OPTIMIZE

FUNCTION Rosen(x() AS DOUBLE) AS DOUBLE
    RETURN 100.0 * (x(1) - x(0) * x(0)) * (x(1) - x(0) * x(0)) + (1.0 - x(0)) * (1.0 - x(0))
END FUNCTION

FUNCTION Scala(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE
    RETURN p(0) * MATH.EXP(p(1) * x)
END FUNCTION

FUNCTION Sistema(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
END FUNCTION

SUB Main()
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM out(1) AS DOUBLE
    CALL OPTIMIZE.NELDERMEAD(Rosen, x0, out)
    PRINT "nm="; MATH.ROUND(out(0), 3); ","; MATH.ROUND(out(1), 3)
    CALL OPTIMIZE.BFGS(Rosen, x0, out)
    PRINT "bfgs="; MATH.ROUND(out(0), 3); ","; MATH.ROUND(out(1), 3)

    DIM xs(4) AS DOUBLE
    xs(0) = 0.0 : xs(1) = 1.0 : xs(2) = 2.0 : xs(3) = 3.0 : xs(4) = 4.0
    DIM ys(4) AS DOUBLE
    ys(0) = 1.0 : ys(1) = MATH.EXP(0.5) : ys(2) = MATH.EXP(1.0) : ys(3) = MATH.EXP(1.5) : ys(4) = MATH.EXP(2.0)
    DIM p0(1) AS DOUBLE
    p0(0) = 0.5 : p0(1) = 0.5
    DIM fit(1) AS DOUBLE
    CALL OPTIMIZE.CURVEFIT(Scala, xs, ys, p0, fit)
    PRINT "fit="; MATH.ROUND(fit(0), 3); ","; MATH.ROUND(fit(1), 3)

    DIM sq(1) AS DOUBLE
    sq(0) = 1.0 : sq(1) = 0.0
    CALL OPTIMIZE.FSOLVE(Sistema, sq, out)
    PRINT "root="; MATH.ROUND(out(0), 3); ","; MATH.ROUND(out(1), 3)
END SUB
