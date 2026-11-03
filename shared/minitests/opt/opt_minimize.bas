' EXPECT: nelder= 1 , 1|bfgs= 1 , 1|lbfgs= 1 , 1|cg= 1 , 1|powell= 1 ,-2|tnc= 1 , 1|tncb= 1 ,-2|cobyla= 1 ,-2|slsqp= 1 , 1|minimize= 1 , 1|minimizem= 1 ,-2|brute= 0.47|basin= 1 , 1|dual= 1 ,-2|de= 1 ,-2
' OPTIMIZE via FFI — minimizzazione multivariata e globali (13 funzioni).
' I callback sono lambda (`FN ...`): forma supportata su tutti e 4 i backend.
IMPORT OPT
IMPORT MATH

SUB Main()
    DIM rosen = FN(x() AS DOUBLE) AS DOUBLE => 100.0 * (x(1) - x(0) * x(0)) * (x(1) - x(0) * x(0)) + (1.0 - x(0)) * (1.0 - x(0))
    DIM quad = FN(x() AS DOUBLE) AS DOUBLE => (x(0) - 1.0) * (x(0) - 1.0) + (x(1) + 2.0) * (x(1) + 2.0)
    DIM half = FN(x() AS DOUBLE) AS DOUBLE => (x(0) - 0.5) * (x(0) - 0.5)

    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM out(1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM bnd(3) AS DOUBLE
    bnd(0) = -big : bnd(1) = big : bnd(2) = -big : bnd(3) = big

    CALL OPT.NELDERMEAD(rosen, x0, out)
    PRINT "nelder="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.BFGS(rosen, x0, out)
    PRINT "bfgs="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.LBFGS(rosen, x0, out)
    PRINT "lbfgs="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.CONJUGATEGRADIENT(rosen, x0, out)
    PRINT "cg="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.POWELL(quad, x0, out)
    PRINT "powell="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.TNC(rosen, x0, out)
    PRINT "tnc="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.TNCB(quad, x0, bnd, out)
    PRINT "tncb="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.COBYLA(quad, x0, out)
    PRINT "cobyla="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.SLSQP(rosen, x0, out)
    PRINT "slsqp="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.MINIMIZE(rosen, x0, out)
    PRINT "minimize="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.MINIMIZEM(quad, x0, "bfgs", out)
    PRINT "minimizem="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)

    DIM rng(1) AS DOUBLE
    rng(0) = -1.0 : rng(1) = 1.0
    DIM o1(0) AS DOUBLE
    CALL OPT.BRUTE(half, rng, o1)
    PRINT "brute="; MATH.ROUND(o1(0), 2)

    DIM xb(1) AS DOUBLE
    xb(0) = -1.0 : xb(1) = 1.0
    CALL OPT.BASINHOPPING(rosen, xb, out)
    PRINT "basin="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)

    DIM bq(3) AS DOUBLE
    bq(0) = -5.0 : bq(1) = 5.0 : bq(2) = -5.0 : bq(3) = 5.0
    CALL OPT.DUALANNEALING(quad, bq, out)
    PRINT "dual="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.DIFFERENTIALEVOLUTION(quad, bq, out)
    PRINT "de="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
END SUB
