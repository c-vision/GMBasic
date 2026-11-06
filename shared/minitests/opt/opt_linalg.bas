' EXPECT: ls= 1 , 2|chol= 1.75 , 1.5|simplex= 0 , 4 , 12|linprog= 2 , 2|linprogb= 2 , 2|linprogeq= 2 , 2
' OPTIMIZE via FFI — algebra e programmazione lineare (4 funzioni + varianti).
IMPORT OPTIMIZE

SUB Main()
    ' A 3x2 (row-major): righe (1,1) (1,2) (1,3); y = 2 + x  ->  (2, 1)? no: y = a + b x
    DIM A(5) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0
    A(2) = 1.0 : A(3) = 2.0
    A(4) = 1.0 : A(5) = 3.0
    ' y = 2 + 0*x? usiamo y = 1 + 2x: (1,1)->3, (1,2)->5, (1,3)->7
    DIM b(2) AS DOUBLE
    b(0) = 3.0 : b(1) = 5.0 : b(2) = 7.0
    DIM x(1) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARES(A, b, x)
    PRINT "ls="; MATH.ROUND(x(0), 4); ","; MATH.ROUND(x(1), 4)

    ' Cholesky: [[4,2],[2,3]] x = [10,8]  ->  (1.75, 1.5)
    DIM CH(3) AS DOUBLE
    CH(0) = 4.0 : CH(1) = 2.0 : CH(2) = 2.0 : CH(3) = 3.0
    DIM bc(1) AS DOUBLE
    bc(0) = 10.0 : bc(1) = 8.0
    DIM xc(1) AS DOUBLE
    CALL OPTIMIZE.CHOLESKYSOLVE(CH, bc, xc)
    PRINT "chol="; MATH.ROUND(xc(0), 4); ","; MATH.ROUND(xc(1), 4)

    ' SIMPLEX: max 2x+3y, x+y<=4, x<=2  ->  x=0, y=4, obiettivo 12
    DIM c(1) AS DOUBLE
    c(0) = 2.0 : c(1) = 3.0
    DIM M(3) AS DOUBLE
    M(0) = 1.0 : M(1) = 1.0 : M(2) = 1.0 : M(3) = 0.0
    DIM bm(1) AS DOUBLE
    bm(0) = 4.0 : bm(1) = 2.0
    DIM xs(2) AS DOUBLE
    CALL OPTIMIZE.SIMPLEX(c, M, bm, xs)
    PRINT "simplex="; MATH.ROUND(xs(0), 4); ","; MATH.ROUND(xs(1), 4); ","; MATH.ROUND(xs(2), 4)

    ' LINPROG: min -x-y, x+y<=4, x<=2  ->  (2,2)
    DIM cmin(1) AS DOUBLE
    cmin(0) = -1.0 : cmin(1) = -1.0
    DIM xl(1) AS DOUBLE
    CALL OPTIMIZE.LINPROG(cmin, M, bm, xl)
    PRINT "linprog="; MATH.ROUND(xl(0), 4); ","; MATH.ROUND(xl(1), 4)

    ' LINPROGB con bounds espliciti (x >= 0, y >= 0..4, x >= 0..2)
    DIM bnd(3) AS DOUBLE
    bnd(0) = 0.0 : bnd(1) = 2.0 : bnd(2) = 0.0 : bnd(3) = 4.0
    CALL OPTIMIZE.LINPROGB(cmin, M, bm, bnd, xl)
    PRINT "linprogb="; MATH.ROUND(xl(0), 4); ","; MATH.ROUND(xl(1), 4)

    ' LINPROGEQ: massimizza x+y con x+2y <= 6 e x = y  ->  (2,2)
    DIM cpos(1) AS DOUBLE
    cpos(0) = -1.0 : cpos(1) = -1.0
    DIM Aub(1) AS DOUBLE
    Aub(0) = 1.0 : Aub(1) = 2.0
    DIM bub(0) AS DOUBLE
    bub(0) = 6.0
    DIM Aeq(1) AS DOUBLE
    Aeq(0) = 1.0 : Aeq(1) = -1.0
    DIM beq(0) AS DOUBLE
    beq(0) = 0.0
    CALL OPTIMIZE.LINPROGEQ(cpos, Aub, bub, Aeq, beq, xl)
    PRINT "linprogeq="; MATH.ROUND(xl(0), 4); ","; MATH.ROUND(xl(1), 4)
END SUB
