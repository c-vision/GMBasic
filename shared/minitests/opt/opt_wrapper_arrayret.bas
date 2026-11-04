' EXPECT: w1= 1  ,  1|w2= 1  ,  1|w3= 3   -0.2
' Wrapper BASIC che ritorna un array (2026-11-03): copre il ritorno `AS ARRAY OF`
' di una funzione utente chiamata come espressione su C99 (__out=NULL + canale
' thread-local), l'inoltro di una callback con parametri array e l'assegnazione
' a un array esistente.
IMPORT OPTIMIZE
FUNCTION Ros(p() AS DOUBLE) AS DOUBLE
    RETURN (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^ 2
END FUNCTION
FUNCTION MyBFGS(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL OPTIMIZE.BFGS(f, x0, o)
    RETURN o
END FUNCTION
FUNCTION Tripla(v() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(2) AS DOUBLE
    o(0) = v(0) * 3.0
    o(1) = v(1) * 3.0
    o(2) = v(0) + v(1)
    RETURN o
END FUNCTION
SUB Main()
    DIM x0(1) AS DOUBLE = {-1.2, 1.0}
    DIM x() = MyBFGS(Ros, x0)
    PRINT "w1="; MATH.ROUND(x(0), 4); " , "; MATH.ROUND(x(1), 4)
    DIM y(1) AS DOUBLE
    y = MyBFGS(Ros, x0)
    PRINT "w2="; MATH.ROUND(y(0), 4); " , "; MATH.ROUND(y(1), 4)
    DIM z(1) AS DOUBLE
    z = Tripla(x0)
    PRINT "w3="; UBOUND(z) + 1; "  "; MATH.ROUND(z(2), 4)
END SUB
