' UDT con 12 campi (S1, 2026-11-06): il vecchio limite era 8 foglie (maschera kind a 8 bit);
' ora il conteggio viaggia su 16 bit e il layout è esplicito.
' EXPECT: sz= 48|sum= 78|inc0= 2|inc11= 13
TYPE Big12
    F0 AS LONG
    F1 AS LONG
    F2 AS LONG
    F3 AS LONG
    F4 AS LONG
    F5 AS LONG
    F6 AS LONG
    F7 AS LONG
    F8 AS LONG
    F9 AS LONG
    F10 AS LONG
    F11 AS LONG
END TYPE

DECLARE FUNCTION Big12Size LIB "liblayout_ext" ALIAS "big12_size" () AS LONG
DECLARE FUNCTION Big12Sum LIB "liblayout_ext" ALIAS "big12_sum" (b AS Big12) AS DOUBLE
DECLARE FUNCTION Big12Inc LIB "liblayout_ext" ALIAS "big12_inc" (b AS Big12) AS Big12

SUB Main()
    PRINT "sz="; Big12Size()

    DIM b AS Big12
    b.F0 = 1 : b.F1 = 2 : b.F2 = 3 : b.F3 = 4 : b.F4 = 5 : b.F5 = 6
    b.F6 = 7 : b.F7 = 8 : b.F8 = 9 : b.F9 = 10 : b.F10 = 11 : b.F11 = 12
    PRINT "sum="; Big12Sum(b)

    DIM r AS Big12
    r = Big12Inc(b)
    PRINT "inc0="; r.F0
    PRINT "inc11="; r.F11
END SUB
