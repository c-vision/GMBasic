' DECLARE NATIVE: ABI esatta come dichiarata — nessuna estensione automatica degli array.
' La funzione C `sum2(double*, int64 d0, int64 d1)` riceve le dimensioni ESPLICITE dichiarate:
' un DECLARE normale emetterebbe da solo le estensioni dopo il puntatore, un DECLARE NATIVE no.
' EXPECT: s2= 10|s3= 36
DECLARE NATIVE FUNCTION Sum2 LIB "libdims_ext" ALIAS "sum2" (a() AS DOUBLE, d0 AS LONG, d1 AS LONG) AS DOUBLE
PUBLIC DECLARE NATIVE FUNCTION Sum3 LIB "libdims_ext" ALIAS "sum3" (a() AS DOUBLE, d0 AS LONG, d1 AS LONG, d2 AS LONG) AS DOUBLE

SUB Main()
    DIM m(1, 1) AS DOUBLE
    m(0, 0) = 1 : m(0, 1) = 2 : m(1, 0) = 3 : m(1, 1) = 4
    PRINT "s2="; Sum2(m, 2, 2)
    DIM c(1, 1, 1) AS DOUBLE
    DIM v AS DOUBLE
    v = 1
    FOR i = 0 TO 1
        FOR j = 0 TO 1
            FOR k = 0 TO 1
                c(i, j, k) = v
                v = v + 1
            NEXT k
        NEXT j
    NEXT i
    PRINT "s3="; Sum3(c, 2, 2, 2)
END SUB
