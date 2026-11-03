' EXPECT: v= 1 , 2
' OPTIMIZE.LEASTSQUARESFIT — MiniTest §4.1 (parità 4 backend).
IMPORT OPT
IMPORT MATH

FUNCTION Res(v() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {v(0) - 1.0, v(0) + v(1) - 3.0, v(0) + 2.0 * v(1) - 5.0, v(0) + 3.0 * v(1) - 7.0, v(0) + 4.0 * v(1) - 9.0}
END FUNCTION

SUB Main()
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM o(4) AS DOUBLE
    CALL OPT.LEASTSQUARESFIT(Res, x0, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
