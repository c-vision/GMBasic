REM FILE: 02-Minimizers.bas — OPT (FFI): minimizzazione multivariata.
IMPORT OPT
IMPORT MATH

SUB Main()
    ' Rosenbrock: minimo (1,1)
    DIM rosen = FN(x() AS DOUBLE) AS DOUBLE => 100.0 * (x(1) - x(0) * x(0)) * (x(1) - x(0) * x(0)) + (1.0 - x(0)) * (1.0 - x(0))
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM out(1) AS DOUBLE

    CALL OPT.NELDERMEAD(rosen, x0, out)
    PRINT "NELDERMEAD  ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.BFGS(rosen, x0, out)
    PRINT "BFGS        ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.LBFGS(rosen, x0, out)
    PRINT "LBFGS       ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.CONJUGATEGRADIENT(rosen, x0, out)
    PRINT "CG          ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.POWELL(rosen, x0, out)
    PRINT "POWELL      ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.TNC(rosen, x0, out)
    PRINT "TNC         ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.COBYLA(rosen, x0, out)
    PRINT "COBYLA      ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.SLSQP(rosen, x0, out)
    PRINT "SLSQP       ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.MINIMIZE(rosen, x0, out)
    PRINT "MINIMIZE    ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)

    ' con bounds espliciti (2*n valori: lo0,hi0,lo1,hi1)
    DIM bnd(3) AS DOUBLE
    bnd(0) = -10.0 : bnd(1) = 10.0 : bnd(2) = -10.0 : bnd(3) = 10.0
    CALL OPT.TNCB(rosen, x0, bnd, out)
    PRINT "TNCB        ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
    CALL OPT.MINIMIZEM(rosen, x0, "bfgs", out)
    PRINT "MINIMIZEM   ="; MATH.ROUND(out(0), 2); ","; MATH.ROUND(out(1), 2)
END SUB
