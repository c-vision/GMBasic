' EXPECT: v= 0.47
' OPTIMIZE.BRUTE — MiniTest §4.1 (parità 4 backend).
IMPORT OPTIMIZE
IMPORT MATH

FUNCTION H(x() AS DOUBLE) AS DOUBLE
    RETURN (x(0) - 0.5) * (x(0) - 0.5)
END FUNCTION

SUB Main()
    DIM r(1) AS DOUBLE
    r(0) = -1.0 : r(1) = 1.0
    DIM o(0) AS DOUBLE
    CALL OPTIMIZE.BRUTE(H, r, o)
    PRINT "v="; MATH.ROUND(o(0), 2)
END SUB
