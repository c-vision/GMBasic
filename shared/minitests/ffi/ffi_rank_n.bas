' Array/matrici a rango N via FFI (S2, 2026-11-06): il buffer viaggia piatto row-major,
' le estensioni d0..dN escono dal valore array (a() invariata, nessun argomento esplicito).
' Copre rango 2, 3 (anche copy-back in place) e 4.
' EXPECT: s2= 10|s3= 36|s4= 136|k0= 2|k7= 16
DECLARE FUNCTION Sum2 LIB "libdims_ext" ALIAS "sum2" (a() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION Sum3 LIB "libdims_ext" ALIAS "sum3" (a() AS DOUBLE) AS DOUBLE
DECLARE SUB Scale3 LIB "libdims_ext" ALIAS "scale3" (a() AS DOUBLE, k AS DOUBLE)
DECLARE FUNCTION Sum4 LIB "libdims_ext" ALIAS "sum4" (a() AS DOUBLE) AS DOUBLE

SUB Main()
    ' matrice 2-D 2x2 = 1,2,3,4 -> 10
    DIM m(1, 1) AS DOUBLE
    m(0, 0) = 1 : m(0, 1) = 2 : m(1, 0) = 3 : m(1, 1) = 4
    PRINT "s2="; Sum2(m)

    ' cubo 2x2x2 = 1..8 -> 36
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
    PRINT "s3="; Sum3(c)

    ' rango 4: 2x2x2x2 con valori 1..16 -> 136
    DIM q(1, 1, 1, 1) AS DOUBLE
    v = 1
    FOR i = 0 TO 1
        FOR j = 0 TO 1
            FOR k = 0 TO 1
                FOR l = 0 TO 1
                    q(i, j, k, l) = v
                    v = v + 1
                NEXT l
            NEXT k
        NEXT j
    NEXT i
    PRINT "s4="; Sum4(q)

    ' copy-back in place sul cubo: raddoppia -> c(0,0,0)=2 ... c(1,1,1)=16
    CALL Scale3(c, 2)
    PRINT "k0="; c(0, 0, 0)
    PRINT "k7="; c(1, 1, 1)
END SUB
