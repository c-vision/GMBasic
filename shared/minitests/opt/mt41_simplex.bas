' EXPECT: v= 0 , 4 , 12
' OPTIMIZE.SIMPLEX — MiniTest §4.1 (parità 4 backend).
IMPORT OPT
IMPORT MATH

' max 2x+3y s.t. x+y<=4, x<=2 -> (0,4) obj 12

SUB Main()
    DIM c(1) AS DOUBLE
    c(0) = 2.0 : c(1) = 3.0
    DIM A(3) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0 : A(2) = 1.0 : A(3) = 0.0
    DIM b(1) AS DOUBLE
    b(0) = 4.0 : b(1) = 2.0
    DIM o(2) AS DOUBLE
    CALL OPT.SIMPLEX(c, 2, A, 2, b, o)
    PRINT "v="; MATH.ROUND(o(0), 3); ","; MATH.ROUND(o(1), 3); ","; MATH.ROUND(o(2), 3)
END SUB
