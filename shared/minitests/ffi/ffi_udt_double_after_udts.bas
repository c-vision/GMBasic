' DOUBLE dopo due UDT piccoli (16 byte) passati per valore: il DOUBLE deve raggiungere il nativo.
' EXPECT: i16= 373 |m16= 373
TYPE I16
    A AS QUAD
    B AS QUAD
END TYPE
TYPE M16
    K AS QUAD
    V AS DOUBLE
END TYPE
DECLARE FUNCTION I16PairTol LIB "libudt_ext" ALIAS "i16_pair_tol" (p AS I16, q AS I16, tol AS DOUBLE) AS DOUBLE
DECLARE FUNCTION M16PairTol LIB "libudt_ext" ALIAS "m16_pair_tol" (p AS M16, q AS M16, tol AS DOUBLE) AS DOUBLE

SUB Main()
    DIM p AS I16
    DIM q AS I16
    p.A = 5 : p.B = 7 : q.A = 2 : q.B = 5
    PRINT "i16="; I16PairTol(p, q, 3.5)
    DIM m AS M16
    DIM n AS M16
    m.K = 5 : m.V = 7 : n.K = 2 : n.V = 5
    PRINT "m16="; M16PairTol(m, n, 3.5)
END SUB
