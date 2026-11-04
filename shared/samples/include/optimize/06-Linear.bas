REM FILE: 06-Linear.bas — OPT (FFI): algebra lineare e programmazione lineare.
REM Le matrici sono PIATTE row-major + dimensioni esplicite.
IMPORT OPTIMIZE

SUB Main()
    ' Minimi quadrati: righe (1,1) (1,2) (1,3), y = 1 + 2x -> (1, 2)
    DIM A(5) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0
    A(2) = 1.0 : A(3) = 2.0
    A(4) = 1.0 : A(5) = 3.0
    DIM b(2) AS DOUBLE
    b(0) = 3.0 : b(1) = 5.0 : b(2) = 7.0
    DIM x(1) AS DOUBLE
    CALL OPTIMIZE.LEASTSQUARES(A, 3, 2, b, x)
    PRINT "LEASTSQUARES   ="; x(0); ","; x(1)

    ' Cholesky [[4,2],[2,3]] x = [10,8] -> (1.75, 1.5)
    DIM CH(3) AS DOUBLE
    CH(0) = 4.0 : CH(1) = 2.0 : CH(2) = 2.0 : CH(3) = 3.0
    DIM bc(1) AS DOUBLE
    bc(0) = 10.0 : bc(1) = 8.0
    DIM xc(1) AS DOUBLE
    CALL OPTIMIZE.CHOLESKYSOLVE(CH, 2, bc, xc)
    PRINT "CHOLESKYSOLVE ="; xc(0); ","; xc(1)

    ' SIMPLEX: max 2x+3y, x+y<=4, x<=2 -> (0,4) obiettivo 12
    DIM c(1) AS DOUBLE
    c(0) = 2.0 : c(1) = 3.0
    DIM M(3) AS DOUBLE
    M(0) = 1.0 : M(1) = 1.0 : M(2) = 1.0 : M(3) = 0.0
    DIM bm(1) AS DOUBLE
    bm(0) = 4.0 : bm(1) = 2.0
    DIM sx(2) AS DOUBLE
    CALL OPTIMIZE.SIMPLEX(c, 2, M, 2, bm, sx)
    PRINT "SIMPLEX        ="; sx(0); ","; sx(1); " obj="; sx(2)

    ' LINPROG: min -x-y con gli stessi vincoli -> (2,2)
    DIM cmin(1) AS DOUBLE
    cmin(0) = -1.0 : cmin(1) = -1.0
    DIM xl(1) AS DOUBLE
    CALL OPTIMIZE.LINPROG(cmin, 2, M, 2, bm, xl)
    PRINT "LINPROG        ="; xl(0); ","; xl(1)

    ' LINPROGEQ: x+y con x+2y<=6, x=y -> (2,2)
    DIM Aub(1) AS DOUBLE
    Aub(0) = 1.0 : Aub(1) = 2.0
    DIM bub(0) AS DOUBLE
    bub(0) = 6.0
    DIM Aeq(1) AS DOUBLE
    Aeq(0) = 1.0 : Aeq(1) = -1.0
    DIM beq(0) AS DOUBLE
    beq(0) = 0.0
    CALL OPTIMIZE.LINPROGEQ(cmin, 2, Aub, 1, bub, Aeq, 1, beq, xl)
    PRINT "LINPROGEQ      ="; xl(0); ","; xl(1)
END SUB
