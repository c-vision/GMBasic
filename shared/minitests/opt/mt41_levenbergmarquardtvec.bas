' EXPECT: v= 1 , 0.5
' OPTIMIZE.LEVENBERGMARQUARDTVEC — MiniTest §4.1 (parità 4 backend).
IMPORT OPTIMIZE
IMPORT MATH

FUNCTION VModel(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY OF DOUBLE
    RETURN {p(0) * MATH.EXP(p(1) * x(0)), p(0) * MATH.EXP(p(1) * x(1)), p(0) * MATH.EXP(p(1) * x(2)), p(0) * MATH.EXP(p(1) * x(3)), p(0) * MATH.EXP(p(1) * x(4))}
END FUNCTION

SUB Main()
    DIM x(4) AS DOUBLE
    x(0) = 0.0 : x(1) = 1.0 : x(2) = 2.0 : x(3) = 3.0 : x(4) = 4.0
    DIM y(4) AS DOUBLE
    y(0) = 1.0 : y(1) = MATH.EXP(0.5) : y(2) = MATH.EXP(1.0) : y(3) = MATH.EXP(1.5) : y(4) = MATH.EXP(2.0)
    DIM p0(1) AS DOUBLE
    p0(0) = 0.5 : p0(1) = 0.5
    DIM o(1) AS DOUBLE
    CALL OPTIMIZE.LEVENBERGMARQUARDTVEC(VModel, p0, x, y, o)
    PRINT "v="; MATH.ROUND(o(0), 3); ","; MATH.ROUND(o(1), 3)
END SUB
