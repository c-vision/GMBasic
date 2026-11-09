' EXPECT: bfgs= 1  ,  1|nm= 1  ,  1|sys=-1.4142  , -1.4142|ls= 1.75  ,  1.5|lp= 0  ,  0|sp= 1  ,  1.5  ,  22|fit= 2  ,  0
' Forma DROP-IN: `DIM x() = OPTIMIZE.BFGS(f, x0)` (FUNCTION omonime delle SUB, 2026-11-03).
' Verifica BFGS/NELDERMEAD/FSOLVE/CHOLESKYSOLVE/LINPROG/SIMPLEX/LEVENBERGMARQUARDT.
IMPORT OPTIMIZE
FUNCTION Ros(p() AS DOUBLE) AS DOUBLE
    RETURN (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
END FUNCTION
FUNCTION Sys(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
END FUNCTION
FUNCTION Mod1(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE
    RETURN p(0) * x + p(1)
END FUNCTION
SUB Main()
    DIM x0(1) AS DOUBLE = {-1.2, 1.0}
    DIM b() = OPTIMIZE.BFGS(Ros, x0)
    PRINT "bfgs="; MATH.ROUND(b(0), 4); " , "; MATH.ROUND(b(1), 4)
    DIM n() = OPTIMIZE.NELDERMEAD(Ros, x0)
    PRINT "nm="; MATH.ROUND(n(0), 4); " , "; MATH.ROUND(n(1), 4)
    DIM s() = OPTIMIZE.FSOLVE(Sys, x0)
    PRINT "sys="; MATH.ROUND(s(0), 4); " , "; MATH.ROUND(s(1), 4)
    DIM A(1, 1) AS DOUBLE
    A = {{4.0, 2.0}, {2.0, 3.0}}
    DIM bb(1) AS DOUBLE = {10.0, 8.0}
    DIM ch(1) AS DOUBLE
    CALL OPTIMIZE.CHOLESKYSOLVE(A, bb, ch)
    PRINT "ls="; MATH.ROUND(ch(0), 4); " , "; MATH.ROUND(ch(1), 4)
    DIM Aub(1, 1) AS DOUBLE
    Aub = {{1.0, 2.0}, {3.0, 2.0}}
    DIM bub(1) AS DOUBLE = {4.0, 6.0}
    DIM lp(1) AS DOUBLE
    CALL OPTIMIZE.LINPROG(bb, Aub, bub, lp)
    PRINT "lp="; MATH.ROUND(lp(0), 4); " , "; MATH.ROUND(lp(1), 4)
    DIM sp(2) AS DOUBLE
    CALL OPTIMIZE.SIMPLEX(bb, Aub, bub, sp)
    PRINT "sp="; MATH.ROUND(sp(0), 4); " , "; MATH.ROUND(sp(1), 4); " , "; MATH.ROUND(sp(2), 4)
    DIM xs(2) AS DOUBLE = {1.0, 2.0, 3.0}
    DIM ys(2) AS DOUBLE = {2.0, 4.0, 6.0}
    DIM p0(1) AS DOUBLE = {0.0, 0.0}
    DIM f() = OPTIMIZE.LEVENBERGMARQUARDT(Mod1, p0, xs, ys)
    PRINT "fit="; MATH.ROUND(f(0), 4); " , "; MATH.ROUND(f(1), 4)
END SUB
