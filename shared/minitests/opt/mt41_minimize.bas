' EXPECT: v= 1 , 1
' OPTIMIZE.MINIMIZE — MiniTest §4.1 (parità 4 backend).
IMPORT OPT
IMPORT MATH

FUNCTION Rosen(x() AS DOUBLE) AS DOUBLE
    RETURN 100.0 * (x(1) - x(0) * x(0)) * (x(1) - x(0) * x(0)) + (1.0 - x(0)) * (1.0 - x(0))
END FUNCTION

SUB Main()
    DIM x0(1) AS DOUBLE
    x0(0) = 0.0 : x0(1) = 0.0
    DIM o(1) AS DOUBLE
    CALL OPT.MINIMIZE(Rosen, x0, o)
    PRINT "v="; MATH.ROUND(o(0), 2); ","; MATH.ROUND(o(1), 2)
END SUB
