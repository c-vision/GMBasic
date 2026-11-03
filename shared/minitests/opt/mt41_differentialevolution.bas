' EXPECT: v= 1 ,-2
' OPTIMIZE.DIFFERENTIALEVOLUTION — MiniTest §4.1 (parità 4 backend).
IMPORT OPT
IMPORT MATH

FUNCTION Quad(x() AS DOUBLE) AS DOUBLE
    RETURN (x(0) - 1.0) * (x(0) - 1.0) + (x(1) + 2.0) * (x(1) + 2.0)
END FUNCTION

SUB Main()
    DIM b(3) AS DOUBLE
    b(0) = -5.0 : b(1) = 5.0 : b(2) = -5.0 : b(3) = 5.0
    DIM o(1) AS DOUBLE
    CALL OPT.DIFFERENTIALEVOLUTION(Quad, b, o)
    PRINT "v="; MATH.ROUND(o(0), 2); ","; MATH.ROUND(o(1), 2)
END SUB
