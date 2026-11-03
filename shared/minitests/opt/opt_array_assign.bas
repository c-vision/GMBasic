' EXPECT: br= 1.4142 , 1.4142|bfgs= 1 , 1|fs= 1.4142 , 1.4142|fs2= 1.4142 , 1.4142|len=  3   2|nm= 1 , 1
' Builtin OPTIMIZE che RITORNANO un array assegnati a un array dichiarato
' (2026-11-03: su C99 dava "simbolo non trovato: funzione o subroutine 'OPTIMIZE.X'").
IMPORT OPTIMIZE
IMPORT MATH
FUNCTION Sys(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
END FUNCTION
FUNCTION Sys3(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) + v(1) + v(2) - 6.0, v(0) - v(1), v(1) - v(2)}
END FUNCTION
FUNCTION Ros(p() AS DOUBLE) AS DOUBLE
    RETURN (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
END FUNCTION
SUB Main()
    DIM x0(1) AS DOUBLE = {1.0, 0.0}
    DIM a(1) AS DOUBLE
    a = OPTIMIZE.BROYDEN(Sys, x0)
    PRINT "br="; MATH.ROUND(a(0), 4); ","; MATH.ROUND(a(1), 4)
    DIM m(1) AS DOUBLE
    m = OPTIMIZE.BFGS(Ros, x0)
    PRINT "bfgs="; MATH.ROUND(m(0), 4); ","; MATH.ROUND(m(1), 4)
    DIM c(1) AS DOUBLE
    c = OPTIMIZE.FSOLVE(Sys, x0)
    PRINT "fs="; MATH.ROUND(c(0), 4); ","; MATH.ROUND(c(1), 4)
    a = OPTIMIZE.FSOLVE(Sys, x0)
    PRINT "fs2="; MATH.ROUND(a(0), 4); ","; MATH.ROUND(a(1), 4)
    ' array-returning assegnato a un array DICHIARATO più corto: la lunghezza si adegua
    DIM x3(2) AS DOUBLE = {1.0, 1.0, 1.0}
    DIM t(1) AS DOUBLE
    t = OPTIMIZE.FSOLVE(Sys3, x3)
    PRINT "len= "; UBOUND(t) + 1; " "; MATH.ROUND(t(0), 4)
    DIM z() AS DOUBLE
    z = OPTIMIZE.NELDERMEAD(Ros, x0)
    PRINT "nm="; MATH.ROUND(z(0), 4); ","; MATH.ROUND(z(1), 4)
END SUB
