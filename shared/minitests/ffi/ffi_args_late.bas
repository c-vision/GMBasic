' Parametri UDT / ARRAY / STRING in POSIZIONE >= 9 (matrice F5: i vecchi limiti `i < 8`).
' EXPECT: u= 39|a= 43|s= 136
TYPE Arr4
    A(4) AS LONG
    B AS DOUBLE
END TYPE

DECLARE FUNCTION UdtAt9 LIB "liblayout_ext" ALIAS "udt_at9" (a1 AS DOUBLE, a2 AS DOUBLE, a3 AS DOUBLE, a4 AS DOUBLE, a5 AS DOUBLE, a6 AS DOUBLE, a7 AS DOUBLE, a8 AS DOUBLE, v AS Arr4) AS DOUBLE
DECLARE FUNCTION ArrAt9 LIB "liblayout_ext" ALIAS "arr_at9" (a1 AS DOUBLE, a2 AS DOUBLE, a3 AS DOUBLE, a4 AS DOUBLE, a5 AS DOUBLE, a6 AS DOUBLE, a7 AS DOUBLE, a8 AS DOUBLE, buf() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE FUNCTION StrAt9 LIB "liblayout_ext" ALIAS "str_at9" (a1 AS DOUBLE, a2 AS DOUBLE, a3 AS DOUBLE, a4 AS DOUBLE, a5 AS DOUBLE, a6 AS DOUBLE, a7 AS DOUBLE, a8 AS DOUBLE, s AS STRING) AS DOUBLE

SUB Main()
    DIM v AS Arr4
    v.A(0) = 1 : v.B = 2
    PRINT "u="; UdtAt9(1, 2, 3, 4, 5, 6, 7, 8, v)

    DIM b(1) AS DOUBLE
    b(0) = 3 : b(1) = 4
    PRINT "a="; ArrAt9(1, 2, 3, 4, 5, 6, 7, 8, b, 2)

    PRINT "s="; StrAt9(1, 2, 3, 4, 5, 6, 7, 8, "xyz")
END SUB
