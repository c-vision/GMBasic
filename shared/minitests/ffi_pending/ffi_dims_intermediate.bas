' @heavy   (pending: richiede il descrittore {rank, extents} NEL VALORE array)
' Caso chiave: l'array attraversa una SUB intermedia; il rango non e' perso perche'
' viaggia col valore. E' il motivo per cui il piano prevede il descrittore (non solo
' l'emissione al call site FFI).
' EXPECT: s2= 10|s3= 36
DECLARE FUNCTION SUM2I LIB "libdims_ext" ALIAS "sum2" (a() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION SUM3I LIB "libdims_ext" ALIAS "sum3" (a() AS DOUBLE) AS DOUBLE

SUB Forward2(m() AS DOUBLE)
    PRINT "s2="; SUM2I(m)
END SUB

SUB Forward3(c() AS DOUBLE)
    PRINT "s3="; SUM3I(c)
END SUB

SUB Main()
    DIM m(1, 1) AS DOUBLE
    m(0, 0) = 1 : m(0, 1) = 2 : m(1, 0) = 3 : m(1, 1) = 4
    Forward2(m)

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
    Forward3(c)
END SUB
