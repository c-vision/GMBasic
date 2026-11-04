IMPORT OPTIMIZE

SUB Main()
    PRINT "newton="; OPTIMIZE.NEWTON(FN(x AS DOUBLE) AS DOUBLE => x * x - 2.0, 1.0)
    PRINT "brentq="; OPTIMIZE.BRENTQ(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 8.0, 1.0, 3.0)

    DIM c(1) AS DOUBLE
    c(0) = 2.0 : c(1) = 3.0
    DIM A(3) AS DOUBLE
    A(0) = 1.0 : A(1) = 1.0 : A(2) = 1.0 : A(3) = 0.0
    DIM b(1) AS DOUBLE
    b(0) = 4.0 : b(1) = 2.0
    DIM sx(2) AS DOUBLE
    CALL OPTIMIZE.SIMPLEX(c, 2, A, 2, b, sx)
    PRINT "simplex="; sx(0); ","; sx(1); " obj="; sx(2)
END SUB
