' EXPECT: v= 1 , 2
' OPTIMIZE.LEASTSQUARES — MiniTest §4.1 (parità 4 backend).
IMPORT OPTIMIZE

' A 3x2 (1,1) (1,2) (1,3); y = 1 + 2x

SUB Main()
    DIM A(2, 1) AS DOUBLE
    A = {{1.0, 1.0}, {1.0, 2.0}, {1.0, 3.0}}
    DIM b(2) AS DOUBLE
    b(0) = 3.0 : b(1) = 5.0 : b(2) = 7.0
    DIM o(1) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARES(A, b, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
