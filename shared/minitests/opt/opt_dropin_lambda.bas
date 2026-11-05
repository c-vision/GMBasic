' EXPECT: nm= 1  ,  1|bfgs= 1  ,  1|lbfgs= 1  ,  1|powell= 1  ,  1|min= 1  ,  1|fit= 2  ,  0
' Regressione: forma DROP-IN `DIM x() = OPTIMIZE.XXX(f, x0)` con LAMBDA a parametro array
' (`FN(v() AS DOUBLE) => ...`). Su IL l'ottimizzatore restituiva il punto iniziale
' (Bugs/IL-Optimize-Lambda-Parametro-Array-Regressione). Le FUNCTION nominate sono in opt_dropin.
IMPORT OPTIMIZE
SUB Main()
    DIM Q = FN(v() AS DOUBLE) AS DOUBLE => (v(0) - 1.0) ^ 2 + (v(1) - 1.0) ^ 2
    DIM G(1) AS DOUBLE = {0.0, 0.0}
    DIM n() = OPTIMIZE.NELDERMEAD(Q, G)
    PRINT "nm="; MATH.ROUND(n(0), 4); " , "; MATH.ROUND(n(1), 4)
    DIM b() = OPTIMIZE.BFGS(Q, G)
    PRINT "bfgs="; MATH.ROUND(b(0), 4); " , "; MATH.ROUND(b(1), 4)
    DIM l() = OPTIMIZE.LBFGS(Q, G)
    PRINT "lbfgs="; MATH.ROUND(l(0), 4); " , "; MATH.ROUND(l(1), 4)
    DIM pw() = OPTIMIZE.POWELL(Q, G)
    PRINT "powell="; MATH.ROUND(pw(0), 4); " , "; MATH.ROUND(pw(1), 4)
    DIM m() = OPTIMIZE.MINIMIZE(Q, G)
    PRINT "min="; MATH.ROUND(m(0), 4); " , "; MATH.ROUND(m(1), 4)
    DIM Mod1 = FN(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE => p(0) * x + p(1)
    DIM xs(2) AS DOUBLE = {1.0, 2.0, 3.0}
    DIM ys(2) AS DOUBLE = {2.0, 4.0, 6.0}
    DIM p0(1) AS DOUBLE = {0.0, 0.0}
    DIM f() = OPTIMIZE.LEVENBERGMARQUARDT(Mod1, p0, xs, ys)
    PRINT "fit="; MATH.ROUND(f(0), 4); " , "; MATH.ROUND(f(1), 4)
END SUB
