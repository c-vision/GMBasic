' EXPECT: v= 1.4142 , 1.4142
' OPTIMIZE.BROYDEN — MiniTest §4.1 (parità 4 backend).
IMPORT OPTIMIZE
IMPORT MATH

FUNCTION Sys(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) * v(0) + v(1) * v(1) - 4.0, v(0) - v(1)}
END FUNCTION

SUB Main()
    DIM x0(1) AS DOUBLE
    x0(0) = 1.0 : x0(1) = 0.0
    DIM o(1) AS DOUBLE
    CALL OPTIMIZE.BROYDEN(Sys, x0, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
