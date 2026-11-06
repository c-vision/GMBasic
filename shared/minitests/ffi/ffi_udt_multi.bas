' UDT oltre le 2 foglie (S0, 2026-11-06): struct a 3 e 8 campi, kind misto come PARAMETRO
' e come RITORNO. Prima del S0 il compilatore rifiutava ogni UDT con più di 2 campi-foglia.
' Libreria: shared/minitests/ffi/udt_ext.c → libudt_ext.dylib.
' EXPECT: s1= 8.5|s2= 27.5|r1.A= 2|r1.B= 4|r1.C= 30|os= 8|o0= 2|o7= 9|i1= 6|i2.A= 4|i2.C= 12
TYPE Mixed3
    A AS DOUBLE
    B AS LONG
    C AS DOUBLE
END TYPE

TYPE Oct8
    F0 AS DOUBLE
    F1 AS DOUBLE
    F2 AS DOUBLE
    F3 AS DOUBLE
    F4 AS DOUBLE
    F5 AS DOUBLE
    F6 AS DOUBLE
    F7 AS DOUBLE
END TYPE

TYPE Int3
    A AS LONG
    B AS LONG
    C AS LONG
END TYPE

DECLARE FUNCTION Mix3Sum LIB "libudt_ext" ALIAS "mix3_sum" (x AS Mixed3) AS DOUBLE
DECLARE FUNCTION Mix3Add LIB "libudt_ext" ALIAS "mix3_add" (x AS Mixed3, y AS Mixed3) AS Mixed3
DECLARE FUNCTION Oct8Sum LIB "libudt_ext" ALIAS "oct8_sum" (o AS Oct8) AS DOUBLE
DECLARE FUNCTION Oct8Inc LIB "libudt_ext" ALIAS "oct8_inc" (o AS Oct8) AS Oct8
DECLARE FUNCTION Int3Sum LIB "libudt_ext" ALIAS "int3_sum" (v AS Int3) AS LONG
DECLARE FUNCTION Int3Scale LIB "libudt_ext" ALIAS "int3_scale" (v AS Int3, k AS LONG) AS Int3

SUB Main()
    DIM m AS Mixed3
    m.A = 1.5 : m.B = 2 : m.C = 5
    PRINT "s1="; Mix3Sum(m)

    DIM n AS Mixed3
    n.A = 0.5 : n.B = 2 : n.C = 25
    PRINT "s2="; Mix3Sum(n)

    DIM r AS Mixed3
    r = Mix3Add(m, n)
    PRINT "r1.A="; r.A
    PRINT "r1.B="; r.B
    PRINT "r1.C="; r.C

    DIM o AS Oct8
    o.F0 = 1 : o.F1 = 1 : o.F2 = 1 : o.F3 = 1
    o.F4 = 1 : o.F5 = 1 : o.F6 = 1 : o.F7 = 1
    PRINT "os="; Oct8Sum(o)

    DIM p AS Oct8
    p = Oct8Inc(o)
    PRINT "o0="; p.F0
    PRINT "o7="; p.F7

    DIM v AS Int3
    v.A = 1 : v.B = 2 : v.C = 3
    PRINT "i1="; Int3Sum(v)

    DIM w AS Int3
    w = Int3Scale(v, 4)
    PRINT "i2.A="; w.A
    PRINT "i2.C="; w.C
END SUB
