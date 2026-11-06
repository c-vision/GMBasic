' 2-D: matrice 2x2 -> il compilatore emette d0=2, d1=2 dopo il puntatore (row-major).
' EXPECT: s2= 10
DECLARE FUNCTION SUM2I LIB "libdims_ext" ALIAS "sum2" (a() AS DOUBLE) AS DOUBLE

SUB Main()
    DIM m(1, 1) AS DOUBLE
    m(0, 0) = 1 : m(0, 1) = 2
    m(1, 0) = 3 : m(1, 1) = 4
    PRINT "s2="; SUM2I(m)
END SUB
