' EXPECT: lstsq= 1.5  ,  0.3333|chol= 1.75  ,  1.5|simplex= 0  ,  0|lp= 1  ,  1.5
' Forme JAGGED: stesse matrici del builtin (ARRAY OF ARRAY), nessuna dimensione esplicita.
IMPORT OPTIMIZE
SUB Main()
    DIM A(2, 1) AS DOUBLE
    A = {{1.0, 1.0}, {2.0, 1.0}, {3.0, 1.0}}
    DIM b(2) AS DOUBLE = {2.0, 3.0, 5.0}
    DIM x() = OPTIMIZE.LEASTSQUARES(A, b)
    PRINT "lstsq="; MATH.ROUND(x(0), 4); " , "; MATH.ROUND(x(1), 4)
    DIM C(1, 1) AS DOUBLE
    C = {{4.0, 2.0}, {2.0, 3.0}}
    DIM d(1) AS DOUBLE = {10.0, 8.0}
    DIM ch() = OPTIMIZE.CHOLESKYSOLVE(C, d)
    PRINT "chol="; MATH.ROUND(ch(0), 4); " , "; MATH.ROUND(ch(1), 4)
    DIM Aub(1, 1) AS DOUBLE
    Aub = {{1.0, 2.0}, {3.0, 2.0}}
    DIM bub(1) AS DOUBLE = {4.0, 6.0}
    DIM cv(1) AS DOUBLE = {-1.0, -1.0}
    DIM sp() = OPTIMIZE.SIMPLEX(cv, Aub, bub)
    PRINT "simplex="; MATH.ROUND(sp(0), 4); " , "; MATH.ROUND(sp(1), 4)
    DIM lp() = OPTIMIZE.LINPROG(cv, Aub, bub)
    PRINT "lp="; MATH.ROUND(lp(0), 4); " , "; MATH.ROUND(lp(1), 4)
END SUB
