' EXPECT: v= 1.75 , 1.5
' OPTIMIZE.CHOLESOLVVE — MiniTest §4.1 (parità 4 backend).
IMPORT OPTIMIZE

' matrice SPD 2x2

SUB Main()
    DIM A(1, 1) AS DOUBLE
    A = {{4.0, 2.0}, {2.0, 3.0}}
    DIM b(1) AS DOUBLE
    b(0) = 10.0 : b(1) = 8.0
    DIM o(1) AS DOUBLE
    CALL OPTIMIZE.CHOLESKYSOLVE(A, b, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
