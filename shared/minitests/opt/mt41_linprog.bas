' EXPECT: v= 2 , 2
' OPTIMIZE.LINPROG — MiniTest §4.1 (parità 4 backend).
IMPORT OPT
IMPORT MATH

' min -x-y s.t. x+y<=4, x<=2  -> (2,2)

SUB Main()
    DIM c(1) AS DOUBLE
    c(0) = -1.0 : c(1) = -1.0
    DIM A(3) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0 : A(2) = 1.0 : A(3) = 0.0
    DIM b(1) AS DOUBLE
    b(0) = 4.0 : b(1) = 2.0
    DIM o(1) AS DOUBLE
    CALL OPT.LINPROG(c, 2, A, 2, b, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
