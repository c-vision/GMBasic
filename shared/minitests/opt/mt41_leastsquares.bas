' EXPECT: v= 1 , 2
' OPTIMIZE.LEASTSQUARES — MiniTest §4.1 (parità 4 backend).
IMPORT OPT
IMPORT MATH

' A 3x2 (1,1) (1,2) (1,3); y = 1 + 2x

SUB Main()
    DIM A(5) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0 : A(2) = 1.0 : A(3) = 2.0 : A(4) = 1.0 : A(5) = 3.0
    DIM b(2) AS DOUBLE
    b(0) = 3.0 : b(1) = 5.0 : b(2) = 7.0
    DIM o(1) AS DOUBLE
    CALL OPT.LEASTSQUARES(A, 3, 2, b, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
