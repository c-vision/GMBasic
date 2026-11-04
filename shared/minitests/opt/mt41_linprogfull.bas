' EXPECT: v= 2 , 2
' OPTIMIZE.LINPROGFULL — MiniTest §4.1 (parità 4 backend).
' min -x-y s.t. x+y<=4, x-y=0, 0<=x,y<=3 -> (2,2)
IMPORT OPTIMIZE

SUB Main()
    DIM c(1) AS DOUBLE
    c(0) = -1.0 : c(1) = -1.0
    DIM A(1) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0
    DIM b(0) AS DOUBLE
    b(0) = 4.0
    DIM Ae(1) AS DOUBLE
    Ae(0) = 1.0 : Ae(1) = -1.0
    DIM be(0) AS DOUBLE
    be(0) = 0.0
    DIM bnd(3) AS DOUBLE
    bnd(0) = 0.0 : bnd(1) = 3.0 : bnd(2) = 0.0 : bnd(3) = 3.0
    DIM o(1) AS DOUBLE
    CALL OPTIMIZE.LINPROGFULL(c, 2, A, 1, b, Ae, 1, be, bnd, o)
    PRINT "v="; MATH.ROUND(o(0), 4); ","; MATH.ROUND(o(1), 4)
END SUB
